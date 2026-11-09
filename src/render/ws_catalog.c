/* Worldseed Engine — unified scene catalog. */
#include "ws_catalog.h"

#include <string.h>

/* Environment scenes are appended after the showcase scenes. Their stable ids
 * come from ws_env_scene_kind_id so the app and manifests agree. */
static int env_kind_for_index(size_t index, size_t showcase_count)
{
    return (int)(index - showcase_count);
}

size_t ws_catalog_count(void)
{
    return ws_showcase_scene_count() + WS_CATALOG_MAX_ENV;
}

const char *ws_catalog_id(size_t index)
{
    size_t showcase_count = ws_showcase_scene_count();
    if (index < showcase_count) {
        return ws_showcase_scene_id(index);
    }
    if (index < showcase_count + WS_CATALOG_MAX_ENV) {
        return ws_env_scene_kind_id(env_kind_for_index(index, showcase_count));
    }
    return NULL;
}

ws_result ws_catalog_build(ws_catalog *catalog, size_t index, uint64_t seed)
{
    if (catalog == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    size_t showcase_count = ws_showcase_scene_count();
    if (index >= ws_catalog_count()) {
        return WS_ERR_INVALID_ARGUMENT;
    }

    /* Zero first: the caller may hand us a fresh, uninitialised catalog, and
     * ws_catalog_free would otherwise read indeterminate pointers. Rebuilding
     * the same catalog requires ws_catalog_free beforehand (documented). */
    memset(catalog, 0, sizeof(*catalog));

    if (index < showcase_count) {
        ws_result r = ws_showcase_init(&catalog->showcase);
        if (r != WS_OK) {
            return r;
        }
        r = ws_showcase_build(&catalog->showcase, index, seed);
        if (r != WS_OK) {
            ws_showcase_free(&catalog->showcase);
            return r;
        }
        catalog->is_env = 0;
        catalog->scene = &catalog->showcase.scene;
        return WS_OK;
    }

    ws_env_scene_options opts = ws_env_scene_options_default(seed);
    opts.scene_kind = env_kind_for_index(index, showcase_count);
    ws_result r = ws_env_scene_create(&catalog->env, &opts);
    if (r != WS_OK) {
        return r;
    }
    catalog->is_env = 1;
    catalog->scene = &catalog->env.scene;
    return WS_OK;
}

const ws_scene *ws_catalog_scene(const ws_catalog *catalog)
{
    if (catalog == NULL) {
        return NULL;
    }
    return catalog->scene;
}

void ws_catalog_free(ws_catalog *catalog)
{
    if (catalog == NULL) {
        return;
    }
    if (catalog->is_env) {
        ws_env_scene_free(&catalog->env);
    } else {
        ws_showcase_free(&catalog->showcase);
    }
    catalog->scene = NULL;
    catalog->is_env = 0;
}
