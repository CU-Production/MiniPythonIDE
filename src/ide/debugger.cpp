#include "debugger.h"

#ifdef ENABLE_DEBUGGER

#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

Debugger::Debugger()
    : m_debugging(false)
    , m_paused(false)
    , m_currentLine(-1)
    , m_process(nullptr)
{
}

Debugger::~Debugger() {
    Stop();
}

bool Debugger::Start(const std::string& code, const std::string& filename,
                     std::function<void(const std::string&)> logCallback) {
    if (m_debugging.load()) {
        return false; // Already debugging
    }
    
    m_logCallback = logCallback;
    
    // Clear previous debug state
    m_currentFile.clear();
    m_currentLine = -1;
    m_stackFrames.clear();
    m_localVariables.clear();
    m_globalVariables.clear();
    
    // Store original filename for breakpoint mapping
    m_originalFilename = filename;
    
    // Determine the script path to debug
    std::string scriptPath;
    bool isRealFile = !filename.empty() && 
                      filename != "<string>" && 
                      filename != "<editor>" &&
                      fs::exists(filename);
    
    if (isRealFile) {
        // Use the actual file if it exists
        scriptPath = filename;
        m_currentFile = filename;
        
        // Write code to the file (in case there are unsaved changes)
        std::ofstream outFile(scriptPath);
        if (!outFile) {
            if (m_logCallback) {
                m_logCallback("[error] Failed to write to file: " + scriptPath + "\n");
            }
            return false;
        }
        outFile << code;
        outFile.close();
        
        if (m_logCallback) {
            m_logCallback("[info] Using file: " + scriptPath + "\n");
        }
    } else {
        // Create a temporary file for unnamed/unsaved code
        m_tempScriptPath = (fs::temp_directory_path() / "minipythonide_debug.py").string();
        scriptPath = m_tempScriptPath;
        m_currentFile = m_tempScriptPath;
        
        std::ofstream outFile(m_tempScriptPath);
        if (!outFile) {
            if (m_logCallback) {
                m_logCallback("[error] Failed to create temporary script file\n");
            }
            return false;
        }
        outFile << code;
        outFile.close();
        
        if (m_logCallback) {
            m_logCallback("[info] Using temporary file: " + m_tempScriptPath + "\n");
        }
    }
    
    // Launch pkpy with --debug flag using SDL_CreateProcess
    if (m_logCallback) {
        m_logCallback("[info] Launching: pkpy --debug " + scriptPath + "\n");
    }
    
    // Use SDL_CreateProcess for cross-platform process creation
    const char* args[] = {
        "pkpy",
        "--debug",
        scriptPath.c_str(),
        nullptr
    };
    
    m_process = SDL_CreateProcess(args, false);
    if (!m_process) {
        if (m_logCallback) {
            m_logCallback("[error] Failed to launch pkpy process: " + std::string(SDL_GetError()) + "\n");
        }
        return false;
    }
    
    // Register process for cleanup on exit
    extern void RegisterProcess(SDL_Process* process);
    RegisterProcess(m_process);
    
    // Create DAP client and setup callbacks first
    m_dapClient = std::make_unique<DAPClient>();
    
    // Set up callbacks
    m_dapClient->OnStopped = [this](const std::string& reason, int threadId, const std::string& file, int line) {
        OnDAPStopped(reason, threadId, file, line);
    };
    m_dapClient->OnContinued = [this](int threadId) {
        OnDAPContinued(threadId);
    };
    m_dapClient->OnTerminated = [this]() {
        OnDAPTerminated();
    };
    m_dapClient->OnOutput = [this](const std::string& output) {
        OnDAPOutput(output);
    };
    m_dapClient->OnInitialized = [this]() {
        OnDAPInitialized();
    };
    
    // Wait and retry connection to DAP server
    if (m_logCallback) {
        m_logCallback("[info] Waiting for debugger to be ready...\n");
    }
    
    const int maxRetries = 10;
    const int retryDelayMs = 500;
    bool connected = false;
    
    for (int i = 0; i < maxRetries; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs));
        
        if (m_dapClient->Connect("127.0.0.1", 6110)) {
            connected = true;
            break;
        }
        
        if (m_logCallback && i < maxRetries - 1) {
            m_logCallback("[info] Retrying connection (" + std::to_string(i + 1) + "/" + std::to_string(maxRetries) + ")...\n");
        }
    }
    
    if (!connected) {
        if (m_logCallback) {
            m_logCallback("[error] Failed to connect to debugger after " + std::to_string(maxRetries) + " attempts\n");
        }
        Stop();
        return false;
    }
    
    if (m_logCallback) {
        m_logCallback("[info] Connected to debugger\n");
    }
    
    // Send initialize request
    if (!m_dapClient->Initialize()) {
        if (m_logCallback) {
            m_logCallback("[error] Failed to send initialize request\n");
        }
        Stop();
        return false;
    }
    
    // Wait a bit for initialize response
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Send attach request (required by pocketpy DAP)
    if (m_logCallback) {
        m_logCallback("[info] Attaching to debugger...\n");
    }
    
    if (!m_dapClient->Attach(0)) {
        if (m_logCallback) {
            m_logCallback("[error] Failed to send attach request\n");
        }
        Stop();
        return false;
    }
    
    // Wait for initialized event (will be handled in callback)
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    
    m_debugging.store(true);
    
    if (m_logCallback) {
        m_logCallback("[info] Debugger started successfully\n");
    }
    
    return true;
}

