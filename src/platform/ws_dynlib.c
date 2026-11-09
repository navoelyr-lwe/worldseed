/* Worldseed Engine — thin dynamic library adapter. */
#include "ws_dynlib.h"

#include <stddef.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
struct ws_dynlib {
    HMODULE handle;
};
#else
#include <dlfcn.h>
struct ws_dynlib {
    void *handle;
};
#endif

ws_result ws_dynlib_open(const char *name, ws_dynlib **out)
{
    if (name == NULL || out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    *out = NULL;
#if defined(_WIN32)
    HMODULE handle = LoadLibraryA(name);
    if (handle == NULL) {
        return WS_ERR_NOT_FOUND;
    }
    struct ws_dynlib *lib = (struct ws_dynlib *)malloc(sizeof(*lib));
    if (lib == NULL) {
        FreeLibrary(handle);
        return WS_ERR_OUT_OF_MEMORY;
    }
    lib->handle = handle;
#else
    void *handle = dlopen(name, RTLD_NOW | RTLD_LOCAL);
    if (handle == NULL) {
        return WS_ERR_NOT_FOUND;
    }
    struct ws_dynlib *lib = (struct ws_dynlib *)malloc(sizeof(*lib));
    if (lib == NULL) {
        dlclose(handle);
        return WS_ERR_OUT_OF_MEMORY;
    }
    lib->handle = handle;
#endif
    *out = lib;
    return WS_OK;
}

void *ws_dynlib_symbol(ws_dynlib *lib, const char *symbol)
{
    if (lib == NULL || symbol == NULL) {
        return NULL;
    }
#if defined(_WIN32)
    return (void *)GetProcAddress(lib->handle, symbol);
#else
    return dlsym(lib->handle, symbol);
#endif
}

void ws_dynlib_close(ws_dynlib *lib)
{
    if (lib == NULL) {
        return;
    }
#if defined(_WIN32)
    FreeLibrary(lib->handle);
#else
    dlclose(lib->handle);
#endif
    free(lib);
}

ws_result ws_dynlib_open_any(const char *const *names, size_t count, ws_dynlib **out)
{
    if (names == NULL || out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    *out = NULL;
    for (size_t i = 0; i < count; ++i) {
        ws_dynlib *lib = NULL;
        if (ws_dynlib_open(names[i], &lib) == WS_OK) {
            *out = lib;
            return WS_OK;
        }
    }
    return WS_ERR_NOT_FOUND;
}
