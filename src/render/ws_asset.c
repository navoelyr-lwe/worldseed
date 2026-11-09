/* Worldseed Engine — runtime mesh asset format, loader and validator. */
#include "ws_asset.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define WS_ASSET_LINE_MAX 512

static int parse_vertex_line(char *line, ws_vertex *out)
{
    double x, y, z, nx, ny, nz, u, v;
    int r, g, b;
    int n = sscanf(line, "vertex %lf %lf %lf %lf %lf %lf %lf %lf %d %d %d",
                   &x, &y, &z, &nx, &ny, &nz, &u, &v, &r, &g, &b);
    if (n != 11) {
        return 0;
    }
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
        return 0;
    }
    out->position = ws_vec3_make(x, y, z);
    out->normal = ws_vec3_make(nx, ny, nz);
    out->u = u;
    out->v = v;
    out->color[0] = (uint8_t)r;
    out->color[1] = (uint8_t)g;
    out->color[2] = (uint8_t)b;
    return 1;
}

ws_result ws_asset_scan_mesh(const char *path, ws_asset_info *info)
{
    if (path == NULL || info == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return WS_ERR_NOT_FOUND;
    }

    char line[WS_ASSET_LINE_MAX];
    unsigned version = 0u;
    size_t vertices = 0u;
    size_t indices = 0u;
    int have_header = 0;
    ws_result result = WS_OK;

    while (fgets(line, sizeof(line), f) != NULL) {
        if (!have_header) {
            if (sscanf(line, "worldseed-mesh %u", &version) == 1) {
                if (version != WS_ASSET_MESH_VERSION) {
                    result = WS_ERR_UNSUPPORTED;
                    break;
                }
                have_header = 1;
                continue;
            }
            /* Skip blank lines before the header. */
            char *p = line;
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') { ++p; }
            if (*p == '\0') {
                continue;
            }
            result = WS_ERR_PARSE;
            break;
        }

        if (strncmp(line, "vertex", 6) == 0 && (line[6] == ' ' || line[6] == '\t')) {
            ws_vertex tmp;
            if (!parse_vertex_line(line, &tmp)) {
                result = WS_ERR_PARSE;
                break;
            }
            vertices += 1u;
        } else if (strncmp(line, "index", 5) == 0 && (line[5] == ' ' || line[5] == '\t')) {
            unsigned a, b, c;
            if (sscanf(line, "index %u %u %u", &a, &b, &c) != 3) {
                result = WS_ERR_PARSE;
                break;
            }
            indices += 3u;
        } else {
            char *p = line;
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') { ++p; }
            if (*p != '\0') {
                result = WS_ERR_PARSE;
                break;
            }
        }
    }
    fclose(f);

    if (result != WS_OK) {
        return result;
    }
    if (!have_header) {
        return WS_ERR_PARSE;
    }
    info->version = version;
    info->vertex_count = vertices;
    info->index_count = indices;
    return WS_OK;
}

ws_result ws_asset_load_mesh(const char *path, ws_mesh *mesh,
                             ws_vertex *vertices, size_t vertex_cap,
                             uint32_t *indices, size_t index_cap)
{
    if (path == NULL || mesh == NULL || vertices == NULL || indices == NULL) {
        return WS_ERR_NULL_POINTER;
    }

    ws_asset_info info;
    ws_result result = ws_asset_scan_mesh(path, &info);
    if (result != WS_OK) {
        return result;
    }
    if (info.vertex_count > vertex_cap || info.index_count > index_cap) {
        return WS_ERR_CAPACITY;
    }

    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return WS_ERR_NOT_FOUND;
    }

    char line[WS_ASSET_LINE_MAX];
    size_t vi = 0u;
    size_t ii = 0u;
    int have_header = 0;

    while (fgets(line, sizeof(line), f) != NULL) {
        if (!have_header) {
            if (strncmp(line, "worldseed-mesh", 14) == 0) {
                have_header = 1;
            }
            continue;
        }
        if (strncmp(line, "vertex", 6) == 0 && (line[6] == ' ' || line[6] == '\t')) {
            if (vi >= info.vertex_count || !parse_vertex_line(line, &vertices[vi])) {
                fclose(f);
                return WS_ERR_PARSE;
            }
            vi += 1u;
        } else if (strncmp(line, "index", 5) == 0 && (line[5] == ' ' || line[5] == '\t')) {
            unsigned a, b, c;
            if (sscanf(line, "index %u %u %u", &a, &b, &c) != 3) {
                fclose(f);
                return WS_ERR_PARSE;
            }
            if (ii + 3u > info.index_count) {
                fclose(f);
                return WS_ERR_PARSE;
            }
            indices[ii++] = (uint32_t)a;
            indices[ii++] = (uint32_t)b;
            indices[ii++] = (uint32_t)c;
        }
    }
    fclose(f);

    mesh->vertices = vertices;
    mesh->vertex_count = (uint32_t)vi;
    mesh->indices = indices;
    mesh->index_count = (uint32_t)ii;

    result = ws_asset_validate_mesh(mesh);
    if (result != WS_OK) {
        mesh->vertices = NULL;
        mesh->indices = NULL;
        mesh->vertex_count = 0u;
        mesh->index_count = 0u;
        return result;
    }
    return WS_OK;
}

