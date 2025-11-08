#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#define PK_IS_PUBLIC_INCLUDE
#include "pocketpy.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if(file == NULL) {
        printf("Error: file not found\n");
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* buffer = PK_MALLOC(size + 1);
    size = fread(buffer, 1, size, file);
    buffer[size] = 0;
    return buffer;
}

static char buf[2048];

// Test module function (C function pointer)
static bool test_is_available(int argc, py_StackRef argv) {
    py_newbool(py_retval(), true);
    return true;
}

static bool test_mod_add(int argc, py_StackRef argv) {
    PY_CHECK_ARGC(2);

    PY_CHECK_ARG_TYPE(0, tp_int);
    PY_CHECK_ARG_TYPE(1, tp_int);

    int _0 = py_toint(py_arg(0));
    int _1 = py_toint(py_arg(1));

    int res = _0 + _1;

    py_newint(py_retval(), res);
    return true;
}

int main(int argc, char** argv) {
#if _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif

    bool profile = false;
    bool debug = false;
    const char* filename = NULL;

    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--profile") == 0) {
            profile = true;
            continue;
        }
        if(strcmp(argv[i], "--debug") == 0) {
            debug = true;
            continue;
        }
        if(filename == NULL) {
            filename = argv[i];
            continue;
        }
        printf("Usage: pocketpy [--profile] [--debug] filename\n");
    }

    if(debug && profile) {
        printf("Error: --debug and --profile cannot be used together.\n");
        return 1;
    }

    py_initialize();
    py_sys_setargv(argc, argv);
    
    // Create simple test module
    py_GlobalRef test_mod = py_newmodule("test");
    
    // Add a version attribute
    py_newstr(py_r0(), "0.1.0");
    py_setdict(test_mod, py_name("__version__"), py_r0());
    
    // Add a simple function as placeholder
    py_bindfunc(test_mod, "is_available", test_is_available);

    // Set pi attribute
    py_newfloat(py_r0(), 3.14);
    py_setdict(test_mod, py_name("pi"), py_r0());

    // Bind add function
    py_bindfunc(test_mod, "add", test_mod_add);

    if(filename == NULL) {
        if(profile) printf("Warning: --profile is ignored in REPL mode.\n");
        if(debug) printf("Warning: --debug is ignored in REPL mode.\n");

        printf("pocketpy " PK_VERSION " (" __DATE__ ", " __TIME__ ") ");
        printf("[%d bit] on %s", (int)(sizeof(void*) * 8), PY_SYS_PLATFORM_STRING);
#ifndef NDEBUG
        printf(" (DEBUG)");
#endif
        printf("\n");
        printf("https://github.com/pocketpy/pocketpy\n");
        printf("Type \"exit()\" to exit.\n");

        while(true) {
            int size = py_replinput(buf, sizeof(buf));
            if(size == -1) {  // Ctrl-D (i.e. EOF)
                printf("\n");
                break;
            }
            assert(size < sizeof(buf));
            if(size >= 0) {
                py_StackRef p0 = py_peek(0);
                if(!py_exec(buf, "<stdin>", SINGLE_MODE, NULL)) {
                    py_printexc();
                    py_clearexc(p0);
                }
            }
        }
    } else {
        if(profile) py_profiler_begin();
        if(debug) py_debugger_waitforattach("127.0.0.1", 6110);

        char* source = read_file(filename);
        if(source) {
            if(!py_exec(source, filename, EXEC_MODE, NULL))
                py_printexc();
            else {
                if(profile) {
                    char* json_report = py_profiler_report();
                    FILE* report_file = fopen("profiler_report.json", "w");
                    if(report_file) {
                        fprintf(report_file, "%s", json_report);
                        fclose(report_file);
                    }
                    PK_FREE(json_report);
                }
            }

            PK_FREE(source);
        }
    }

    int code = py_checkexc() ? 1 : 0;
    py_finalize();

    if(debug) py_debugger_exit(code);
    return code;
}
