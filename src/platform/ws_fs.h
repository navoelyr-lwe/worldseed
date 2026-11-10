/* Worldseed Engine — small portable filesystem helpers.
 *
 * Platform code lives only under src/platform. These helpers create directories
 * for tool and evidence output paths, with the OS difference isolated here
 * (POSIX mkdir vs Win32 _mkdir). They are not used by the simulation itself.
 */
#ifndef WS_PLATFORM_FS_H
#define WS_PLATFORM_FS_H

#include "../core/ws_result.h"

/* Create `path` and any missing parents. Returns WS_OK if the directory exists
 * afterwards (already-existing directories are not an error). */
ws_result ws_fs_make_dirs(const char *path);

/* Create a single directory level. Returns WS_OK if it already exists. */
ws_result ws_fs_make_dir(const char *path);

#endif /* WS_PLATFORM_FS_H */
