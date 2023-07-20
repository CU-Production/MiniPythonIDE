#define SOKOL_IMPL
#define SOKOL_NO_ENTRY
#define SOKOL_GLCORE33
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#include "imgui.h"
#include "util/sokol_imgui.h"
#include "TextEditor.h"
#include "pocketpy.h"

#include <fstream>
#include <streambuf>

sg_pass_action pass_action{};

bool show_test_window = true;
bool show_another_window = false;

TextEditor editor;
static const char* fileToEdit = "test.py";

pkpy::VM* vm;

void init() {
    sg_desc desc = {};
    desc.context = sapp_sgcontext();
    desc.logger.func = slog_func;
    sg_setup(&desc);

    simgui_desc_t simgui_desc = {};
    simgui_setup(&simgui_desc);

    ImGui::CreateContext();
    ImGuiIO* io = &ImGui::GetIO();
    io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;

    {
//        auto lang = TextEditor::LanguageDefinition::CPlusPlus();
        auto lang = TextEditor::LanguageDefinition::Python();
        editor.SetLanguageDefinition(lang);
        //editor.SetPalette(TextEditor::GetLightPalette());

        {
            std::ifstream t(fileToEdit);
            if (t.good())
            {
                std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
                editor.SetText(str);
            }
        }

        vm = new pkpy::VM();

        pkpy::PyObject* mod = vm->new_module("test");
        mod->attr().set("pi", pkpy::py_var(vm, 3.14));

        vm->bind_func<2>(mod, "add", [](pkpy::VM* vm, pkpy::ArgsView args){
            pkpy::i64 a = pkpy::py_cast<pkpy::i64>(vm, args[0]);
            pkpy::i64 b = pkpy::py_cast<pkpy::i64>(vm, args[1]);
            return pkpy::py_var(vm, a + b);
        });

    }
}

void frame() {
    const int width = sapp_width();
    const int height = sapp_height();

    sg_begin_default_pass(&pass_action, width, height);

    simgui_new_frame({ width, height, sapp_frame_duration(), sapp_dpi_scale() });

    {
        // When a module is recompiled, ImGui's static context will be empty. Setting it every frame
        // ensures that the context remains set.
        ImGui::SetCurrentContext(ImGui::GetCurrentContext());

        if (true) {
            // 1. Show a simple window
            // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
            static float f = 0.0f;
            ImGui::Text("Hello, world!");
            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
            ImGui::ColorEdit3("clear color", &pass_action.colors[0].clear_value.r);
            if (ImGui::Button("Test Window")) show_test_window ^= 1;
            if (ImGui::Button("Another Window")) show_another_window ^= 1;
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
                        ImGui::GetIO().Framerate);
            ImGui::Text("w: %d, h: %d, dpi_scale: %.1f", sapp_width(), sapp_height(), sapp_dpi_scale());
            if (ImGui::Button(sapp_is_fullscreen() ? "Switch to windowed" : "Switch to fullscreen")) {
                sapp_toggle_fullscreen();
            }

            // 2. Show another simple window, this time using an explicit Begin/End pair
            if (show_another_window) {
                ImGui::SetNextWindowSize(ImVec2(200, 100), ImGuiCond_FirstUseEver);
                ImGui::Begin("Another Window", &show_another_window);
                ImGui::Text("Hello");
                ImGui::End();
            }

            // 3. Show the ImGui test window. Most of the sample code is in ImGui::ShowDemoWindow()
            if (show_test_window) {
                ImGui::SetNextWindowPos(ImVec2(460, 20), ImGuiCond_FirstUseEver);
                ImGui::ShowDemoWindow();
            }
        }

        {
            auto cpos = editor.GetCursorPosition();
            ImGui::Begin("Text Editor Demo", nullptr, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_MenuBar);
            ImGui::SetWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    if (ImGui::MenuItem("Save"))
                    {
                        auto textToSave = editor.GetText();
                        /// save text....
                    }
                    if (ImGui::MenuItem("Quit", "Alt-F4"))
                        sapp_request_quit();
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Edit"))
                {
                    bool ro = editor.IsReadOnly();
                    if (ImGui::MenuItem("Read-only mode", nullptr, &ro))
                        editor.SetReadOnly(ro);
                    ImGui::Separator();

                    if (ImGui::MenuItem("Undo", "ALT-Backspace", nullptr, !ro && editor.CanUndo()))
                        editor.Undo();
                    if (ImGui::MenuItem("Redo", "Ctrl-Y", nullptr, !ro && editor.CanRedo()))
                        editor.Redo();

                    ImGui::Separator();

                    if (ImGui::MenuItem("Copy", "Ctrl-C", nullptr, editor.HasSelection()))
                        editor.Copy();
                    if (ImGui::MenuItem("Cut", "Ctrl-X", nullptr, !ro && editor.HasSelection()))
                        editor.Cut();
                    if (ImGui::MenuItem("Delete", "Del", nullptr, !ro && editor.HasSelection()))
                        editor.Delete();
                    if (ImGui::MenuItem("Paste", "Ctrl-V", nullptr, !ro && ImGui::GetClipboardText() != nullptr))
                        editor.Paste();

                    ImGui::Separator();

                    if (ImGui::MenuItem("Select all", nullptr, nullptr))
                        editor.SetSelection(TextEditor::Coordinates(), TextEditor::Coordinates(editor.GetTotalLines(), 0));

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("View"))
                {
                    if (ImGui::MenuItem("Dark palette"))
                        editor.SetPalette(TextEditor::GetDarkPalette());
                    if (ImGui::MenuItem("Light palette"))
                        editor.SetPalette(TextEditor::GetLightPalette());
                    if (ImGui::MenuItem("Retro blue palette"))
                        editor.SetPalette(TextEditor::GetRetroBluePalette());
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Debug"))
                {
                    if (ImGui::MenuItem("Run", "Ctrl-R"))
                    {
                        auto textNow = editor.GetText();
                        vm->exec(textNow, fileToEdit, pkpy::EXEC_MODE);
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Imgui Demo"))
                {
                    ImGui::MenuItem("Show Demo", NULL, &show_test_window);
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }

            ImGui::Text("%6d/%-6d %6d lines  | %s | %s | %s | %s", cpos.mLine + 1, cpos.mColumn + 1, editor.GetTotalLines(),
                        editor.IsOverwrite() ? "Ovr" : "Ins",
                        editor.CanUndo() ? "*" : " ",
                        editor.GetLanguageDefinition().mName.c_str(), fileToEdit);

            editor.Render("TextEditor");
            ImGui::End();
        }
    }

    simgui_render();

    sg_end_pass();
    sg_commit();
}

void cleanup() {
    delete vm;
    simgui_shutdown();
    sg_shutdown();
}

void input(const sapp_event* event) {
    simgui_handle_event(event);
}

int main(int argc, const char* argv[]) {
    sapp_desc desc = {};
    desc.init_cb = init;
    desc.frame_cb = frame;
    desc.cleanup_cb = cleanup,
    desc.event_cb = input,
    desc.width = 1280,
    desc.height = 720,
    desc.window_title = "triangle",
    desc.icon.sokol_default = true,
    desc.logger.func = slog_func;
    sapp_run(desc);

    return 0;
}
