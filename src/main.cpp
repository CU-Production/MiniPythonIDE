#define SOKOL_IMPL
#define SOKOL_NO_ENTRY
#define SOKOL_GLCORE33
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#include "imgui.h"
#include "util/sokol_imgui.h"
#include "pocketpy.h"
#include "../fonts/Cousine-Regular.cpp"
#include "tinyfiledialogs.h"

#include <fstream>
#include <streambuf>
#include <filesystem>
#include "config_app.h"
namespace fs = std::filesystem;
#include "editor.h"

sg_pass_action pass_action{};

bool show_console_window = false;
pkpy::VM* vm;

struct ExampleAppConsole
{
    char                  InputBuf[256];
    ImVector<char*>       Items;
    ImVector<const char*> Commands;
    ImVector<char*>       History;
    int                   HistoryPos;    // -1: new line, 0..History.Size-1 browsing history.
    ImGuiTextFilter       Filter;
    bool                  AutoScroll;
    bool                  ScrollToBottom;

    ExampleAppConsole()
    {
        //IMGUI_DEMO_MARKER("Examples/Console");
        ClearLog();
        memset(InputBuf, 0, sizeof(InputBuf));
        HistoryPos = -1;

        // "CLASSIFY" is here to provide the test case where "C"+[tab] completes to "CL" and display multiple matches.
        Commands.push_back("HELP");
        Commands.push_back("HISTORY");
        Commands.push_back("CLEAR");
        Commands.push_back("CLASSIFY");
        AutoScroll = true;
        ScrollToBottom = false;
        AddLog("Welcome to Dear ImGui!");
    }
    ~ExampleAppConsole()
    {
        ClearLog();
        for (int i = 0; i < History.Size; i++)
            free(History[i]);
    }

    // Portable helpers
    static int   Stricmp(const char* s1, const char* s2)         { int d; while ((d = toupper(*s2) - toupper(*s1)) == 0 && *s1) { s1++; s2++; } return d; }
    static int   Strnicmp(const char* s1, const char* s2, int n) { int d = 0; while (n > 0 && (d = toupper(*s2) - toupper(*s1)) == 0 && *s1) { s1++; s2++; n--; } return d; }
    static char* Strdup(const char* s)                           { IM_ASSERT(s); size_t len = strlen(s) + 1; void* buf = malloc(len); IM_ASSERT(buf); return (char*)memcpy(buf, (const void*)s, len); }
    static void  Strtrim(char* s)                                { char* str_end = s + strlen(s); while (str_end > s && str_end[-1] == ' ') str_end--; *str_end = 0; }

    void    ClearLog()
    {
        for (int i = 0; i < Items.Size; i++)
            free(Items[i]);
        Items.clear();
    }

    void    AddLog(const char* fmt, ...) IM_FMTARGS(2)
    {
        // FIXME-OPT
        char buf[1024];
        va_list args;
                va_start(args, fmt);
        vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
        buf[IM_ARRAYSIZE(buf)-1] = 0;
                va_end(args);
        Items.push_back(Strdup(buf));
    }

