/* Worldseed Engine — product visual-quality checklist harness.
 *
 * Renders the six product reference scenes (docs/00 §5) from the real scene
 * catalog and evaluates each against the objective checklist in
 * docs/QUALITY-CHECKLIST.md, writing a CSV of metrics and a per-scene verdict.
 * This is the automated evidence for the P12 "visual quality checklist"
 * acceptance gate; a golden hash alone is not enough (docs/12).
 *
 * Usage: ws_quality <output-dir> [seed]
 * Exits non-zero (and prints WS_QUALITY_FAIL) when any scene fails its spec.
 */
#include "core/ws_hash.h"
#include "render/ws_catalog.h"
#include "render/ws_framebuffer.h"
#include "render/ws_quality.h"
#include "render/ws_render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QUALITY_WIDTH 640u
#define QUALITY_HEIGHT 360u

static const char *failure_names(uint32_t fail)
{
    static char buf[128];
    buf[0] = '\0';
    struct { uint32_t bit; const char *name; } names[] = {
        {WS_QUALITY_FAIL_GEOMETRY, "geometry"},
        {WS_QUALITY_FAIL_COVERAGE, "coverage"},
        {WS_QUALITY_FAIL_CONTRAST, "contrast"},
        {WS_QUALITY_FAIL_VARIETY, "variety"},
        {WS_QUALITY_FAIL_SATURATION, "saturation"},
        {WS_QUALITY_FAIL_SKY, "sky"},
        {WS_QUALITY_FAIL_OBJECTS, "objects"},
        {WS_QUALITY_FAIL_BLOWN, "blown"},
        {WS_QUALITY_FAIL_DETERMINISM, "determinism"},
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (fail & names[i].bit) {
            if (buf[0] != '\0') {
                strncat(buf, "+", sizeof(buf) - strlen(buf) - 1u);
            }
            strncat(buf, names[i].name, sizeof(buf) - strlen(buf) - 1u);
        }
    }
    return buf;
}

/* Index of a catalog scene by its stable id, or -1. */
static long find_scene(const char *id)
{
    size_t n = ws_catalog_count();
    for (size_t i = 0; i < n; ++i) {
        const char *cid = ws_catalog_id(i);
        if (cid != NULL && strcmp(cid, id) == 0) {
            return (long)i;
        }
    }
    return -1;
}

