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
#include <chrono>
#include <thread>

DAPClient::DAPClient()
    : m_socket(-1)
    , m_connected(false)
    , m_initialized(false)
    , m_stopped(false)
    , m_nextSeq(1)
    , m_currentLine(-1)
    , m_currentThreadId(0)
    , m_currentFrameId(0)
    , m_localScopeRef(-1)
    , m_globalScopeRef(-1)
{
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

DAPClient::~DAPClient() {
    // Ensure disconnection
    Disconnect();
    
    // CRITICAL: Ensure thread is joined even if Disconnect() was already called
    // This prevents std::terminate() when thread is still joinable
    if (m_receiveThread.joinable()) {
        // If thread is still running, force it to stop
        m_connected.store(false);
        
        // Give it a moment to exit
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Join or detach to avoid terminate()
        if (m_receiveThread.joinable()) {
            try {
                m_receiveThread.join();
            } catch (...) {
                // If join fails, detach to prevent terminate()
                m_receiveThread.detach();
            }
        }
    }
    
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
    // Always set connected to false, even if already disconnected
    bool wasConnected = m_connected.exchange(false);
    
    // Shutdown and close socket to unblock recv()
    if (m_socket >= 0) {
        // Shutdown both directions to unblock any recv/send calls
#ifdef _WIN32
        shutdown(m_socket, SD_BOTH);
#else
        shutdown(m_socket, SHUT_RDWR);
#endif
        
        // Small delay to let recv() return
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        close(m_socket);
        m_socket = -1;
    }
    
    // Always wait for receive thread to finish, even if wasConnected was false
    // This is crucial for preventing crashes during destruction
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
            json msg = json::parse(message);
            std::cerr << "Json string : " << msg.dump() << std::endl;
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
        
        // Reset current line so stackTrace will update it
        m_currentLine = -1;
        m_currentFile = "";
        
        // Request stack trace to get current location
        StackTrace(m_currentThreadId);
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
        {"threadId", threadId},
        {"startFrame", 0},
        {"levels", 20}
    };
    
    int seq;
    {
        std::lock_guard<std::mutex> lock(m_requestMutex);
        seq = m_nextSeq++;
        m_pendingRequests[seq] = [this](const json& response) {
            m_stackFrames.clear();
            
            if (response.contains("body") && response["body"].contains("stackFrames")) {
                auto& frames = response["body"]["stackFrames"];
                
                for (const auto& frame : frames) {
                    DAPStackFrame dapFrame;
                    dapFrame.id = frame.value("id", -1);
                    dapFrame.name = frame.value("name", "");
                    dapFrame.line = frame.value("line", -1);
                    dapFrame.column = frame.value("column", 0);
                    
                    if (frame.contains("source") && frame["source"].contains("path")) {
                        dapFrame.source = frame["source"]["path"];
                    }
                    
                    m_stackFrames.push_back(dapFrame);
                    
                    // Always update current location from top frame (first frame)
                    if (m_stackFrames.size() == 1 && !dapFrame.source.empty()) {
                        m_currentLine = dapFrame.line;
                        m_currentFile = dapFrame.source;
                        m_currentFrameId = dapFrame.id;
                    }
                }
            }
            
            // After getting stack frames, trigger OnStopped callback
            if (OnStopped) {
                OnStopped(m_stoppedReason, m_currentThreadId, m_currentFile, m_currentLine);
            }
            
            // Request scopes in a separate thread to avoid deadlock
            if (!m_stackFrames.empty() && m_stackFrames[0].id >= 0) {
                int topFrameId = m_stackFrames[0].id;
                std::thread([this, topFrameId]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    Scopes(topFrameId);
                }).detach();
            }
        };
    }
    
    json msg = {
        {"seq", seq},
        {"type", "request"},
        {"command", "stackTrace"},
        {"arguments", args}
    };
    
    SendDAPMessage(msg);
    return true;
}

bool DAPClient::Scopes(int frameId) {
    json args = {
        {"frameId", frameId}
    };
    
    std::cout << "[DAP] Requesting scopes for frame " << frameId << std::endl;
    
    int seq;
    {
        std::lock_guard<std::mutex> lock(m_requestMutex);
        seq = m_nextSeq++;
        m_pendingRequests[seq] = [this](const json& response) {
            std::cout << "[DAP] Received scopes response" << std::endl;
            
            if (!response.contains("body")) {
                std::cout << "[DAP] ERROR: No 'body' in scopes response" << std::endl;
                return;
            }
            
            if (!response["body"].contains("scopes")) {
                std::cout << "[DAP] ERROR: No 'scopes' in response body" << std::endl;
                return;
            }
            
            auto& scopes = response["body"]["scopes"];
            std::cout << "[DAP] Found " << scopes.size() << " scopes" << std::endl;
            
            // Reset scope references
            m_localScopeRef = -1;
            m_globalScopeRef = -1;
            
            // Find both local and global scopes
            // Note: pocketpy's debug adapter returns Chinese names "局部变量" and "全局变量"
            for (const auto& scope : scopes) {
                std::string name = scope.value("name", "");
                std::string hint = scope.value("presentationHint", "");
                int varRef = scope.value("variablesReference", -1);
                
                if (varRef <= 0) continue;
                
                // Check for local scope (by name or hint)
                // Note: pocketpy.c returns lowercase "locals", debug_adapter.py returns Chinese "局部变量"
                if (name == "locals" || name == "局部变量" || name == "Locals" || name == "Local" || hint == "locals") {
                    m_localScopeRef = varRef;
                    std::cout << "[DAP] Found local scope '" << name << "' with ref: " << varRef << std::endl;
                }
                // Check for global scope (by name or hint)
                // Note: pocketpy.c returns lowercase "globals", debug_adapter.py returns Chinese "全局变量"
                else if (name == "globals" || name == "全局变量" || name == "Globals" || name == "Global" || hint == "globals") {
                    m_globalScopeRef = varRef;
                    std::cout << "[DAP] Found global scope '" << name << "' with ref: " << varRef << std::endl;
                }
            }
            
            // Request variables for local scope
            if (m_localScopeRef > 0) {
                std::thread([this, localRef = m_localScopeRef]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    Variables(localRef);
                }).detach();
            }
            
            // Request variables for global scope
            if (m_globalScopeRef > 0) {
                std::thread([this, globalRef = m_globalScopeRef]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    Variables(globalRef);
                }).detach();
            }
        };
    }
    
    json msg = {
        {"seq", seq},
        {"type", "request"},
        {"command", "scopes"},
        {"arguments", args}
    };
    
    SendDAPMessage(msg);
    return true;
}