    void    Draw(const char* title, bool* p_open)
    {
        ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(title, p_open))
        {
            ImGui::End();
            return;
        }

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Close Console"))
                *p_open = false;
            ImGui::EndPopup();
        }

        ImGui::TextWrapped("completion (TAB key) and history (Up/Down keys).");
        ImGui::TextWrapped("Enter 'HELP' for help.");

        // TODO: display items starting from the bottom
        if (ImGui::SmallButton("Clear"))           { ClearLog(); }
        ImGui::SameLine();
        bool copy_to_clipboard = ImGui::SmallButton("Copy");
        //static float t = 0.0f; if (ImGui::GetTime() - t > 0.02f) { t = ImGui::GetTime(); AddLog("Spam %f", t); }

        ImGui::Separator();

        // Options menu
        if (ImGui::BeginPopup("Options"))
        {
            ImGui::Checkbox("Auto-scroll", &AutoScroll);
            ImGui::EndPopup();
        }

        // Options, Filter
        if (ImGui::Button("Options"))
            ImGui::OpenPopup("Options");
        ImGui::SameLine();
        Filter.Draw("Filter (\"incl,-excl\") (\"error\")", 180);
        ImGui::Separator();

        // Reserve enough left-over height for 1 separator + 1 input text
        const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar))
        {
            if (ImGui::BeginPopupContextWindow())
            {
                if (ImGui::Selectable("Clear")) ClearLog();
                ImGui::EndPopup();
            }

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
            if (copy_to_clipboard)
                ImGui::LogToClipboard();
            for (int i = 0; i < Items.Size; i++)
            {
                const char* item = Items[i];
                if (!Filter.PassFilter(item))
                    continue;

                // Normally you would store more information in your item than just a string.
                // (e.g. make Items[] an array of structure, store color/type etc.)
                ImVec4 color;
                bool has_color = false;
                if (strstr(item, "[error]")) { color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); has_color = true; }
                else if (strncmp(item, "# ", 2) == 0) { color = ImVec4(1.0f, 0.8f, 0.6f, 1.0f); has_color = true; }
                if (has_color)
                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::TextUnformatted(item);
                if (has_color)
                    ImGui::PopStyleColor();
            }
            if (copy_to_clipboard)
                ImGui::LogFinish();

            // Keep up at the bottom of the scroll region if we were already at the bottom at the beginning of the frame.
            // Using a scrollbar or mouse-wheel will take away from the bottom edge.
            if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
                ImGui::SetScrollHereY(1.0f);
            ScrollToBottom = false;

            ImGui::PopStyleVar();
        }
        ImGui::EndChild();
        ImGui::Separator();

        // Command-line
        bool reclaim_focus = false;
        ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;
        if (ImGui::InputText("Input", InputBuf, IM_ARRAYSIZE(InputBuf), input_text_flags, &TextEditCallbackStub, (void*)this))
        {
            char* s = InputBuf;
            Strtrim(s);
            if (s[0])
                ExecCommand(s);
            strcpy(s, "");
            reclaim_focus = true;
        }

        // Auto-focus on window apparition
        ImGui::SetItemDefaultFocus();
        if (reclaim_focus)
            ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget

        ImGui::End();
    }

    void    ExecCommand(const char* command_line)
    {
        AddLog("# %s\n", command_line);

        // Insert into history. First find match and delete it so it can be pushed to the back.
        // This isn't trying to be smart or optimal.
        HistoryPos = -1;
        for (int i = History.Size - 1; i >= 0; i--)
            if (Stricmp(History[i], command_line) == 0)
            {
                free(History[i]);
                History.erase(History.begin() + i);
                break;
            }
        History.push_back(Strdup(command_line));

        // Process command
        if (Stricmp(command_line, "CLEAR") == 0)
        {
            ClearLog();
        }
        else if (Stricmp(command_line, "HELP") == 0)
        {
            AddLog("Commands:");
            for (int i = 0; i < Commands.Size; i++)
                AddLog("- %s", Commands[i]);
        }
        else if (Stricmp(command_line, "HISTORY") == 0)
        {
            int first = History.Size - 10;
            for (int i = first > 0 ? first : 0; i < History.Size; i++)
                AddLog("%3d: %s\n", i, History[i]);
        }
        else
        {
            AddLog("Unknown command: '%s'\n", command_line);
        }

        // On command input, we scroll to bottom even if AutoScroll==false
        ScrollToBottom = true;
    }

    // In C++11 you'd be better off using lambdas for this sort of forwarding callbacks
    static int TextEditCallbackStub(ImGuiInputTextCallbackData* data)
    {
        ExampleAppConsole* console = (ExampleAppConsole*)data->UserData;
        return console->TextEditCallback(data);
    }

    int     TextEditCallback(ImGuiInputTextCallbackData* data)
    {
        //AddLog("cursor: %d, selection: %d-%d", data->CursorPos, data->SelectionStart, data->SelectionEnd);
        switch (data->EventFlag)
        {
            case ImGuiInputTextFlags_CallbackCompletion:
            {
                // Example of TEXT COMPLETION

                // Locate beginning of current word
                const char* word_end = data->Buf + data->CursorPos;
                const char* word_start = word_end;
                while (word_start > data->Buf)
                {
                    const char c = word_start[-1];
                    if (c == ' ' || c == '\t' || c == ',' || c == ';')
                        break;
                    word_start--;
                }

                // Build a list of candidates
                ImVector<const char*> candidates;
                for (int i = 0; i < Commands.Size; i++)
                    if (Strnicmp(Commands[i], word_start, (int)(word_end - word_start)) == 0)
                        candidates.push_back(Commands[i]);

                if (candidates.Size == 0)
                {
                    // No match
                    AddLog("No match for \"%.*s\"!\n", (int)(word_end - word_start), word_start);
                }
                else if (candidates.Size == 1)
                {
                    // Single match. Delete the beginning of the word and replace it entirely so we've got nice casing.
                    data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
                    data->InsertChars(data->CursorPos, candidates[0]);
                    data->InsertChars(data->CursorPos, " ");
                }
                else
                {
                    // Multiple matches. Complete as much as we can..
                    // So inputing "C"+Tab will complete to "CL" then display "CLEAR" and "CLASSIFY" as matches.
                    int match_len = (int)(word_end - word_start);
                    for (;;)
                    {
                        int c = 0;
                        bool all_candidates_matches = true;
                        for (int i = 0; i < candidates.Size && all_candidates_matches; i++)
                            if (i == 0)
                                c = toupper(candidates[i][match_len]);
                            else if (c == 0 || c != toupper(candidates[i][match_len]))
                                all_candidates_matches = false;
                        if (!all_candidates_matches)
                            break;
                        match_len++;
                    }

                    if (match_len > 0)
                    {
                        data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
                        data->InsertChars(data->CursorPos, candidates[0], candidates[0] + match_len);
                    }

                    // List matches
                    AddLog("Possible matches:\n");
                    for (int i = 0; i < candidates.Size; i++)
                        AddLog("- %s\n", candidates[i]);
                }

                break;
            }
            case ImGuiInputTextFlags_CallbackHistory:
            {
                // Example of HISTORY
                const int prev_history_pos = HistoryPos;
                if (data->EventKey == ImGuiKey_UpArrow)
                {
                    if (HistoryPos == -1)
                        HistoryPos = History.Size - 1;
                    else if (HistoryPos > 0)
                        HistoryPos--;
                }
                else if (data->EventKey == ImGuiKey_DownArrow)
                {
                    if (HistoryPos != -1)
                        if (++HistoryPos >= History.Size)
                            HistoryPos = -1;
                }

                // A better implementation would preserve the data on the current input line along with cursor position.
                if (prev_history_pos != HistoryPos)
                {
                    const char* history_str = (HistoryPos >= 0) ? History[HistoryPos] : "";
                    data->DeleteChars(0, data->BufTextLen);
                    data->InsertChars(0, history_str);
                }
            }
        }
        return 0;
    }
};

