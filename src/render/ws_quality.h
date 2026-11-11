/* Worldseed Engine — product visual-quality checklist.
 *
 * P12 acceptance (docs/13, docs/19) requires the six product reference scenes
 * to be judged by a documented visual-quality checklist, not by a golden hash
 * alone: geometry, material, light, shadow and atmosphere must each carry a
 * measurable signal in the rendered frame. This module turns those qualitative
 * aspects into objective, deterministic per-frame metrics and a per-scene
 * reference specification with explicit thresholds, so the checklist is
 * automated evidence rather than a human claim.
 *
 * The metrics are computed from the real rendered framebuffer (display space)
 * and the scene that produced it, so they cannot drift from the product. They
 * are technical signals only; the aesthetic reference targets stay in
 * docs/QUALITY-CHECKLIST.md.
 */
#ifndef WS_RENDER_QUALITY_H
#define WS_RENDER_QUALITY_H

#include <stddef.h>
#include <stdint.h>

#include "../core/ws_result.h"
#include "ws_framebuffer.h"
#include "ws_scene.h"

/* Objective, deterministic metrics of one rendered frame. */
typedef struct ws_quality_metrics {
    /* geometry */
    uint64_t triangles_submitted;
    uint64_t triangles_drawn;
    uint64_t pixels_shaded;
    uint32_t object_count;
    double geometry_coverage; /* pixels_shaded / pixel count, capped at 1 */
    double coverage;          /* fraction of pixels differing from clear colour */
    /* light / tone */
    double mean_luma;         /* mean luminance, 0..255 */
    double max_luma;          /* peak luminance, 0..255 */
    double luma_stddev;       /* shading contrast across the frame */
    double dark_fraction;     /* fraction of pixels below the shadow floor */
    double blown_fraction;    /* fraction of pixels near white (over-exposed) */
    /* material / colour */
    double mean_saturation;   /* mean HSV saturation, 0..1 */
    double colour_variety;    /* distinct quantised colours in the frame (count) */
    /* atmosphere */
    double sky_gradient;      /* top-band minus bottom-band mean luminance */
} ws_quality_metrics;

/* Per-scene reference thresholds. A metric equal to its threshold passes. */
typedef struct ws_quality_spec {
    const char *scene_id;     /* catalog scene id, e.g. "terrain_day" */
    double min_geometry_coverage; /* lit geometry must fill enough of the frame */
    double min_coverage;
    double min_luma_stddev;
    double min_colour_variety; /* distinct quantised colours (absolute count) */
    double min_saturation;
    double min_sky_gradient;  /* only checked when require_sky is set */
    uint32_t min_objects;     /* geometric detail: objects in the scene */
    double max_blown;         /* over-exposure ceiling */
    int require_sky;          /* scene must show a gradient sky */
} ws_quality_spec;

/* Failure bitmask values reported by ws_quality_check. */
#define WS_QUALITY_FAIL_GEOMETRY   0x01u
#define WS_QUALITY_FAIL_COVERAGE   0x02u
#define WS_QUALITY_FAIL_CONTRAST   0x04u
#define WS_QUALITY_FAIL_VARIETY    0x08u
#define WS_QUALITY_FAIL_SATURATION 0x10u
#define WS_QUALITY_FAIL_SKY        0x20u
#define WS_QUALITY_FAIL_OBJECTS    0x40u
#define WS_QUALITY_FAIL_BLOWN      0x80u
/* Tool-level: the same seed did not reproduce a bit-identical frame. */
#define WS_QUALITY_FAIL_DETERMINISM 0x100u

/* Measure the objective metrics of a rendered frame. The triangle/pixel
 * counters come from the render stats of the same render. */
ws_result ws_quality_measure(const ws_scene *scene, const ws_framebuffer *fb,
                             uint64_t triangles_submitted, uint64_t triangles_drawn,
                             uint64_t pixels_shaded, ws_quality_metrics *out);

/* The six product reference scenes (docs/00 §5) and their thresholds. */
size_t ws_quality_spec_count(void);
const ws_quality_spec *ws_quality_spec_at(size_t index);

/* Evaluate metrics against a spec. Returns WS_OK when every check passes;
 * WS_ERR_INVALID_ARGUMENT otherwise, writing the failed-check bitmask to
 * `out_failures` (may be NULL). */
ws_result ws_quality_check(const ws_quality_spec *spec, const ws_quality_metrics *m,
                           uint32_t *out_failures);

#endif /* WS_RENDER_QUALITY_H */
