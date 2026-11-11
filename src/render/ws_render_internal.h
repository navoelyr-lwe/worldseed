/* Worldseed Engine — internal renderer hooks (not part of the product API).
 *
 * These exist only so the unit tests can drive the renderer's internal stages
 * directly and prove invariants that are not observable through the public
 * entry point — most importantly that the parallel, row-banded shadow-map build
 * is bit-identical to the serial build. Nothing outside the library links these.
 */
#ifndef WS_RENDER_RENDER_INTERNAL_H
#define WS_RENDER_RENDER_INTERNAL_H

#include "../core/ws_result.h"
#include "ws_scene.h"

/* Build the scene's directional shadow depth map (size*size, row-major, filled
 * with 1.0 where nothing occludes) using exactly `band_count` disjoint row
 * bands. band_count == 1 is the serial build; larger values are split across the
 * worker pool. The result must be bit-identical for every band_count, which is
 * what makes the parallel product path safe to enable.
 *
 * `depth` must hold size*size floats. Returns WS_OK, or an error for invalid
 * arguments or when the scene has no geometry to occlude. */
ws_result ws_render_shadow_build_for_test(const ws_scene *scene, float *depth,
                                          unsigned size, unsigned band_count);

#endif /* WS_RENDER_RENDER_INTERNAL_H */
