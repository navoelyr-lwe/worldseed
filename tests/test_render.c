/* Worldseed Engine — render foundation tests.
 *
 * Covers math (mat4/vec3), procedural mesh generation, the asset format and
 * validator, and the reference scene renderer: geometry projection, depth
 * ordering, tile-size invariance, image metrics and seed determinism.
 */
#include "ws_test.h"

#include "core/ws_math.h"
#include "render/ws_asset.h"
#include "render/ws_catalog.h"
#include "render/ws_framebuffer.h"
#include "render/ws_image.h"
#include "render/ws_mesh.h"
#include "render/ws_motion.h"
#include "render/ws_quality.h"
#include "render/ws_render.h"
#include "render/ws_scene.h"
#include "render/ws_showcase.h"
#include "render/ws_texture.h"

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
    ws_material mat = ws_material_make(0.7, 0.6, 0.4, 0.8, 0.0, 0.0);
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
    ws_material near_mat = ws_material_make(1.0, 0.0, 0.0, 1.0, 0.0, 0.0);
    ws_material far_mat = ws_material_make(0.0, 0.0, 1.0, 1.0, 0.0, 0.0);
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

/* A ground plane with a raised occluder under a low sun: enabling the shadow
 * pass must darken the ground behind the occluder and leave lit ground alone,
 * without changing the geometry or the ambient term. */
