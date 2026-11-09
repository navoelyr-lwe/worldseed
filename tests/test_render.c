/* Worldseed Engine — render foundation tests.
 *
 * Covers math (mat4/vec3), procedural mesh generation, the asset format and
 * validator, and the reference scene renderer: geometry projection, depth
 * ordering, tile-size invariance, image metrics and seed determinism.
 */
#include "ws_test.h"

#include "core/ws_math.h"
#include "render/ws_asset.h"
#include "render/ws_framebuffer.h"
#include "render/ws_image.h"
#include "render/ws_mesh.h"
#include "render/ws_render.h"
#include "render/ws_scene.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define APPROX(a, b, eps) (fabs((a) - (b)) <= (eps))

static int test_math_mat4_identity(void)
{
    ws_mat4 id = ws_mat4_identity();
    ws_vec4 v = ws_vec4_make(1.0, 2.0, 3.0, 1.0);
    ws_vec4 r = ws_mat4_mul_vec4(id, v);
    WS_CHECK(APPROX(r.x, 1.0, 1e-12) && APPROX(r.y, 2.0, 1e-12) && APPROX(r.z, 3.0, 1e-12));
    return 1;
}

static int test_math_mat4_multiply_order(void)
{
    /* translate * scale applied to a point: scale first, then translate. */
    ws_mat4 t = ws_mat4_translation(ws_vec3_make(10.0, 0.0, 0.0));
    ws_mat4 s = ws_mat4_scale(ws_vec3_make(2.0, 2.0, 2.0));
    ws_mat4 ts = ws_mat4_multiply(t, s);
    ws_vec4 r = ws_mat4_mul_vec4(ts, ws_vec4_make(1.0, 1.0, 1.0, 1.0));
    WS_CHECK(APPROX(r.x, 12.0, 1e-12) && APPROX(r.y, 2.0, 1e-12) && APPROX(r.z, 2.0, 1e-12));
    return 1;
}

static int test_math_vec3_ops(void)
{
    ws_vec3 a = ws_vec3_make(1.0, 0.0, 0.0);
    ws_vec3 b = ws_vec3_make(0.0, 1.0, 0.0);
    ws_vec3 c = ws_vec3_cross(a, b);
    WS_CHECK(APPROX(c.x, 0.0, 1e-12) && APPROX(c.y, 0.0, 1e-12) && APPROX(c.z, 1.0, 1e-12));
    ws_vec3 n = ws_vec3_normalize(ws_vec3_make(3.0, 4.0, 0.0));
    WS_CHECK(APPROX(ws_vec3_length(n), 1.0, 1e-12));
    ws_vec3 zero = ws_vec3_normalize(ws_vec3_make(0.0, 0.0, 0.0));
    WS_CHECK(APPROX(ws_vec3_length(zero), 0.0, 1e-12));
    return 1;
}

static int test_math_perspective(void)
{
    /* A point on the near plane must map to ndc z = -1; on the far plane +1. */
    ws_mat4 p = ws_mat4_perspective(ws_deg_to_rad(60.0), 1.0, 1.0, 100.0);
    ws_vec4 near_pt = ws_mat4_mul_vec4(p, ws_vec4_make(0.0, 0.0, -1.0, 1.0));
    ws_vec4 far_pt = ws_mat4_mul_vec4(p, ws_vec4_make(0.0, 0.0, -100.0, 1.0));
    WS_CHECK(APPROX(near_pt.z / near_pt.w, -1.0, 1e-9));
    WS_CHECK(APPROX(far_pt.z / far_pt.w, 1.0, 1e-9));
    return 1;
}

static int test_mesh_cube(void)
{
    ws_vertex verts[24];
    uint32_t idx[36];
    const uint8_t color[3] = {200u, 120u, 60u};
    ws_mesh mesh;
    WS_CHECK(ws_mesh_make_cube(&mesh, verts, 24u, idx, 36u, color) == WS_OK);
    WS_CHECK(mesh.vertex_count == 24u && mesh.index_count == 36u);
    WS_CHECK(ws_asset_validate_mesh(&mesh) == WS_OK);
    ws_aabb3 box = ws_mesh_bounds(&mesh);
    WS_CHECK(APPROX(box.min.x, -0.5, 1e-12) && APPROX(box.max.z, 0.5, 1e-12));
    /* Too-small arrays must be rejected. */
    WS_CHECK(ws_mesh_make_cube(&mesh, verts, 10u, idx, 36u, color) == WS_ERR_CAPACITY);
    return 1;
}

