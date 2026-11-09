/* Worldseed Engine — golden-scene harness.
 *
 * Renders a small set of fixed, versioned reference scenes on the CPU path and
 * writes each frame as PPM plus a manifest line with technical metrics
 * (triangle counts, coverage, mean/max luminance, frame SHA-256, render time).
 * This is the automated visual-regression and coverage evidence for P2. It does
 * not claim aesthetic quality; the quality protocol in docs/08 is separate.
 *
 * Usage: ws_golden <output-dir> [seed]
 */
#include "core/ws_hash.h"
#include "platform/ws_clock.h"
#include "render/ws_framebuffer.h"
#include "render/ws_image.h"
#include "render/ws_mesh.h"
#include "render/ws_render.h"
#include "render/ws_scene.h"
#include "render/ws_texture.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GOLDEN_WIDTH 320u
#define GOLDEN_HEIGHT 240u
#define GOLDEN_MAX_OBJECTS 12u

typedef struct golden_buffers {
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
    ws_scene_object objects[GOLDEN_MAX_OBJECTS];
    ws_scene scene;
    ws_framebuffer fb;
    float *depth;
} golden_buffers;

static int buffers_init(golden_buffers *b)
{
    const uint8_t cube_color[3] = {180u, 150u, 90u};
    const uint8_t plane_color[3] = {70u, 110u, 60u};
    const uint8_t sphere_color[3] = {120u, 90u, 70u};
    if (ws_mesh_make_cube(&b->cube, b->cube_verts, 24u, b->cube_idx, 36u, cube_color) != WS_OK) {
        return 0;
    }
    if (ws_mesh_make_plane(&b->plane, b->plane_verts, 4u, b->plane_idx, 6u, 6.0, 1u, plane_color) != WS_OK) {
        return 0;
    }
    if (ws_mesh_make_sphere(&b->sphere, b->sphere_verts, 17u * 9u, b->sphere_idx, 16u * 9u * 6u,
                            16u, 8u, sphere_color) != WS_OK) {
        return 0;
    }
    const uint8_t c0[3] = {200u, 200u, 200u};
    const uint8_t c1[3] = {60u, 60u, 60u};
    if (ws_texture_make_checker(&b->checker, 64u, 8u, c0, c1) != WS_OK) {
        return 0;
    }
    const uint8_t base[3] = {150u, 120u, 90u};
    if (ws_texture_make_noise(&b->noise, 64u, 1234ULL, base, 0.25) != WS_OK) {
        return 0;
    }
    if (ws_scene_init(&b->scene, b->objects, GOLDEN_MAX_OBJECTS) != WS_OK) {
        return 0;
    }
    if (ws_framebuffer_create(&b->fb, GOLDEN_WIDTH, GOLDEN_HEIGHT) != WS_OK) {
        return 0;
    }
    size_t count = (size_t)b->fb.width * (size_t)b->fb.height;
    b->depth = malloc(count * sizeof(float));
    return b->depth != NULL;
}

static void buffers_free(golden_buffers *b)
{
    free(b->depth);
    ws_framebuffer_free(&b->fb);
    ws_texture_free(&b->checker);
    ws_texture_free(&b->noise);
}

static void scene_ground(golden_buffers *b)
{
    ws_mat4 t = ws_mat4_translation(ws_vec3_make(0.0, -0.5, 0.0));
    (void)ws_scene_add_object(&b->scene, &b->plane, t,
                              ws_material_make(0.30, 0.42, 0.24, 0.9, 0.0, 0.0));
}

