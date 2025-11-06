#pragma once

#include "TextEditor.h"
#include "imgui.h"
#include <filesystem>

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
    
private:
    TextEditor m_textEditor;
    std::filesystem::path m_currentFile;
};
