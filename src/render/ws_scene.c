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

ws_light ws_light_default(void)
{
    ws_light light;
    light.direction = ws_vec3_normalize(ws_vec3_make(-0.4, -1.0, -0.3));
    light.intensity = 0.85;
    light.color[0] = 255u;
    light.color[1] = 248u;
    light.color[2] = 235u;
    light.ambient = 40u;
    return light;
}
