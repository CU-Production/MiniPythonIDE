// Dear ImGui: standalone example application for SDL3 + SDL_GPU

#define NOMINMAX
#include "pocketpy.h"
#include "tinyfiledialogs.h"

#include <fstream>
#include <streambuf>
#include <filesystem>
#include "config_app.h"
namespace fs = std::filesystem;
#include "editor.h"
#include "../fonts/Cousine-Regular.cpp"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"
#include <stdio.h>
#include <SDL3/SDL.h>

struct ExampleAppConsole
{
    char                  InputBuf[256];
    ImVector<char*>       Items;
    ImVector<const char*> Commands;
    ImVector<char*>       History;
    int                   HistoryPos;
    ImGuiTextFilter       Filter;
    bool                  AutoScroll;
    bool                  ScrollToBottom;

    ExampleAppConsole()
    {
        ClearLog();
        memset(InputBuf, 0, sizeof(InputBuf));
        HistoryPos = -1;
        Commands.push_back("HELP");
        Commands.push_back("HISTORY");
        Commands.push_back("CLEAR");
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

    static int   Stricmp(const char* s1, const char* s2)         { int d; while ((d = toupper(*s2) - toupper(*s1)) == 0 && *s1) { s1++; s2++; } return d; }
    static int   Strnicmp(const char* s1, const char* s2, int n) { int d = 0; while (n > 0 && (d = toupper(*s2) - toupper(*s1)) == 0 && *s1) { s1++; s2++; n--; } return d; }
    static char* Strdup(const char* s)                           { IM_ASSERT(s); size_t len = strlen(s) + 1; void* buf = malloc(len); IM_ASSERT(buf); return (char*)memcpy(buf, (const void*)s, len); }
    static void  Strtrim(char* s)                                { char* str_end = s + strlen(s); while (str_end > s && str_end[-1] == ' ') str_end--; *str_end = 0; }

    void ClearLog()
    {
        for (int i = 0; i < Items.Size; i++)
            free(Items[i]);
        Items.clear();
    }

    void AddLog(const char* fmt, ...) IM_FMTARGS(2)
    {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
        buf[IM_ARRAYSIZE(buf)-1] = 0;
        va_end(args);
        Items.push_back(Strdup(buf));
    }

    void Draw(const char* title, bool* p_open)
    {
        ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(title, p_open))
        {
            ImGui::End();
            return;
        }

        if (ImGui::SmallButton("Clear")) { ClearLog(); }
        ImGui::SameLine();
        bool copy_to_clipboard = ImGui::SmallButton("Copy");

        ImGui::Separator();

        const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar))
        {
            if (ImGui::BeginPopupContextWindow())
            {
                if (ImGui::Selectable("Clear")) ClearLog();
                ImGui::EndPopup();
            }

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
            if (copy_to_clipboard)
                ImGui::LogToClipboard();
            for (int i = 0; i < Items.Size; i++)
            {
                const char* item = Items[i];
                if (!Filter.PassFilter(item))
                    continue;

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

            if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
                ImGui::SetScrollHereY(1.0f);
            ScrollToBottom = false;

            ImGui::PopStyleVar();
        }
        ImGui::EndChild();
        ImGui::Separator();

        bool reclaim_focus = false;
        ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll;
        if (ImGui::InputText("Input", InputBuf, IM_ARRAYSIZE(InputBuf), input_text_flags))
        {
            char* s = InputBuf;
            Strtrim(s);
            if (s[0])
                ExecCommand(s);
            strcpy(s, "");
            reclaim_focus = true;
        }

        ImGui::SetItemDefaultFocus();
        if (reclaim_focus)
            ImGui::SetKeyboardFocusHere(-1);

        ImGui::End();
    }

    void ExecCommand(const char* command_line)
    {
        AddLog("# %s\n", command_line);
        HistoryPos = -1;
        for (int i = History.Size - 1; i >= 0; i--)
            if (Stricmp(History[i], command_line) == 0)
            {
                free(History[i]);
                History.erase(History.begin() + i);
                break;
            }
        History.push_back(Strdup(command_line));

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

        ScrollToBottom = true;
    }
};

static ExampleAppConsole console;

