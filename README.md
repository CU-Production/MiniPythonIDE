# MiniPythonIDE

A lightweight Python IDE built with ImGui, SDL3, and PocketPy.

## Features

- **Code Editor** with Python syntax highlighting
- **Python Console** for output and errors
- **File Operations** (Open, Save)
- **Script Execution** (F5)
- **Integrated Debugger**:
  - Set breakpoints by double-clicking line numbers
  - Step-by-step execution (F10: Step Over, F11: Step Into, Shift+F11: Step Out)
  - Visual breakpoint markers (red circles ●)
  - Current line highlighting during debugging
- **Chinese Font Support** via FreeType

## Build

### Requirements
- CMake 3.25+
- C++20 compiler
- SDL3 (fetched automatically)
- FreeType (fetched automatically)

### Build Steps

```bash
cmake -B build
cmake --build build
```

## Usage

### Basic Operations
- **F5**: Run Python script (or Continue when debugging)
- **Ctrl+Enter**: Run Python script
- **Ctrl+O**: Open file
- **Ctrl+S**: Save file

### Debugging
1. **Set Breakpoints**: Double-click on line numbers to toggle breakpoints (red circles ●)
2. **Start Debugging**: Press **F9** or use **Debug > Start Debugging**
3. **Debug Controls** (when paused):
   - **F5**: Continue
   - **F10**: Step Over
   - **F11**: Step Into
   - **Shift+F11**: Step Out
4. **Stop Debugging**: Click "Stop Debugging" button

## Build Options

### Debugger Support

The debugger can be enabled/disabled at build time:

```bash
# Enable debugger (default)
cmake -B build -DENABLE_DEBUGGER=ON

# Disable debugger
cmake -B build -DENABLE_DEBUGGER=OFF
```

When disabled, the Debug menu and debugger UI will not be available.

## Project Structure

```
MiniPythonIDE/
├── src/
│   ├── main.cpp                    # Application entry point
│   ├── editor.h/cpp                # Editor wrapper
│   ├── debugger.h/cpp              # Debugger integration
│   ├── pocketpy_debugger_internal.h # PocketPy internal API bindings
├── 3rd_party/
│   ├── imgui/                      # Dear ImGui
│   ├── ImGuiColorTextEdit/         # Text editor widget
│   ├── pocketpy/                   # Python interpreter
│   └── tinyfiledialogs/            # File dialogs
└── DEBUGGER_GUIDE.md               # Detailed debugger documentation
```

## How It Works

The debugger uses PocketPy's internal debugging API (`c11_debugger_*` functions) exposed through `pocketpy_debugger_internal.h`. This allows full control over breakpoints, stepping, and execution flow without the blocking DAP server.

## License

This project uses several open-source libraries. Please refer to their respective licenses.
