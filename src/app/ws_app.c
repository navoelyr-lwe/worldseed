/* Worldseed Engine — application entry and headless capture. */
#include "ws_app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/ws_hash.h"
#include "../render/ws_image.h"
#include "../render/ws_render.h"
#include "../render/ws_showcase.h"

#define APP_DEFAULT_WIDTH 960u
#define APP_DEFAULT_HEIGHT 540u

ws_result ws_app_parse_options(int argc, char **argv, ws_app_options *opts)
{
    if (opts == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    opts->list_scenes = 0;
    opts->headless = 0;
    opts->selftest = 0;
    opts->capture = 0;
    opts->scene_index = 0;
    opts->seed = 20260101ULL;
    opts->width = APP_DEFAULT_WIDTH;
    opts->height = APP_DEFAULT_HEIGHT;
    opts->capture_path = NULL;

    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (strcmp(a, "--list") == 0) {
            opts->list_scenes = 1;
        } else if (strcmp(a, "--headless") == 0) {
            opts->headless = 1;
        } else if (strcmp(a, "--selftest") == 0) {
            opts->selftest = 1;
        } else if (strcmp(a, "--capture") == 0) {
            if (i + 1 >= argc) {
                return WS_ERR_INVALID_ARGUMENT;
            }
            opts->capture = 1;
            opts->capture_path = argv[++i];
        } else if (strcmp(a, "--scene") == 0) {
            if (i + 1 >= argc) {
                return WS_ERR_INVALID_ARGUMENT;
            }
            long v = strtol(argv[++i], NULL, 10);
            if (v < 0 || (size_t)v >= ws_showcase_scene_count()) {
                return WS_ERR_INVALID_ARGUMENT;
            }
            opts->scene_index = (int)v;
        } else if (strcmp(a, "--seed") == 0) {
            if (i + 1 >= argc) {
                return WS_ERR_INVALID_ARGUMENT;
            }
            opts->seed = strtoull(argv[++i], NULL, 10);
        } else if (strcmp(a, "--size") == 0) {
            if (i + 1 >= argc) {
                return WS_ERR_INVALID_ARGUMENT;
            }
            unsigned w = 0u;
            unsigned h = 0u;
            if (sscanf(argv[++i], "%ux%u", &w, &h) != 2 || w == 0u || h == 0u ||
                w > 16384u || h > 16384u) {
                return WS_ERR_INVALID_ARGUMENT;
            }
            opts->width = w;
            opts->height = h;
        } else {
            return WS_ERR_INVALID_ARGUMENT;
        }
    }
    return WS_OK;
}

static ws_result render_into(const ws_app_options *opts, size_t scene_index,
                             ws_framebuffer *fb, float *depth)
{
    ws_showcase showcase;
    ws_result r = ws_showcase_init(&showcase);
    if (r != WS_OK) {
        return r;
    }
    r = ws_showcase_build(&showcase, scene_index, opts->seed);
    if (r != WS_OK) {
        ws_showcase_free(&showcase);
        return r;
    }
    r = ws_render_scene(&showcase.scene, fb, depth, 32u, NULL);
    ws_showcase_free(&showcase);
    return r;
}

