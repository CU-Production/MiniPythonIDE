#pragma once

#ifdef ENABLE_DEBUGGER

#include "dap_client.h"
#include "../../3rd_party/nlohmann/json.hpp"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <atomic>
#include <functional>
#include <memory>
#include <SDL3/SDL.h>

using json = nlohmann::json;

// Stack frame information
struct DebugStackFrame {
    std::string filename;
    int lineno;
    std::string function_name;
};

// Variable information (compatible with UI)
struct DebugVariable {
    std::string name;
    std::string value;
    std::string type;
    std::vector<DebugVariable> children;
    bool has_children;
    bool children_loaded;
    int variables_reference;  // For lazy loading
    
    DebugVariable() : has_children(false), children_loaded(false), variables_reference(0) {}
};

class Debugger {
public:
    Debugger();
    ~Debugger();

    // Start debugging: launches pkpy with debug flag
    bool Start(const std::string& script, const std::string& filename,
               std::function<void(const std::string&)> logCallback);
    
    // Stop debugging
    void Stop();

    // Breakpoint management
    void AddBreakpoint(const std::string& filename, int line);
    void RemoveBreakpoint(const std::string& filename, int line);
    void ClearBreakpoints();
    bool HasBreakpoint(const std::string& filename, int line) const;
    const std::set<int>& GetBreakpoints(const std::string& filename) const;
    void SyncBreakpoints(); // Sync to DAP server

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
    
    // Get variable tree version (increments on each step to reset UI state)
    int GetVariableTreeVersion() const { return m_variableTreeVersion; }
    
    // Convert debug variables to JSON format (for JSON tree viewer)
    json VariablesToJson(const std::vector<DebugVariable>& vars);
    
    // Request to expand a variable's children (for lazy loading)
    void RequestExpandVariable(int variablesReference);

private:
    void OnDAPStopped(const std::string& reason, int threadId, const std::string& file, int line);
    void OnDAPContinued(int threadId);
    void OnDAPTerminated();
    void OnDAPOutput(const std::string& output);
    void OnDAPInitialized();
    
    void UpdateDebugInfo();
    void ConvertDAPVariables(const std::vector<DAPVariable>& dapVars, std::vector<DebugVariable>& outVars);
    void UpdateVariableChildren(int variablesReference);
    bool UpdateVariableChildrenInTree(std::vector<DebugVariable>& vars, int variablesReference, const std::vector<DebugVariable>& children);
    
    std::atomic<bool> m_debugging;
    std::atomic<bool> m_paused;
    
    std::string m_currentFile;
    int m_currentLine;
    std::string m_originalFilename;  // The original filename passed to Start()
    
    // Breakpoints: filename -> set of line numbers
    std::map<std::string, std::set<int>> m_breakpoints;
    
    // Stack and variables
    std::vector<DebugStackFrame> m_stackFrames;
    std::vector<DebugVariable> m_localVariables;
    std::vector<DebugVariable> m_globalVariables;
    int m_variableTreeVersion;  // Incremented on each stop to reset UI state
    
    // Logging callback
    std::function<void(const std::string&)> m_logCallback;
    
    // DAP client
    std::unique_ptr<DAPClient> m_dapClient;
    
    // Process handle for pkpy
    SDL_Process* m_process;
    std::string m_tempScriptPath;
};

#endif // ENABLE_DEBUGGER
