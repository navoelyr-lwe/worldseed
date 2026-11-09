/* Worldseed Engine — unified scene catalog.
 *
 * A single ordered list of reference scenes shared by the golden harness
 * (visual-regression evidence) and the Windows application (the user-visible
 * preview), so the two can never drift apart. The catalog holds two families:
 *  - the fixed showcase scenes (P2 asset/lighting/material coverage), and
 *  - procedural world scenes built from a seed (P3 terrain, water, habitat,
 *    vegetation and environment).
 *
 * The catalog owns whichever scene is currently built; callers read the scene
 * through ws_catalog_scene and never write to it.
 */
#ifndef WS_RENDER_CATALOG_H
#define WS_RENDER_CATALOG_H

#include <stddef.h>
#include <stdint.h>

#include "../core/ws_result.h"
#include "ws_env_scene.h"
#include "ws_scene.h"
#include "ws_showcase.h"

#define WS_CATALOG_MAX_ENV 4u

typedef struct ws_catalog {
    int is_env;            /* 1 when the current scene is procedural */
    ws_showcase showcase;
    ws_env_scene env;
    ws_scene *scene;       /* points at showcase.scene or env.scene */
} ws_catalog;

/* Total number of catalog scenes (showcase + environment). */
size_t ws_catalog_count(void);

/* Stable identifier for a catalog scene index (NULL when out of range). */
const char *ws_catalog_id(size_t index);

/* Build scene `index` with `seed` into the catalog. Allocates for environment
 * scenes; the caller must call ws_catalog_free before rebuilding or freeing. */
ws_result ws_catalog_build(ws_catalog *catalog, size_t index, uint64_t seed);

/* The currently built scene (NULL when nothing is built). */
const ws_scene *ws_catalog_scene(const ws_catalog *catalog);

/* Release any owned scene storage. Safe on a zeroed catalog. */
void ws_catalog_free(ws_catalog *catalog);

#endif /* WS_RENDER_CATALOG_H */