static int scene_orbit(golden_buffers *b, uint64_t seed, const char *dir)
{
    (void)seed; (void)dir;
    scene_ground(b);
    ws_mat4 t = ws_mat4_multiply(ws_mat4_translation(ws_vec3_make(0.0, 0.0, 0.0)),
                                 ws_mat4_rotation_y(ws_deg_to_rad(30.0)));
    if (ws_scene_add_object(&b->scene, &b->cube, t,
                            ws_material_make(0.72, 0.58, 0.36, 0.7, 0.0, 0.15)) != WS_OK) {
        return 0;
    }
    ws_mat4 s = ws_mat4_multiply(ws_mat4_translation(ws_vec3_make(1.4, 0.0, 0.6)),
                                 ws_mat4_scale(ws_vec3_make(0.8, 0.8, 0.8)));
    if (ws_scene_add_object(&b->scene, &b->sphere, s,
                            ws_material_make(0.55, 0.42, 0.32, 0.5, 0.0, 0.3)) != WS_OK) {
        return 0;
    }
    b->scene.camera = ws_camera_default();
    b->scene.camera.eye = ws_vec3_make(2.6, 1.8, 3.2);
    b->scene.camera.target = ws_vec3_make(0.3, 0.1, 0.0);
    return 1;
}

static int scene_lowlight(golden_buffers *b, uint64_t seed, const char *dir)
{
    (void)seed; (void)dir;
    scene_ground(b);
    ws_mat4 t = ws_mat4_multiply(ws_mat4_translation(ws_vec3_make(-0.3, 0.1, 0.0)),
                                 ws_mat4_rotation_y(ws_deg_to_rad(-20.0)));
    if (ws_scene_add_object(&b->scene, &b->cube, t,
                            ws_material_make(0.60, 0.50, 0.34, 0.6, 0.0, 0.1)) != WS_OK) {
        return 0;
    }
    b->scene.camera = ws_camera_default();
    b->scene.camera.eye = ws_vec3_make(-2.4, 1.4, 2.8);
    b->scene.camera.target = ws_vec3_make(0.0, 0.0, 0.0);
    b->scene.light = ws_light_default();
    b->scene.light.direction = ws_vec3_normalize(ws_vec3_make(0.5, -0.7, 0.4));
    b->scene.light.intensity = 0.45;
    b->scene.light.ambient = 18u;
    b->scene.light.ambient_intensity = 0.08;
    b->scene.background[0] = 12u;
    b->scene.background[1] = 14u;
    b->scene.background[2] = 22u;
    return 1;
}

static int scene_spheres(golden_buffers *b, uint64_t seed, const char *dir)
{
    (void)seed; (void)dir;
    scene_ground(b);
    for (int i = 0; i < 3; ++i) {
        ws_mat4 t = ws_mat4_multiply(
            ws_mat4_translation(ws_vec3_make(-1.2 + 1.2 * (double)i, 0.0, -0.4)),
            ws_mat4_scale(ws_vec3_make(0.9, 0.9, 0.9)));
        ws_material m = ws_material_make(0.5 + 0.15 * (double)i, 0.4, 0.35, 0.4, 0.0, 0.2);
        if (ws_scene_add_object(&b->scene, &b->sphere, t, m) != WS_OK) {
            return 0;
        }
    }
    b->scene.camera = ws_camera_default();
    b->scene.camera.eye = ws_vec3_make(0.0, 1.6, 4.2);
    b->scene.camera.target = ws_vec3_make(0.0, 0.0, 0.0);
    return 1;
}

/* A row of spheres sweeping roughness and metallic: a material-response scene. */
static int scene_materials(golden_buffers *b, uint64_t seed, const char *dir)
{
    (void)seed; (void)dir;
    scene_ground(b);
    for (int i = 0; i < 5; ++i) {
        double rough = 0.1 + 0.2 * (double)i;
        double metal = (i % 2 == 0) ? 0.0 : 0.9;
        ws_mat4 t = ws_mat4_multiply(
            ws_mat4_translation(ws_vec3_make(-1.6 + 0.8 * (double)i, 0.0, 0.0)),
            ws_mat4_scale(ws_vec3_make(0.7, 0.7, 0.7)));
        ws_material m = ws_material_make(0.8, 0.6, 0.3, rough, metal, 0.8);
        if (ws_scene_add_object(&b->scene, &b->sphere, t, m) != WS_OK) {
            return 0;
        }
    }
    b->scene.camera = ws_camera_default();
    b->scene.camera.eye = ws_vec3_make(0.0, 1.4, 5.2);
    b->scene.camera.target = ws_vec3_make(0.0, 0.0, 0.0);
    b->scene.light.direction = ws_vec3_normalize(ws_vec3_make(-0.5, -1.0, -0.6));
    return 1;
}

