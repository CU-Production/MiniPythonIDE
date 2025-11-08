#pragma once

#ifdef ENABLE_DEBUGGER

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>
#include <vector>
#include <json.hpp>

// Use nlohmann::json
using json = nlohmann::json;

// DAP protocol structures
struct DAPSourceLocation {
    std::string source;
    int line;
    std::string function;
};

struct DAPVariable {
    std::string name;
    std::string value;
    std::string type;
    int variablesReference;
    bool hasChildren;
    
    std::vector<DAPVariable> children; // Cached children
};

struct DAPStackFrame {
    int id;
    std::string name;
    std::string source;
    int line;
    int column;
};

struct DAPBreakpoint {
    int id;
    bool verified;
    std::string message;
};

class DAPClient {
public:
    DAPClient();
    ~DAPClient();
    
    // Connect to debug server
    bool Connect(const std::string& host, int port);
    void Disconnect();
    bool IsConnected() const { return m_connected.load(); }
    
    // DAP protocol methods
    bool Initialize();
    bool Launch(const std::string& program, const std::vector<std::string>& args = {});
    bool Attach(int processId);
    bool SetBreakpoints(const std::string& file, const std::vector<int>& lines);
    bool SetExceptionBreakpoints(const std::vector<std::string>& filters);
    
    // Execution control
    bool Continue(int threadId = 0);
    bool Next(int threadId = 0);  // Step over
    bool StepIn(int threadId = 0);
    bool StepOut(int threadId = 0);
    bool Pause(int threadId = 0);
    
    // Data inspection
    bool StackTrace(int threadId = 0);
    bool Scopes(int frameId);
    bool Variables(int variablesReference);
    bool Evaluate(const std::string& expression, int frameId = 0);
    
    // Disconnect and terminate
    bool DisconnectRequest();
    bool Terminate();
    
    // Generic request (for configuration done, etc.)
    bool SendRequest(const std::string& command);
    bool SendRequest(const std::string& command, const json& arguments);
    
    // State getters
    bool IsStopped() const { return m_stopped.load(); }
    const std::string& GetStoppedReason() const { return m_stoppedReason; }
    int GetCurrentLine() const { return m_currentLine; }
    const std::string& GetCurrentFile() const { return m_currentFile; }
    
    const std::vector<DAPStackFrame>& GetStackFrames() const { return m_stackFrames; }
    const std::vector<DAPVariable>& GetLocalVariables() const { return m_localVariables; }
    const std::vector<DAPVariable>& GetGlobalVariables() const { return m_globalVariables; }
    
    // Event callbacks
    std::function<void(const std::string& output)> OnOutput;
    std::function<void(const std::string& reason, int threadId, const std::string& file, int line)> OnStopped;
    std::function<void(int threadId)> OnContinued;
    std::function<void()> OnTerminated;
    std::function<void()> OnInitialized;
    
private:
    void ReceiveThread();
    void ProcessMessage(const json& msg);
    void ProcessEvent(const std::string& event, const json& body);
    void ProcessResponse(int requestId, const json& response);
    
    bool SendRequestInternal(const std::string& command);
    bool SendRequestInternal(const std::string& command, const json& arguments);
    bool SendDAPMessage(const json& msg);
    std::string ReadMessage();
    
    // Helper methods for automatic variable fetching
    void RequestScopes(int frameId);
    void RequestVariables(int variablesReference, bool isLocal);
    void ParseVariables(const json& variables, std::vector<DAPVariable>& outVars);
    
    int m_socket;
    std::atomic<bool> m_connected;
    std::atomic<bool> m_initialized;
    std::atomic<bool> m_stopped;
    std::thread m_receiveThread;
    std::mutex m_sendMutex;
    
    int m_nextSeq;
    std::map<int, std::function<void(const json&)>> m_pendingRequests;
    std::mutex m_requestMutex;
    
    // Current state
    std::string m_stoppedReason;
    int m_currentLine;
    std::string m_currentFile;
    int m_currentThreadId;
    int m_currentFrameId;
    
    std::vector<DAPStackFrame> m_stackFrames;
    std::vector<DAPVariable> m_localVariables;
    std::vector<DAPVariable> m_globalVariables;
    std::map<int, std::vector<DAPVariable>> m_variablesCache;
};

#endif // ENABLE_DEBUGGER