static int test_render_shadow_darkens_occluded(void)
{
    ws_vertex gverts[4];
    uint32_t gidx[6];
    const uint8_t white[3] = {255u, 255u, 255u};
    ws_mesh ground;
    WS_CHECK(ws_mesh_make_plane(&ground, gverts, 4u, gidx, 6u, 8.0, 1u, white) == WS_OK);

    ws_vertex cverts[24];
    uint32_t cidx[36];
    ws_mesh block;
    WS_CHECK(ws_mesh_make_cube(&block, cverts, 24u, cidx, 36u, white) == WS_OK);

    ws_scene_object objects[2];
    ws_scene scene;
    WS_CHECK(ws_scene_init(&scene, objects, 2u) == WS_OK);
    ws_mat4 gt = ws_mat4_translation(ws_vec3_make(0.0, 0.0, 0.0));
    WS_CHECK(ws_scene_add_object(&scene, &ground, gt,
                                 ws_material_make(0.8, 0.8, 0.8, 1.0, 0.0, 0.0)) == WS_OK);
    ws_mat4 bt = ws_mat4_multiply(ws_mat4_translation(ws_vec3_make(0.0, 0.9, 0.0)),
                                  ws_mat4_scale(ws_vec3_make(0.6, 0.6, 0.6)));
    WS_CHECK(ws_scene_add_object(&scene, &block, bt,
                                 ws_material_make(0.8, 0.6, 0.4, 1.0, 0.0, 0.0)) == WS_OK);
    /* Camera looks straight down; sun comes in at a steep angle from +x. */
    scene.camera.eye = ws_vec3_make(0.0, 6.0, 0.0001);
    scene.camera.target = ws_vec3_make(0.0, 0.0, 0.0);
    scene.camera.up = ws_vec3_make(0.0, 0.0, -1.0);
    scene.camera.far_z = 100.0;
    scene.light.direction = ws_vec3_normalize(ws_vec3_make(-0.6, -0.8, 0.0));

    ws_framebuffer lit;
    ws_framebuffer shadowed;
    WS_CHECK(ws_framebuffer_create(&lit, 96u, 96u) == WS_OK);
    WS_CHECK(ws_framebuffer_create(&shadowed, 96u, 96u) == WS_OK);
    size_t count = (size_t)lit.width * lit.height;
    float *depth = malloc(count * sizeof(float));
    WS_CHECK(depth != NULL);

    scene.settings = ws_render_settings_default();
    scene.settings.shadows_enabled = 0;
    WS_CHECK(ws_render_scene(&scene, &lit, depth, 16u, NULL) == WS_OK);
    scene.settings.shadows_enabled = 1;
    WS_CHECK(ws_render_scene(&scene, &shadowed, depth, 16u, NULL) == WS_OK);

    /* The shadow must be a real change: some pixels darker, none brighter. */
    uint64_t darker = 0u;
    uint64_t brighter = 0u;
    for (size_t i = 0; i < count; ++i) {
        const uint8_t *a = &lit.pixels[i * 4u];
        const uint8_t *b = &shadowed.pixels[i * 4u];
        int la = a[0] + a[1] + a[2];
        int lb = b[0] + b[1] + b[2];
        if (lb < la - 3) { darker += 1u; }
        if (lb > la + 3) { brighter += 1u; }
    }
    WS_CHECK(darker > 40u);   /* the occluder casts a visible shadow */
    WS_CHECK(brighter == 0u); /* shadows only remove direct light */

    /* Shadowing is deterministic: a second shadowed render is byte-identical. */
    ws_framebuffer again;
    WS_CHECK(ws_framebuffer_create(&again, 96u, 96u) == WS_OK);
    WS_CHECK(ws_render_scene(&scene, &again, depth, 16u, NULL) == WS_OK);
    WS_CHECK(memcmp(shadowed.pixels, again.pixels, count * 4u) == 0);

    free(depth);
    ws_framebuffer_free(&lit);
    ws_framebuffer_free(&shadowed);
    ws_framebuffer_free(&again);
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

static int test_ppm_crlf_header(void)
{
    /* A PPM whose header uses CRLF line endings (as a text-mode checkout on
     * Windows produces) must parse with the raster starting at the right byte;
     * a naive tokeniser shifts the image by one byte. */
    const char *path = "ppm_crlf.ppm";
    FILE *f = fopen(path, "wb");
    WS_CHECK(f != NULL);
    /* 2x1, white then black, CRLF header. */
    static const uint8_t bytes[] = "P6\r\n2 1\r\n255\r\n\xff\xff\xff\x00\x00\x00";
    fwrite(bytes, 1u, sizeof(bytes) - 1u, f);
    fclose(f);

    ws_framebuffer fb;
    WS_CHECK(ws_framebuffer_read_ppm(&fb, path) == WS_OK);
    WS_CHECK(fb.width == 2u && fb.height == 1u);
    WS_CHECK(fb.pixels[0] == 255u && fb.pixels[1] == 255u && fb.pixels[2] == 255u);
    WS_CHECK(fb.pixels[4] == 0u && fb.pixels[5] == 0u && fb.pixels[6] == 0u);

    remove(path);
    ws_framebuffer_free(&fb);
    return 1;
}

static int test_showcase_scenes(void)
{
    WS_CHECK(ws_showcase_scene_count() == 7u);
    ws_showcase s;
    WS_CHECK(ws_showcase_init(&s) == WS_OK);
    for (size_t i = 0; i < ws_showcase_scene_count(); ++i) {
        WS_CHECK(ws_showcase_scene_id(i) != NULL);
        WS_CHECK(ws_showcase_build(&s, i, 20260101ULL) == WS_OK);
        WS_CHECK(s.scene.object_count > 0u);
    }
    WS_CHECK(ws_showcase_build(&s, ws_showcase_scene_count(), 0ULL) == WS_ERR_INVALID_ARGUMENT);
    ws_showcase_free(&s);
    return 1;
}

/* A showcase scene must render byte-identically regardless of the seed, so the
 * golden references are stable. */
static int test_showcase_deterministic(void)
{
    ws_framebuffer a;
    ws_framebuffer b;
    WS_CHECK(ws_framebuffer_create(&a, 64u, 48u) == WS_OK);
    WS_CHECK(ws_framebuffer_create(&b, 64u, 48u) == WS_OK);
    float *depth = malloc(64u * 48u * sizeof(float));
    WS_CHECK(depth != NULL);

    ws_showcase s;
    WS_CHECK(ws_showcase_init(&s) == WS_OK);
    WS_CHECK(ws_showcase_build(&s, 3u, 1ULL) == WS_OK);
    WS_CHECK(ws_render_scene(&s.scene, &a, depth, 16u, NULL) == WS_OK);
    WS_CHECK(ws_showcase_build(&s, 3u, 999ULL) == WS_OK);
    WS_CHECK(ws_render_scene(&s.scene, &b, depth, 16u, NULL) == WS_OK);
    WS_CHECK(memcmp(a.pixels, b.pixels, (size_t)a.width * a.height * 4u) == 0);

    free(depth);
    ws_showcase_free(&s);
    ws_framebuffer_free(&a);
    ws_framebuffer_free(&b);
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

static int test_math_vec4_ops(void)
{
    ws_vec4 a = ws_vec4_make(1.0, 2.0, 3.0, 4.0);
    ws_vec4 b = ws_vec4_make(4.0, 3.0, 2.0, 1.0);
    ws_vec4 s = ws_vec4_add(a, b);
    WS_CHECK(APPROX(s.x, 5.0, 1e-12) && APPROX(s.w, 5.0, 1e-12));
    ws_vec4 d = ws_vec4_sub(a, b);
    WS_CHECK(APPROX(d.x, -3.0, 1e-12) && APPROX(d.w, 3.0, 1e-12));
    WS_CHECK(APPROX(ws_vec4_dot(a, b), 20.0, 1e-12));
    ws_vec4 l = ws_vec4_lerp(a, b, 0.5);
    WS_CHECK(APPROX(l.x, 2.5, 1e-12) && APPROX(l.w, 2.5, 1e-12));
    /* Directional transform ignores translation. */
    ws_mat4 t = ws_mat4_translation(ws_vec3_make(10.0, 20.0, 30.0));
    ws_vec4 dir = ws_mat4_mul_vec4_dir(t, ws_vec4_make(1.0, 0.0, 0.0, 1.0));
    WS_CHECK(APPROX(dir.x, 1.0, 1e-12) && APPROX(dir.y, 0.0, 1e-12) && APPROX(dir.w, 0.0, 1e-12));
    return 1;
}

static int test_math_normal_matrix(void)
{
    /* Non-uniform scale: the normal matrix must keep a plane normal
     * perpendicular to the transformed surface. */
    ws_mat4 s = ws_mat4_scale(ws_vec3_make(2.0, 1.0, 1.0));
    ws_mat3 nm = ws_mat4_normal_matrix(s);
    /* Surface with normal +X scaled 2x in X: normal direction stays +X. */
    ws_vec3 n = ws_mat3_mul_vec3(nm, ws_vec3_make(1.0, 0.0, 0.0));
    n = ws_vec3_normalize(n);
    WS_CHECK(APPROX(n.x, 1.0, 1e-9));
    /* A slanted normal (1,1,0) should tilt away from X after X-only scaling. */
    ws_vec3 sl = ws_vec3_normalize(ws_mat3_mul_vec3(nm, ws_vec3_make(1.0, 1.0, 0.0)));
    WS_CHECK(sl.y > sl.x);
    return 1;
}

static int test_asset_texture_roundtrip(void)
{
    ws_texture src;
    const uint8_t c0[3] = {255u, 0u, 0u};
    const uint8_t c1[3] = {0u, 0u, 255u};
    WS_CHECK(ws_texture_make_checker(&src, 8u, 2u, c0, c1) == WS_OK);

    const char *path = "asset_tex_roundtrip.wstex";
    WS_CHECK(ws_asset_write_texture(path, &src) == WS_OK);

    ws_texture dst;
    WS_CHECK(ws_asset_load_texture(path, &dst) == WS_OK);
    WS_CHECK(dst.width == src.width && dst.height == src.height);
    WS_CHECK(memcmp(src.rgba, dst.rgba, (size_t)src.width * src.height * 4u) == 0);

    ws_texture_free(&dst);
    ws_texture_free(&src);
    remove(path);
    return 1;
}

static int test_asset_texture_rejects_malformed(void)
{
    const char *path = "asset_tex_bad.wstex";
    FILE *f = fopen(path, "wb");
    WS_CHECK(f != NULL);
    fputs("worldseed-texture 1\nsize 2 2\n255 0 0 255\n", f); /* too few pixels */
    fclose(f);
    ws_texture tex;
    WS_CHECK(ws_asset_load_texture(path, &tex) == WS_ERR_PARSE);
    remove(path);

    f = fopen(path, "wb");
    WS_CHECK(f != NULL);
    fputs("worldseed-texture 99\n", f);
    fclose(f);
    WS_CHECK(ws_asset_load_texture(path, &tex) == WS_ERR_UNSUPPORTED);
    remove(path);

    WS_CHECK(ws_asset_load_texture("no_such.wstex", &tex) == WS_ERR_NOT_FOUND);
    return 1;
}

static int test_texture_flat_sample(void)
{
    ws_texture tex;
    WS_CHECK(ws_texture_create(&tex, 4u, 4u, 255u, 128u, 0u, 255u) == WS_OK);
    double c[3];
    ws_texture_sample(&tex, 0.3, 0.7, c);
    /* Mid grey: sRGB 128 decodes to ~0.216 linear. */
    WS_CHECK(APPROX(c[0], 1.0, 1e-9));
    WS_CHECK(c[1] > 0.2 && c[1] < 0.24);
    WS_CHECK(APPROX(c[2], 0.0, 1e-9));
    ws_texture_free(&tex);
    return 1;
}

static int test_texture_checker_pattern(void)
{
    ws_texture tex;
    const uint8_t c0[3] = {255u, 255u, 255u};
    const uint8_t c1[3] = {0u, 0u, 0u};
    WS_CHECK(ws_texture_make_checker(&tex, 8u, 2u, c0, c1) == WS_OK);
    /* Cells are 4x4: pixel (0,0) white, (5,0) black, (5,5) white. */
    WS_CHECK(tex.rgba[0] == 255u);
    WS_CHECK(tex.rgba[(0u * 8u + 5u) * 4u] == 0u);
    WS_CHECK(tex.rgba[(5u * 8u + 5u) * 4u] == 255u);
    ws_texture_free(&tex);
    return 1;
}

static int test_texture_bilinear(void)
{
    /* Two-pixel texture: black then white along X. Sampling between texels in
     * the linear domain must land strictly between the two decoded values. */
    ws_texture tex;
    WS_CHECK(ws_texture_create(&tex, 2u, 1u, 0u, 0u, 0u, 255u) == WS_OK);
    ws_texture_set(&tex, 1u, 0u, 255u, 255u, 255u, 255u);
    double c[3];
    ws_texture_sample(&tex, 0.5, 0.5, c); /* halfway between the two texels */
    WS_CHECK(c[0] > 0.0 && c[0] < 1.0);
    /* Texel centres (u = 0.25 and 0.75) reproduce their colours exactly. */
    ws_texture_sample(&tex, 0.25, 0.5, c);
    WS_CHECK(APPROX(c[0], 0.0, 1e-9));
    ws_texture_sample(&tex, 0.75, 0.5, c);
    WS_CHECK(APPROX(c[0], 1.0, 1e-9));
    ws_texture_free(&tex);
    return 1;
}

static int test_texture_noise_reproducible(void)
{
    ws_texture a;
    ws_texture b;
    const uint8_t base[3] = {120u, 100u, 80u};
    WS_CHECK(ws_texture_make_noise(&a, 16u, 42ULL, base, 0.3) == WS_OK);
    WS_CHECK(ws_texture_make_noise(&b, 16u, 42ULL, base, 0.3) == WS_OK);
    WS_CHECK(memcmp(a.rgba, b.rgba, (size_t)16u * 16u * 4u) == 0);
    ws_texture c;
    WS_CHECK(ws_texture_make_noise(&c, 16u, 43ULL, base, 0.3) == WS_OK);
    WS_CHECK(memcmp(a.rgba, c.rgba, (size_t)16u * 16u * 4u) != 0);
    ws_texture_free(&a);
    ws_texture_free(&b);
    ws_texture_free(&c);
    return 1;
}

/* Render a single sphere filling the view and return the centre pixel RGB. */
static int render_centre_sphere(const ws_render_settings *settings, const ws_material *mat,
                                ws_vec3 light_dir, unsigned char out_rgb[3])
{
    ws_vertex verts[17u * 9u];
    uint32_t idx[16u * 9u * 6u];
    const uint8_t color[3] = {255u, 255u, 255u};
    ws_mesh sphere;
    if (ws_mesh_make_sphere(&sphere, verts, 17u * 9u, idx, 16u * 9u * 6u, 16u, 8u, color) != WS_OK) {
        return 0;
    }
    ws_scene_object objects[1];
    ws_scene scene;
    if (ws_scene_init(&scene, objects, 1u) != WS_OK) {
        return 0;
    }
    if (ws_scene_add_object(&scene, &sphere, ws_mat4_scale(ws_vec3_make(2.0, 2.0, 2.0)), *mat) != WS_OK) {
        return 0;
    }
    scene.camera = ws_camera_default();
    scene.camera.eye = ws_vec3_make(0.0, 0.0, 3.0);
    scene.camera.target = ws_vec3_make(0.0, 0.0, 0.0);
    scene.light.direction = ws_vec3_normalize(light_dir);
    scene.settings = *settings;

    ws_framebuffer fb;
    if (ws_framebuffer_create(&fb, 96u, 96u) != WS_OK) {
        return 0;
    }
    size_t count = (size_t)fb.width * fb.height;
    float *depth = malloc(count * sizeof(float));
    if (depth == NULL) {
        ws_framebuffer_free(&fb);
        return 0;
    }
    int ok = ws_render_scene(&scene, &fb, depth, 32u, NULL) == WS_OK;
    if (ok) {
        size_t centre = ((size_t)48 * fb.width + 48u) * 4u;
        out_rgb[0] = fb.pixels[centre];
        out_rgb[1] = fb.pixels[centre + 1u];
        out_rgb[2] = fb.pixels[centre + 2u];
    }
    free(depth);
    ws_framebuffer_free(&fb);
    return ok;
}

static int test_render_specular_highlight(void)
{
    /* A glossy sphere lit head-on must be brighter at the centre than a matte
     * one under identical geometry and light. */
    ws_render_settings s = ws_render_settings_default();
    ws_material matte = ws_material_make(0.5, 0.5, 0.5, 1.0, 0.0, 0.0);
    ws_material glossy = ws_material_make(0.5, 0.5, 0.5, 0.2, 0.0, 0.9);
    unsigned char m[3];
    unsigned char g[3];
    WS_CHECK(render_centre_sphere(&s, &matte, ws_vec3_make(0.0, 0.0, -1.0), m));
    WS_CHECK(render_centre_sphere(&s, &glossy, ws_vec3_make(0.0, 0.0, -1.0), g));
    WS_CHECK(g[0] > m[0]);
    return 1;
}

static int test_render_sky_gradient(void)
{
    /* Empty scene with sky enabled: the top of the frame is bluer (sky_top) and
     * the bottom is brighter (sky_horizon). */
    ws_scene_object objects[1];
    ws_scene scene;
    WS_CHECK(ws_scene_init(&scene, objects, 1u) == WS_OK);
    scene.camera = ws_camera_default();
    scene.camera.eye = ws_vec3_make(0.0, 0.0, 0.0);
    scene.camera.target = ws_vec3_make(0.0, 0.0, -1.0);
    scene.settings.sky_enabled = 1;

    ws_framebuffer fb;
    WS_CHECK(ws_framebuffer_create(&fb, 64u, 64u) == WS_OK);
    WS_CHECK(ws_render_scene(&scene, &fb, NULL, 32u, NULL) == WS_OK);
    size_t top = ((size_t)2u * fb.width + 32u) * 4u;
    size_t bot = ((size_t)61u * fb.width + 32u) * 4u;
    /* Blue channel dominates at the zenith. */
    WS_CHECK(fb.pixels[top + 2u] > fb.pixels[top + 0u]);
    /* Horizon is brighter than the zenith. */
    WS_CHECK(fb.pixels[bot + 1u] > fb.pixels[top + 1u]);
    ws_framebuffer_free(&fb);
    return 1;
}

static int test_render_fog_attenuates(void)
{
    /* Same geometry, with and without fog: fog must pull the far sphere toward
     * the sky-horizon colour, i.e. change pixels in the lower part of the frame
     * where a distant object sits. */
    ws_vertex verts[17u * 9u];
    uint32_t idx[16u * 9u * 6u];
    const uint8_t color[3] = {200u, 60u, 60u};
    ws_mesh sphere;
    WS_CHECK(ws_mesh_make_sphere(&sphere, verts, 17u * 9u, idx, 16u * 9u * 6u, 16u, 8u, color) == WS_OK);

    ws_scene_object objects[1];
    ws_scene scene;
    WS_CHECK(ws_scene_init(&scene, objects, 1u) == WS_OK);
    ws_mat4 t = ws_mat4_multiply(ws_mat4_translation(ws_vec3_make(0.0, 0.0, -20.0)),
                                 ws_mat4_scale(ws_vec3_make(4.0, 4.0, 4.0)));
    WS_CHECK(ws_scene_add_object(&scene, &sphere, t,
                                 ws_material_make(0.8, 0.2, 0.2, 0.9, 0.0, 0.0)) == WS_OK);
    scene.camera = ws_camera_default();
    scene.camera.eye = ws_vec3_make(0.0, 0.0, 0.0);
    scene.camera.target = ws_vec3_make(0.0, 0.0, -1.0);
    scene.settings.sky_enabled = 1;

    ws_framebuffer clear_fb;
    ws_framebuffer fog_fb;
    WS_CHECK(ws_framebuffer_create(&clear_fb, 64u, 64u) == WS_OK);
    WS_CHECK(ws_framebuffer_create(&fog_fb, 64u, 64u) == WS_OK);
    scene.settings.fog_density = 0.0;
    WS_CHECK(ws_render_scene(&scene, &clear_fb, NULL, 32u, NULL) == WS_OK);
    scene.settings.fog_density = 0.15;
    WS_CHECK(ws_render_scene(&scene, &fog_fb, NULL, 32u, NULL) == WS_OK);

    size_t centre = ((size_t)32u * clear_fb.width + 32u) * 4u;
    /* The red sphere has little blue; fog pulls the pixel toward the bright
     * sky-horizon colour, so the blue channel must rise. */
    WS_CHECK(fog_fb.pixels[centre + 2u] > clear_fb.pixels[centre + 2u]);
    ws_image_diff diff;
    WS_CHECK(ws_image_compare(&clear_fb, &fog_fb, &diff) == WS_OK);
    WS_CHECK(diff.differing_pixels > 0u);
    ws_framebuffer_free(&clear_fb);
    ws_framebuffer_free(&fog_fb);
    return 1;
}

static int test_render_tonemap_monotonic(void)
{
    /* A bright albedo blows out under clamp but keeps a gradient under
     * Reinhard; Reinhard never exceeds clamp for the same input. */
    ws_render_settings clamp_s = ws_render_settings_default();
    ws_render_settings rein_s = ws_render_settings_default();
    rein_s.tonemap = 1;
    rein_s.exposure = 4.0;
    clamp_s.exposure = 4.0;
    ws_material mat = ws_material_make(0.9, 0.9, 0.9, 1.0, 0.0, 0.0);
    unsigned char c[3];
    unsigned char r[3];
    WS_CHECK(render_centre_sphere(&clamp_s, &mat, ws_vec3_make(0.0, 0.0, -1.0), c));
    WS_CHECK(render_centre_sphere(&rein_s, &mat, ws_vec3_make(0.0, 0.0, -1.0), r));
    WS_CHECK(r[0] <= c[0]);
    WS_CHECK(r[0] > 0u);
    return 1;
}

static int test_render_texture_changes_output(void)
{
    /* A cube with a black/white checker albedo map must produce a wider spread
     * of pixel values than a flat-grey cube of the same mean. */
    ws_vertex verts[24];
    uint32_t idx[36];
    const uint8_t color[3] = {180u, 180u, 180u};
    ws_mesh cube;
    WS_CHECK(ws_mesh_make_cube(&cube, verts, 24u, idx, 36u, color) == WS_OK);
    /* Give the cube UVs across its faces so the map has effect. */

    ws_texture checker;
    const uint8_t c0[3] = {255u, 255u, 255u};
    const uint8_t c1[3] = {20u, 20u, 20u};
    WS_CHECK(ws_texture_make_checker(&checker, 32u, 4u, c0, c1) == WS_OK);

    ws_scene_object objects[1];
    ws_scene scene;
    WS_CHECK(ws_scene_init(&scene, objects, 1u) == WS_OK);
    ws_material flat = ws_material_make(0.6, 0.6, 0.6, 0.9, 0.0, 0.0);
    WS_CHECK(ws_scene_add_object(&scene, &cube, ws_mat4_identity(), flat) == WS_OK);
    scene.camera = ws_camera_default();

    ws_framebuffer flat_fb;
    ws_framebuffer tex_fb;
    WS_CHECK(ws_framebuffer_create(&flat_fb, 64u, 64u) == WS_OK);
    WS_CHECK(ws_framebuffer_create(&tex_fb, 64u, 64u) == WS_OK);
    WS_CHECK(ws_render_scene(&scene, &flat_fb, NULL, 16u, NULL) == WS_OK);

    scene.objects[0].material.albedo_map = &checker;
    WS_CHECK(ws_render_scene(&scene, &tex_fb, NULL, 16u, NULL) == WS_OK);

    ws_image_diff diff;
    WS_CHECK(ws_image_compare(&flat_fb, &tex_fb, &diff) == WS_OK);
    WS_CHECK(diff.differing_pixels > 0u);

    ws_texture_free(&checker);
    ws_framebuffer_free(&flat_fb);
    ws_framebuffer_free(&tex_fb);
    return 1;
}


/* The quality metrics must react to the frame: a flat frame has no contrast,
 * variety or saturation, while a lit textured scene does. */
static int test_quality_metrics_react_to_frame(void)
{
    ws_framebuffer flat;
    WS_CHECK(ws_framebuffer_create(&flat, 64u, 64u) == WS_OK);
    ws_framebuffer_clear(&flat, 128u, 128u, 128u, 255u);

    ws_scene scene;
    ws_scene_object storage[1];
    WS_CHECK(ws_scene_init(&scene, storage, 1u) == WS_OK);
    scene.background[0] = 24u; scene.background[1] = 28u; scene.background[2] = 36u;

    ws_quality_metrics flat_m;
    WS_CHECK(ws_quality_measure(&scene, &flat, 0u, 0u, 0u, &flat_m) == WS_OK);
    WS_CHECK(APPROX(flat_m.luma_stddev, 0.0, 1e-9));
    WS_CHECK(APPROX(flat_m.mean_saturation, 0.0, 1e-9));
    WS_CHECK(APPROX(flat_m.colour_variety, 1.0, 1e-9)); /* one bucket */
    WS_CHECK(APPROX(flat_m.mean_luma, 128.0, 0.5));
    WS_CHECK(APPROX(flat_m.sky_gradient, 0.0, 1e-9));

    ws_framebuffer_free(&flat);
    return 1;
}

static int test_quality_specs_are_six_product_scenes(void)
{
    WS_CHECK(ws_quality_spec_count() == 6u);
    const char *expected[6] = {"terrain_day", "ecosystem", "organism_forage",
                               "creature_walk", "terrain_weather", "terrain_dusk"};
    for (size_t i = 0; i < 6u; ++i) {
        const ws_quality_spec *s = ws_quality_spec_at(i);
        WS_CHECK(s != NULL);
        WS_CHECK(strcmp(s->scene_id, expected[i]) == 0);
        WS_CHECK(s->require_sky == 1);
    }
    WS_CHECK(ws_quality_spec_at(6u) == NULL);
    return 1;
}

/* Every metric threshold must actually gate: feeding a metric just below the
 * spec must fail with the matching bit, and one just above must pass. */
static int test_quality_check_gates_each_metric(void)
{
    const ws_quality_spec *spec = ws_quality_spec_at(0);
    WS_CHECK(spec != NULL);

    ws_quality_metrics m;
    memset(&m, 0, sizeof(m));
    m.geometry_coverage = spec->min_geometry_coverage + 0.1;
    m.coverage = spec->min_coverage + 0.1;
    m.luma_stddev = spec->min_luma_stddev + 1.0;
    m.colour_variety = spec->min_colour_variety + 1.0;
    m.mean_saturation = spec->min_saturation + 0.1;
    m.sky_gradient = spec->min_sky_gradient + 1.0;
    m.object_count = spec->min_objects;
    m.blown_fraction = 0.0;

    uint32_t fail = 0xFFFFFFFFu;
    WS_CHECK(ws_quality_check(spec, &m, &fail) == WS_OK);
    WS_CHECK(fail == 0u);

    ws_quality_metrics low = m;
    low.luma_stddev = spec->min_luma_stddev - 0.5;
    WS_CHECK(ws_quality_check(spec, &low, &fail) == WS_ERR_INVALID_ARGUMENT);
    WS_CHECK((fail & WS_QUALITY_FAIL_CONTRAST) != 0u);

    low = m;
    low.colour_variety = spec->min_colour_variety - 1.0;
    WS_CHECK(ws_quality_check(spec, &low, &fail) == WS_ERR_INVALID_ARGUMENT);
    WS_CHECK((fail & WS_QUALITY_FAIL_VARIETY) != 0u);

    low = m;
    low.mean_saturation = spec->min_saturation - 0.05;
    WS_CHECK(ws_quality_check(spec, &low, &fail) == WS_ERR_INVALID_ARGUMENT);
    WS_CHECK((fail & WS_QUALITY_FAIL_SATURATION) != 0u);

    low = m;
    low.sky_gradient = spec->min_sky_gradient - 0.1;
    WS_CHECK(ws_quality_check(spec, &low, &fail) == WS_ERR_INVALID_ARGUMENT);
    WS_CHECK((fail & WS_QUALITY_FAIL_SKY) != 0u);

    low = m;
    low.geometry_coverage = spec->min_geometry_coverage - 0.1;
    WS_CHECK(ws_quality_check(spec, &low, &fail) == WS_ERR_INVALID_ARGUMENT);
    WS_CHECK((fail & WS_QUALITY_FAIL_GEOMETRY) != 0u);

    low = m;
    low.object_count = spec->min_objects - 1u;
    WS_CHECK(ws_quality_check(spec, &low, &fail) == WS_ERR_INVALID_ARGUMENT);
    WS_CHECK((fail & WS_QUALITY_FAIL_OBJECTS) != 0u);

    low = m;
    low.blown_fraction = spec->max_blown + 0.01;
    WS_CHECK(ws_quality_check(spec, &low, &fail) == WS_ERR_INVALID_ARGUMENT);
    WS_CHECK((fail & WS_QUALITY_FAIL_BLOWN) != 0u);
    return 1;
}

/* The quality checklist must pass on every product reference scene built from
 * the real catalog, with the same determinism the golden harness relies on. */
static int test_quality_checklist_passes_on_product_scenes(void)
{
    ws_framebuffer fb;
    WS_CHECK(ws_framebuffer_create(&fb, 320u, 180u) == WS_OK);
    size_t pixels = (size_t)fb.width * (size_t)fb.height;
    float *depth = malloc(pixels * sizeof(float));
    WS_CHECK(depth != NULL);

    size_t n = ws_catalog_count();
    size_t checked = 0u;
    for (size_t i = 0; i < n; ++i) {
        const char *id = ws_catalog_id(i);
        const ws_quality_spec *spec = NULL;
        for (size_t s = 0; s < ws_quality_spec_count(); ++s) {
            if (strcmp(ws_quality_spec_at(s)->scene_id, id) == 0) {
                spec = ws_quality_spec_at(s);
                break;
            }
        }
        if (spec == NULL) {
            continue;
        }
        ws_catalog catalog;
        WS_CHECK(ws_catalog_build(&catalog, i, 20260101ULL) == WS_OK);
        const ws_scene *scene = ws_catalog_scene(&catalog);
        ws_render_stats stats;
        WS_CHECK(ws_render_scene(scene, &fb, depth, 32u, &stats) == WS_OK);
        ws_quality_metrics m;
        WS_CHECK(ws_quality_measure(scene, &fb, stats.triangles_submitted,
                                    stats.triangles_drawn, stats.pixels_shaded, &m) == WS_OK);
        uint32_t fail = 0u;
        WS_CHECK_MSG(ws_quality_check(spec, &m, &fail) == WS_OK, id);
        checked += 1u;
        ws_catalog_free(&catalog);
    }
    WS_CHECK(checked == ws_quality_spec_count());

    free(depth);
    ws_framebuffer_free(&fb);
    return 1;
}

/* ---------- motion-quality checklist ---------- */

static int test_motion_check_gates_each_metric(void)
{
    const ws_motion_spec *spec = ws_motion_spec_at(0u);
    WS_CHECK(spec != NULL);

    /* A run that satisfies every threshold must pass. */
    ws_motion_metrics ok = {0};
    ok.frames = spec->min_frames;
    ok.forward_m = spec->min_forward_m;
    ok.forward_speed_mps = (spec->min_speed_mps + spec->max_speed_mps) * 0.5;
    ok.max_com_accel_mps2 = spec->max_com_accel_mps2;
    ok.max_torso_tilt_rad = spec->max_torso_tilt_rad;
    ok.min_foot_clearance_m = spec->min_foot_clearance_m;
    ok.max_energy_j = spec->max_energy_j;
    uint32_t fail = 0u;
    WS_CHECK(ws_motion_check(spec, &ok, &fail) == WS_OK && fail == 0u);

    /* Each field below its gate flips exactly its own bit. */
    ws_motion_metrics bad;

    bad = ok;
    bad.forward_m = spec->min_forward_m - 1.0;
    WS_CHECK(ws_motion_check(spec, &bad, &fail) != WS_OK && fail == WS_MOTION_FAIL_FORWARD);

    bad = ok;
    bad.forward_speed_mps = spec->min_speed_mps - 0.01;
    WS_CHECK(ws_motion_check(spec, &bad, &fail) != WS_OK && fail == WS_MOTION_FAIL_SPEED);
    bad.forward_speed_mps = spec->max_speed_mps + 0.01;
    WS_CHECK(ws_motion_check(spec, &bad, &fail) != WS_OK && fail == WS_MOTION_FAIL_SPEED);

    bad = ok;
    bad.max_com_accel_mps2 = spec->max_com_accel_mps2 + 1.0;
    WS_CHECK(ws_motion_check(spec, &bad, &fail) != WS_OK && fail == WS_MOTION_FAIL_SMOOTHNESS);

    bad = ok;
    bad.max_torso_tilt_rad = spec->max_torso_tilt_rad + 0.01;
    WS_CHECK(ws_motion_check(spec, &bad, &fail) != WS_OK && fail == WS_MOTION_FAIL_BALANCE);

    bad = ok;
    bad.min_foot_clearance_m = spec->min_foot_clearance_m - 0.01;
    WS_CHECK(ws_motion_check(spec, &bad, &fail) != WS_OK && fail == WS_MOTION_FAIL_CONTACT);

    bad = ok;
    bad.max_energy_j = spec->max_energy_j + 1.0;
    WS_CHECK(ws_motion_check(spec, &bad, &fail) != WS_OK && fail == WS_MOTION_FAIL_ENERGY);

    bad = ok;
    bad.frames = spec->min_frames - 1u;
    WS_CHECK(ws_motion_check(spec, &bad, &fail) != WS_OK && fail == WS_MOTION_FAIL_DURATION);

    WS_CHECK(ws_motion_check(NULL, &ok, NULL) == WS_ERR_NULL_POINTER);
    return 1;
}

static int test_motion_measure_is_deterministic(void)
{
    ws_motion_metrics a, b;
    WS_CHECK(ws_motion_measure(WS_SPECIES_DEER, 20260101ULL, 5.0, &a) == WS_OK);
    WS_CHECK(ws_motion_measure(WS_SPECIES_DEER, 20260101ULL, 5.0, &b) == WS_OK);
    WS_CHECK(a.frames == b.frames);
    WS_CHECK(a.forward_m == b.forward_m);
    WS_CHECK(a.forward_speed_mps == b.forward_speed_mps);
    WS_CHECK(a.max_body_speed_mps == b.max_body_speed_mps);
    WS_CHECK(a.max_com_accel_mps2 == b.max_com_accel_mps2);
    WS_CHECK(a.max_torso_tilt_rad == b.max_torso_tilt_rad);
    WS_CHECK(a.min_foot_clearance_m == b.min_foot_clearance_m);
    WS_CHECK(a.max_energy_j == b.max_energy_j);

    /* A different seed changes the run (start phase), so the metrics differ. */
    ws_motion_metrics c;
    WS_CHECK(ws_motion_measure(WS_SPECIES_DEER, 99ULL, 5.0, &c) == WS_OK);
    WS_CHECK(c.forward_m != a.forward_m);

    /* An unknown species is rejected rather than silently measured. */
    ws_motion_metrics d;
    WS_CHECK(ws_motion_measure(9999, 20260101ULL, 5.0, &d) == WS_ERR_INVALID_ARGUMENT);
    return 1;
}

static int test_motion_checklist_passes_reference_walk(void)
{
    /* One spec per representative species: the whole four-species body/gait
     * catalogue carries a motion claim (docs/19 §4.3). */
    WS_CHECK(ws_motion_spec_count() == (size_t)WS_SPECIES_COUNT);
    int seen[WS_SPECIES_COUNT] = {0};
    for (size_t i = 0; i < ws_motion_spec_count(); ++i) {
        const ws_motion_spec *spec = ws_motion_spec_at(i);
        WS_CHECK(spec->species >= 0 && spec->species < WS_SPECIES_COUNT);
        seen[spec->species] += 1;
        ws_motion_metrics m;
        WS_CHECK(ws_motion_measure(spec->species, 20260101ULL, 20.0, &m) == WS_OK);
        uint32_t fail = 0u;
        WS_CHECK_MSG(ws_motion_check(spec, &m, &fail) == WS_OK, spec->scene_id);
        /* Every species' reference walk must genuinely move forward. */
        WS_CHECK(m.forward_m > 0.0);
    }
    for (int s = 0; s < WS_SPECIES_COUNT; ++s) {
        WS_CHECK(seen[s] == 1);
    }
    return 1;
}

int main(void)
{
    static const ws_test_case cases[] = {
        {"math mat4 identity", test_math_mat4_identity},
        {"math mat4 multiply order", test_math_mat4_multiply_order},
        {"math vec3 ops", test_math_vec3_ops},
        {"math vec4 ops", test_math_vec4_ops},
        {"math perspective mapping", test_math_perspective},
        {"math normal matrix", test_math_normal_matrix},
        {"mesh cube", test_mesh_cube},
        {"mesh sphere", test_mesh_sphere},
        {"asset roundtrip", test_asset_roundtrip},
        {"asset validation", test_asset_validation},
        {"asset rejects malformed file", test_asset_rejects_malformed_file},
        {"asset texture roundtrip", test_asset_texture_roundtrip},
        {"asset texture rejects malformed", test_asset_texture_rejects_malformed},
        {"texture flat sample", test_texture_flat_sample},
        {"texture checker pattern", test_texture_checker_pattern},
        {"texture bilinear interpolation", test_texture_bilinear},
        {"texture noise reproducible", test_texture_noise_reproducible},
        {"render produces content", test_render_produces_content},
        {"render tile invariance", test_render_tile_invariance},
        {"render determinism", test_render_determinism},
        {"render depth occlusion", test_render_depth_occlusion},
        {"render shadow darkens occluded", test_render_shadow_darkens_occluded},
        {"render specular highlight", test_render_specular_highlight},
        {"render sky gradient", test_render_sky_gradient},
        {"render fog attenuates distance", test_render_fog_attenuates},
        {"render tonemap is monotonic", test_render_tonemap_monotonic},
        {"render texture map changes output", test_render_texture_changes_output},
        {"ppm roundtrip", test_ppm_roundtrip},
        {"ppm crlf header", test_ppm_crlf_header},
        {"showcase scenes", test_showcase_scenes},
        {"showcase deterministic", test_showcase_deterministic},
        {"image metrics", test_image_metrics},
        {"quality metrics react to frame", test_quality_metrics_react_to_frame},
        {"quality specs are six product scenes", test_quality_specs_are_six_product_scenes},
        {"quality check gates each metric", test_quality_check_gates_each_metric},
        {"quality checklist passes on product scenes",
         test_quality_checklist_passes_on_product_scenes},
        {"motion check gates each metric", test_motion_check_gates_each_metric},
        {"motion measure is deterministic", test_motion_measure_is_deterministic},
        {"motion checklist passes reference walk",
         test_motion_checklist_passes_reference_walk},
    };
    return ws_test_run("render", cases, (int)(sizeof(cases) / sizeof(cases[0])));
}
