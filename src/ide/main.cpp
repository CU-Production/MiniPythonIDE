// Dear ImGui: standalone example application for SDL3 + SDL_GPU

#define NOMINMAX
#define PK_IS_PUBLIC_INCLUDE
#include "pocketpy.h"
#include "tinyfiledialogs.h"

#include <fstream>
#include <streambuf>
#include <iostream>
#include <filesystem>
namespace fs = std::filesystem;
#include "editor.h"
#ifdef ENABLE_DEBUGGER
#include "debugger.h"
#endif
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

#ifdef ENABLE_DEBUGGER
static Debugger debugger;
#endif

// Global process tracking for cleanup on exit
static std::vector<SDL_Process*> g_runningProcesses;
static std::mutex g_processesMutex;

// Helper to register a process for cleanup
void RegisterProcess(SDL_Process* process) {
    if (process) {
        std::lock_guard<std::mutex> lock(g_processesMutex);
        g_runningProcesses.push_back(process);
    }
}

// Helper to unregister a process
void UnregisterProcess(SDL_Process* process) {
    if (process) {
        std::lock_guard<std::mutex> lock(g_processesMutex);
        auto it = std::find(g_runningProcesses.begin(), g_runningProcesses.end(), process);
        if (it != g_runningProcesses.end()) {
            g_runningProcesses.erase(it);
        }
    }
}

// Cleanup all running processes on exit
static void CleanupAllProcesses() {
    std::lock_guard<std::mutex> lock(g_processesMutex);
    for (SDL_Process* proc : g_runningProcesses) {
        if (proc) {
            SDL_KillProcess(proc, true);
            SDL_DestroyProcess(proc);
        }
    }
    g_runningProcesses.clear();
}

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
#ifdef ENABLE_DEBUGGER
    bool show_variables_window = false;
    bool show_callstack_window = false;
    bool show_breakpoints_window = false;
    
    // Breakpoint add dialog state
    static char breakpoint_file[256] = "";
    static int breakpoint_line = 1;
    
    // Flag to indicate we're in a nested event loop (debugging paused)
    static bool in_debug_pause = false;
#endif
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Init editor
    Editor editor;
    
    // Load test.py if exists
    if (fs::exists("test.py"))
    {
        editor.LoadFile("test.py");
    }

#ifdef ENABLE_DEBUGGER
    // Setup breakpoint callback - sync editor breakpoints with debugger
    editor.SetBreakpointCallback([&](int line, bool added) {
        auto currentFile = editor.GetCurrentFile();
        std::string filename = currentFile.empty() ? "<string>" : currentFile.string();
        
        if (added) {
            debugger.AddBreakpoint(filename, line);
            console.AddLog("Breakpoint added: %s:%d\n", filename.c_str(), line);
        } else {
            debugger.RemoveBreakpoint(filename, line);
            console.AddLog("Breakpoint removed: %s:%d\n", filename.c_str(), line);
        }
    });
