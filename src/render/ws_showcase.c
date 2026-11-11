/* Worldseed Engine — showcase scenes and their original assets. */
#include "ws_showcase.h"

#include <string.h>

static ws_result showcase_assets(ws_showcase *s)
{
    const uint8_t cube_color[3] = {180u, 150u, 90u};
    const uint8_t plane_color[3] = {70u, 110u, 60u};
    const uint8_t sphere_color[3] = {120u, 90u, 70u};
    ws_result r = ws_mesh_make_cube(&s->cube, s->cube_verts, 24u, s->cube_idx, 36u, cube_color);
    if (r != WS_OK) {
        return r;
    }
    r = ws_mesh_make_plane(&s->plane, s->plane_verts, 4u, s->plane_idx, 6u, 6.0, 1u, plane_color);
    if (r != WS_OK) {
        return r;
    }
    r = ws_mesh_make_sphere(&s->sphere, s->sphere_verts, 17u * 9u, s->sphere_idx,
                            16u * 9u * 6u, 16u, 8u, sphere_color);
    if (r != WS_OK) {
        return r;
    }
    const uint8_t c0[3] = {200u, 200u, 200u};
    const uint8_t c1[3] = {60u, 60u, 60u};
    r = ws_texture_make_checker(&s->checker, 64u, 8u, c0, c1);
    if (r != WS_OK) {
        return r;
    }
    const uint8_t base[3] = {150u, 120u, 90u};
    return ws_texture_make_noise(&s->noise, 64u, 1234ULL, base, 0.25);
}

