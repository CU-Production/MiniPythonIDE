#include "debugger.h"

#ifdef ENABLE_DEBUGGER

#include <sstream>
#include <chrono>

Debugger* Debugger::s_instance = nullptr;

// Static callback for VM 1 print function
static std::function<void(const std::string&)> s_vm1_logCallback;

Debugger::Debugger()
    : m_debugging(false)
    , m_paused(false)
    , m_currentLine(-1)
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
    
    // Ensure previous thread is cleaned up
    if (m_executionThread.joinable()) {
        m_executionThread.join();
    }

    m_logCallback = logCallback;
    m_debugging.store(true);
    m_paused.store(false);
    s_instance = this;
    
    if (logCallback) {
        logCallback("[info] Starting debugger in background thread (VM 1)...\n");
    }
    
    // Start Python execution in background thread using VM 1
    // Main thread continues to use VM 0
    m_executionThread = std::thread(&Debugger::ExecuteInThread, this, code, filename);
    
    return true;
}

void Debugger::Stop() {
    if (!m_debugging.load()) {
        return;
    }
    
    m_debugging.store(false);
    
    // If currently paused, wake up the execution thread
    if (m_paused.load()) {
        m_paused.store(false);
        m_pauseCondition.notify_one();
    }
    
    // Wait for execution thread to finish
    if (m_executionThread.joinable()) {
        m_executionThread.join();
    }

    if (m_logCallback) {
        m_logCallback("[info] Debug session ended\n");
    }
    
    m_currentFile.clear();
    m_currentLine = -1;
    m_stackFrames.clear();
    m_localVariables.clear();
    m_globalVariables.clear();
    s_instance = nullptr;
}

void Debugger::AddBreakpoint(const std::string& filename, int line) {
    m_breakpoints[filename].insert(line);
    
    // Note: Breakpoints will be synced when execution thread starts
    // We can't call c11_debugger_setbreakpoint from main thread
}

void Debugger::RemoveBreakpoint(const std::string& filename, int line) {
    auto it = m_breakpoints.find(filename);
    if (it != m_breakpoints.end()) {
        it->second.erase(line);
        
        // Note: Can't modify debugger from main thread
        // Breakpoint changes only take effect for next debug session
    }
}

