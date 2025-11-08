#pragma once

#include "TextEditor.h"
#include "imgui.h"
#include <filesystem>
#include <functional>
#include <set>
#include <unordered_set>

// Simple wrapper class for TextEditor
class Editor
{
public:
    Editor();
    ~Editor();
    
    void LoadFile(const std::filesystem::path& path);
    void SaveFile(const std::filesystem::path& path);
    std::string GetText() const;
    void SetText(const std::string& text);
    void Render(const char* title = "Text Editor", const ImVec2& size = ImVec2(), bool border = false);
    
    TextEditor& GetTextEditor() { return m_textEditor; }
    const std::filesystem::path& GetCurrentFile() const { return m_currentFile; }
    
    // Breakpoint callback - called when breakpoint is toggled in editor
    void SetBreakpointCallback(std::function<void(int line, bool added)> callback) {
        m_breakpointCallback = callback;
    }
    
    // Sync breakpoints from external source (e.g., Debugger)
    void SyncBreakpoints(const std::set<int>& breakpoints);
    
    // Debug current line (where debugger is paused)
    void SetDebugCurrentLine(int line) { m_textEditor.SetDebugCurrentLine(line); }
    void ClearDebugCurrentLine() { m_textEditor.ClearDebugCurrentLine(); }
    
private:
    TextEditor m_textEditor;
    std::filesystem::path m_currentFile;
    std::unordered_set<int> m_lastKnownBreakpoints;  // Use unordered_set to match TextEditor::Breakpoints
    std::function<void(int line, bool added)> m_breakpointCallback;
};