/* Textured cube on a textured ground: exercises the texture sampler path. */
static int scene_textured(golden_buffers *b, uint64_t seed, const char *dir)
{
    (void)seed; (void)dir;
    ws_material ground = ws_material_make(0.5, 0.5, 0.5, 0.95, 0.0, 0.0);
    ground.albedo_map = &b->checker;
    ws_mat4 gt = ws_mat4_translation(ws_vec3_make(0.0, -0.5, 0.0));
    if (ws_scene_add_object(&b->scene, &b->plane, gt, ground) != WS_OK) {
        return 0;
    }
    ws_material cube = ws_material_make(0.7, 0.7, 0.7, 0.7, 0.0, 0.2);
    cube.albedo_map = &b->noise;
    ws_mat4 t = ws_mat4_multiply(ws_mat4_translation(ws_vec3_make(0.0, 0.0, 0.0)),
                                 ws_mat4_rotation_y(ws_deg_to_rad(35.0)));
    if (ws_scene_add_object(&b->scene, &b->cube, t, cube) != WS_OK) {
        return 0;
    }
    b->scene.camera = ws_camera_default();
    b->scene.camera.eye = ws_vec3_make(2.2, 1.6, 2.8);
    b->scene.camera.target = ws_vec3_make(0.0, 0.0, 0.0);
    return 1;
}

/* Daylight sky, hemisphere ambient, distance fog and a filmic tonemap. */
static int scene_sky(golden_buffers *b, uint64_t seed, const char *dir)
{
    (void)seed; (void)dir;
    for (int i = 0; i < 4; ++i) {
        ws_mat4 t = ws_mat4_multiply(
            ws_mat4_translation(ws_vec3_make(-3.0 + 2.0 * (double)i, -0.3, -6.0 - 2.0 * (double)i)),
            ws_mat4_scale(ws_vec3_make(0.8, 0.8, 0.8)));
        ws_material m = ws_material_make(0.35, 0.5, 0.25, 0.8, 0.0, 0.05);
        if (ws_scene_add_object(&b->scene, &b->sphere, t, m) != WS_OK) {
            return 0;
        }
    }
    ws_mat4 gt = ws_mat4_translation(ws_vec3_make(0.0, -0.9, 0.0));
    (void)ws_scene_add_object(&b->scene, &b->plane, gt,
                              ws_material_make(0.28, 0.36, 0.22, 0.95, 0.0, 0.0));
    b->scene.camera = ws_camera_default();
    b->scene.camera.eye = ws_vec3_make(0.0, 1.2, 6.0);
    b->scene.camera.target = ws_vec3_make(0.0, 0.4, -6.0);
    b->scene.camera.far_z = 200.0;
    b->scene.settings = ws_render_settings_default();
    b->scene.settings.sky_enabled = 1;
    b->scene.settings.tonemap = 1;
    b->scene.settings.exposure = 1.2;
    b->scene.settings.fog_density = 0.05;
    return 1;
}

typedef int (*golden_scene_fn)(golden_buffers *, uint64_t, const char *);

typedef struct golden_scene_def {
    const char *id;
    golden_scene_fn build;
} golden_scene_def;

