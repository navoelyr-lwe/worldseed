/* Worldseed Engine — render scene contract.
 *
 * The scene is the only interface between the world/simulation and the
 * renderer. The renderer reads the scene and never writes to the world. A scene
 * is a flat list of instanced meshes plus a camera and one directional light,
 * which is enough for the P2 reference renderer and golden scenes.
 */
#ifndef WS_RENDER_SCENE_H
#define WS_RENDER_SCENE_H

#include <stddef.h>
#include <stdint.h>

#include "../core/ws_math.h"
#include "../core/ws_result.h"
#include "ws_mesh.h"

typedef struct ws_camera {
    ws_vec3 eye;
    ws_vec3 target;
    ws_vec3 up;
    double fov_y_radians;
    double near_z;
    double far_z;
} ws_camera;

typedef struct ws_light {
    ws_vec3 direction; /* points from the light toward the scene */
    double intensity;  /* 0..1 scale on the diffuse term */
    uint8_t color[3];
    uint8_t ambient;   /* 0..255 flat ambient floor */
} ws_light;

typedef struct ws_material {
    double albedo[3];  /* linear 0..1 base colour */
    double roughness;  /* 0..1 */
    double metallic;   /* 0..1 */
} ws_material;

typedef struct ws_scene_object {
    const ws_mesh *mesh;
    ws_mat4 transform;   /* model matrix */
    ws_material material;
} ws_scene_object;

typedef struct ws_scene {
    ws_scene_object *objects;
    uint32_t object_count;
    uint32_t object_capacity;
    ws_camera camera;
    ws_light light;
    uint8_t background[3]; /* linear 0..255 clear colour */
} ws_scene;

/* Initialise an empty scene over caller-owned object storage. */
ws_result ws_scene_init(ws_scene *scene, ws_scene_object *storage, uint32_t capacity);

/* Append an object. Returns WS_ERR_CAPACITY when the storage is full. */
ws_result ws_scene_add_object(ws_scene *scene, const ws_mesh *mesh,
                              ws_mat4 transform, ws_material material);

/* A camera with sensible defaults; callers override what they need. */
ws_camera ws_camera_default(void);

/* A light with sensible defaults (soft daylight from above). */
ws_light ws_light_default(void);

#endif /* WS_RENDER_SCENE_H */
