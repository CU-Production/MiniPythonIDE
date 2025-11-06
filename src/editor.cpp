#include "editor.h"
#include <fstream>
#include <sstream>

Editor::Editor()
{
    // Set language to Python with full syntax highlighting support
    auto lang = TextEditor::LanguageDefinition::Python();
    m_textEditor.SetLanguageDefinition(lang);
    
    // Set default style (dark theme)
    m_textEditor.SetPalette(TextEditor::GetDarkPalette());
    
    // Show line numbers
    m_textEditor.SetShowWhitespaces(false);
    
    // Enable auto-indentation
    m_textEditor.SetTabSize(4);
    
    // Ensure editor is not in read-only mode
    m_textEditor.SetReadOnly(false);
    
    // Ensure keyboard and mouse input handling is enabled
    m_textEditor.SetHandleKeyboardInputs(true);
    m_textEditor.SetHandleMouseInputs(true);
}

Editor::~Editor()
{
}

void Editor::LoadFile(const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (file)
    {
        std::stringstream buffer;
        buffer << file.rdbuf();
        m_textEditor.SetText(buffer.str());
        m_currentFile = path;
    }
}

void Editor::SaveFile(const std::filesystem::path& path)
{
    std::ofstream file(path);
    if (file)
    {
        auto text = m_textEditor.GetText();
        file << text;
        m_currentFile = path;
    }
}

std::string Editor::GetText() const
{
    return m_textEditor.GetText();
}

void Editor::SetText(const std::string& text)
{
    m_textEditor.SetText(text);
}

void Editor::Render(const char* title, const ImVec2& size, bool border)
{
    m_textEditor.Render(title, size, border);
}