static int test_mesh_sphere(void)
{
    size_t vc = ws_mesh_sphere_vertex_count(16u, 8u);
    size_t ic = ws_mesh_sphere_index_count(16u, 8u);
    WS_CHECK(vc == 17u * 9u && ic == 16u * 8u * 6u);
    ws_vertex *verts = malloc(vc * sizeof(ws_vertex));
    uint32_t *idx = malloc(ic * sizeof(uint32_t));
    WS_CHECK(verts != NULL && idx != NULL);
    const uint8_t color[3] = {90u, 160u, 70u};
    ws_mesh mesh;
    WS_CHECK(ws_mesh_make_sphere(&mesh, verts, vc, idx, ic, 16u, 8u, color) == WS_OK);
    WS_CHECK(ws_asset_validate_mesh(&mesh) == WS_OK);
    /* Every vertex lies on the unit-diameter sphere. */
    for (uint32_t i = 0; i < mesh.vertex_count; ++i) {
        double len = ws_vec3_length(mesh.vertices[i].position);
        WS_CHECK(APPROX(len, 0.5, 1e-9));
    }
    free(verts);
    free(idx);
    return 1;
}

static int test_asset_roundtrip(void)
{
    ws_vertex verts[24];
    uint32_t idx[36];
    const uint8_t color[3] = {10u, 20u, 30u};
    ws_mesh cube;
    WS_CHECK(ws_mesh_make_cube(&cube, verts, 24u, idx, 36u, color) == WS_OK);

    const char *path = "asset_roundtrip.wsmesh";
    WS_CHECK(ws_asset_write_mesh(path, &cube) == WS_OK);

    ws_asset_info info;
    WS_CHECK(ws_asset_scan_mesh(path, &info) == WS_OK);
    WS_CHECK(info.version == WS_ASSET_MESH_VERSION);
    WS_CHECK(info.vertex_count == 24u && info.index_count == 36u);

    ws_vertex verts2[24];
    uint32_t idx2[36];
    ws_mesh loaded;
    WS_CHECK(ws_asset_load_mesh(path, &loaded, verts2, 24u, idx2, 36u) == WS_OK);
    WS_CHECK(loaded.vertex_count == 24u && loaded.index_count == 36u);
    /* Compare field by field: ws_vertex has padding that memcmp must not read. */
    for (uint32_t i = 0; i < 24u; ++i) {
        WS_CHECK(APPROX(verts[i].position.x, verts2[i].position.x, 1e-9));
        WS_CHECK(APPROX(verts[i].position.y, verts2[i].position.y, 1e-9));
        WS_CHECK(APPROX(verts[i].position.z, verts2[i].position.z, 1e-9));
        WS_CHECK(APPROX(verts[i].normal.x, verts2[i].normal.x, 1e-9));
        WS_CHECK(APPROX(verts[i].u, verts2[i].u, 1e-9));
        WS_CHECK(APPROX(verts[i].v, verts2[i].v, 1e-9));
        WS_CHECK(verts[i].color[0] == verts2[i].color[0]);
        WS_CHECK(verts[i].color[1] == verts2[i].color[1]);
        WS_CHECK(verts[i].color[2] == verts2[i].color[2]);
    }
    WS_CHECK(memcmp(idx, idx2, sizeof(idx)) == 0);

    /* Too-small load buffers must fail cleanly. */
    ws_mesh small;
    WS_CHECK(ws_asset_load_mesh(path, &small, verts2, 10u, idx2, 36u) == WS_ERR_CAPACITY);

    remove(path);
    return 1;
}

