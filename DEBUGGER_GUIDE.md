# MiniPythonIDE Debugger Guide

## Overview

MiniPythonIDE now includes a complete debugging system powered by PocketPy 2.0's trace API. This allows you to step through your Python code, inspect variables, view the call stack, and set breakpoints.

## Features

### 1. **Debugger Control**
- **Start Debugging (F9)**: Begin a debugging session
- **Stop Debugging**: End the current debugging session
- **Continue (F5 during debug)**: Resume execution until the next breakpoint
- **Step Over (F10)**: Execute the next line without stepping into functions
- **Step Into (F11)**: Step into function calls
- **Step Out (Shift+F11)**: Step out of the current function
- **Pause**: Pause execution at the next line

### 2. **Breakpoints**
- Click on the line number gutter in the editor to toggle breakpoints
- Breakpoints are shown with a red circle
- The debugger will pause execution when a breakpoint is hit

### 3. **Variables Window**
- View local and global variables
- Shows variable names, values, and types
- Automatically updates when paused at a breakpoint or step

### 4. **Call Stack Window**
- View the current call stack
- See which functions are currently executing
- Click on stack frames to view their location (future feature)

### 5. **Current Execution Indicator**
- The current line being executed is highlighted in the editor
- The debugger toolbar shows the current file and line number

## Usage

### Starting a Debug Session

1. Write or open a Python file
2. Set breakpoints by clicking on line numbers (optional)
3. Click "Start Debug (F9)" or use the Debug menu
4. The code will execute until it hits a breakpoint or completes

### Stepping Through Code

When paused at a breakpoint:
- **F10 (Step Over)**: Execute the current line and move to the next line
- **F11 (Step Into)**: If the current line contains a function call, step into that function
- **Shift+F11 (Step Out)**: Continue execution until the current function returns
- **F5 (Continue)**: Resume normal execution until the next breakpoint

### Inspecting Variables

When paused:
1. Open the "Variables" window (Debug menu → Show Variables)
2. Expand "Local Variables" to see function-scope variables
3. Expand "Global Variables" to see module-scope variables

### Viewing the Call Stack

1. Open the "Call Stack" window (Debug menu → Show Call Stack)
2. See the hierarchy of function calls
3. Hover over frames to see file location

## Keyboard Shortcuts

| Action | Shortcut |
|--------|----------|
| Start Debugging | F9 |
| Continue | F5 (during debug) |
| Run Without Debug | F5 (when not debugging) |
| Step Over | F10 |
| Step Into | F11 |
| Step Out | Shift+F11 |

## Technical Details

The debugger is implemented using:
- **PocketPy 2.0 Trace API** (`py_sys_settrace`)
- **Frame introspection** (`py_Frame_sourceloc`, `py_Frame_newlocals`, `py_Frame_newglobals`)
- **Event-driven architecture** (LINE, PUSH, POP events)

## Limitations

Current limitations:
1. Variable inspection shows simplified information (full dict iteration coming soon)
2. Breakpoint persistence is not yet implemented (breakpoints are cleared when closing the IDE)
3. Conditional breakpoints are not yet supported
4. Watch expressions are not yet implemented

## Future Enhancements

Planned features:
- Persistent breakpoints (save with project)
- Conditional breakpoints
- Watch expressions
- Edit and continue
- Remote debugging
- Call stack navigation (click to jump to frame)
- Variable value editing

## Example

```python
def factorial(n):
    if n == 0:
        return 1  # Set breakpoint here
    return n * factorial(n-1)

result = factorial(5)
print(result)
```

1. Set a breakpoint on the `return 1` line
2. Press F9 to start debugging
3. When paused, inspect the `n` variable
4. Press F10 to step through each iteration
5. Watch the call stack grow and shrink as recursion happens

