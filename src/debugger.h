#pragma once

#include "pocketpy.h"
#include <set>
#include <string>
#include <vector>
#include <map>

// Debugger states
enum class DebugState {
    Idle,           // Not debugging
    Running,        // Running without breaks
    Paused,         // Paused at breakpoint or step
    StepOver,       // Execute next line in current frame
    StepInto,       // Step into function call
    StepOut         // Step out of current frame
};

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
};

class Debugger {
public:
    Debugger();
    ~Debugger();

    // Breakpoint management
    void AddBreakpoint(const std::string& filename, int line);
    void RemoveBreakpoint(const std::string& filename, int line);
    void ClearBreakpoints();
    bool HasBreakpoint(const std::string& filename, int line) const;
    const std::set<int>& GetBreakpoints(const std::string& filename) const;

    // Debugging control
    void Start();
    void Stop();
    void Continue();
    void StepOver();
    void StepInto();
    void StepOut();
    void Pause();

    // State queries
    bool IsDebugging() const { return m_state != DebugState::Idle; }
    bool IsPaused() const { return m_state == DebugState::Paused; }
    DebugState GetState() const { return m_state; }
    
    // Get current execution location
    const std::string& GetCurrentFile() const { return m_currentFile; }
    int GetCurrentLine() const { return m_currentLine; }

    // Get stack frames
    const std::vector<DebugStackFrame>& GetStackFrames() const { return m_stackFrames; }
    
    // Get local and global variables
    const std::vector<DebugVariable>& GetLocalVariables() const { return m_localVariables; }
    const std::vector<DebugVariable>& GetGlobalVariables() const { return m_globalVariables; }

    // Trace function callback (called by PocketPy)
    static void TraceCallback(py_Frame* frame, enum py_TraceEvent event);

private:
    void UpdateVariables(py_Frame* frame);
    void UpdateStackFrames(py_Frame* frame);
    bool ShouldBreak(const std::string& filename, int line, int frameDepth);
    std::string GetValueString(py_Ref value);

    DebugState m_state;
    std::string m_currentFile;
    int m_currentLine;
    int m_targetFrameDepth;  // For step out
    int m_currentFrameDepth;
    
    // Breakpoints: filename -> set of line numbers
    std::map<std::string, std::set<int>> m_breakpoints;
    
    // Stack and variables
    std::vector<DebugStackFrame> m_stackFrames;
    std::vector<DebugVariable> m_localVariables;
    std::vector<DebugVariable> m_globalVariables;
    
    // Singleton instance
    static Debugger* s_instance;
};