ws_result ws_app_render_scene(const ws_app_options *opts, ws_framebuffer *fb,
                              float *depth)
{
    if (opts == NULL || fb == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    return render_into(opts, (size_t)opts->scene_index, fb, depth);
}

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

static int capture_one(const ws_app_options *opts, size_t scene_index,
                       ws_framebuffer *fb, float *depth)
{
    const char *id = ws_showcase_scene_id(scene_index);
    if (render_into(opts, scene_index, fb, depth) != WS_OK) {
        fprintf(stderr, "render failed for scene %s\n", id);
        return 0;
    }
    if (ws_framebuffer_write_ppm(fb, opts->capture_path) != WS_OK) {
        fprintf(stderr, "cannot write %s\n", opts->capture_path);
        return 0;
    }
    uint8_t digest[WS_SHA256_DIGEST_SIZE];
    char hex[WS_SHA256_DIGEST_SIZE * 2u + 1u];
    ws_sha256_buffer(fb->pixels, (size_t)fb->width * fb->height * 4u, digest);
    ws_sha256_hex(digest, hex);
    printf("capture scene=%s size=%ux%u path=%s sha256=%s\n",
           id, fb->width, fb->height, opts->capture_path, hex);
    return 1;
}

static int selftest(const ws_app_options *opts, ws_framebuffer *fb, float *depth)
{
    size_t scene_count = ws_showcase_scene_count();
    int ok = 1;
    for (size_t i = 0; i < scene_count; ++i) {
        const char *id = ws_showcase_scene_id(i);
        if (render_into(opts, i, fb, depth) != WS_OK) {
            fprintf(stderr, "selftest: render failed for %s\n", id);
            ok = 0;
            continue;
        }
        double coverage = 0.0;
        uint8_t bg[3] = {24u, 28u, 36u};
        (void)ws_image_coverage(fb, bg, 8, &coverage);
        double mean_luma = 0.0;
        double max_luma = 0.0;
        luminance_stats(fb, &mean_luma, &max_luma);
        uint8_t digest[WS_SHA256_DIGEST_SIZE];
        char hex[WS_SHA256_DIGEST_SIZE * 2u + 1u];
        ws_sha256_buffer(fb->pixels, (size_t)fb->width * fb->height * 4u, digest);
        ws_sha256_hex(digest, hex);
        printf("selftest scene=%s sha256=%s coverage=%.4f mean_luma=%.2f max_luma=%.2f\n",
               id, hex, coverage, mean_luma, max_luma);
        if (mean_luma <= 1.0 || max_luma < 30.0) {
            fprintf(stderr, "selftest: scene %s carries no signal\n", id);
            ok = 0;
        }
    }
    return ok;
}

int ws_app_run(const ws_app_options *opts)
{
    if (opts == NULL) {
        return 2;
    }
    if (opts->list_scenes) {
        size_t n = ws_showcase_scene_count();
        for (size_t i = 0; i < n; ++i) {
            printf("%zu %s\n", i, ws_showcase_scene_id(i));
        }
        return 0;
    }

    ws_framebuffer fb;
    if (ws_framebuffer_create(&fb, opts->width, opts->height) != WS_OK) {
        fprintf(stderr, "framebuffer allocation failed\n");
        return 1;
    }
    float *depth = malloc((size_t)opts->width * opts->height * sizeof(float));
    if (depth == NULL) {
        fprintf(stderr, "depth allocation failed\n");
        ws_framebuffer_free(&fb);
        return 1;
    }

    int rc = 0;
    if (opts->selftest) {
        rc = selftest(opts, &fb, depth) ? 0 : 1;
        if (rc == 0) {
            printf("APP_SELFTEST_OK scenes=%zu\n", ws_showcase_scene_count());
        }
    } else if (opts->capture) {
        rc = capture_one(opts, (size_t)opts->scene_index, &fb, depth) ? 0 : 1;
    } else if (opts->headless) {
        if (render_into(opts, (size_t)opts->scene_index, &fb, depth) != WS_OK) {
            fprintf(stderr, "render failed\n");
            rc = 1;
        } else {
            double mean_luma = 0.0;
            double max_luma = 0.0;
            luminance_stats(&fb, &mean_luma, &max_luma);
            printf("headless scene=%s size=%ux%u mean_luma=%.2f max_luma=%.2f\n",
                   ws_showcase_scene_id((size_t)opts->scene_index),
                   fb.width, fb.height, mean_luma, max_luma);
        }
    } else {
        rc = ws_app_present_win32(opts);
    }

    free(depth);
    ws_framebuffer_free(&fb);
    return rc;
}