#endif

    // Helper function to setup Python VM environment
    auto setupPythonVM = []() {
        // Setup stdout/stderr callbacks
        py_callbacks()->print = [](const char* s) {
            console.AddLog("%s", s);
            std::cout << s;
        };
        
        // Create test module
        py_GlobalRef mod = py_newmodule("test");
        
        // Set pi attribute
        py_newfloat(py_r0(), 3.14);
        py_setdict(mod, py_name("pi"), py_r0());
        
        // Bind add function
        py_bindfunc(mod, "add", [](int argc, py_StackRef argv) -> bool {
            if (argc != 2) return TypeError("add() requires 2 arguments");
            
            py_i64 a = py_toint(py_offset(argv, 0));
            py_i64 b = py_toint(py_offset(argv, 1));
            
            // Create return value using retval
            py_newint(py_retval(), a + b);
            return true;
        });
    };
    
    // Helper function to run script via pkpy process with output capture
    auto runScriptViaProcess = [&](const std::string& code, const std::string& filename) {
        // Determine script path
        std::string scriptPath;
        bool isRealFile = !filename.empty() && 
                          filename != "<string>" && 
                          filename != "<editor>" &&
                          fs::exists(filename);
        
        if (isRealFile) {
            scriptPath = filename;
            // Write current content to file (handle unsaved changes)
            std::ofstream outFile(scriptPath);
            if (outFile) {
                outFile << code;
                outFile.close();
                console.AddLog("[info] Using file: %s\n", scriptPath.c_str());
            } else {
                console.AddLog("[error] Failed to write to file: %s\n", scriptPath.c_str());
                return;
            }
        } else {
            // Create temporary file
            scriptPath = (fs::temp_directory_path() / "minipythonide_run.py").string();
            std::ofstream outFile(scriptPath);
            if (outFile) {
                outFile << code;
                outFile.close();
                console.AddLog("[info] Using temporary file: %s\n", scriptPath.c_str());
            } else {
                console.AddLog("[error] Failed to create temporary file\n");
                return;
            }
        }
        
        // Launch pkpy without --debug flag, with output piping
        console.AddLog("[info] Running: pkpy %s\n", scriptPath.c_str());
        
        const char* args[] = {
            "pkpy",
            scriptPath.c_str(),
            nullptr
        };
        
        // Create process with stdout/stderr piping
        SDL_Process* process = SDL_CreateProcess(args, true);
        if (!process) {
            console.AddLog("[error] Failed to launch pkpy: %s\n", SDL_GetError());
            return;
        }
        
        // Register process for cleanup on exit
        RegisterProcess(process);
        
        // Read output from process
        char buffer[4096];
        bool has_output = false;
        
        size_t bytes_read = 0;
        void* stream_data = SDL_ReadProcess(process, &bytes_read, nullptr);
        
        while (stream_data && bytes_read > 0) {
            // Copy to buffer and ensure null termination
            size_t copy_size = (bytes_read < sizeof(buffer) - 1) ? bytes_read : sizeof(buffer) - 1;
            memcpy(buffer, stream_data, copy_size);
            buffer[copy_size] = '\0';
            
            console.AddLog("%s", buffer);
            has_output = true;
            
            SDL_free(stream_data);
            
            // Continue reading
            stream_data = SDL_ReadProcess(process, &bytes_read, nullptr);
        }
        
        if (stream_data) {
            SDL_free(stream_data);
        }
        
        // Wait for process to complete
        int exit_code = 0;
        SDL_WaitProcess(process, true, &exit_code);
        
        // Unregister and destroy process
        UnregisterProcess(process);
        SDL_DestroyProcess(process);
        
        if (exit_code == 0) {
            if (!has_output) {
                console.AddLog("[info] Script completed successfully (no output)\n");
            }
        } else {
            console.AddLog("[error] Script exited with code %d\n", exit_code);
        }
        
        show_console_window = true;
    };
    
    // Init pocket.py
    py_initialize();
    
    // Initial setup for VM 0
    setupPythonVM();


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

#ifdef ENABLE_DEBUGGER
        // Update debug current line highlighting
        if (debugger.IsPaused())
        {
            editor.SetDebugCurrentLine(debugger.GetCurrentLine());
        }
        else
        {
            editor.ClearDebugCurrentLine();
        }
#endif

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
#ifdef ENABLE_DEBUGGER
                bool canRun = !debugger.IsDebugging();
#else
                bool canRun = true;
#endif
                if (ImGui::MenuItem("Run Script", "F5", false, canRun))
                {
                    std::string code = editor.GetText();
                    auto currentFile = editor.GetCurrentFile();
                    std::string filename = currentFile.empty() ? "<editor>" : currentFile.string();
                    
                    // Run script via pkpy process with output capture
                    runScriptViaProcess(code, filename);
                }
                
                ImGui::Separator();
                ImGui::MenuItem("Show Console", nullptr, &show_console_window);
                ImGui::EndMenu();
            }

