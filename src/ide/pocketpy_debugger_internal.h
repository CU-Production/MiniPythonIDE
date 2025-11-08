#pragma once

// This header exposes internal debugger functions from pocketpy.c
// to enable embedded debugging without using the blocking DAP server

#ifdef __cplusplus
extern "C" {
#endif

#include "pocketpy.h"

// Enums from pocketpy.c debugger implementation
typedef enum { 
    C11_STEP_IN, 
    C11_STEP_OVER, 
    C11_STEP_OUT, 
    C11_STEP_CONTINUE 
} C11_STEP_MODE;

typedef enum { 
    C11_DEBUGGER_NOSTOP, 
    C11_DEBUGGER_STEP, 
    C11_DEBUGGER_EXCEPTION, 
    C11_DEBUGGER_BP
} C11_STOP_REASON;

typedef enum {
    C11_DEBUGGER_SUCCESS = 0,
    C11_DEBUGGER_EXIT = 1,
    C11_DEBUGGER_UNKNOW_ERROR = 3,
    C11_DEBUGGER_FILEPATH_ERROR = 7
} C11_DEBUGGER_STATUS;

// String buffer type from pocketpy (opaque)
typedef struct c11_sbuf c11_sbuf;

// Internal debugger functions (defined in pocketpy.c)
// These are declared here to make them accessible from C++

// Initialize debugger
extern void c11_debugger_init(void);

// Set step mode (continue, step in, step over, step out)
extern void c11_debugger_set_step_mode(C11_STEP_MODE mode);

// Trace callback handler
extern C11_DEBUGGER_STATUS c11_debugger_on_trace(py_Frame* frame, enum py_TraceEvent event);

// Check if should pause
extern C11_STOP_REASON c11_debugger_should_pause(void);

// Check if should keep paused
extern int c11_debugger_should_keep_pause(void);

// Breakpoint management
extern int c11_debugger_setbreakpoint(const char* filename, int lineno);
extern int c11_debugger_reset_breakpoints_by_source(const char* sourcesname);

// Get debug information (these write to c11_sbuf)
extern void c11_debugger_frames(c11_sbuf* buffer);
extern void c11_debugger_scopes(int frameid, c11_sbuf* buffer);
extern bool c11_debugger_unfold_var(int var_id, c11_sbuf* buffer);

// Exception handling
extern void c11_debugger_exception_on_trace(py_Ref exc);
extern const char* c11_debugger_excinfo(const char** message);

#ifdef __cplusplus
}
#endif

