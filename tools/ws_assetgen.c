/* Worldseed Engine — primitive asset generator.
 *
 * Emits our versioned mesh assets (see ws_asset) from the procedural
 * generators, plus an ASSET-MANIFEST.csv row per file recording the producer
 * tool, version, derivation chain and provenance. No external asset is read;
 * everything is produced in-project (ADR-008).
 *
 * Usage: ws_assetgen <output-dir>
 */
#include "core/ws_hash.h"
#include "render/ws_asset.h"
#include "render/ws_mesh.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GEN_VERSION "1"

static int emit(const char *dir, const char *name, ws_mesh *mesh,
                FILE *manifest, const char *provenance)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.wsmesh", dir, name);
    ws_result res = ws_asset_write_mesh(path, mesh);
    if (res != WS_OK) {
        fprintf(stderr, "write %s failed: %s\n", path, ws_result_name(res));
        return 0;
    }
    /* Re-read the bytes to hash the exact file on disk. */
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return 0;
    }
    uint8_t buf[8192];
    ws_sha256 ctx;
    ws_sha256_init(&ctx);
    size_t n;
    while ((n = fread(buf, 1u, sizeof(buf), f)) > 0u) {
        ws_sha256_update(&ctx, buf, n);
    }
    fclose(f);
    uint8_t digest[WS_SHA256_DIGEST_SIZE];
    ws_sha256_final(&ctx, digest);
    char hex[WS_SHA256_DIGEST_SIZE * 2u + 1u];
    ws_sha256_hex(digest, hex);

    fprintf(manifest, "%s.wsmesh,tools/ws_assetgen,%s,%s,project-original,%s\n",
            name, GEN_VERSION, provenance, hex);
    printf("asset %s.wsmesh vertices=%u indices=%u sha256=%s\n",
           name, mesh->vertex_count, mesh->index_count, hex);
    return 1;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "assets/primitives";

    char manifest_path[512];
    snprintf(manifest_path, sizeof(manifest_path), "%s/ASSET-MANIFEST.csv", dir);
    FILE *manifest = fopen(manifest_path, "wb");
    if (manifest == NULL) {
        fprintf(stderr, "cannot write manifest %s (does the directory exist?)\n", manifest_path);
        return 1;
    }
    fprintf(manifest, "file,producer,tool_version,derivation,license,sha256\n");

    int ok = 1;

    ws_vertex cube_v[24];
    uint32_t cube_i[36];
    const uint8_t cube_c[3] = {180u, 150u, 90u};
    ws_mesh cube;
    if (ws_mesh_make_cube(&cube, cube_v, 24u, cube_i, 36u, cube_c) != WS_OK) {
        ok = 0;
    } else {
        ok = emit(dir, "cube", &cube, manifest, "procedural cube") && ok;
    }

    ws_vertex plane_v[4];
    uint32_t plane_i[6];
    const uint8_t plane_c[3] = {70u, 110u, 60u};
    ws_mesh plane;
    if (ws_mesh_make_plane(&plane, plane_v, 4u, plane_i, 6u, 8.0, 1u, plane_c) != WS_OK) {
        ok = 0;
    } else {
        ok = emit(dir, "plane", &plane, manifest, "procedural plane") && ok;
    }

    enum { SPH_V = 17 * 9, SPH_I = 16 * 9 * 6 };
    ws_vertex sphere_v[SPH_V];
    uint32_t sphere_i[SPH_I];
    const uint8_t sphere_c[3] = {120u, 90u, 70u};
    ws_mesh sphere;
    if (ws_mesh_make_sphere(&sphere, sphere_v, SPH_V, sphere_i, SPH_I, 16u, 8u, sphere_c) != WS_OK) {
        ok = 0;
    } else {
        ok = emit(dir, "sphere", &sphere, manifest, "procedural uv-sphere") && ok;
    }

    fclose(manifest);
    if (!ok) {
        return 1;
    }
    printf("WS_ASSETGEN_OK manifest=%s\n", manifest_path);
    return 0;
}