// Main code
int main(int, char**)
{
    // Setup SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        printf("Error: SDL_Init(): %s\n", SDL_GetError());
        return -1;
    }

    // Enable native IME.
    SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, "1");

    // Create window with graphics context
    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window* window = SDL_CreateWindow("Mini Python IDE", (int)(1280 * main_scale), (int)(720 * main_scale), window_flags);
    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return -1;
    }
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    // Create GPU Device
    SDL_GPUDevice* gpu_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_METALLIB,true,nullptr);
    if (gpu_device == nullptr)
    {
        printf("Error: SDL_CreateGPUDevice(): %s\n", SDL_GetError());
        return -1;
    }

    // Claim window for GPU Device
    if (!SDL_ClaimWindowForGPUDevice(gpu_device, window))
    {
        printf("Error: SDL_ClaimWindowForGPUDevice(): %s\n", SDL_GetError());
        return -1;
    }
    SDL_SetGPUSwapchainParameters(gpu_device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_MAILBOX);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForSDLGPU(window);
    ImGui_ImplSDLGPU3_InitInfo init_info = {};
    init_info.Device = gpu_device;
    init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(gpu_device, window);
    init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    ImGui_ImplSDLGPU3_Init(&init_info);

    // Enable SDL3 text input (CRITICAL for TextEditor to receive characters)
    SDL_StartTextInput(window);

    // Load Fonts
    if (!ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(Cousine_Regular_compressed_data, Cousine_Regular_compressed_size, 18.f))
    {
        ImGui::GetIO().Fonts->AddFontDefault();
    }

    // Our state
    bool show_console_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Init editor
    Editor editor;
    
    // Load test.py if exists
    if (fs::exists("test.py"))
    {
        editor.LoadFile("test.py");
    }

    // Init pocket.py
    pkpy::VM* vm = new pkpy::VM();
    {
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

    // Main loop
    bool done = false;
    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                done = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // Start the Dear ImGui frame
        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // Menu Bar
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Open", "Ctrl+O"))
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
                        editor.LoadFile(openFileName);
                    }
                }
                
                if (ImGui::MenuItem("Save", "Ctrl+S"))
                {
                    if (!editor.GetCurrentFile().empty())
                    {
                        editor.SaveFile(editor.GetCurrentFile());
                    }
                }
                
                if (ImGui::MenuItem("Save As..."))
                {
                    auto saveFileName = tinyfd_saveFileDialog(
                        "Save file as",
                        "untitled.py",
                        0,
                        nullptr,
                        nullptr);
                    if (saveFileName != nullptr)
                    {
                        editor.SaveFile(saveFileName);
                    }
                }
                
                ImGui::Separator();
                if (ImGui::MenuItem("Exit"))
                {
                    done = true;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit"))
            {
                auto& textEditor = editor.GetTextEditor();
                if (ImGui::MenuItem("Undo", "Ctrl-Z", nullptr, textEditor.CanUndo()))
                    textEditor.Undo();
                if (ImGui::MenuItem("Redo", "Ctrl-Y", nullptr, textEditor.CanRedo()))
                    textEditor.Redo();
                    
                ImGui::Separator();
                
                if (ImGui::MenuItem("Copy", "Ctrl-C", nullptr, textEditor.HasSelection()))
                    textEditor.Copy();
                if (ImGui::MenuItem("Cut", "Ctrl-X", nullptr, textEditor.HasSelection()))
                    textEditor.Cut();
                if (ImGui::MenuItem("Paste", "Ctrl-V", nullptr, ImGui::GetClipboardText() != nullptr))
                    textEditor.Paste();
                    
                ImGui::Separator();
                
                if (ImGui::MenuItem("Select All", "Ctrl-A"))
                    textEditor.SelectAll();
                    
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                auto& textEditor = editor.GetTextEditor();
                bool showWhitespace = textEditor.IsShowingWhitespaces();
                if (ImGui::MenuItem("Show Whitespace", nullptr, &showWhitespace))
                    textEditor.SetShowWhitespaces(showWhitespace);
                    
                ImGui::Separator();
                
                if (ImGui::BeginMenu("Color Scheme"))
                {
                    if (ImGui::MenuItem("Dark"))
                        textEditor.SetPalette(TextEditor::GetDarkPalette());
                    if (ImGui::MenuItem("Light"))
                        textEditor.SetPalette(TextEditor::GetLightPalette());
                    if (ImGui::MenuItem("Retro Blue"))
                        textEditor.SetPalette(TextEditor::GetRetroBluePalette());
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Run"))
            {
                if (ImGui::MenuItem("Run Script", "F5"))
                {
                    std::string code = editor.GetText();
                    auto currentFile = editor.GetCurrentFile();
                    std::string filename = currentFile.empty() ? "<string>" : currentFile.string();
                    
                    try {
                        vm->exec(code, filename, pkpy::EXEC_MODE);
                        console.AddLog("Script executed successfully.\n");
                    } catch (...) {
                        console.AddLog("[error] Script execution failed.\n");
                    }
                }
                
                ImGui::Separator();
                ImGui::MenuItem("Show Console", nullptr, &show_console_window);
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        // Handle hotkeys - must be checked outside of menu
        // F5 - Run Script
        if (ImGui::IsKeyPressed(ImGuiKey_F5))
        {
            std::string code = editor.GetText();
            auto currentFile = editor.GetCurrentFile();
            std::string filename = currentFile.empty() ? "<string>" : currentFile.string();
            
            try {
                vm->exec(code, filename, pkpy::EXEC_MODE);
                console.AddLog("Script executed successfully.\n");
                show_console_window = true;  // Auto-show console when running
            } catch (...) {
                console.AddLog("[error] Script execution failed.\n");
                show_console_window = true;
            }
        }
        
        // Ctrl+O - Open File
        if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl))
        {
            if (ImGui::IsKeyPressed(ImGuiKey_O))
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
                    editor.LoadFile(openFileName);
                }
            }
            
            // Ctrl+S - Save File
            if (ImGui::IsKeyPressed(ImGuiKey_S))
            {
                if (!editor.GetCurrentFile().empty())
                {
                    editor.SaveFile(editor.GetCurrentFile());
                    console.AddLog("File saved: %s\n", editor.GetCurrentFile().string().c_str());
                }
                else
                {
                    auto saveFileName = tinyfd_saveFileDialog(
                        "Save file as",
                        "untitled.py",
                        0,
                        nullptr,
                        nullptr);
                    if (saveFileName != nullptr)
                    {
                        editor.SaveFile(saveFileName);
                        console.AddLog("File saved: %s\n", saveFileName);
                    }
                }
            }
        }

        // Editor Window
        ImGui::Begin("Code Editor", nullptr, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_MenuBar);
        
        if (ImGui::BeginMenuBar())
        {
            auto& textEditor = editor.GetTextEditor();
            auto cursorPos = textEditor.GetCursorPosition();
            ImGui::Text("Ln %d, Col %d | %d lines | %s", 
                cursorPos.mLine + 1, 
                cursorPos.mColumn + 1, 
                textEditor.GetTotalLines(),
                textEditor.IsOverwrite() ? "Ovr" : "Ins");
                
            if (!editor.GetCurrentFile().empty())
            {
                ImGui::Text(" | %s", editor.GetCurrentFile().filename().string().c_str());
            }
            ImGui::EndMenuBar();
        }
        
        auto csize = ImGui::GetContentRegionAvail();
        
        // Set keyboard focus to the next widget (the editor child window)
        static bool first_frame = true;
        if (first_frame)
        {
            ImGui::SetKeyboardFocusHere();
            first_frame = false;
        }
        
        editor.Render("##editor", csize);
        ImGui::End();

        // Console Window
        if (show_console_window)
        {
            console.Draw("Python Console", &show_console_window);
        }

        // Rendering
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);

        SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(gpu_device);
        SDL_GPUTexture* swapchain_texture;
        SDL_AcquireGPUSwapchainTexture(command_buffer, window, &swapchain_texture, nullptr, nullptr);

        if (swapchain_texture != nullptr && !is_minimized)
        {
            ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);

            SDL_GPUColorTargetInfo target_info = {};
            target_info.texture = swapchain_texture;
            target_info.clear_color = SDL_FColor { clear_color.x, clear_color.y, clear_color.z, clear_color.w };
            target_info.load_op = SDL_GPU_LOADOP_CLEAR;
            target_info.store_op = SDL_GPU_STOREOP_STORE;
            target_info.mip_level = 0;
            target_info.layer_or_depth_plane = 0;
            target_info.cycle = false;
            SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, &target_info, 1, nullptr);

            ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, render_pass);

            SDL_EndGPURenderPass(render_pass);
        }

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        SDL_SubmitGPUCommandBuffer(command_buffer);
    }

    // Cleanup
    delete vm;

    SDL_WaitForGPUIdle(gpu_device);
    ImGui_ImplSDL3_Shutdown();
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui::DestroyContext();

    SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
    SDL_DestroyGPUDevice(gpu_device);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
