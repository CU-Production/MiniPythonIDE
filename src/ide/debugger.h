#pragma once

#ifdef ENABLE_DEBUGGER
#define PK_IS_PUBLIC_INCLUDE
#include "pocketpy.h"
#include "pocketpy_debugger_internal.h"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <atomic>
#include <functional>
#include <thread>
#include <mutex>

// Stack frame information
struct DebugStackFrame {
    std::string filename;
    int lineno;
    std::string function_name;
};

// Variable information
struct DebugVariable {
    std::string name;
    std::string value;
    std::string type;
    std::vector<DebugVariable> children;  // For expandable types (list, dict, tuple, etc.)
    bool has_children;  // Whether this variable has been expanded
    
    DebugVariable() : has_children(false) {}
};

class Debugger {
public:
    Debugger();
    ~Debugger();

    // Start debugging: executes code with debugging enabled
    bool Start(const std::string& code, const std::string& filename, 
               std::function<void(const std::string&)> logCallback);
    
    // Stop debugging
    void Stop();

    // Breakpoint management
    void AddBreakpoint(const std::string& filename, int line);
    void RemoveBreakpoint(const std::string& filename, int line);
    void ClearBreakpoints();
    bool HasBreakpoint(const std::string& filename, int line) const;
    const std::set<int>& GetBreakpoints(const std::string& filename) const;

    // Debugging control
    void Continue();
    void StepOver();
    void StepInto();
    void StepOut();

    // State queries
    bool IsDebugging() const { return m_debugging.load(); }
    bool IsPaused() const { return m_paused.load(); }
    bool IsRunning() const { return m_debugging.load() && !m_paused.load(); }
    
    // Get current execution location
    const std::string& GetCurrentFile() const { return m_currentFile; }
    int GetCurrentLine() const { return m_currentLine; }

    // Get stack frames
    const std::vector<DebugStackFrame>& GetStackFrames() const { return m_stackFrames; }
    
    // Get variables
    const std::vector<DebugVariable>& GetLocalVariables() const { return m_localVariables; }
    const std::vector<DebugVariable>& GetGlobalVariables() const { return m_globalVariables; }
    
    // Get children of a variable (for expanding collections)
    std::vector<DebugVariable> GetVariableChildren(const std::string& varName, bool isLocal) const;

    // Trace callback (called by PocketPy)
    static void TraceCallback(py_Frame* frame, enum py_TraceEvent event);

private:
    void UpdateDebugInfo(py_Frame* frame);
    void ExtractVariables(py_Ref obj, std::vector<DebugVariable>& variables, bool filter_builtins);
    void SyncBreakpointsToDebugger();
    void ExecuteInThread(const std::string& code, const std::string& filename);

    std::atomic<bool> m_debugging;
    std::atomic<bool> m_paused;
    
    // Thread synchronization for pause
    std::mutex m_pauseMutex;
    std::condition_variable m_pauseCondition;
    std::thread m_executionThread;
    
    std::string m_currentFile;
    int m_currentLine;
    
    // Breakpoints: filename -> set of line numbers
    std::map<std::string, std::set<int>> m_breakpoints;
    
    // Stack and variables
    std::vector<DebugStackFrame> m_stackFrames;
    std::vector<DebugVariable> m_localVariables;
    std::vector<DebugVariable> m_globalVariables;
    
    // Logging callback
    std::function<void(const std::string&)> m_logCallback;
    
    // Singleton instance for trace callback
    static Debugger* s_instance;
};

#endif // ENABLE_DEBUGGER
