/* Worldseed Engine — runtime mesh asset format, loader and validator.
 *
 * Text-based, versioned, dependency-free. The format stores indexed triangle
 * geometry produced by our own tools (see ws_mesh). Loading validates structure
 * and index ranges so a malformed file cannot drive out-of-bounds reads.
 *
 * Grammar (whitespace-separated, one record per line):
 *   worldseed-mesh <version>
 *   vertex <x> <y> <z> <nx> <ny> <nz> <u> <v> <r> <g> <b>
 *   index <i0> <i1> <i2>
 * Colour components are integers 0..255; all other numbers are decimal.
 */
#ifndef WS_RENDER_ASSET_H
#define WS_RENDER_ASSET_H

#include <stddef.h>

#include "../core/ws_result.h"
#include "ws_mesh.h"

#define WS_ASSET_MESH_VERSION 1

/* Counts parsed from a header/records without allocating. Fills the record
 * counts and the detected version. */
typedef struct ws_asset_info {
    unsigned version;
    size_t vertex_count;
    size_t index_count; /* number of indices (3 per triangle) */
} ws_asset_info;

/* Scan a mesh file and report its counts. Returns WS_OK, or WS_ERR_* on a
 * missing file, bad header or malformed record. */
ws_result ws_asset_scan_mesh(const char *path, ws_asset_info *info);

/* Load a mesh file into caller-owned arrays. On WS_OK, `mesh` points at the
 * caller's arrays and the used counts are set. Returns WS_ERR_CAPACITY if the
 * arrays are too small, or WS_ERR_PARSE on malformed data. */
ws_result ws_asset_load_mesh(const char *path, ws_mesh *mesh,
                             ws_vertex *vertices, size_t vertex_cap,
                             uint32_t *indices, size_t index_cap);

/* Write a mesh to `path` in the format above. */
ws_result ws_asset_write_mesh(const char *path, const ws_mesh *mesh);

/* Structural validation: non-NULL arrays, index_count % 3 == 0, every index
 * within [0, vertex_count), finite positions/normals, and indices covering no
 * degenerate-only mesh. Returns WS_OK or WS_ERR_INVALID_ARGUMENT. */
ws_result ws_asset_validate_mesh(const ws_mesh *mesh);

#endif /* WS_RENDER_ASSET_H */