static const golden_scene_def GOLDEN_SCENES[] = {
    {"orbit", scene_orbit},
    {"lowlight", scene_lowlight},
    {"spheres", scene_spheres},
    {"materials", scene_materials},
    {"textured", scene_textured},
    {"sky", scene_sky},
};

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

    int failed = 0;
    size_t scene_count = sizeof(GOLDEN_SCENES) / sizeof(GOLDEN_SCENES[0]);
    for (size_t i = 0; i < scene_count; ++i) {
        const golden_scene_def *def = &GOLDEN_SCENES[i];
        golden_buffers b;
        memset(&b, 0, sizeof(b));
        if (!buffers_init(&b)) {
            fprintf(stderr, "buffer init failed for scene %s\n", def->id);
            failed = 1;
            buffers_free(&b);
            continue;
        }
        if (!def->build(&b, seed, out_dir)) {
            fprintf(stderr, "scene build failed for %s\n", def->id);
            failed = 1;
            buffers_free(&b);
            continue;
        }

        ws_render_stats stats;
        uint64_t t0 = ws_clock_monotonic_ns();
        ws_result res = ws_render_scene(&b.scene, &b.fb, b.depth, 32u, &stats);
        uint64_t t1 = ws_clock_monotonic_ns();
        if (res != WS_OK) {
            fprintf(stderr, "render failed for %s: %s\n", def->id, ws_result_name(res));
            failed = 1;
            buffers_free(&b);
            continue;
        }

        char frame_path[512];
        snprintf(frame_path, sizeof(frame_path), "%s/%s.ppm", out_dir, def->id);
        if (ws_framebuffer_write_ppm(&b.fb, frame_path) != WS_OK) {
            fprintf(stderr, "ppm write failed for %s\n", def->id);
            failed = 1;
            buffers_free(&b);
            continue;
        }

        size_t bytes = (size_t)b.fb.width * (size_t)b.fb.height * 4u;
        char digest[WS_SHA256_DIGEST_SIZE * 2u + 1u];
        hash_hex(b.fb.pixels, bytes, digest);

        double coverage = 0.0;
        (void)ws_image_coverage(&b.fb, b.scene.background, 8, &coverage);
        double mean_luma = 0.0;
        double max_luma = 0.0;
        luminance_stats(&b.fb, &mean_luma, &max_luma);
        double render_ms = ws_clock_ns_to_seconds(t1 - t0) * 1000.0;

        fprintf(manifest, "%s,%llu,%u,%u,%llu,%llu,%llu,%.4f,%.3f,%.3f,%.3f,%s\n",
                def->id, (unsigned long long)seed, b.fb.width, b.fb.height,
                (unsigned long long)stats.triangles_submitted,
                (unsigned long long)stats.triangles_drawn,
                (unsigned long long)stats.pixels_shaded,
                coverage, mean_luma, max_luma, render_ms, digest);
        printf("golden scene=%s sha256=%s coverage=%.4f mean_luma=%.2f max_luma=%.2f triangles=%llu\n",
               def->id, digest, coverage, mean_luma, max_luma,
               (unsigned long long)stats.triangles_submitted);

        /* Coverage measures deviation from the clear colour; it is only a
         * meaningful sanity gate when a flat background is used. Sky scenes
         * fill the frame on purpose, so they are checked differently. */
        if (!b.scene.settings.sky_enabled) {
            if (coverage <= 0.02 || coverage >= 0.98) {
                fprintf(stderr, "scene %s coverage out of range: %.4f\n", def->id, coverage);
                failed = 1;
            }
        } else if (stats.triangles_drawn == 0u) {
            fprintf(stderr, "scene %s drew no geometry\n", def->id);
            failed = 1;
        }
        /* A frame must carry signal: not black, and not fully blown out. */
        if (mean_luma <= 1.0) {
            fprintf(stderr, "scene %s is near-black (mean_luma=%.3f)\n", def->id, mean_luma);
            failed = 1;
        }
        if (max_luma < 30.0) {
            fprintf(stderr, "scene %s has no bright pixels (max_luma=%.3f)\n", def->id, max_luma);
            failed = 1;
        }
        buffers_free(&b);
    }

    fclose(manifest);
    if (failed) {
        return 1;
    }
    printf("WS_GOLDEN_OK scenes=%zu manifest=%s\n", scene_count, manifest_path);
    return 0;
}