void Debugger::ClearBreakpoints() {
    m_breakpoints.clear();
    
    // Note: Can't modify debugger from main thread
    // Breakpoint changes only take effect for next debug session
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

void Debugger::Continue() {
    if (!m_debugging.load() || !m_paused.load()) {
        return;
    }
    
    c11_debugger_set_step_mode(C11_STEP_CONTINUE);
    m_paused.store(false);
    m_pauseCondition.notify_one(); // Wake up execution thread
}

void Debugger::StepOver() {
    if (!m_debugging.load() || !m_paused.load()) {
        return;
    }
    
    c11_debugger_set_step_mode(C11_STEP_OVER);
    m_paused.store(false);
    m_pauseCondition.notify_one(); // Wake up execution thread
}

void Debugger::StepInto() {
    if (!m_debugging.load() || !m_paused.load()) {
        return;
    }
    
    c11_debugger_set_step_mode(C11_STEP_IN);
    m_paused.store(false);
    m_pauseCondition.notify_one(); // Wake up execution thread
}

void Debugger::StepOut() {
    if (!m_debugging.load() || !m_paused.load()) {
        return;
    }
    
    c11_debugger_set_step_mode(C11_STEP_OUT);
    m_paused.store(false);
    m_pauseCondition.notify_one(); // Wake up execution thread
}

void Debugger::SyncBreakpointsToDebugger() {
    for (const auto& pair : m_breakpoints) {
        for (int line : pair.second) {
            c11_debugger_setbreakpoint(pair.first.c_str(), line);
        }
    }
}

void Debugger::UpdateDebugInfo(py_Frame* frame) {
    // Get current location
    const char* filename = py_Frame_sourceloc(frame, &m_currentLine);
    if (filename) {
        m_currentFile = filename;
    }
    
    // TODO: Parse c11_debugger_frames() and c11_debugger_scopes() output
    // to populate m_stackFrames, m_localVariables, m_globalVariables
    // For now, we just have basic file/line info
}

void Debugger::TraceCallback(py_Frame* frame, enum py_TraceEvent event) {
    if (!s_instance || !s_instance->m_debugging.load()) {
        return;
    }
    
    // Call internal debugger trace handler
    C11_DEBUGGER_STATUS status = c11_debugger_on_trace(frame, event);
    
    if (status != C11_DEBUGGER_SUCCESS) {
        // Error occurred, stop debugging
        s_instance->m_debugging.store(false);
        return;
    }
    
    // Check if we should pause
    C11_STOP_REASON reason = c11_debugger_should_pause();
    
    if (reason != C11_DEBUGGER_NOSTOP) {
        // We should pause
        s_instance->m_paused.store(true);
        s_instance->UpdateDebugInfo(frame);
        
        if (s_instance->m_logCallback) {
            std::ostringstream msg;
            msg << "[debug] Paused at " << s_instance->m_currentFile 
                << ":" << s_instance->m_currentLine;
            
            switch (reason) {
                case C11_DEBUGGER_STEP:
                    msg << " (step)";
                    break;
                case C11_DEBUGGER_BP:
                    msg << " (breakpoint)";
                    break;
                case C11_DEBUGGER_EXCEPTION:
                    msg << " (exception)";
                    break;
                default:
                    break;
            }
            msg << "\n";
            s_instance->m_logCallback(msg.str());
        }
        
        // Wait for user action using condition variable
        // This blocks the Python thread but NOT the main GUI thread
        std::unique_lock<std::mutex> lock(s_instance->m_pauseMutex);
        while (c11_debugger_should_keep_pause() && s_instance->m_debugging.load()) {
            // Wait for Continue/Step/Stop command from main thread
            s_instance->m_pauseCondition.wait(lock);
        }
        
        s_instance->m_paused.store(false);
    }
}

void Debugger::ExecuteInThread(const std::string& code, const std::string& filename) {
    // NOTE: This runs in a background thread
    // Switch to VM 1 (main thread uses VM 0)
    py_switchvm(1);
    
    // Setup callbacks for VM 1 (redirect to our log callback)
    // Store callback in static variable so we can use a non-capturing lambda
    s_vm1_logCallback = m_logCallback;
    py_callbacks()->print = [](const char* s) {
        if (s_vm1_logCallback) {
            s_vm1_logCallback(s);
        }
    };
    
    // Create test module for VM 1 if it doesn't exist
    // Try to get existing module first
    py_GlobalRef mod = py_getmodule("test");
    if (!mod) {
        // Module doesn't exist in VM 1, create it
        mod = py_newmodule("test");
        
        // Set pi attribute
        py_newfloat(py_r0(), 3.14);
        py_setdict(mod, py_name("pi"), py_r0());
        
        // Bind add function
        py_bindfunc(mod, "add", [](int argc, py_StackRef argv) -> bool {
            if (argc != 2) return TypeError("add() requires 2 arguments");
            
            py_i64 a = py_toint(py_offset(argv, 0));
            py_i64 b = py_toint(py_offset(argv, 1));
            
            // Create return value using retval
            py_newint(py_retval(), a + b);
            return true;
        });
    }
    
    // Initialize debugger (must be done AFTER py_switchvm)
    c11_debugger_init();
    
    // Sync breakpoints to internal debugger
    SyncBreakpointsToDebugger();
    
    // Set step mode to continue initially
    c11_debugger_set_step_mode(C11_STEP_CONTINUE);
    
    // Install trace callback
    py_sys_settrace(TraceCallback, true);
    
    // Execute code
    bool success = py_exec(code.c_str(), filename.c_str(), EXEC_MODE, NULL);
    
    // Remove trace callback
    py_sys_settrace(NULL, false);
    
    if (!success) {
        char* exc_msg = py_formatexc();
        if (m_logCallback && exc_msg) {
            std::string error = "[error] ";
            error += exc_msg;
            error += "\n";
            m_logCallback(error);
        }
        py_free(exc_msg);
        py_clearexc(NULL);
    }
    
    if (m_logCallback) {
        m_logCallback("[info] Python execution completed\n");
    }
    
    // Switch back to VM 0 before thread exits
    py_switchvm(0);
    
    m_debugging.store(false);
    m_paused.store(false);
}

#endif // ENABLE_DEBUGGER