static int test_asset_validation(void)
{
    ws_vertex verts[3];
    uint32_t idx[3];
    const uint8_t color[3] = {1u, 2u, 3u};
    memset(verts, 0, sizeof(verts));
    verts[0].position = ws_vec3_make(0.0, 0.0, 0.0);
    verts[1].position = ws_vec3_make(1.0, 0.0, 0.0);
    verts[2].position = ws_vec3_make(0.0, 1.0, 0.0);
    (void)color;

    ws_mesh mesh = {verts, 3u, idx, 3u};
    idx[0] = 0u; idx[1] = 1u; idx[2] = 2u;
    WS_CHECK(ws_asset_validate_mesh(&mesh) == WS_OK);

    /* Out-of-range index. */
    idx[2] = 7u;
    WS_CHECK(ws_asset_validate_mesh(&mesh) == WS_ERR_INVALID_ARGUMENT);
    idx[2] = 2u;

    /* Non-multiple-of-three index count. */
    mesh.index_count = 2u;
    WS_CHECK(ws_asset_validate_mesh(&mesh) == WS_ERR_INVALID_ARGUMENT);
    mesh.index_count = 3u;

    /* Non-finite position. */
    verts[1].position.x = NAN;
    WS_CHECK(ws_asset_validate_mesh(&mesh) == WS_ERR_INVALID_ARGUMENT);
    verts[1].position.x = 1.0;

    WS_CHECK(ws_asset_validate_mesh(NULL) == WS_ERR_INVALID_ARGUMENT);
    return 1;
}

static int test_asset_rejects_malformed_file(void)
{
    const char *path = "asset_bad.wsmesh";
    FILE *f = fopen(path, "wb");
    WS_CHECK(f != NULL);
    fputs("worldseed-mesh 1\n", f);
    fputs("vertex 0 0 0 0 1 0 0 0 10 20 30\n", f);
    fputs("garbage line here\n", f);
    fclose(f);
    ws_asset_info info;
    WS_CHECK(ws_asset_scan_mesh(path, &info) == WS_ERR_PARSE);
    remove(path);

    /* Unsupported version. */
    f = fopen(path, "wb");
    WS_CHECK(f != NULL);
    fputs("worldseed-mesh 99\n", f);
    fclose(f);
    WS_CHECK(ws_asset_scan_mesh(path, &info) == WS_ERR_UNSUPPORTED);
    remove(path);

    WS_CHECK(ws_asset_scan_mesh("does_not_exist.wsmesh", &info) == WS_ERR_NOT_FOUND);
    return 1;
}

/* Shared scene fixture: a lit cube in front of the camera. */
typedef struct render_fixture {
    ws_vertex verts[24];
    uint32_t idx[36];
    ws_mesh cube;
    ws_scene_object objects[4];
    ws_scene scene;
    ws_framebuffer fb;
    float *depth;
} render_fixture;

static int fixture_build(render_fixture *fx)
{
    const uint8_t color[3] = {180u, 150u, 90u};
    if (ws_mesh_make_cube(&fx->cube, fx->verts, 24u, fx->idx, 36u, color) != WS_OK) {
        return 0;
    }
    if (ws_scene_init(&fx->scene, fx->objects, 4u) != WS_OK) {
        return 0;
    }
    ws_material mat = {{0.7, 0.6, 0.4}, 0.8, 0.0};
    ws_mat4 model = ws_mat4_multiply(ws_mat4_translation(ws_vec3_make(0.0, 0.0, 0.0)),
                                     ws_mat4_rotation_y(ws_deg_to_rad(30.0)));
    if (ws_scene_add_object(&fx->scene, &fx->cube, model, mat) != WS_OK) {
        return 0;
    }
    fx->scene.camera = ws_camera_default();
    if (ws_framebuffer_create(&fx->fb, 160u, 120u) != WS_OK) {
        return 0;
    }
    size_t count = (size_t)fx->fb.width * (size_t)fx->fb.height;
    fx->depth = malloc(count * sizeof(float));
    return fx->depth != NULL;
}

static void fixture_free(render_fixture *fx)
{
    free(fx->depth);
    ws_framebuffer_free(&fx->fb);
}