ws_result ws_asset_write_mesh(const char *path, const ws_mesh *mesh)
{
    if (path == NULL || mesh == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    ws_result valid = ws_asset_validate_mesh(mesh);
    if (valid != WS_OK) {
        return valid;
    }

    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        return WS_ERR_IO;
    }

    fprintf(f, "worldseed-mesh %d\n", WS_ASSET_MESH_VERSION);
    for (uint32_t i = 0u; i < mesh->vertex_count; ++i) {
        const ws_vertex *v = &mesh->vertices[i];
        fprintf(f, "vertex %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %u %u %u\n",
                v->position.x, v->position.y, v->position.z,
                v->normal.x, v->normal.y, v->normal.z,
                v->u, v->v,
                (unsigned)v->color[0], (unsigned)v->color[1], (unsigned)v->color[2]);
    }
    for (uint32_t i = 0u; i + 3u <= mesh->index_count; i += 3u) {
        fprintf(f, "index %u %u %u\n", mesh->indices[i], mesh->indices[i + 1u], mesh->indices[i + 2u]);
    }
    fclose(f);
    return WS_OK;
}

ws_result ws_asset_validate_mesh(const ws_mesh *mesh)
{
    if (mesh == NULL || mesh->vertices == NULL || mesh->indices == NULL) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    if (mesh->vertex_count == 0u || mesh->index_count == 0u) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    if (mesh->index_count % 3u != 0u) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    for (uint32_t i = 0u; i < mesh->vertex_count; ++i) {
        const ws_vec3 *p = &mesh->vertices[i].position;
        const ws_vec3 *n = &mesh->vertices[i].normal;
        if (!isfinite(p->x) || !isfinite(p->y) || !isfinite(p->z) ||
            !isfinite(n->x) || !isfinite(n->y) || !isfinite(n->z)) {
            return WS_ERR_INVALID_ARGUMENT;
        }
    }
    for (uint32_t i = 0u; i < mesh->index_count; ++i) {
        if (mesh->indices[i] >= mesh->vertex_count) {
            return WS_ERR_INVALID_ARGUMENT;
        }
    }
    return WS_OK;
}

ws_result ws_asset_write_texture(const char *path, const ws_texture *tex)
{
    if (path == NULL || tex == NULL || tex->rgba == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (tex->width == 0u || tex->height == 0u) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        return WS_ERR_IO;
    }
    fprintf(f, "worldseed-texture %d\n", WS_ASSET_TEXTURE_VERSION);
    fprintf(f, "size %u %u\n", tex->width, tex->height);
    size_t count = (size_t)tex->width * (size_t)tex->height;
    for (size_t i = 0; i < count; ++i) {
        const uint8_t *p = &tex->rgba[i * 4u];
        fprintf(f, "%u %u %u %u\n", (unsigned)p[0], (unsigned)p[1], (unsigned)p[2], (unsigned)p[3]);
    }
    fclose(f);
    return WS_OK;
}

ws_result ws_asset_load_texture(const char *path, ws_texture *tex)
{
    if (path == NULL || tex == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return WS_ERR_NOT_FOUND;
    }
    char line[WS_ASSET_LINE_MAX];
    unsigned version = 0u;
    unsigned width = 0u;
    unsigned height = 0u;
    if (fgets(line, sizeof(line), f) == NULL || sscanf(line, "worldseed-texture %u", &version) != 1) {
        fclose(f);
        return WS_ERR_PARSE;
    }
    if (version != WS_ASSET_TEXTURE_VERSION) {
        fclose(f);
        return WS_ERR_UNSUPPORTED;
    }
    if (fgets(line, sizeof(line), f) == NULL || sscanf(line, "size %u %u", &width, &height) != 2) {
        fclose(f);
        return WS_ERR_PARSE;
    }
    if (width == 0u || height == 0u) {
        fclose(f);
        return WS_ERR_PARSE;
    }
    ws_texture out;
    if (ws_texture_create(&out, width, height, 0u, 0u, 0u, 255u) != WS_OK) {
        fclose(f);
        return WS_ERR_OUT_OF_MEMORY;
    }
    size_t count = (size_t)width * (size_t)height;
    for (size_t i = 0; i < count; ++i) {
        unsigned r, g, b, a;
        if (fgets(line, sizeof(line), f) == NULL ||
            sscanf(line, "%u %u %u %u", &r, &g, &b, &a) != 4 ||
            r > 255u || g > 255u || b > 255u || a > 255u) {
            fclose(f);
            ws_texture_free(&out);
            return WS_ERR_PARSE;
        }
        out.rgba[i * 4u + 0u] = (uint8_t)r;
        out.rgba[i * 4u + 1u] = (uint8_t)g;
        out.rgba[i * 4u + 2u] = (uint8_t)b;
        out.rgba[i * 4u + 3u] = (uint8_t)a;
    }
    fclose(f);
    *tex = out;
    return WS_OK;
}
