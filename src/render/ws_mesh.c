/* Worldseed Engine — runtime mesh and procedural geometry. */
#include "ws_mesh.h"

#include <math.h>
#include <string.h>

#include "../core/ws_math.h"

static ws_vertex make_vertex(double x, double y, double z,
                             double nx, double ny, double nz,
                             double u, double v, const uint8_t color[3])
{
    ws_vertex vert;
    /* Zero first so the trailing padding after color[3] is deterministic:
     * callers hash and compare vertex buffers as raw bytes. */
    memset(&vert, 0, sizeof(vert));
    vert.position = ws_vec3_make(x, y, z);
    vert.normal = ws_vec3_make(nx, ny, nz);
    vert.u = u;
    vert.v = v;
    vert.color[0] = color[0];
    vert.color[1] = color[1];
    vert.color[2] = color[2];
    return vert;
}

size_t ws_mesh_cube_vertex_count(void)
{
    return 24u; /* 4 per face, so each face keeps a flat normal */
}

size_t ws_mesh_cube_index_count(void)
{
    return 36u; /* 6 faces x 2 triangles x 3 indices */
}

size_t ws_mesh_plane_vertex_count(void)
{
    return 4u;
}

size_t ws_mesh_plane_index_count(void)
{
    return 6u;
}

size_t ws_mesh_sphere_vertex_count(unsigned segments, unsigned rings)
{
    if (segments < 3u || rings < 2u) {
        return 0u;
    }
    return (size_t)(segments + 1u) * (size_t)(rings + 1u);
}

size_t ws_mesh_sphere_index_count(unsigned segments, unsigned rings)
{
    if (segments < 3u || rings < 2u) {
        return 0u;
    }
    return (size_t)segments * (size_t)rings * 6u;
}

size_t ws_mesh_cone_vertex_count(unsigned segments)
{
    if (segments < 3u) {
        return 0u;
    }
    return (size_t)segments + 2u; /* base ring (side normals) + apex + base centre */
}

size_t ws_mesh_cone_index_count(unsigned segments)
{
    if (segments < 3u) {
        return 0u;
    }
    return (size_t)segments * 6u; /* side fan + base fan */
}

ws_result ws_mesh_make_cube(ws_mesh *mesh, ws_vertex *vertices, size_t vertex_cap,
                            uint32_t *indices, size_t index_cap,
                            const uint8_t color[3])
{
    if (mesh == NULL || vertices == NULL || indices == NULL || color == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (vertex_cap < ws_mesh_cube_vertex_count() || index_cap < ws_mesh_cube_index_count()) {
        return WS_ERR_CAPACITY;
    }

    /* Six faces, each with its own normal. Half extent 0.5, centred at origin. */
    static const double face_normal[6][3] = {
        {0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0},
    };
    /* Corner offsets per face in CCW order. */
    static const double face_corner[6][4][3] = {
        {{-0.5, -0.5, 0.5}, {0.5, -0.5, 0.5}, {0.5, 0.5, 0.5}, {-0.5, 0.5, 0.5}},
        {{0.5, -0.5, -0.5}, {-0.5, -0.5, -0.5}, {-0.5, 0.5, -0.5}, {0.5, 0.5, -0.5}},
        {{0.5, -0.5, 0.5}, {0.5, -0.5, -0.5}, {0.5, 0.5, -0.5}, {0.5, 0.5, 0.5}},
        {{-0.5, -0.5, -0.5}, {-0.5, -0.5, 0.5}, {-0.5, 0.5, 0.5}, {-0.5, 0.5, -0.5}},
        {{-0.5, 0.5, 0.5}, {0.5, 0.5, 0.5}, {0.5, 0.5, -0.5}, {-0.5, 0.5, -0.5}},
        {{-0.5, -0.5, -0.5}, {0.5, -0.5, -0.5}, {0.5, -0.5, 0.5}, {-0.5, -0.5, 0.5}},
    };
    static const double uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};

    uint32_t vi = 0u;
    uint32_t ii = 0u;
    for (int f = 0; f < 6; ++f) {
        for (int c = 0; c < 4; ++c) {
            vertices[vi++] = make_vertex(face_corner[f][c][0], face_corner[f][c][1],
                                         face_corner[f][c][2],
                                         face_normal[f][0], face_normal[f][1],
                                         face_normal[f][2],
                                         uv[c][0], uv[c][1], color);
        }
        uint32_t base = (uint32_t)(f * 4);
        indices[ii++] = base + 0u;
        indices[ii++] = base + 1u;
        indices[ii++] = base + 2u;
        indices[ii++] = base + 0u;
        indices[ii++] = base + 2u;
        indices[ii++] = base + 3u;
    }

    mesh->vertices = vertices;
    mesh->vertex_count = vi;
    mesh->indices = indices;
    mesh->index_count = ii;
    return WS_OK;
}