static int test_render_produces_content(void)
{
    render_fixture fx;
    WS_CHECK(fixture_build(&fx));
    ws_render_stats stats;
    WS_CHECK(ws_render_scene(&fx.scene, &fx.fb, fx.depth, 32u, &stats) == WS_OK);
    WS_CHECK(stats.triangles_submitted == 12u);
    WS_CHECK(stats.triangles_drawn > 0u);
    WS_CHECK(stats.pixels_shaded > 0u);

    double coverage = 0.0;
    WS_CHECK(ws_image_coverage(&fx.fb, fx.scene.background, 8, &coverage) == WS_OK);
    WS_CHECK(coverage > 0.02 && coverage < 0.98); /* not blank, not full */

    fixture_free(&fx);
    return 1;
}

static int test_render_tile_invariance(void)
{
    render_fixture a;
    render_fixture b;
    WS_CHECK(fixture_build(&a));
    WS_CHECK(fixture_build(&b));
    WS_CHECK(ws_render_scene(&a.scene, &a.fb, a.depth, 8u, NULL) == WS_OK);
    WS_CHECK(ws_render_scene(&b.scene, &b.fb, b.depth, 64u, NULL) == WS_OK);
    WS_CHECK(memcmp(a.fb.pixels, b.fb.pixels, (size_t)a.fb.width * a.fb.height * 4u) == 0);

    ws_image_diff diff;
    WS_CHECK(ws_image_compare(&a.fb, &b.fb, &diff) == WS_OK);
    WS_CHECK(diff.mean_abs_error == 0.0);
    WS_CHECK(diff.ssim > 0.999);
    fixture_free(&a);
    fixture_free(&b);
    return 1;
}

static int test_render_determinism(void)
{
    render_fixture a;
    render_fixture b;
    WS_CHECK(fixture_build(&a));
    WS_CHECK(fixture_build(&b));
    WS_CHECK(ws_render_scene(&a.scene, &a.fb, a.depth, 16u, NULL) == WS_OK);
    WS_CHECK(ws_render_scene(&b.scene, &b.fb, b.depth, 16u, NULL) == WS_OK);
    WS_CHECK(memcmp(a.fb.pixels, b.fb.pixels, (size_t)a.fb.width * a.fb.height * 4u) == 0);
    fixture_free(&a);
    fixture_free(&b);
    return 1;
}

static int test_render_depth_occlusion(void)
{
    /* Two coplanar-facing quads at different depths; the nearer one must win. */
    ws_vertex verts[4];
    uint32_t idx[6];
    const uint8_t color[3] = {255u, 255u, 255u};
    ws_mesh quad;
    WS_CHECK(ws_mesh_make_plane(&quad, verts, 4u, idx, 6u, 1.0, 1u, color) == WS_OK);

    ws_scene_object objects[2];
    ws_scene scene;
    WS_CHECK(ws_scene_init(&scene, objects, 2u) == WS_OK);
    ws_material near_mat = {{1.0, 0.0, 0.0}, 1.0, 0.0};
    ws_material far_mat = {{0.0, 0.0, 1.0}, 1.0, 0.0};
    /* Quads face +Y; camera looks down at them from above. Near is lower y? No:
     * with a +Y up camera, a smaller distance to camera means smaller y here. */
    ws_mat4 far_t = ws_mat4_translation(ws_vec3_make(0.0, -0.2, 0.0));
    ws_mat4 near_t = ws_mat4_translation(ws_vec3_make(0.0, 0.2, 0.0));
    WS_CHECK(ws_scene_add_object(&scene, &quad, far_t, far_mat) == WS_OK);
    WS_CHECK(ws_scene_add_object(&scene, &quad, near_t, near_mat) == WS_OK);
    scene.camera.eye = ws_vec3_make(0.0, 3.0, 0.0);
    scene.camera.target = ws_vec3_make(0.0, 0.0, 0.0);
    scene.camera.up = ws_vec3_make(0.0, 0.0, -1.0);

    ws_framebuffer fb;
    WS_CHECK(ws_framebuffer_create(&fb, 64u, 64u) == WS_OK);
    size_t count = (size_t)fb.width * fb.height;
    float *depth = malloc(count * sizeof(float));
    WS_CHECK(depth != NULL);
    WS_CHECK(ws_render_scene(&scene, &fb, depth, 16u, NULL) == WS_OK);

    /* Centre pixel should be the near (red) quad. */
    size_t centre = ((size_t)32 * fb.width + 32u) * 4u;
    WS_CHECK(fb.pixels[centre] > 150u && fb.pixels[centre + 2u] < 60u);
    free(depth);
    ws_framebuffer_free(&fb);
    return 1;
}

