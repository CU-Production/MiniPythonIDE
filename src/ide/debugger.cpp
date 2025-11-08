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
    , m_variableTreeVersion(0)
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
    
    // Set debugging flag early so callbacks work correctly
    m_debugging.store(true);
    
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
    
    if (m_logCallback) {
        m_logCallback("[info] Debugger initialization complete\n");
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
    
    // Update debug info asynchronously to avoid blocking the receive thread
    // This gives time for scopes and variables requests to complete
    std::thread([this]() {
        // Wait a bit for the async scopes/variables chain to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        UpdateDebugInfo();
    }).detach();
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
    
    // Increment version to force UI to reset TreeNode states
    // This prevents ImGui from trying to re-expand variables with old variablesReference
    m_variableTreeVersion++;
    
    // Clear old variables immediately to prevent UI from showing stale data
    m_localVariables.clear();
    m_globalVariables.clear();
    
    // Get stack frames from DAP client (already requested in stopped event)
    const auto& dapFrames = m_dapClient->GetStackFrames();
    m_stackFrames.clear();
    for (const auto& dapFrame : dapFrames) {
        DebugStackFrame frame;
        frame.filename = dapFrame.source;
        frame.lineno = dapFrame.line;
        frame.function_name = dapFrame.name;
        m_stackFrames.push_back(frame);
    }
    
    if (m_logCallback) {
        m_logCallback("[debug] Stack frames: " + std::to_string(m_stackFrames.size()) + "\n");
    }
    
    // Wait for async variable requests to complete with retry
    // Note: We already waited 200ms in OnDAPStopped before calling this
    // So variables should be ready or arriving soon
    const int maxRetries = 5;   // Reduced since we pre-waited
    const int retryDelayMs = 20; // Quick checks
    
    for (int retry = 0; retry < maxRetries; retry++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs));
        
        // Get variables from DAP client
        const auto& dapLocals = m_dapClient->GetLocalVariables();
        const auto& dapGlobals = m_dapClient->GetGlobalVariables();
        
        // If we got variables, break early
        if (!dapLocals.empty() || !dapGlobals.empty()) {
            if (m_logCallback) {
                m_logCallback("[debug] DAP locals: " + std::to_string(dapLocals.size()) + 
                             ", globals: " + std::to_string(dapGlobals.size()) + 
                             " (after " + std::to_string((retry + 1) * retryDelayMs) + "ms)\n");
            }
            
            ConvertDAPVariables(dapLocals, m_localVariables);
            ConvertDAPVariables(dapGlobals, m_globalVariables);
            
            if (m_logCallback) {
                m_logCallback("[debug] Converted locals: " + std::to_string(m_localVariables.size()) + 
                             ", globals: " + std::to_string(m_globalVariables.size()) + "\n");
            }
            return;
        }
    }
    
    // If we get here, no variables were received even after retries
    if (m_logCallback) {
        m_logCallback("[warning] No variables received (waited ~" + 
                     std::to_string(200 + maxRetries * retryDelayMs) + "ms total)\n");
    }
    
    // Variables were already cleared at the start, so UI will show empty list
}

void Debugger::ConvertDAPVariables(const std::vector<DAPVariable>& dapVars, std::vector<DebugVariable>& outVars) {
    outVars.clear();
    
    for (const auto& dapVar : dapVars) {
        DebugVariable var;
        var.name = dapVar.name;
        var.value = dapVar.value;
        var.type = dapVar.type;
        var.has_children = dapVar.hasChildren;
        var.variables_reference = dapVar.variablesReference;
        // Important: children_loaded is false initially, even if children exist
        // This ensures lazy loading works correctly after each step
        var.children_loaded = false;
        var.children.clear();  // Ensure no stale children data
        
        // Note: We don't copy children here to enforce lazy loading
        // Children will be loaded on-demand when user expands the variable
        
        outVars.push_back(var);
    }
}

void Debugger::RequestExpandVariable(int variablesReference) {
    if (!m_dapClient || !m_dapClient->IsConnected() || !m_dapClient->IsStopped()) {
        return;
    }
    
    // Capture current tree version to detect if user steps during expansion
    int currentVersion = m_variableTreeVersion;
    
    // Send the request synchronously (blocks UI but acceptable for debugger)
    if (!m_dapClient->ExpandVariable(variablesReference)) {
        return;
    }
    
    // Wait for the response with timeout (synchronous polling)
    const int maxWaitMs = 500;
    const int pollIntervalMs = 10;
    int elapsedMs = 0;
    
    const auto& cache = m_dapClient->GetVariablesCache();
    while (elapsedMs < maxWaitMs) {
        // Check if response arrived
        if (cache.find(variablesReference) != cache.end()) {
            // Check if we're still in the same debug session
            if (m_dapClient->IsStopped() && m_variableTreeVersion == currentVersion) {
                UpdateVariableChildren(variablesReference);
            }
            return;
        }
        
        // Check if user stepped or continued (version changed or not stopped)
        if (!m_dapClient->IsStopped() || m_variableTreeVersion != currentVersion) {
            // User has moved on, abort
            return;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
        elapsedMs += pollIntervalMs;
    }
    
    // Timeout - response didn't arrive in time
    if (m_logCallback) {
        m_logCallback("[warning] Timeout waiting for variable expansion\n");
    }
}

void Debugger::UpdateVariableChildren(int variablesReference) {
    if (!m_dapClient || !m_dapClient->IsStopped()) {
        return;  // Program state changed, abort update
    }
    
    // Get the updated children from DAPClient's cache
    const auto& dapCache = m_dapClient->GetVariablesCache();
    auto it = dapCache.find(variablesReference);
    
    if (it == dapCache.end()) {
        return;  // Children not loaded yet or cache was cleared
    }
    
    // Convert DAP variables to debug variables
    std::vector<DebugVariable> children;
    ConvertDAPVariables(it->second, children);
    
    // Update the variable tree in both local and global variables
    // These will silently fail if the parent variable no longer exists (after step)
    UpdateVariableChildrenInTree(m_localVariables, variablesReference, children);
    UpdateVariableChildrenInTree(m_globalVariables, variablesReference, children);
}

bool Debugger::UpdateVariableChildrenInTree(std::vector<DebugVariable>& vars, int variablesReference, const std::vector<DebugVariable>& children) {
    for (auto& var : vars) {
        if (var.variables_reference == variablesReference) {
            var.children = children;
            var.children_loaded = true;
            return true;
        }
        
        // Recursively search in children
        if (!var.children.empty()) {
            if (UpdateVariableChildrenInTree(var.children, variablesReference, children)) {
                return true;
            }
        }
    }
    return false;
}

json Debugger::VariablesToJson(const std::vector<DebugVariable>& vars) {
    json result = json::array();
    
    for (const auto& var : vars) {
        json item;
        item["name"] = var.name;
        item["value"] = var.value;
        item["type"] = var.type;
        item["variablesReference"] = var.variables_reference;
        
        // For JSON viewer: create a "display" string that combines value and type
        std::string displayValue = var.value;
        if (!var.type.empty()) {
            displayValue += " (" + var.type + ")";
        }
        item["display"] = displayValue;
        
        // If has children and they're loaded, recursively convert them
        if (!var.children.empty()) {
            item["children"] = VariablesToJson(var.children);
        }
        // If has children but not loaded, mark as expandable
        else if (var.has_children || var.variables_reference > 0) {
            item["children"] = json::array();  // Empty array indicates expandable but not loaded
            item["expandable"] = true;
        }
        
        result.push_back(item);
    }
    
    return result;
}

#endif // ENABLE_DEBUGGER