int main(int argc, char **argv)
{
    const char *out_dir = argc > 1 ? argv[1] : "evidence/quality";
    uint64_t seed = argc > 2 ? strtoull(argv[2], NULL, 10) : 20260101ULL;

    char csv_path[512];
    snprintf(csv_path, sizeof(csv_path), "%s/quality.csv", out_dir);
    FILE *csv = fopen(csv_path, "wb");
    if (csv == NULL) {
        fprintf(stderr, "cannot write %s (does the directory exist?)\n", csv_path);
        return 1;
    }
    fprintf(csv, "scene,seed,width,height,objects,triangles_submitted,triangles_drawn,"
                 "pixels_shaded,geometry_coverage,coverage,mean_luma,max_luma,luma_stddev,"
                 "dark_fraction,blown_fraction,mean_saturation,colour_variety,sky_gradient,"
                 "failures,verdict\n");

    ws_framebuffer fb;
    if (ws_framebuffer_create(&fb, QUALITY_WIDTH, QUALITY_HEIGHT) != WS_OK) {
        fprintf(stderr, "framebuffer allocation failed\n");
        fclose(csv);
        return 1;
    }
    size_t pixel_count = (size_t)fb.width * (size_t)fb.height;
    float *depth = malloc(pixel_count * sizeof(float));
    if (depth == NULL) {
        fprintf(stderr, "depth allocation failed\n");
        ws_framebuffer_free(&fb);
        fclose(csv);
        return 1;
    }

    int failed = 0;
    size_t spec_count = ws_quality_spec_count();
    for (size_t i = 0; i < spec_count; ++i) {
        const ws_quality_spec *spec = ws_quality_spec_at(i);
        long index = find_scene(spec->scene_id);
        if (index < 0) {
            fprintf(stderr, "quality: scene %s not in catalog\n", spec->scene_id);
            failed = 1;
            continue;
        }
        ws_catalog catalog;
        if (ws_catalog_build(&catalog, (size_t)index, seed) != WS_OK) {
            fprintf(stderr, "quality: catalog build failed for %s\n", spec->scene_id);
            failed = 1;
            continue;
        }
        const ws_scene *scene = ws_catalog_scene(&catalog);

        ws_render_stats stats;
        ws_result res = ws_render_scene(scene, &fb, depth, 32u, &stats);
        if (res != WS_OK) {
            fprintf(stderr, "quality: render failed for %s: %s\n", spec->scene_id,
                    ws_result_name(res));
            failed = 1;
            ws_catalog_free(&catalog);
            continue;
        }

        /* Determinism: the same seed must reproduce the same frame. */
        uint8_t digest[WS_SHA256_DIGEST_SIZE];
        ws_sha256_buffer(fb.pixels, pixel_count * 4u, digest);
        ws_render_scene(scene, &fb, depth, 32u, &stats);
        uint8_t digest2[WS_SHA256_DIGEST_SIZE];
        ws_sha256_buffer(fb.pixels, pixel_count * 4u, digest2);
        int reproducible = memcmp(digest, digest2, sizeof(digest)) == 0;

        ws_quality_metrics m;
        if (ws_quality_measure(scene, &fb, stats.triangles_submitted,
                               stats.triangles_drawn, stats.pixels_shaded, &m) != WS_OK) {
            fprintf(stderr, "quality: measure failed for %s\n", spec->scene_id);
            failed = 1;
            ws_catalog_free(&catalog);
            continue;
        }

        uint32_t failures = 0u;
        ws_result verdict = ws_quality_check(spec, &m, &failures);
        if (!reproducible) {
            failures |= WS_QUALITY_FAIL_DETERMINISM;
        }
        int scene_ok = (verdict == WS_OK) && reproducible;
        if (!scene_ok) {
            failed = 1;
        }

        fprintf(csv, "%s,%llu,%u,%u,%u,%llu,%llu,%llu,%.4f,%.4f,%.3f,%.3f,%.3f,"
                     "%.4f,%.4f,%.4f,%.6f,%.3f,%s,%s\n",
                spec->scene_id, (unsigned long long)seed, fb.width, fb.height,
                m.object_count,
                (unsigned long long)m.triangles_submitted,
                (unsigned long long)m.triangles_drawn,
                (unsigned long long)m.pixels_shaded,
                m.geometry_coverage, m.coverage, m.mean_luma, m.max_luma, m.luma_stddev,
                m.dark_fraction, m.blown_fraction, m.mean_saturation,
                m.colour_variety, m.sky_gradient,
                scene_ok ? "-" : failure_names(failures),
                scene_ok ? "PASS" : "FAIL");

        printf("quality scene=%s verdict=%s reproducible=%d objects=%u geom_cov=%.3f "
               "coverage=%.3f mean_luma=%.2f stddev=%.2f sat=%.3f variety=%.5f "
               "sky_gradient=%.3f failures=%s\n",
               spec->scene_id, scene_ok ? "PASS" : "FAIL", reproducible, m.object_count,
               m.geometry_coverage, m.coverage, m.mean_luma, m.luma_stddev,
               m.mean_saturation, m.colour_variety, m.sky_gradient,
               scene_ok ? "-" : failure_names(failures));

        ws_catalog_free(&catalog);
    }

    free(depth);
    ws_framebuffer_free(&fb);
    fclose(csv);
    if (failed) {
        printf("WS_QUALITY_FAIL scenes=%zu\n", spec_count);
        return 1;
    }
    printf("WS_QUALITY_OK scenes=%zu csv=%s\n", spec_count, csv_path);
    return 0;
}
