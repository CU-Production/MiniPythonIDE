#include "dap_client.h"

#ifdef ENABLE_DEBUGGER

#include <json.hpp>
using json = nlohmann::json;

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
    #define close closesocket
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <unistd.h>
#endif

#include <sstream>
#include <iostream>

DAPClient::DAPClient()
    : m_socket(-1)
    , m_connected(false)
    , m_initialized(false)
    , m_stopped(false)
    , m_nextSeq(1)
    , m_currentLine(-1)
    , m_currentThreadId(0)
    , m_currentFrameId(0)
{
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

DAPClient::~DAPClient() {
    Disconnect();
#ifdef _WIN32
    WSACleanup();
#endif
}

bool DAPClient::Connect(const std::string& host, int port) {
    if (m_connected.load()) {
        return false;
    }
    
    // Create socket
    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket < 0) {
        return false;
    }
    
    // Setup address
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    
    // Convert IP address
    if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0) {
        close(m_socket);
        m_socket = -1;
        return false;
    }
    
    // Connect
    if (connect(m_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        close(m_socket);
        m_socket = -1;
        return false;
    }
    
    m_connected.store(true);
    
    // Start receive thread
    m_receiveThread = std::thread(&DAPClient::ReceiveThread, this);
    
    return true;
}

void DAPClient::Disconnect() {
    if (!m_connected.load()) {
        return;
    }
    
    m_connected.store(false);
    
    if (m_socket >= 0) {
        close(m_socket);
        m_socket = -1;
    }
    
    if (m_receiveThread.joinable()) {
        m_receiveThread.join();
    }
}

void DAPClient::ReceiveThread() {
    while (m_connected.load()) {
        std::string message = ReadMessage();
        if (message.empty()) {
            // Connection closed or error
            m_connected.store(false);
            if (OnTerminated) {
                OnTerminated();
            }
            break;
        }
        
        try {
            json msg = json::parse(message);
            ProcessMessage(msg);
        } catch (const std::exception& e) {
            std::cerr << "Error parsing message: " << e.what() << std::endl;
        }
    }
}

std::string DAPClient::ReadMessage() {
    // Read Content-Length header
    std::string headers;
    int contentLength = -1;
    
    while (true) {
        char c;
        int n = recv(m_socket, &c, 1, 0);
        if (n <= 0) {
            return "";
        }
        
        headers += c;
        
        // Check for end of headers (\r\n\r\n)
        if (headers.size() >= 4 && 
            headers.substr(headers.size() - 4) == "\r\n\r\n") {
            break;
        }
    }
    
    // Parse Content-Length
    size_t pos = headers.find("Content-Length: ");
    if (pos != std::string::npos) {
        pos += 16;
        size_t endPos = headers.find("\r\n", pos);
        std::string lenStr = headers.substr(pos, endPos - pos);
        contentLength = std::stoi(lenStr);
    }
    
    if (contentLength <= 0) {
        return "";
    }
    
    // Read content
    std::string content;
    content.resize(contentLength);
    int totalRead = 0;
    
    while (totalRead < contentLength) {
        int n = recv(m_socket, &content[totalRead], contentLength - totalRead, 0);
        if (n <= 0) {
            return "";
        }
        totalRead += n;
    }
    
    return content;
}