bool DAPClient::Variables(int variablesReference) {
    json args = {
        {"variablesReference", variablesReference}
    };
    
    std::cout << "[DAP] Requesting variables for ref " << variablesReference << std::endl;
    
    int seq;
    {
        std::lock_guard<std::mutex> lock(m_requestMutex);
        seq = m_nextSeq++;
        m_pendingRequests[seq] = [this, variablesReference](const json& response) {
            std::cout << "[DAP] Received variables response for ref " << variablesReference << std::endl;
            
            if (!response.contains("body")) {
                std::cout << "[DAP] ERROR: No 'body' in variables response" << std::endl;
                return;
            }
            
            if (!response["body"].contains("variables")) {
                std::cout << "[DAP] ERROR: No 'variables' in response body" << std::endl;
                return;
            }
            
            auto& variables = response["body"]["variables"];
            std::cout << "[DAP] Found " << variables.size() << " variables for ref " << variablesReference << std::endl;
            
            // Determine if this is a local or global scope request
            if (variablesReference == m_localScopeRef) {
                // This is the local variables scope
                std::cout << "[DAP] Parsing local variables (ref: " << variablesReference << ")" << std::endl;
                ParseVariables(variables, m_localVariables);
            } 
            else if (variablesReference == m_globalScopeRef) {
                // This is the global variables scope
                std::cout << "[DAP] Parsing global variables (ref: " << variablesReference << ")" << std::endl;
                ParseVariables(variables, m_globalVariables);
            }
            else {
                // This is a child variable expansion request
                // Store in cache for later retrieval
                std::cout << "[DAP] Caching child variables (ref: " << variablesReference << ")" << std::endl;
                std::vector<DAPVariable> childVars;
                ParseVariables(variables, childVars);
                m_variablesCache[variablesReference] = childVars;
                
                // Update the parent variable's children
                UpdateVariableChildren(variablesReference, childVars);
            }
        };
    }
    
    json msg = {
        {"seq", seq},
        {"type", "request"},
        {"command", "variables"},
        {"arguments", args}
    };
    
    SendDAPMessage(msg);
    return true;
}

bool DAPClient::Evaluate(const std::string& expression, int frameId) {
    json args = {
        {"expression", expression},
        {"frameId", frameId},
        {"context", "watch"}
    };
    
    return SendRequestInternal("evaluate", args);
}

bool DAPClient::ExpandVariable(int variablesReference) {
    // Simply call Variables to fetch the children
    return Variables(variablesReference);
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

void DAPClient::ParseVariables(const json& variables, std::vector<DAPVariable>& outVars) {
    outVars.clear();
    
    std::cout << "[DAP] Parsing " << variables.size() << " variables" << std::endl;
    
    for (const auto& var : variables) {
        DAPVariable dapVar;
        dapVar.name = var.value("name", "");
        dapVar.value = var.value("value", "");
        dapVar.type = var.value("type", "");
        dapVar.variablesReference = var.value("variablesReference", 0);
        dapVar.hasChildren = (dapVar.variablesReference > 0);
        
        std::cout << "[DAP]   Variable: " << dapVar.name << " = " << dapVar.value 
                  << " (type: " << dapVar.type << ", hasChildren: " << dapVar.hasChildren 
                  << ", varRef: " << dapVar.variablesReference << ")" << std::endl;
        
        outVars.push_back(dapVar);
    }
    
    std::cout << "[DAP] Total variables stored: " << outVars.size() << std::endl;
}

void DAPClient::UpdateVariableChildren(int variablesReference, const std::vector<DAPVariable>& children) {
    // Helper function to recursively search and update a variable in a vector
    auto updateInVector = [&children](std::vector<DAPVariable>& vars, int varRef, auto& updateInVector_ref) -> bool {
        for (auto& var : vars) {
            if (var.variablesReference == varRef) {
                var.children = children;
                std::cout << "[DAP] Updated children for variable '" << var.name 
                         << "' (ref: " << varRef << "), added " << children.size() << " children" << std::endl;
                return true;
            }
            // Recursively search in children
            if (!var.children.empty()) {
                if (updateInVector_ref(var.children, varRef, updateInVector_ref)) {
                    return true;
                }
            }
        }
        return false;
    };
    
    // Try to find and update the variable in local variables
    if (updateInVector(m_localVariables, variablesReference, updateInVector)) {
        return;
    }
    
    // Try to find and update the variable in global variables
    if (updateInVector(m_globalVariables, variablesReference, updateInVector)) {
        return;
    }
    
    std::cout << "[DAP] Warning: Could not find parent variable with ref " << variablesReference << std::endl;
}

#endif // ENABLE_DEBUGGER