ws_result ws_showcase_init(ws_showcase *showcase)
{
    if (showcase == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    memset(showcase, 0, sizeof(*showcase));
    ws_result r = showcase_assets(showcase);
    if (r != WS_OK) {
        return r;
    }
    return ws_scene_init(&showcase->scene, showcase->objects, WS_SHOWCASE_MAX_OBJECTS);
}

void ws_showcase_free(ws_showcase *showcase)
{
    if (showcase == NULL) {
        return;
    }
    ws_texture_free(&showcase->checker);
    ws_texture_free(&showcase->noise);
}

static void scene_ground(ws_showcase *s)
{
    ws_mat4 t = ws_mat4_translation(ws_vec3_make(0.0, -0.5, 0.0));
    (void)ws_scene_add_object(&s->scene, &s->plane, t,
                              ws_material_make(0.30, 0.42, 0.24, 0.9, 0.0, 0.0));
}

static ws_result scene_orbit(ws_showcase *s)
{
    scene_ground(s);
    ws_mat4 t = ws_mat4_multiply(ws_mat4_translation(ws_vec3_make(0.0, 0.0, 0.0)),
                                 ws_mat4_rotation_y(ws_deg_to_rad(30.0)));
    ws_result r = ws_scene_add_object(&s->scene, &s->cube, t,
                                      ws_material_make(0.72, 0.58, 0.36, 0.7, 0.0, 0.15));
    if (r != WS_OK) {
        return r;
    }
    ws_mat4 sc = ws_mat4_multiply(ws_mat4_translation(ws_vec3_make(1.4, 0.0, 0.6)),
                                  ws_mat4_scale(ws_vec3_make(0.8, 0.8, 0.8)));
    r = ws_scene_add_object(&s->scene, &s->sphere, sc,
                            ws_material_make(0.55, 0.42, 0.32, 0.5, 0.0, 0.3));
    if (r != WS_OK) {
        return r;
    }
    s->scene.camera = ws_camera_default();
    s->scene.camera.eye = ws_vec3_make(2.6, 1.8, 3.2);
    s->scene.camera.target = ws_vec3_make(0.3, 0.1, 0.0);
    return WS_OK;
}

static ws_result scene_lowlight(ws_showcase *s)
{
    scene_ground(s);
    ws_mat4 t = ws_mat4_multiply(ws_mat4_translation(ws_vec3_make(-0.3, 0.1, 0.0)),
                                 ws_mat4_rotation_y(ws_deg_to_rad(-20.0)));
    ws_result r = ws_scene_add_object(&s->scene, &s->cube, t,
                                      ws_material_make(0.60, 0.50, 0.34, 0.6, 0.0, 0.1));
    if (r != WS_OK) {
        return r;
    }
    s->scene.camera = ws_camera_default();
    s->scene.camera.eye = ws_vec3_make(-2.4, 1.4, 2.8);
    s->scene.camera.target = ws_vec3_make(0.0, 0.0, 0.0);
    s->scene.light = ws_light_default();
    s->scene.light.direction = ws_vec3_normalize(ws_vec3_make(0.5, -0.7, 0.4));
    s->scene.light.intensity = 0.45;
    s->scene.light.ambient = 18u;
    s->scene.light.ambient_intensity = 0.08;
    s->scene.background[0] = 12u;
    s->scene.background[1] = 14u;
    s->scene.background[2] = 22u;
    return WS_OK;
}

static ws_result scene_spheres(ws_showcase *s)
{
    scene_ground(s);
    for (int i = 0; i < 3; ++i) {
        ws_mat4 t = ws_mat4_multiply(
            ws_mat4_translation(ws_vec3_make(-1.2 + 1.2 * (double)i, 0.0, -0.4)),
            ws_mat4_scale(ws_vec3_make(0.9, 0.9, 0.9)));
        ws_material m = ws_material_make(0.5 + 0.15 * (double)i, 0.4, 0.35, 0.4, 0.0, 0.2);
        ws_result r = ws_scene_add_object(&s->scene, &s->sphere, t, m);
        if (r != WS_OK) {
            return r;
        }
    }
    s->scene.camera = ws_camera_default();
    s->scene.camera.eye = ws_vec3_make(0.0, 1.6, 4.2);
    s->scene.camera.target = ws_vec3_make(0.0, 0.0, 0.0);
    return WS_OK;
}

/* A row of spheres sweeping roughness and metallic: a material-response scene. */
static ws_result scene_materials(ws_showcase *s)
{
    scene_ground(s);
    for (int i = 0; i < 5; ++i) {
        double rough = 0.1 + 0.2 * (double)i;
        double metal = (i % 2 == 0) ? 0.0 : 0.9;
        ws_mat4 t = ws_mat4_multiply(
            ws_mat4_translation(ws_vec3_make(-1.6 + 0.8 * (double)i, 0.0, 0.0)),
            ws_mat4_scale(ws_vec3_make(0.7, 0.7, 0.7)));
        ws_material m = ws_material_make(0.8, 0.6, 0.3, rough, metal, 0.8);
        ws_result r = ws_scene_add_object(&s->scene, &s->sphere, t, m);
        if (r != WS_OK) {
            return r;
        }
    }
    s->scene.camera = ws_camera_default();
    s->scene.camera.eye = ws_vec3_make(0.0, 1.4, 5.2);
    s->scene.camera.target = ws_vec3_make(0.0, 0.0, 0.0);
    s->scene.light.direction = ws_vec3_normalize(ws_vec3_make(-0.5, -1.0, -0.6));
    return WS_OK;
}

/* Textured cube on a textured ground: exercises the texture sampler path. */
static ws_result scene_textured(ws_showcase *s)
{
    ws_material ground = ws_material_make(0.5, 0.5, 0.5, 0.95, 0.0, 0.0);
    ground.albedo_map = &s->checker;
    ws_mat4 gt = ws_mat4_translation(ws_vec3_make(0.0, -0.5, 0.0));
    ws_result r = ws_scene_add_object(&s->scene, &s->plane, gt, ground);
    if (r != WS_OK) {
        return r;
    }
    ws_material cube = ws_material_make(0.7, 0.7, 0.7, 0.7, 0.0, 0.2);
    cube.albedo_map = &s->noise;
    ws_mat4 t = ws_mat4_multiply(ws_mat4_translation(ws_vec3_make(0.0, 0.0, 0.0)),
                                 ws_mat4_rotation_y(ws_deg_to_rad(35.0)));
    r = ws_scene_add_object(&s->scene, &s->cube, t, cube);
    if (r != WS_OK) {
        return r;
    }
    s->scene.camera = ws_camera_default();
    s->scene.camera.eye = ws_vec3_make(2.2, 1.6, 2.8);
    s->scene.camera.target = ws_vec3_make(0.0, 0.0, 0.0);
    return WS_OK;
}

/* Daylight sky, hemisphere ambient, distance fog and a filmic tonemap. */
static ws_result scene_sky(ws_showcase *s)
{
    for (int i = 0; i < 4; ++i) {
        ws_mat4 t = ws_mat4_multiply(
            ws_mat4_translation(ws_vec3_make(-3.0 + 2.0 * (double)i, -0.3, -6.0 - 2.0 * (double)i)),
            ws_mat4_scale(ws_vec3_make(0.8, 0.8, 0.8)));
        ws_material m = ws_material_make(0.35, 0.5, 0.25, 0.8, 0.0, 0.05);
        ws_result r = ws_scene_add_object(&s->scene, &s->sphere, t, m);
        if (r != WS_OK) {
            return r;
        }
    }
    ws_mat4 gt = ws_mat4_translation(ws_vec3_make(0.0, -0.9, 0.0));
    ws_result r = ws_scene_add_object(&s->scene, &s->plane, gt,
                                      ws_material_make(0.28, 0.36, 0.22, 0.95, 0.0, 0.0));
    if (r != WS_OK) {
        return r;
    }
    s->scene.camera = ws_camera_default();
    s->scene.camera.eye = ws_vec3_make(0.0, 1.2, 6.0);
    s->scene.camera.target = ws_vec3_make(0.0, 0.4, -6.0);
    s->scene.camera.far_z = 200.0;
    s->scene.settings = ws_render_settings_default();
    s->scene.settings.sky_enabled = 1;
    s->scene.settings.tonemap = 1;
    s->scene.settings.exposure = 1.2;
    s->scene.settings.fog_density = 0.05;
    return WS_OK;
}

/* Directional shadows: a raised block and a sphere cast onto the ground from a
 * low sun. The scene opts into the shadow pass; every other scene leaves it off
 * so their golden frames are unaffected. */
static ws_result scene_shadows(ws_showcase *s)
{
    scene_ground(s);
    ws_mat4 box = ws_mat4_multiply(ws_mat4_translation(ws_vec3_make(-0.8, 0.4, -0.3)),
                                   ws_mat4_scale(ws_vec3_make(1.1, 1.1, 1.1)));
    ws_result r = ws_scene_add_object(&s->scene, &s->cube, box,
                                      ws_material_make(0.70, 0.55, 0.34, 0.7, 0.0, 0.1));
    if (r != WS_OK) {
        return r;
    }
    ws_mat4 ball = ws_mat4_multiply(ws_mat4_translation(ws_vec3_make(0.9, 0.6, 0.2)),
                                    ws_mat4_scale(ws_vec3_make(1.2, 1.2, 1.2)));
    r = ws_scene_add_object(&s->scene, &s->sphere, ball,
                            ws_material_make(0.55, 0.42, 0.32, 0.5, 0.0, 0.3));
    if (r != WS_OK) {
        return r;
    }
    s->scene.camera = ws_camera_default();
    s->scene.camera.eye = ws_vec3_make(3.2, 2.6, 5.0);
    s->scene.camera.target = ws_vec3_make(0.0, 0.2, 0.0);
    s->scene.light = ws_light_default();
    s->scene.light.direction = ws_vec3_normalize(ws_vec3_make(-0.55, -0.75, -0.35));
    s->scene.settings = ws_render_settings_default();
    s->scene.settings.shadows_enabled = 1;
    /* Wider penumbra so the product shadow reference shows a soft contact
     * shadow, not a hard PCF edge (P12 "wider penumbra" slice). The value is
     * the filter radius in shadow-map texels. */
    s->scene.settings.shadow_softness = 3.0;
    return WS_OK;
}

typedef ws_result (*showcase_scene_fn)(ws_showcase *);

typedef struct showcase_scene_def {
    const char *id;
    showcase_scene_fn build;
} showcase_scene_def;

static const showcase_scene_def SHOWCASE_SCENES[] = {
    {"orbit", scene_orbit},
    {"lowlight", scene_lowlight},
    {"spheres", scene_spheres},
    {"materials", scene_materials},
    {"textured", scene_textured},
    {"sky", scene_sky},
    {"shadows", scene_shadows},
};

size_t ws_showcase_scene_count(void)
{
    return sizeof(SHOWCASE_SCENES) / sizeof(SHOWCASE_SCENES[0]);
}

const char *ws_showcase_scene_id(size_t index)
{
    if (index >= ws_showcase_scene_count()) {
        return NULL;
    }
    return SHOWCASE_SCENES[index].id;
}

ws_result ws_showcase_build(ws_showcase *showcase, size_t index, uint64_t seed)
{
    (void)seed;
    if (showcase == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (index >= ws_showcase_scene_count()) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    ws_result r = ws_scene_init(&showcase->scene, showcase->objects, WS_SHOWCASE_MAX_OBJECTS);
    if (r != WS_OK) {
        return r;
    }
    return SHOWCASE_SCENES[index].build(showcase);
}