static ExampleAppConsole console;

void init() {
    sg_desc desc = {};
    desc.context = sapp_sgcontext();
    desc.logger.func = slog_func;
    sg_setup(&desc);

    simgui_desc_t simgui_desc = {};
    simgui_desc.no_default_font = true;
    simgui_setup(&simgui_desc);

    ImGui::CreateContext();
    ImGuiIO* io = &ImGui::GetIO();
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    //io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    //io->ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;
    //io->ConfigFlags |= ImGuiConfigFlags_ViewportsNoTaskBarIcons;
    //io->ConfigFlags |= ImGuiConfigFlags_ViewportsNoMerge;


    // IMGUI Font texture init
    if( !ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(Cousine_Regular_compressed_data, Cousine_Regular_compressed_size, 18.f) )
    {
        ImGui::GetIO().Fonts->AddFontDefault();
    }
    {

        ImGuiIO& io = ImGui::GetIO();

        // Build texture atlas
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);   // Load as RGBA 32-bit (75% of the memory is wasted, but default font is so small) because it is more likely to be compatible with user's existing shaders. If your ImTextureId represent a higher-level concept than just a GL texture id, consider calling GetTexDataAsAlpha8() instead to save on GPU memory.

        sg_image_desc img_desc{};
        img_desc.width = width;
        img_desc.height = height;
        img_desc.label = "font-texture";
        img_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
        img_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
        img_desc.min_filter = SG_FILTER_LINEAR;
        img_desc.mag_filter = SG_FILTER_LINEAR;
        img_desc.data.subimage[0][0].ptr = pixels;
        img_desc.data.subimage[0][0].size = (width * height) * sizeof(uint32_t);

        sg_image img = sg_make_image(&img_desc);

        io.Fonts->TexID = (ImTextureID)(uintptr_t)img.id;
    }

    pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    pass_action.colors[0].clear_value = {0.45f, 0.55f, 0.60f, 1.00f};

    // init pocket.py
    {
        vm = new pkpy::VM();

        pkpy::PyObject* mod = vm->new_module("test");
        mod->attr().set("pi", pkpy::py_var(vm, 3.14));

        vm->bind_func<2>(mod, "add", [](pkpy::VM* vm, pkpy::ArgsView args){
            pkpy::i64 a = pkpy::py_cast<pkpy::i64>(vm, args[0]);
            pkpy::i64 b = pkpy::py_cast<pkpy::i64>(vm, args[1]);
            return pkpy::py_var(vm, a + b);
        });

        vm->_stdout = [](pkpy::VM* vm, const pkpy::Str& s) {
            PK_UNUSED(vm);
            console.AddLog(s.c_str());
            std::cout << s;
        };
        vm->_stderr = [](pkpy::VM* vm, const pkpy::Str& s) {
            PK_UNUSED(vm);
            console.AddLog("[error] %s", s.c_str());
            std::cerr << s;
        };

    }

    // init VIM (zep editor)
    {
        // Called once the fonts/device is guaranteed setup
        zep_init(Zep::NVec2f(1.0f, 1.0f));
        zep_load(fs::path("test.py") );
    }
}

