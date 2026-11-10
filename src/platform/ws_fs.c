/* Worldseed Engine — small portable filesystem helpers. */
#include "ws_fs.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define WS_MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define WS_MKDIR(p) mkdir((p), 0777)
#endif

ws_result ws_fs_make_dir(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return WS_ERR_INVALID_ARGUMENT;
    }
    if (WS_MKDIR(path) == 0) {
        return WS_OK;
    }
    if (errno == EEXIST) {
        return WS_OK;
    }
    return WS_ERR_IO;
}

ws_result ws_fs_make_dirs(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return WS_ERR_INVALID_ARGUMENT;
    }
    char buf[1024];
    size_t n = strlen(path);
    if (n + 1u > sizeof(buf)) {
        return WS_ERR_CAPACITY;
    }
    memcpy(buf, path, n + 1u);
    /* Create each prefix in turn: "a/b/c" -> "a", "a/b", "a/b/c". */
    for (size_t i = 1; i < n; ++i) {
        if (buf[i] == '/' || buf[i] == '\\') {
            char sep = buf[i];
            buf[i] = '\0';
            if (ws_fs_make_dir(buf) != WS_OK) {
                buf[i] = sep;
                return WS_ERR_IO;
            }
            buf[i] = sep;
        }
    }
    return ws_fs_make_dir(buf);
}
