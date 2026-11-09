/* Worldseed Engine — thin dynamic library adapter.
 *
 * The only OS-facing part of GPU access. OpenCL is loaded at runtime from the
 * system driver; the product never ships an OpenCL runtime. This keeps the
 * shared core free of platform headers.
 */
#ifndef WS_PLATFORM_DYNLIB_H
#define WS_PLATFORM_DYNLIB_H

#include <stddef.h>

#include "../core/ws_result.h"

typedef struct ws_dynlib ws_dynlib;

/* Open a library by name (e.g. "OpenCL.dll" or "libOpenCL.so.1").
 * Returns WS_OK and sets *out on success. */
ws_result ws_dynlib_open(const char *name, ws_dynlib **out);

/* Resolve a symbol. Returns NULL when not found. */
void *ws_dynlib_symbol(ws_dynlib *lib, const char *symbol);

/* Close a library. Safe to call with NULL. */
void ws_dynlib_close(ws_dynlib *lib);

/* Try a list of candidate names in order; first success wins. */
ws_result ws_dynlib_open_any(const char *const *names, size_t count, ws_dynlib **out);

#endif /* WS_PLATFORM_DYNLIB_H */