bool DAPClient::SendDAPMessage(const json& msg) {
    std::lock_guard<std::mutex> lock(m_sendMutex);
    
    if (!m_connected.load()) {
        return false;
    }
    
    std::string content = msg.dump();
    std::ostringstream oss;
    oss << "Content-Length: " << content.size() << "\r\n\r\n" << content;
    
    std::string message = oss.str();
    int sent = 0;
    int total = message.size();
    
    while (sent < total) {
        int n = send(m_socket, message.c_str() + sent, total - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += n;
    }
    
    return true;
}

bool DAPClient::SendRequestInternal(const std::string& command) {
    json msg = {
        {"seq", m_nextSeq},
        {"type", "request"},
        {"command", command}
    };
    
    m_nextSeq++;
    return SendDAPMessage(msg);
}

bool DAPClient::SendRequestInternal(const std::string& command, const json& arguments) {
    json msg = {
        {"seq", m_nextSeq},
        {"type", "request"},
        {"command", command}
    };
    
    if (!arguments.is_null()) {
        msg["arguments"] = arguments;
    }
    
    m_nextSeq++;
    return SendDAPMessage(msg);
}

bool DAPClient::SendRequest(const std::string& command) {
    return SendRequestInternal(command);
}

bool DAPClient::SendRequest(const std::string& command, const json& arguments) {
    return SendRequestInternal(command, arguments);
}

void DAPClient::ProcessMessage(const json& msg) {
    std::string type = msg["type"];
    
    if (type == "event") {
        std::string event = msg["event"];
        json body = msg.value("body", json::object());
        ProcessEvent(event, body);
    } else if (type == "response") {
        int requestId = msg["request_seq"];
        ProcessResponse(requestId, msg);
    }
}

void DAPClient::ProcessEvent(const std::string& event, const json& body) {
    if (event == "output") {
        if (OnOutput && body.contains("output")) {
            OnOutput(body["output"]);
        }
    } else if (event == "stopped") {
        m_stopped.store(true);
        m_stoppedReason = body.value("reason", "");
        m_currentThreadId = body.value("threadId", 0);
        
        // Request stack trace to get current location
        // The stopped event doesn't include file/line info directly
        json stackTraceArgs = {
            {"threadId", m_currentThreadId},
            {"startFrame", 0},
            {"levels", 1}  // Only get top frame
        };
        
        // Send stackTrace request and handle response
        int seq = m_nextSeq++;
        {
            std::lock_guard<std::mutex> lock(m_requestMutex);
            m_pendingRequests[seq] = [this](const json& response) {
                if (response.contains("body") && response["body"].contains("stackFrames")) {
                    auto& frames = response["body"]["stackFrames"];
                    if (!frames.empty()) {
                        auto& frame = frames[0];
                        
                        // Extract line number
                        if (frame.contains("line")) {
                            m_currentLine = frame["line"];
                        }
                        
                        // Extract file path
                        if (frame.contains("source") && frame["source"].contains("path")) {
                            m_currentFile = frame["source"]["path"];
                        }
                    }
                }
                
                // Now trigger the OnStopped callback with correct info
                if (OnStopped) {
                    OnStopped(m_stoppedReason, m_currentThreadId, m_currentFile, m_currentLine);
                }
            };
        }
        
        json msg = {
            {"seq", seq},
            {"type", "request"},
            {"command", "stackTrace"},
            {"arguments", stackTraceArgs}
        };
        
        SendDAPMessage(msg);
    } else if (event == "continued") {
        m_stopped.store(false);
        if (OnContinued) {
            OnContinued(m_currentThreadId);
        }
    } else if (event == "terminated") {
        m_stopped.store(false);
        if (OnTerminated) {
            OnTerminated();
        }
    } else if (event == "initialized") {
        m_initialized.store(true);
        if (OnInitialized) {
            OnInitialized();
        }
    }
}

void DAPClient::ProcessResponse(int requestId, const json& response) {
    std::lock_guard<std::mutex> lock(m_requestMutex);
    
    auto it = m_pendingRequests.find(requestId);
    if (it != m_pendingRequests.end()) {
        if (it->second) {
            it->second(response);
        }
        m_pendingRequests.erase(it);
    }
}

bool DAPClient::Initialize() {
    json args = {
        {"clientID", "MiniPythonIDE"},
        {"clientName", "Mini Python IDE"},
        {"adapterID", "pocketpy"},
        {"linesStartAt1", true},
        {"columnsStartAt1", true},
        {"pathFormat", "path"}
    };
    
    return SendRequestInternal("initialize", args);
}

bool DAPClient::Launch(const std::string& program, const std::vector<std::string>& args) {
    json jsonArgs = {
        {"program", program},
        {"stopOnEntry", true},
        {"args", args}
    };
    
    return SendRequestInternal("launch", jsonArgs);
}

bool DAPClient::Attach(int processId) {
    json args = {
        {"processId", processId}
    };
    
    return SendRequestInternal("attach", args);
}

bool DAPClient::SetBreakpoints(const std::string& file, const std::vector<int>& lines) {
    json breakpoints = json::array();
    for (int line : lines) {
        breakpoints.push_back({{"line", line}});
    }
    
    json args = {
        {"source", {{"path", file}}},
        {"breakpoints", breakpoints},
        {"lines", lines}
    };
    
    return SendRequestInternal("setBreakpoints", args);
}

bool DAPClient::SetExceptionBreakpoints(const std::vector<std::string>& filters) {
    json args = {
        {"filters", filters}
    };
    
    return SendRequestInternal("setExceptionBreakpoints", args);
}

bool DAPClient::Continue(int threadId) {
    json args = {
        {"threadId", threadId}
    };
    
    return SendRequestInternal("continue", args);
}

bool DAPClient::Next(int threadId) {
    json args = {
        {"threadId", threadId}
    };
    
    return SendRequestInternal("next", args);
}

bool DAPClient::StepIn(int threadId) {
    json args = {
        {"threadId", threadId}
    };
    
    return SendRequestInternal("stepIn", args);
}

bool DAPClient::StepOut(int threadId) {
    json args = {
        {"threadId", threadId}
    };
    
    return SendRequestInternal("stepOut", args);
}

bool DAPClient::Pause(int threadId) {
    json args = {
        {"threadId", threadId}
    };
    
    return SendRequestInternal("pause", args);
}

bool DAPClient::StackTrace(int threadId) {
    if (threadId == 0) threadId = m_currentThreadId;
    
    json args = {
        {"threadId", threadId}
    };
    
    return SendRequestInternal("stackTrace", args);
}

bool DAPClient::Scopes(int frameId) {
    json args = {
        {"frameId", frameId}
    };
    
    return SendRequestInternal("scopes", args);
}

bool DAPClient::Variables(int variablesReference) {
    json args = {
        {"variablesReference", variablesReference}
    };
    
    return SendRequestInternal("variables", args);
}

bool DAPClient::Evaluate(const std::string& expression, int frameId) {
    json args = {
        {"expression", expression},
        {"frameId", frameId},
        {"context", "watch"}
    };
    
    return SendRequestInternal("evaluate", args);
}

bool DAPClient::DisconnectRequest() {
    json args = {
        {"restart", false},
        {"terminateDebuggee", true}
    };
    
    return SendRequestInternal("disconnect", args);
}

bool DAPClient::Terminate() {
    return SendRequestInternal("terminate");
}

#endif // ENABLE_DEBUGGER

