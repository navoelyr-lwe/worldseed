/* Worldseed Engine — render scene contract. */
#include "ws_scene.h"

ws_result ws_scene_init(ws_scene *scene, ws_scene_object *storage, uint32_t capacity)
{
    if (scene == NULL || storage == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (capacity == 0u) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    scene->objects = storage;
    scene->object_count = 0u;
    scene->object_capacity = capacity;
    scene->camera = ws_camera_default();
    scene->light = ws_light_default();
    scene->settings = ws_render_settings_default();
    scene->background[0] = 24u;
    scene->background[1] = 28u;
    scene->background[2] = 36u;
    return WS_OK;
}

ws_result ws_scene_add_object(ws_scene *scene, const ws_mesh *mesh,
                              ws_mat4 transform, ws_material material)
{
    if (scene == NULL || mesh == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (scene->object_count >= scene->object_capacity) {
        return WS_ERR_CAPACITY;
    }
    ws_scene_object *obj = &scene->objects[scene->object_count];
    obj->mesh = mesh;
    obj->transform = transform;
    obj->material = material;
    scene->object_count += 1u;
    return WS_OK;
}

ws_camera ws_camera_default(void)
{
    ws_camera cam;
    cam.eye = ws_vec3_make(2.0, 1.6, 2.6);
    cam.target = ws_vec3_make(0.0, 0.4, 0.0);
    cam.up = ws_vec3_make(0.0, 1.0, 0.0);
    cam.fov_y_radians = ws_deg_to_rad(50.0);
    cam.near_z = 0.05;
    cam.far_z = 500.0;
    return cam;
}

ws_material ws_material_make(double r, double g, double b,
                             double roughness, double metallic, double specular)
{
    ws_material m;
    m.albedo[0] = r;
    m.albedo[1] = g;
    m.albedo[2] = b;
    m.roughness = roughness;
    m.metallic = metallic;
    m.specular = specular;
    m.albedo_map = NULL;
    return m;
}

ws_light ws_light_default(void)
{
    ws_light light;
    light.direction = ws_vec3_normalize(ws_vec3_make(-0.4, -1.0, -0.3));
    light.intensity = 0.85;
    light.color[0] = 255u;
    light.color[1] = 248u;
    light.color[2] = 235u;
    light.ambient = 40u;
    light.ambient_intensity = 0.25;
    light.sky_color[0] = 130u;
    light.sky_color[1] = 160u;
    light.sky_color[2] = 210u;
    light.ground_color[0] = 70u;
    light.ground_color[1] = 64u;
    light.ground_color[2] = 54u;
    light.sky_top[0] = 70u;
    light.sky_top[1] = 120u;
    light.sky_top[2] = 200u;
    light.sky_horizon[0] = 190u;
    light.sky_horizon[1] = 205u;
    light.sky_horizon[2] = 220u;
    return light;
}

ws_render_settings ws_render_settings_default(void)
{
    ws_render_settings s;
    s.exposure = 1.0;
    s.tonemap = 0;
    s.sky_enabled = 0;
    s.fog_density = 0.0;
    s.shadows_enabled = 0;
    s.shadow_strength = 1.0;
    s.shadow_bias = 0.0015;
    s.shadow_map_size = 1024u;
    return s;
}