#ifdef ENABLE_DEBUGGER
            if (ImGui::BeginMenu("Debug"))
            {
                if (ImGui::MenuItem("Start Debugging", "F9", false, !debugger.IsDebugging()))
                {
                    show_variables_window = true;
                    show_callstack_window = true;
                    show_console_window = true;
                    
                    // Get code and filename
                    std::string code = editor.GetText();
                    std::string filename = editor.GetCurrentFile().empty() ? 
                        "<editor>" : editor.GetCurrentFile().string();
                    
                    // Start debugging with log callback
                    // NOTE: StartBlocking() will BLOCK until debug session completes
                    auto logCallback = [](const std::string& msg) {
                        console.AddLog("%s", msg.c_str());
                    };
                    
                    debugger.Start(code, filename, logCallback);
                }
                
                if (ImGui::MenuItem("Stop Debugging", nullptr, false, debugger.IsDebugging()))
                {
                    debugger.Stop();
                }
                
                ImGui::Separator();
                
                if (ImGui::MenuItem("Continue", "F5", false, debugger.IsPaused()))
                {
                    debugger.Continue();
                }
                
                if (ImGui::MenuItem("Step Over", "F10", false, debugger.IsPaused()))
                {
                    debugger.StepOver();
                }
                
                if (ImGui::MenuItem("Step Into", "F11", false, debugger.IsPaused()))
                {
                    debugger.StepInto();
                }
                
                if (ImGui::MenuItem("Step Out", "Shift+F11", false, debugger.IsPaused()))
                {
                    debugger.StepOut();
                }
                
                ImGui::Separator();
                ImGui::MenuItem("Show Variables", nullptr, &show_variables_window, debugger.IsDebugging());
                ImGui::MenuItem("Show Call Stack", nullptr, &show_callstack_window, debugger.IsDebugging());
                ImGui::MenuItem("Show Breakpoints", nullptr, &show_breakpoints_window);
                
                ImGui::EndMenu();
            }
#endif

            ImGui::EndMainMenuBar();
        }

        // Handle hotkeys - must be checked outside of menu
        
#ifdef ENABLE_DEBUGGER
        // F9 - Start Debugging
        if (ImGui::IsKeyPressed(ImGuiKey_F9) && !debugger.IsDebugging())
        {
            show_variables_window = true;
            show_callstack_window = true;
            show_console_window = true;
            
            // Get code and filename
            std::string code = editor.GetText();
            std::string filename = editor.GetCurrentFile().empty() ? 
                "<editor>" : editor.GetCurrentFile().string();
            
            // Start debugging with log callback
            auto logCallback = [](const std::string& msg) {
                console.AddLog("%s", msg.c_str());
            };
            
            debugger.Start(code, filename, logCallback);
        }
        
        // F5 - Continue (when debugging) or Run Script via pkpy process (when not debugging)
        if (ImGui::IsKeyPressed(ImGuiKey_F5))
        {
            if (debugger.IsDebugging() && debugger.IsPaused())
            {
                debugger.Continue();
            }
            else if (!debugger.IsDebugging())
            {
                std::string code = editor.GetText();
                auto currentFile = editor.GetCurrentFile();
                std::string filename = currentFile.empty() ? "<editor>" : currentFile.string();
                
                // Run script via pkpy process with output capture
                runScriptViaProcess(code, filename);
            }
        }
        
        // F10 - Step Over
        if (ImGui::IsKeyPressed(ImGuiKey_F10) && debugger.IsPaused())
        {
            debugger.StepOver();
        }
        
        // F11 - Step Into
        if (ImGui::IsKeyPressed(ImGuiKey_F11) && debugger.IsPaused())
        {
            bool shift = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
            if (shift)
            {
                // Shift+F11 = Step Out
                debugger.StepOut();
            }
            else
            {
                // F11 = Step Into
                debugger.StepInto();
            }
        }
#else
        // F5 - Run Script (when debugger not enabled)
        if (ImGui::IsKeyPressed(ImGuiKey_F5))
        {
            std::string code = editor.GetText();
            auto currentFile = editor.GetCurrentFile();
            std::string filename = currentFile.empty() ? "<editor>" : currentFile.string();
            
            // Run script via pkpy process with output capture
            runScriptViaProcess(code, filename);
        }
#endif
        
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