void Debugger::Stop() {
    if (!m_debugging.load()) {
        return;
    }
    
    if (m_logCallback) {
        m_logCallback("[info] Stopping debugger...\n");
    }
    
    // Disconnect from DAP server
    if (m_dapClient) {
        m_dapClient->DisconnectRequest();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        m_dapClient->Disconnect();
        m_dapClient.reset();
    }
    
    // Terminate process
    if (m_process) {
        SDL_KillProcess(m_process, true);
        int exitCode = 0;
        SDL_WaitProcess(m_process, true, &exitCode);
        
        // Unregister and destroy process
        extern void UnregisterProcess(SDL_Process* process);
        UnregisterProcess(m_process);
        SDL_DestroyProcess(m_process);
        m_process = nullptr;
    }
    
    // Clean up temporary script file
    if (!m_tempScriptPath.empty()) {
        try {
            fs::remove(m_tempScriptPath);
        } catch (...) {}
        m_tempScriptPath.clear();
    }
    
    m_debugging.store(false);
    m_paused.store(false);
    
    if (m_logCallback) {
        m_logCallback("[info] Debug session ended\n");
    }
    
    // Clear debug state
    m_currentFile.clear();
    m_currentLine = -1;
    m_stackFrames.clear();
    m_localVariables.clear();
    m_globalVariables.clear();
}

void Debugger::AddBreakpoint(const std::string& filename, int line) {
    m_breakpoints[filename].insert(line);
}

void Debugger::RemoveBreakpoint(const std::string& filename, int line) {
    auto it = m_breakpoints.find(filename);
    if (it != m_breakpoints.end()) {
        it->second.erase(line);
    }
}

void Debugger::ClearBreakpoints() {
    m_breakpoints.clear();
}

bool Debugger::HasBreakpoint(const std::string& filename, int line) const {
    auto it = m_breakpoints.find(filename);
    if (it == m_breakpoints.end()) {
        return false;
    }
    return it->second.count(line) > 0;
}

const std::set<int>& Debugger::GetBreakpoints(const std::string& filename) const {
    static const std::set<int> empty;
    auto it = m_breakpoints.find(filename);
    return (it != m_breakpoints.end()) ? it->second : empty;
}

void Debugger::SyncBreakpoints() {
    if (!m_dapClient || !m_dapClient->IsConnected()) {
        return;
    }
    
    if (m_currentFile.empty()) {
        if (m_logCallback) {
            m_logCallback("[warning] No current file for breakpoint sync\n");
        }
        return;
    }
    
    // Map breakpoints from editor filename to debug filename
    // The editor uses the original filename (or "<string>"/"<editor>")
    // But DAP needs the actual file path being debugged (which might be temp file)
    
    for (const auto& pair : m_breakpoints) {
        const std::string& bpFile = pair.first;
        const std::set<int>& bpLines = pair.second;
        
        // Determine if this breakpoint file matches the current debug session
        bool isMatch = false;
        
        if (bpFile == m_currentFile) {
            // Direct match (real file debugging)
            isMatch = true;
        } else if (bpFile == m_originalFilename) {
            // Match via original filename
            isMatch = true;
        } else if ((bpFile == "<string>" || bpFile == "<editor>") && 
                   !m_originalFilename.empty() &&
                   (m_originalFilename == "<string>" || m_originalFilename == "<editor>")) {
            // Unnamed file debugging
            isMatch = true;
        }
        
        if (isMatch && !bpLines.empty()) {
            std::vector<int> lines(bpLines.begin(), bpLines.end());
            m_dapClient->SetBreakpoints(m_currentFile, lines);
            
            if (m_logCallback) {
                m_logCallback("[info] Synced " + std::to_string(lines.size()) + 
                            " breakpoint(s) for " + bpFile + 
                            " -> " + m_currentFile + "\n");
            }
        }
    }
}

