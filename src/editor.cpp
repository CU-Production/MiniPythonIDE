
#if ZEP_SINGLE_HEADER == 1
#define ZEP_SINGLE_HEADER_BUILD
#endif

// #define ZEP_CONSOLE 
#include "editor.h"
#include <functional>
#include <filesystem>
#include "config_app.h"
#ifdef ZEP_CONSOLE
#include <zep\imgui\console_imgui.h>
#endif
namespace fs = std::filesystem;

using namespace Zep;

using cmdFunc = std::function<void(const std::vector<std::string>&)>; 
class ZepCmd : public ZepExCommand
{
public:
    ZepCmd(ZepEditor& editor, const std::string name, cmdFunc fn)
        : ZepExCommand(editor)
        , m_name(name)
        , m_func(fn)
    {
    }

    virtual void Run(const std::vector<std::string>& args) override
    {
        m_func(args);
    }

    virtual const char* ExCommandName() const override
    {
        return m_name.c_str();
    }

private:
    std::string m_name;
    cmdFunc m_func;
};

struct ZepWrapper : public Zep::IZepComponent
{
    ZepWrapper(const fs::path& root_path, const Zep::NVec2f& pixelScale, std::function<void(std::shared_ptr<Zep::ZepMessage>)> fnCommandCB)
        : zepEditor(fs::path(root_path.string()), pixelScale)
        , Callback(fnCommandCB)
    {
        zepEditor.RegisterCallback(this);

    }

    virtual Zep::ZepEditor& GetEditor() const override
    {
        return (Zep::ZepEditor&)zepEditor;
    }

    virtual void Notify(std::shared_ptr<Zep::ZepMessage> message) override
    {
        Callback(message);

        return;
    }

    virtual void HandleInput()
    {
        zepEditor.HandleInput();
    }

    Zep::ZepEditor_ImGui zepEditor;
    std::function<void(std::shared_ptr<Zep::ZepMessage>)> Callback;
};

#ifdef ZEP_CONSOLE
std::shared_ptr<ImGui::ZepConsole> spZep;
#else
std::shared_ptr<ZepWrapper> spZep;
#endif

void zep_init(const Zep::NVec2f& pixelScale)
{
#ifdef ZEP_CONSOLE
    spZep = std::make_shared<ImGui::ZepConsole>(Zep::ZepPath(APP_ROOT));
#else
    // Initialize the editor and watch for changes
    spZep = std::make_shared<ZepWrapper>("", Zep::NVec2f(pixelScale.x, pixelScale.y), [](std::shared_ptr<ZepMessage> spMessage) -> void {
    });
#endif

    // This is an example of adding different fonts for text styles.
    // If you ":e test.md" in the editor and type "# Heading 1" you will
    // see that Zep picks a different font size for the heading.
    auto& display = spZep->GetEditor().GetDisplay();
    auto pImFont = ImGui::GetIO().Fonts[0].Fonts[0];
    auto pixelHeight = pImFont->FontSize;
    display.SetFont(ZepTextType::UI, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pixelHeight)));
    display.SetFont(ZepTextType::Text, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pixelHeight)));
    display.SetFont(ZepTextType::Heading1, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pixelHeight * 1.5)));
    display.SetFont(ZepTextType::Heading2, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pixelHeight * 1.25)));
    display.SetFont(ZepTextType::Heading3, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pixelHeight * 1.125)));

}

void zep_update()
{
    // This is required to make the editor cursor blink, and for the :ZTestFlash
    // example
    if (spZep)
    {
        spZep->GetEditor().RefreshRequired();
    }
}

void zep_destroy()
{
    spZep.reset();
}

ZepEditor& zep_get_editor()
{
    return spZep->GetEditor();
}

void zep_load(const fs::path& file)
{
#ifndef ZEP_CONSOLE
    auto pBuffer = zep_get_editor().InitWithFileOrDir(file.string());
#endif
}

void zep_show(const Zep::NVec2i& displaySize, const Zep::NVec2i& displayPos)
{
    bool show = true;
#ifdef ZEP_CONSOLE
    spZep->Draw("Console", &show, ImVec4(0, 0, 500, 400), true);
    spZep->AddLog("Hello!");
#else
    ImGui::SetNextWindowSize(ImVec2(displaySize.x, displaySize.y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(displayPos.x, displayPos.y), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Zep", &show, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar))
    {
        ImGui::End();
        return;
    }

    auto min = ImGui::GetCursorScreenPos();
    auto max = ImGui::GetContentRegionAvail();
    if (max.x <= 0)
        max.x = 1;
    if (max.y <= 0)
        max.y = 1;
    ImGui::InvisibleButton("ZepContainer", max);

    // Fill the window
    max.x = min.x + max.x;
    max.y = min.y + max.y;

    spZep->zepEditor.SetDisplayRegion(Zep::NVec2f(min.x, min.y), Zep::NVec2f(max.x, max.y));
    spZep->zepEditor.Display();
    bool zep_focused = ImGui::IsWindowFocused();
    if (zep_focused)
    {
        spZep->zepEditor.HandleInput();
    }

    // TODO: A Better solution for this; I think the audio graph is creating a new window and stealing focus
    static int focus_count = 0;
    if (focus_count++ < 2)
    {
        ImGui::SetWindowFocus();
    }
    ImGui::End();
#endif
}