void frame() {
    const int width = sapp_width();
    const int height = sapp_height();

    sg_begin_default_pass(&pass_action, width, height);

    simgui_new_frame({ width, height, sapp_frame_duration(), sapp_dpi_scale() });

    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Open"))
                {
                    auto openFileName = tinyfd_openFileDialog(
                            "Choose a file",
                            "",
                            0,
                            nullptr,
                            nullptr,
                            0);
                    if (openFileName != nullptr)
                    {
                        auto pBuffer = zep_get_editor().GetFileBuffer(openFileName);
                        if (pBuffer)
                        {
                            zep_get_editor().EnsureWindow(*pBuffer);
                        }
                    }
                }
                ImGui::EndMenu();
            }

            const auto pBuffer = zep_get_editor().GetActiveBuffer();

            if (ImGui::BeginMenu("Settings"))
            {
                if (ImGui::BeginMenu("Editor Mode", pBuffer))
                {
                    bool enabledVim = strcmp(pBuffer->GetMode()->Name(), Zep::ZepMode_Vim::StaticName()) == 0;
                    bool enabledNormal = !enabledVim;
                    if (ImGui::MenuItem("Vim", "CTRL+2", &enabledVim))
                    {
                        zep_get_editor().SetGlobalMode(Zep::ZepMode_Vim::StaticName());
                    }
                    else if (ImGui::MenuItem("Standard", "CTRL+1", &enabledNormal))
                    {
                        zep_get_editor().SetGlobalMode(Zep::ZepMode_Standard::StaticName());
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Theme"))
                {
                    bool enabledDark = zep_get_editor().GetTheme().GetThemeType() == Zep::ThemeType::Dark ? true : false;
                    bool enabledLight = !enabledDark;

                    if (ImGui::MenuItem("Dark", "", &enabledDark))
                    {
                        zep_get_editor().GetTheme().SetThemeType(Zep::ThemeType::Dark);
                    }
                    else if (ImGui::MenuItem("Light", "", &enabledLight))
                    {
                        zep_get_editor().GetTheme().SetThemeType(Zep::ThemeType::Light);
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Window"))
            {
                auto pTabWindow = zep_get_editor().GetActiveTabWindow();
                bool selected = false;
                if (ImGui::MenuItem("Horizontal Split", "", &selected, pBuffer != nullptr))
                {
                    pTabWindow->AddWindow(pBuffer, pTabWindow->GetActiveWindow(), Zep::RegionLayoutType::VBox);
                }
                else if (ImGui::MenuItem("Vertical Split", "", &selected, pBuffer != nullptr))
                {
                    pTabWindow->AddWindow(pBuffer, pTabWindow->GetActiveWindow(), Zep::RegionLayoutType::HBox);
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Debug"))
            {
                if (ImGui::MenuItem("Run", "Ctrl-R"))
                {
                    auto zepBuffers = zep_get_editor().GetBuffers();
                    int bufferNum = zepBuffers.size();
                    if (bufferNum > 0)
                    {
                        auto& zepBuffer0 = zepBuffers[0];
                        std::string extension_name = zepBuffer0->GetFileExtension();
                        if (extension_name == ".py")
                        {
                            std::string textNow = zepBuffers[0]->GetBufferText(zepBuffer0->Begin(), zepBuffer0->End());
                            vm->exec(textNow, zepBuffer0->GetFilePath().string(), pkpy::EXEC_MODE);
                        }
                    }
                }
                ImGui::MenuItem("Show Console", NULL, &show_console_window);
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    {
        // Required for CTRL+P and flashing cursor.
        zep_update();

        // Just show it
        const int main_menu_height = 22;
        static Zep::NVec2i size = Zep::NVec2i(1280, 720 - main_menu_height);
        static Zep::NVec2i pos = Zep::NVec2i(0, main_menu_height);
        zep_show(size, pos);
    }

    if (show_console_window)
    {
        console.Draw("Python Console", &show_console_window);
    }

    simgui_render();

    sg_end_pass();
    sg_commit();
}

void cleanup() {
    zep_destroy();
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
    desc.cleanup_cb = cleanup;
    desc.event_cb = input;
    desc.width = 1280;
    desc.height = 720;
    desc.high_dpi = true;
    desc.window_title = "Mini Python IDE";
    desc.icon.sokol_default = true;
    desc.logger.func = slog_func;
    sapp_run(desc);

    return 0;
}