void Debugger::Continue() {
    if (!m_dapClient || !m_paused.load()) {
        return;
    }
    
    m_dapClient->Continue();
}

void Debugger::StepOver() {
    if (!m_dapClient || !m_paused.load()) {
        return;
    }
    
    m_dapClient->Next();
}

void Debugger::StepInto() {
    if (!m_dapClient || !m_paused.load()) {
        return;
    }
    
    m_dapClient->StepIn();
}

void Debugger::StepOut() {
    if (!m_dapClient || !m_paused.load()) {
        return;
    }
    
    m_dapClient->StepOut();
}

void Debugger::OnDAPStopped(const std::string& reason, int threadId, const std::string& file, int line) {
    m_paused.store(true);
    
    // Map the file path (in case DAP returns temp file path)
    // Keep our original file path for UI consistency
    if (!file.empty()) {
        m_currentFile = file;
    }
    m_currentLine = line;
    
    if (m_logCallback) {
        std::ostringstream msg;
        msg << "[debug] Paused at " << m_currentFile << ":" << line << " (" << reason << ")\n";
        m_logCallback(msg.str());
    }
    
    // Request stack trace and variables
    UpdateDebugInfo();
}

void Debugger::OnDAPContinued(int threadId) {
    m_paused.store(false);
    
    if (m_logCallback) {
        m_logCallback("[debug] Execution continued\n");
    }
}

void Debugger::OnDAPTerminated() {
    m_debugging.store(false);
    m_paused.store(false);
    
    if (m_logCallback) {
        m_logCallback("[info] Program terminated\n");
    }
}

void Debugger::OnDAPOutput(const std::string& output) {
    if (m_logCallback) {
        m_logCallback(output);
    }
}

void Debugger::OnDAPInitialized() {
    if (m_logCallback) {
        m_logCallback("[info] Debugger initialized\n");
    }
    
    // Sync breakpoints
    SyncBreakpoints();
    
    // Set configuration done
    m_dapClient->SendRequest("configurationDone");
    
    // Start execution (continue from initial pause)
    if (m_logCallback) {
        m_logCallback("[info] Starting program execution...\n");
    }
    m_dapClient->Continue();
}

void Debugger::UpdateDebugInfo() {
    if (!m_dapClient || !m_dapClient->IsStopped()) {
        return;
    }
    
    // Request stack trace
    m_dapClient->StackTrace();
    
    // TODO: Wait for response and parse stack frames
    // For now, we'll create a simple stack frame from current location
    m_stackFrames.clear();
    if (!m_currentFile.empty() && m_currentLine > 0) {
        DebugStackFrame frame;
        frame.filename = m_currentFile;
        frame.lineno = m_currentLine;
        frame.function_name = "<module>";
        m_stackFrames.push_back(frame);
    }
    
    // TODO: Request scopes and variables
    // For now, clear variables
    m_localVariables.clear();
    m_globalVariables.clear();
}

void Debugger::ConvertDAPVariables(const std::vector<DAPVariable>& dapVars, std::vector<DebugVariable>& outVars) {
    outVars.clear();
    
    for (const auto& dapVar : dapVars) {
        DebugVariable var;
        var.name = dapVar.name;
        var.value = dapVar.value;
        var.type = dapVar.type;
        var.has_children = dapVar.hasChildren;
        var.children_loaded = !dapVar.children.empty();
        
        // Recursively convert children
        if (!dapVar.children.empty()) {
            ConvertDAPVariables(dapVar.children, var.children);
        }
        
        outVars.push_back(var);
    }
}

#endif // ENABLE_DEBUGGER
