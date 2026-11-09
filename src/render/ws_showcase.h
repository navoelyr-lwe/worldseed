/* Worldseed Engine — showcase scenes and their original assets.
 *
 * A single source of truth for the fixed reference scenes shared by the golden
 * harness (visual-regression evidence) and the Windows application (the
 * user-visible preview). The geometry and textures are generated in-process by
 * our own C17 generators (ADR-008); no external asset pack is read.
 *
 * Storage is caller-owned so there is no hidden allocation contract: the caller
 * embeds a ws_showcase and calls ws_showcase_init / ws_showcase_free.
 */
#ifndef WS_RENDER_SHOWCASE_H
#define WS_RENDER_SHOWCASE_H

#include <stddef.h>
#include <stdint.h>

#include "../core/ws_result.h"
#include "ws_mesh.h"
#include "ws_scene.h"
#include "ws_texture.h"

#define WS_SHOWCASE_MAX_OBJECTS 12u

typedef struct ws_showcase {
    ws_vertex cube_verts[24];
    uint32_t cube_idx[36];
    ws_mesh cube;
    ws_vertex plane_verts[4];
    uint32_t plane_idx[6];
    ws_mesh plane;
    ws_vertex sphere_verts[17u * 9u];
    uint32_t sphere_idx[16u * 9u * 6u];
    ws_mesh sphere;
    ws_texture checker;
    ws_texture noise;
    ws_scene_object objects[WS_SHOWCASE_MAX_OBJECTS];
    ws_scene scene;
} ws_showcase;

/* Build the shared geometry/textures and initialise an empty scene. */
ws_result ws_showcase_init(ws_showcase *showcase);

/* Release owned texture storage. Safe on a zeroed showcase. */
void ws_showcase_free(ws_showcase *showcase);

/* Number of showcase scenes and their stable identifiers. */
size_t ws_showcase_scene_count(void);
const char *ws_showcase_scene_id(size_t index);

/* Reset the scene and rebuild showcase scene `index`. The seed is reserved for
 * future procedural variation and currently does not change the fixed scenes.
 * Returns WS_ERR_INVALID_ARGUMENT for an out-of-range index. */
ws_result ws_showcase_build(ws_showcase *showcase, size_t index, uint64_t seed);

#endif /* WS_RENDER_SHOWCASE_H */
