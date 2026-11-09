/* Worldseed Engine — runtime mesh and procedural geometry.
 *
 * A mesh is an indexed triangle list of vertices (position, normal, UV, colour)
 * stored in caller-owned memory. Procedural generators below produce original
 * geometry with no external asset dependency, matching the C17 asset rule in
 * ADR-008.
 */
#ifndef WS_RENDER_MESH_H
#define WS_RENDER_MESH_H

#include <stddef.h>
#include <stdint.h>

#include "../core/ws_math.h"
#include "../core/ws_result.h"

typedef struct ws_vertex {
    ws_vec3 position;
    ws_vec3 normal;
    double u;
    double v;
    uint8_t color[3];
} ws_vertex;

typedef struct ws_mesh {
    ws_vertex *vertices;
    uint32_t vertex_count;
    uint32_t *indices; /* 3 per triangle */
    uint32_t index_count;
} ws_mesh;

/* Number of vertices/indices each generator needs, so callers can size their
 * arena exactly. */
size_t ws_mesh_cube_vertex_count(void);
size_t ws_mesh_cube_index_count(void);
size_t ws_mesh_plane_vertex_count(void);
size_t ws_mesh_plane_index_count(void);
size_t ws_mesh_sphere_vertex_count(unsigned segments, unsigned rings);
size_t ws_mesh_sphere_index_count(unsigned segments, unsigned rings);

/* Generate geometry into caller-owned arrays. `mesh` is filled to point at
 * `vertices`/`indices`; the caller keeps ownership. Returns WS_OK or an error
 * when an argument is invalid or the arrays are too small. */
ws_result ws_mesh_make_cube(ws_mesh *mesh, ws_vertex *vertices, size_t vertex_cap,
                            uint32_t *indices, size_t index_cap,
                            const uint8_t color[3]);
ws_result ws_mesh_make_plane(ws_mesh *mesh, ws_vertex *vertices, size_t vertex_cap,
                             uint32_t *indices, size_t index_cap,
                             double size, unsigned divisions, const uint8_t color[3]);
ws_result ws_mesh_make_sphere(ws_mesh *mesh, ws_vertex *vertices, size_t vertex_cap,
                              uint32_t *indices, size_t index_cap,
                              unsigned segments, unsigned rings, const uint8_t color[3]);

/* Axis-aligned bounds of a mesh in local space. */
ws_aabb3 ws_mesh_bounds(const ws_mesh *mesh);

#endif /* WS_RENDER_MESH_H */