ws_result ws_mesh_make_plane(ws_mesh *mesh, ws_vertex *vertices, size_t vertex_cap,
                             uint32_t *indices, size_t index_cap,
                             double size, unsigned divisions, const uint8_t color[3])
{
    (void)divisions; /* subdivided plane comes with terrain in P3 */
    if (mesh == NULL || vertices == NULL || indices == NULL || color == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (size <= 0.0) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    if (vertex_cap < ws_mesh_plane_vertex_count() || index_cap < ws_mesh_plane_index_count()) {
        return WS_ERR_CAPACITY;
    }

    double h = size * 0.5;
    vertices[0] = make_vertex(-h, 0.0, h, 0.0, 1.0, 0.0, 0.0, 0.0, color);
    vertices[1] = make_vertex(h, 0.0, h, 0.0, 1.0, 0.0, 1.0, 0.0, color);
    vertices[2] = make_vertex(h, 0.0, -h, 0.0, 1.0, 0.0, 1.0, 1.0, color);
    vertices[3] = make_vertex(-h, 0.0, -h, 0.0, 1.0, 0.0, 0.0, 1.0, color);

    indices[0] = 0u; indices[1] = 1u; indices[2] = 2u;
    indices[3] = 0u; indices[4] = 2u; indices[5] = 3u;

    mesh->vertices = vertices;
    mesh->vertex_count = 4u;
    mesh->indices = indices;
    mesh->index_count = 6u;
    return WS_OK;
}

ws_result ws_mesh_make_sphere(ws_mesh *mesh, ws_vertex *vertices, size_t vertex_cap,
                              uint32_t *indices, size_t index_cap,
                              unsigned segments, unsigned rings, const uint8_t color[3])
{
    if (mesh == NULL || vertices == NULL || indices == NULL || color == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (segments < 3u || rings < 2u) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    if (vertex_cap < ws_mesh_sphere_vertex_count(segments, rings) ||
        index_cap < ws_mesh_sphere_index_count(segments, rings)) {
        return WS_ERR_CAPACITY;
    }

    uint32_t vi = 0u;
    for (unsigned r = 0; r <= rings; ++r) {
        double phi = WS_PI * (double)r / (double)rings; /* 0 (north) .. pi (south) */
        double y = cos(phi) * 0.5;
        double radius = sin(phi) * 0.5;
        for (unsigned s = 0; s <= segments; ++s) {
            double theta = 2.0 * WS_PI * (double)s / (double)segments;
            double x = cos(theta) * radius;
            double z = sin(theta) * radius;
            ws_vec3 n = ws_vec3_normalize(ws_vec3_make(x, y, z));
            vertices[vi++] = make_vertex(x, y, z, n.x, n.y, n.z,
                                         (double)s / (double)segments,
                                         (double)r / (double)rings, color);
        }
    }

    uint32_t ii = 0u;
    for (unsigned r = 0; r < rings; ++r) {
        for (unsigned s = 0; s < segments; ++s) {
            uint32_t a = r * (segments + 1u) + s;
            uint32_t b = a + segments + 1u;
            indices[ii++] = a;
            indices[ii++] = b;
            indices[ii++] = a + 1u;
            indices[ii++] = a + 1u;
            indices[ii++] = b;
            indices[ii++] = b + 1u;
        }
    }

    mesh->vertices = vertices;
    mesh->vertex_count = vi;
    mesh->indices = indices;
    mesh->index_count = ii;
    return WS_OK;
}

ws_result ws_mesh_make_cone(ws_mesh *mesh, ws_vertex *vertices, size_t vertex_cap,
                            uint32_t *indices, size_t index_cap,
                            unsigned segments, const uint8_t color[3])
{
    if (mesh == NULL || vertices == NULL || indices == NULL || color == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (segments < 3u) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    if (vertex_cap < ws_mesh_cone_vertex_count(segments) ||
        index_cap < ws_mesh_cone_index_count(segments)) {
        return WS_ERR_CAPACITY;
    }

    /* Slant normal: for a cone of height 1 and base radius 0.5 the side normal
     * tilts outward and upward by a fixed amount, so a single normal per ring
     * vertex is exact for a faceted (low-poly) silhouette. */
    double slope = 0.5; /* radius / height */
    for (unsigned s = 0; s < segments; ++s) {
        double theta = 2.0 * WS_PI * (double)s / (double)segments;
        double cx = cos(theta);
        double cz = sin(theta);
        ws_vec3 n = ws_vec3_normalize(ws_vec3_make(cx, slope, cz));
        vertices[s] = make_vertex(cx * 0.5, 0.0, cz * 0.5, n.x, n.y, n.z,
                                  (double)s / (double)segments, 0.0, color);
    }
    vertices[segments] = make_vertex(0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.5, 1.0, color);
    vertices[segments + 1u] = make_vertex(0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.5, 0.5, color);

    uint32_t apex = segments;
    uint32_t base_centre = segments + 1u;
    uint32_t ii = 0u;
    for (unsigned s = 0; s < segments; ++s) {
        uint32_t a = s;
        uint32_t b = (s + 1u) % segments;
        indices[ii++] = a;
        indices[ii++] = b;
        indices[ii++] = apex;
        indices[ii++] = b;
        indices[ii++] = a;
        indices[ii++] = base_centre;
    }

    mesh->vertices = vertices;
    mesh->vertex_count = segments + 2u;
    mesh->indices = indices;
    mesh->index_count = ii;
    return WS_OK;
}

ws_aabb3 ws_mesh_bounds(const ws_mesh *mesh)
{
    ws_aabb3 box;
    box.min = ws_vec3_make(0.0, 0.0, 0.0);
    box.max = ws_vec3_make(0.0, 0.0, 0.0);
    if (mesh == NULL || mesh->vertices == NULL || mesh->vertex_count == 0u) {
        return box;
    }
    box.min = mesh->vertices[0].position;
    box.max = mesh->vertices[0].position;
    for (uint32_t i = 1u; i < mesh->vertex_count; ++i) {
        ws_vec3 p = mesh->vertices[i].position;
        if (p.x < box.min.x) { box.min.x = p.x; }
        if (p.y < box.min.y) { box.min.y = p.y; }
        if (p.z < box.min.z) { box.min.z = p.z; }
        if (p.x > box.max.x) { box.max.x = p.x; }
        if (p.y > box.max.y) { box.max.y = p.y; }
        if (p.z > box.max.z) { box.max.z = p.z; }
    }
    return box;
}
