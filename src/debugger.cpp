#include "debugger.h"
#include <algorithm>

Debugger* Debugger::s_instance = nullptr;

Debugger::Debugger() 
    : m_state(DebugState::Idle)
    , m_currentLine(-1)
    , m_targetFrameDepth(0)
    , m_currentFrameDepth(0)
{
    s_instance = this;
}

Debugger::~Debugger() {
    s_instance = nullptr;
}

void Debugger::AddBreakpoint(const std::string& filename, int line) {
    m_breakpoints[filename].insert(line);
}

void Debugger::RemoveBreakpoint(const std::string& filename, int line) {
    auto it = m_breakpoints.find(filename);
    if (it != m_breakpoints.end()) {
        it->second.erase(line);
        if (it->second.empty()) {
            m_breakpoints.erase(it);
        }
    }
}

void Debugger::ClearBreakpoints() {
    m_breakpoints.clear();
}

bool Debugger::HasBreakpoint(const std::string& filename, int line) const {
    auto it = m_breakpoints.find(filename);
    if (it != m_breakpoints.end()) {
        return it->second.find(line) != it->second.end();
    }
    return false;
}

const std::set<int>& Debugger::GetBreakpoints(const std::string& filename) const {
    static std::set<int> empty;
    auto it = m_breakpoints.find(filename);
    return (it != m_breakpoints.end()) ? it->second : empty;
}

void Debugger::Start() {
    m_state = DebugState::Running;
    m_currentLine = -1;
    m_currentFile.clear();
    m_currentFrameDepth = 0;
    m_stackFrames.clear();
    m_localVariables.clear();
    m_globalVariables.clear();
    
    // Install trace function
    py_sys_settrace(TraceCallback, true);
}

void Debugger::Stop() {
    m_state = DebugState::Idle;
    m_currentLine = -1;
    m_currentFile.clear();
    m_stackFrames.clear();
    m_localVariables.clear();
    m_globalVariables.clear();
    
    // Remove trace function
    py_sys_settrace(nullptr, true);
}

void Debugger::Continue() {
    if (m_state == DebugState::Paused) {
        m_state = DebugState::Running;
    }
}

void Debugger::StepOver() {
    if (m_state == DebugState::Paused) {
        m_state = DebugState::StepOver;
        m_targetFrameDepth = m_currentFrameDepth;
    }
}

void Debugger::StepInto() {
    if (m_state == DebugState::Paused) {
        m_state = DebugState::StepInto;
    }
}

void Debugger::StepOut() {
    if (m_state == DebugState::Paused && m_currentFrameDepth > 0) {
        m_state = DebugState::StepOut;
        m_targetFrameDepth = m_currentFrameDepth - 1;
    }
}

void Debugger::Pause() {
    if (m_state == DebugState::Running) {
        m_state = DebugState::Paused;
    }
}

bool Debugger::ShouldBreak(const std::string& filename, int line, int frameDepth) {
    // Check breakpoint
    if (HasBreakpoint(filename, line)) {
        return true;
    }
    
    // Check step operations
    switch (m_state) {
        case DebugState::StepInto:
            return true;
            
        case DebugState::StepOver:
            return frameDepth <= m_targetFrameDepth;
            
        case DebugState::StepOut:
            return frameDepth < m_targetFrameDepth;
            
        default:
            return false;
    }
}

std::string Debugger::GetValueString(py_Ref value) {
    if (!value || py_isnil(value)) {
        return "nil";
    }
    
    // Try to convert to string
    py_push(value);
    if (py_str(value)) {
        const char* str = py_tostr(py_retval());
        std::string result = str ? str : "<error>";
        py_pop();
        return result;
    }
    py_pop();
    
    return "<cannot display>";
}

void Debugger::UpdateVariables(py_Frame* frame) {
    m_localVariables.clear();
    m_globalVariables.clear();
    
    if (!frame) return;
    
    // Get local variables
    py_Frame_newlocals(frame, py_r0());
    py_Ref locals = py_r0();
    
    if (py_isdict(locals)) {
        // Iterate over dict items (simplified - would need proper dict iteration API)
        // For now, we'll just show that locals exist
        m_localVariables.push_back({"<locals>", "dict", "dict"});
    }
    
    // Get global variables
    py_Frame_newglobals(frame, py_r1());
    py_Ref globals = py_r1();
    
    if (py_isdict(globals)) {
        m_globalVariables.push_back({"<globals>", "dict", "dict"});
    }
}

void Debugger::UpdateStackFrames(py_Frame* frame) {
    m_stackFrames.clear();
    
    if (!frame) return;
    
    // Get current frame info
    int lineno = 0;
    const char* filename = py_Frame_sourceloc(frame, &lineno);
    
    if (filename) {
        DebugStackFrame stackFrame;
        stackFrame.filename = filename;
        stackFrame.lineno = lineno;
        
        // Try to get function name
        py_StackRef func = py_Frame_function(frame);
        if (func) {
            stackFrame.function_name = GetValueString(func);
        } else {
            stackFrame.function_name = "<module>";
        }
        
        m_stackFrames.push_back(stackFrame);
    }
}

void Debugger::TraceCallback(py_Frame* frame, enum py_TraceEvent event) {
    if (!s_instance || !frame) return;
    
    Debugger* debugger = s_instance;
    
    switch (event) {
        case TRACE_EVENT_LINE: {
            int lineno = 0;
            const char* filename = py_Frame_sourceloc(frame, &lineno);
            
            if (filename && lineno > 0) {
                std::string file = filename;
                
                // Update frame depth (simplified)
                // In a real implementation, we'd track this more accurately
                
                // Check if we should break
                if (debugger->ShouldBreak(file, lineno, debugger->m_currentFrameDepth)) {
                    debugger->m_state = DebugState::Paused;
                    debugger->m_currentFile = file;
                    debugger->m_currentLine = lineno;
                    
                    // Update variables and stack
                    debugger->UpdateVariables(frame);
                    debugger->UpdateStackFrames(frame);
                    
                    // Wait until user continues
                    while (debugger->m_state == DebugState::Paused) {
                        // In a real implementation, we'd need a proper event loop here
                        // For now, this is a placeholder - the actual pausing would need
                        // to be integrated with the main application loop
                        break;
                    }
                }
            }
            break;
        }
        
        case TRACE_EVENT_PUSH:
            debugger->m_currentFrameDepth++;
            break;
            
        case TRACE_EVENT_POP:
            if (debugger->m_currentFrameDepth > 0) {
                debugger->m_currentFrameDepth--;
            }
            break;
    }
}