#ifdef ENABLE_DEBUGGER

        // Simple helper to render one variable row
        auto renderVariableRow = [](const DebugVariable& var, int idx) {
            ImGui::PushID(idx);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                
            // Variable name
            std::string displayName = var.name.empty() ? "<unnamed>" : var.name;
            ImGui::Text("%s", displayName.c_str());
            
            // Value column
                    ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", var.value.c_str());
            
            // Type column
                    ImGui::TableNextColumn();
            if (!var.type.empty()) {
                ImGui::TextColored(ImVec4(0.5f, 0.7f, 0.9f, 1.0f), "%s", var.type.c_str());
                    }
            
                ImGui::PopID();
        };

        // Variables Window
        if (show_variables_window && debugger.IsDebugging())
        {
            if (ImGui::Begin("Variables", &show_variables_window))
            {
                if (ImGui::CollapsingHeader("Local Variables", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    const auto& locals = debugger.GetLocalVariables();
                    if (locals.empty())
                    {
                        ImGui::TextDisabled("(no local variables)");
                    }
                    else
                    {
                        if (ImGui::BeginTable("LocalVars", 3, 
                            ImGuiTableFlags_Borders | 
                            ImGuiTableFlags_RowBg | 
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_ScrollY))
                        {
                            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.3f);
                            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.5f);
                            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.2f);
                            ImGui::TableHeadersRow();
                            
                            for (size_t i = 0; i < locals.size(); i++)
                            {
                                renderVariableRow(locals[i], (int)i);
                            }
                            ImGui::EndTable();
                        }
                    }
                }
                
                if (ImGui::CollapsingHeader("Global Variables"))
                {
                    const auto& globals = debugger.GetGlobalVariables();
                    if (globals.empty())
                    {
                        ImGui::TextDisabled("(no global variables)");
                    }
                    else
                    {
                        if (ImGui::BeginTable("GlobalVars", 3, 
                            ImGuiTableFlags_Borders | 
                            ImGuiTableFlags_RowBg | 
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_ScrollY))
                        {
                            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.3f);
                            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.5f);
                            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.2f);
                            ImGui::TableHeadersRow();
                            
                            for (size_t i = 0; i < globals.size(); i++)
                            {
                                renderVariableRow(globals[i], (int)(i + 10000)); // offset to avoid ID collision
                            }
                            ImGui::EndTable();
                        }
                    }
                }
            }
            ImGui::End();
        }

        // Call Stack Window
        if (show_callstack_window && debugger.IsDebugging())
        {
            if (ImGui::Begin("Call Stack", &show_callstack_window))
            {
                const auto& frames = debugger.GetStackFrames();
                if (frames.empty())
                {
                    ImGui::TextDisabled("(no stack frames)");
                }
                else
                {
                    for (size_t i = 0; i < frames.size(); i++)
                    {
                        const auto& frame = frames[i];
                        if (ImGui::Selectable(frame.function_name.c_str()))
                        {
                            // Could jump to frame location here
                        }
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::SetTooltip("%s:%d", frame.filename.c_str(), frame.lineno);
                        }
                    }
                }
            }
            ImGui::End();
        }

        // Breakpoints Window
        if (show_breakpoints_window)
        {
            if (ImGui::Begin("Breakpoints", &show_breakpoints_window))
            {
                ImGui::Text("Manage Breakpoints");
                ImGui::Separator();
                
                // Add breakpoint section
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.3f, 0.4f, 1.0f));
                ImGui::BeginChild("AddBreakpoint", ImVec2(0, 100), true);
                ImGui::Text("Add New Breakpoint");
                ImGui::Spacing();
                
                ImGui::Text("File:");
                ImGui::SameLine();
                ImGui::InputText("##bpfile", breakpoint_file, sizeof(breakpoint_file));
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Enter filename (e.g., test.py or <string>)");
                }
                
                ImGui::Text("Line:");
                ImGui::SameLine();
                ImGui::InputInt("##bpline", &breakpoint_line);
                if (breakpoint_line < 1) breakpoint_line = 1;
                
                ImGui::Spacing();
                if (ImGui::Button("Add Breakpoint"))
                {
                    if (strlen(breakpoint_file) > 0 && breakpoint_line > 0)
                    {
                        debugger.AddBreakpoint(breakpoint_file, breakpoint_line);
                        console.AddLog("Breakpoint added: %s:%d\n", breakpoint_file, breakpoint_line);
                        
                        // Sync to editor if it's the current file
                        auto currentFile = editor.GetCurrentFile();
                        std::string currentFilename = currentFile.empty() ? "<string>" : currentFile.string();
                        if (std::string(breakpoint_file) == currentFilename) {
                            editor.SyncBreakpoints(debugger.GetBreakpoints(currentFilename));
                        }
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Add to Current File"))
                {
                    auto currentFile = editor.GetCurrentFile();
                    std::string filename;
                    if (!currentFile.empty())
                    {
                        filename = currentFile.string();
                        debugger.AddBreakpoint(filename, breakpoint_line);
                        console.AddLog("Breakpoint added: %s:%d\n", filename.c_str(), breakpoint_line);
                    }
                    else
                    {
                        filename = "<string>";
                        debugger.AddBreakpoint(filename, breakpoint_line);
                        console.AddLog("Breakpoint added: <string>:%d\n", breakpoint_line);
                    }
                    
                    // Sync to editor
                    editor.SyncBreakpoints(debugger.GetBreakpoints(filename));
                }
                
                ImGui::EndChild();
                ImGui::PopStyleColor();
                
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Text("Active Breakpoints:");
                ImGui::Spacing();
                
                // List all breakpoints
                bool hasBreakpoints = false;
                
                // Get current file's breakpoints
                auto currentFile = editor.GetCurrentFile();
                std::string currentFilename = currentFile.empty() ? "<string>" : currentFile.string();
                
                const auto& breakpoints = debugger.GetBreakpoints(currentFilename);
                if (!breakpoints.empty())
                {
                    hasBreakpoints = true;
                    ImGui::Text("In current file (%s):", currentFilename.c_str());
                    ImGui::Indent();
                    
                    // Convert to vector for easier iteration with deletion
                    std::vector<int> bps_to_remove;
                    for (int line : breakpoints)
                    {
                        ImGui::PushID(line);
                        if (ImGui::SmallButton("X"))
                        {
                            bps_to_remove.push_back(line);
                        }
                        ImGui::SameLine();
                        ImGui::Text("Line %d", line);
                        ImGui::PopID();
                    }
                    
                    // Remove breakpoints after iteration
                    for (int line : bps_to_remove)
                    {
                        debugger.RemoveBreakpoint(currentFilename, line);
                        console.AddLog("Breakpoint removed: %s:%d\n", currentFilename.c_str(), line);
                    }
                    
                    // Sync to editor if needed
                    if (!bps_to_remove.empty()) {
                        editor.SyncBreakpoints(debugger.GetBreakpoints(currentFilename));
                    }
                    
                    ImGui::Unindent();
                    ImGui::Spacing();
                }
                
                // Show breakpoints in other files
                if (strlen(breakpoint_file) > 0 && std::string(breakpoint_file) != currentFilename)
                {
                    const auto& other_bps = debugger.GetBreakpoints(breakpoint_file);
                    if (!other_bps.empty())
                    {
                        hasBreakpoints = true;
                        ImGui::Text("In %s:", breakpoint_file);
                        ImGui::Indent();
                        
                        std::vector<int> bps_to_remove;
                        for (int line : other_bps)
                        {
                            ImGui::PushID((breakpoint_file + std::to_string(line)).c_str());
                            if (ImGui::SmallButton("X"))
                            {
                                bps_to_remove.push_back(line);
                            }
                            ImGui::SameLine();
                            ImGui::Text("Line %d", line);
                            ImGui::PopID();
                        }
                        
                        for (int line : bps_to_remove)
                        {
                            debugger.RemoveBreakpoint(breakpoint_file, line);
                            console.AddLog("Breakpoint removed: %s:%d\n", breakpoint_file, line);
                        }
                        
                        ImGui::Unindent();
                    }
                }
                
                if (!hasBreakpoints)
                {
                    ImGui::TextDisabled("(no breakpoints set)");
                }
                
                ImGui::Spacing();
                ImGui::Separator();
                
                if (ImGui::Button("Clear All Breakpoints"))
                {
                    debugger.ClearBreakpoints();
                    console.AddLog("All breakpoints cleared.\n");
                    
                    // Sync to editor
                    std::set<int> empty;
                    editor.SyncBreakpoints(empty);
                }
            }
            ImGui::End();
        }
#endif // ENABLE_DEBUGGER

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
#ifdef ENABLE_DEBUGGER
    // Stop debugger if still running
    if (debugger.IsDebugging()) {
        debugger.Stop();
    }
#endif
    
    // Kill all remaining pkpy processes before exit
    CleanupAllProcesses();
    
    py_finalize();

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
