/* Worldseed Engine — golden-scene harness.
 *
 * Renders a small set of fixed, versioned reference scenes on the CPU path and
 * writes each frame as PPM plus a manifest line with technical metrics
 * (triangle counts, coverage, mean/max luminance, frame SHA-256, render time).
 * This is the automated visual-regression and coverage evidence for P2. It does
 * not claim aesthetic quality; the quality protocol in docs/08 is separate.
 *
 * The scenes and their assets come from the shared showcase module
 * (ws_showcase), which the Windows application also renders, so the reference
 * frames and the user-visible preview cannot drift apart.
 *
 * Usage: ws_golden <output-dir> [seed]
 */
#include "core/ws_hash.h"
#include "platform/ws_clock.h"
#include "render/ws_catalog.h"
#include "render/ws_framebuffer.h"
#include "render/ws_image.h"
#include "render/ws_render.h"
#include "render/ws_showcase.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GOLDEN_WIDTH 320u
#define GOLDEN_HEIGHT 240u

static void hash_hex(const uint8_t *data, size_t len, char *out)
{
    uint8_t digest[WS_SHA256_DIGEST_SIZE];
    ws_sha256_buffer(data, len, digest);
    ws_sha256_hex(digest, out);
}

/* Mean and max luminance over the RGB channels (0..255 display space). */
static void luminance_stats(const ws_framebuffer *fb, double *mean_out, double *max_out)
{
    double sum = 0.0;
    double peak = 0.0;
    size_t count = (size_t)fb->width * (size_t)fb->height;
    for (size_t i = 0; i < count; ++i) {
        const uint8_t *p = &fb->pixels[i * 4u];
        double lum = 0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2];
        sum += lum;
        if (lum > peak) {
            peak = lum;
        }
    }
    *mean_out = count ? sum / (double)count : 0.0;
    *max_out = peak;
}

int main(int argc, char **argv)
{
    const char *out_dir = argc > 1 ? argv[1] : "evidence/golden";
    uint64_t seed = argc > 2 ? strtoull(argv[2], NULL, 10) : 20260101ULL;

    char manifest_path[512];
    snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.csv", out_dir);
    FILE *manifest = fopen(manifest_path, "wb");
    if (manifest == NULL) {
        fprintf(stderr, "cannot write manifest at %s (does the directory exist?)\n", manifest_path);
        return 1;
    }
    fprintf(manifest, "scene,seed,width,height,triangles_submitted,triangles_drawn,pixels_shaded,coverage,mean_luma,max_luma,render_ms,sha256\n");

    ws_framebuffer fb;
    if (ws_framebuffer_create(&fb, GOLDEN_WIDTH, GOLDEN_HEIGHT) != WS_OK) {
        fprintf(stderr, "framebuffer allocation failed\n");
        fclose(manifest);
        return 1;
    }
    size_t pixel_count = (size_t)fb.width * (size_t)fb.height;
    float *depth = malloc(pixel_count * sizeof(float));
    if (depth == NULL) {
        fprintf(stderr, "depth allocation failed\n");
        ws_framebuffer_free(&fb);
        fclose(manifest);
        return 1;
    }

    int failed = 0;
    size_t scene_count = ws_catalog_count();
    for (size_t i = 0; i < scene_count; ++i) {
        const char *id = ws_catalog_id(i);
        ws_catalog catalog;
        if (ws_catalog_build(&catalog, i, seed) != WS_OK) {
            fprintf(stderr, "catalog build failed for scene %s\n", id);
            failed = 1;
            continue;
        }
        const ws_scene *scene = ws_catalog_scene(&catalog);

        ws_render_stats stats;
        uint64_t t0 = ws_clock_monotonic_ns();
        ws_result res = ws_render_scene(scene, &fb, depth, 32u, &stats);
        uint64_t t1 = ws_clock_monotonic_ns();
        if (res != WS_OK) {
            fprintf(stderr, "render failed for %s: %s\n", id, ws_result_name(res));
            failed = 1;
            ws_catalog_free(&catalog);
            continue;
        }

        char frame_path[512];
        snprintf(frame_path, sizeof(frame_path), "%s/%s.ppm", out_dir, id);
        if (ws_framebuffer_write_ppm(&fb, frame_path) != WS_OK) {
            fprintf(stderr, "ppm write failed for %s\n", id);
            failed = 1;
            ws_catalog_free(&catalog);
            continue;
        }

        size_t bytes = pixel_count * 4u;
        char digest[WS_SHA256_DIGEST_SIZE * 2u + 1u];
        hash_hex(fb.pixels, bytes, digest);

        double coverage = 0.0;
        (void)ws_image_coverage(&fb, scene->background, 8, &coverage);
        double mean_luma = 0.0;
        double max_luma = 0.0;
        luminance_stats(&fb, &mean_luma, &max_luma);
        double render_ms = ws_clock_ns_to_seconds(t1 - t0) * 1000.0;

        fprintf(manifest, "%s,%llu,%u,%u,%llu,%llu,%llu,%.4f,%.3f,%.3f,%.3f,%s\n",
                id, (unsigned long long)seed, fb.width, fb.height,
                (unsigned long long)stats.triangles_submitted,
                (unsigned long long)stats.triangles_drawn,
                (unsigned long long)stats.pixels_shaded,
                coverage, mean_luma, max_luma, render_ms, digest);
        printf("golden scene=%s sha256=%s coverage=%.4f mean_luma=%.2f max_luma=%.2f triangles=%llu\n",
               id, digest, coverage, mean_luma, max_luma,
               (unsigned long long)stats.triangles_submitted);

        /* Coverage measures deviation from the clear colour; it is only a
         * meaningful sanity gate when a flat background is used. Sky scenes
         * fill the frame on purpose, so they are checked differently. */
        if (!scene->settings.sky_enabled) {
            if (coverage <= 0.02 || coverage >= 0.98) {
                fprintf(stderr, "scene %s coverage out of range: %.4f\n", id, coverage);
                failed = 1;
            }
        } else if (stats.triangles_drawn == 0u) {
            fprintf(stderr, "scene %s drew no geometry\n", id);
            failed = 1;
        }
        /* A frame must carry signal: not black, and not fully blown out. */
        if (mean_luma <= 1.0) {
            fprintf(stderr, "scene %s is near-black (mean_luma=%.3f)\n", id, mean_luma);
            failed = 1;
        }
        if (max_luma < 30.0) {
            fprintf(stderr, "scene %s has no bright pixels (max_luma=%.3f)\n", id, max_luma);
            failed = 1;
        }
        ws_catalog_free(&catalog);
    }

    free(depth);
    ws_framebuffer_free(&fb);
    fclose(manifest);
    if (failed) {
        return 1;
    }
    printf("WS_GOLDEN_OK scenes=%zu manifest=%s\n", scene_count, manifest_path);
    return 0;
}