static int test_ppm_roundtrip(void)
{
    ws_framebuffer src;
    WS_CHECK(ws_framebuffer_create(&src, 5u, 3u) == WS_OK);
    for (uint32_t y = 0; y < src.height; ++y) {
        for (uint32_t x = 0; x < src.width; ++x) {
            ws_framebuffer_set(&src, x, y, (uint8_t)(x * 10u), (uint8_t)(y * 20u), 7u, 255u);
        }
    }
    const char *path = "ppm_roundtrip.ppm";
    WS_CHECK(ws_framebuffer_write_ppm(&src, path) == WS_OK);

    ws_framebuffer dst;
    WS_CHECK(ws_framebuffer_read_ppm(&dst, path) == WS_OK);
    WS_CHECK(dst.width == src.width && dst.height == src.height);
    WS_CHECK(memcmp(src.pixels, dst.pixels, (size_t)src.width * src.height * 4u) == 0);

    ws_framebuffer missing;
    WS_CHECK(ws_framebuffer_read_ppm(&missing, "no_such.ppm") == WS_ERR_NOT_FOUND);

    remove(path);
    ws_framebuffer_free(&src);
    ws_framebuffer_free(&dst);
    return 1;
}

static int test_image_metrics(void)
{
    ws_framebuffer a;
    ws_framebuffer b;
    WS_CHECK(ws_framebuffer_create(&a, 8u, 8u) == WS_OK);
    WS_CHECK(ws_framebuffer_create(&b, 8u, 8u) == WS_OK);
    ws_framebuffer_clear(&a, 10u, 20u, 30u, 255u);
    ws_framebuffer_clear(&b, 10u, 20u, 30u, 255u);

    ws_image_diff diff;
    WS_CHECK(ws_image_compare(&a, &b, &diff) == WS_OK);
    WS_CHECK(diff.mean_abs_error == 0.0);
    WS_CHECK(isinf(diff.psnr_db) && diff.psnr_db > 0.0);
    WS_CHECK(diff.ssim > 0.999);
    WS_CHECK(diff.differing_pixels == 0u);

    ws_framebuffer_clear(&b, 10u, 20u, 40u, 255u);
    WS_CHECK(ws_image_compare(&a, &b, &diff) == WS_OK);
    WS_CHECK(diff.differing_pixels == 64u);
    WS_CHECK(diff.max_abs_error == 10.0);

    double coverage = 0.0;
    WS_CHECK(ws_image_coverage(&a, a.pixels, 8, &coverage) == WS_OK);
    WS_CHECK(coverage == 0.0);

    ws_framebuffer_free(&a);
    ws_framebuffer_free(&b);
    return 1;
}

int main(void)
{
    static const ws_test_case cases[] = {
        {"math mat4 identity", test_math_mat4_identity},
        {"math mat4 multiply order", test_math_mat4_multiply_order},
        {"math vec3 ops", test_math_vec3_ops},
        {"math perspective mapping", test_math_perspective},
        {"mesh cube", test_mesh_cube},
        {"mesh sphere", test_mesh_sphere},
        {"asset roundtrip", test_asset_roundtrip},
        {"asset validation", test_asset_validation},
        {"asset rejects malformed file", test_asset_rejects_malformed_file},
        {"render produces content", test_render_produces_content},
        {"render tile invariance", test_render_tile_invariance},
        {"render determinism", test_render_determinism},
        {"render depth occlusion", test_render_depth_occlusion},
        {"ppm roundtrip", test_ppm_roundtrip},
        {"image metrics", test_image_metrics},
    };
    return ws_test_run("render", cases, (int)(sizeof(cases) / sizeof(cases[0])));
}
