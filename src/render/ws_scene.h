/* Worldseed Engine — render scene contract.
 *
 * The scene is the only interface between the world/simulation and the
 * renderer. The renderer reads the scene and never writes to the world. A scene
 * is a flat list of instanced meshes plus a camera, one directional light and a
 * small set of render settings, which is enough for the P2 reference renderer
 * and golden scenes.
 */
#ifndef WS_RENDER_SCENE_H
#define WS_RENDER_SCENE_H

#include <stddef.h>
#include <stdint.h>

#include "../core/ws_math.h"
#include "../core/ws_result.h"
#include "ws_mesh.h"
#include "ws_texture.h"

typedef struct ws_camera {
    ws_vec3 eye;
    ws_vec3 target;
    ws_vec3 up;
    double fov_y_radians;
    double near_z;
    double far_z;
} ws_camera;

/* A single directional light plus a hemisphere ambient term and a gradient
 * sky. All colours are sRGB 0..255 on the wire and decoded to linear for
 * shading; the sky/ambient terms are only visible when settings.sky_enabled. */
typedef struct ws_light {
    ws_vec3 direction;        /* points from the light toward the scene */
    double intensity;         /* diffuse scale on the direct term */
    uint8_t color[3];         /* sRGB direct light colour */
    uint8_t ambient;          /* 0..255 flat ambient floor (kept for scenes) */
    double ambient_intensity; /* 0..1 scale on the hemisphere ambient */
    uint8_t sky_color[3];     /* sRGB hemisphere colour facing up */
    uint8_t ground_color[3];  /* sRGB hemisphere colour facing down */
    uint8_t sky_top[3];       /* sRGB gradient sky at the zenith */
    uint8_t sky_horizon[3];   /* sRGB gradient sky at the horizon */
} ws_light;

typedef struct ws_material {
    double albedo[3];             /* linear 0..1 base colour (when no map) */
    double roughness;             /* 0..1; drives the specular exponent */
    double metallic;              /* 0..1; 1 tints specular with albedo */
    double specular;              /* 0..1 specular scale (0 = fully matte) */
    const ws_texture *albedo_map; /* optional sRGB texture; overrides albedo */
} ws_material;

typedef struct ws_scene_object {
    const ws_mesh *mesh;
    ws_mat4 transform;   /* model matrix */
    ws_material material;
} ws_scene_object;

typedef struct ws_render_settings {
    double exposure;    /* linear multiplier before tonemapping */
    int tonemap;        /* 0 = clamp, 1 = Reinhard filmic shoulder */
    int sky_enabled;    /* 1 = draw the gradient sky behind the geometry */
    double fog_density; /* exponential distance fog; 0 disables it */
    /* Directional shadow map. 0 (the default) keeps the direct-only path, so
     * existing scenes and golden frames are unchanged; 1 renders a shadow pass
     * from the light and attenuates the direct term where geometry occludes it.
     * The shadow pass is an extra scene render and costs a second frame's worth
     * of geometry work, so scenes opt in. */
    int shadows_enabled;
    double shadow_strength; /* 0..1 how dark full shadow is (1 = black) */
    double shadow_bias;     /* depth bias in light space to fight acne */
    unsigned shadow_map_size; /* square shadow map edge in texels (0 = 1024) */
    /* Penumbra width in shadow-map texels. 0 (the default) keeps the 3x3 PCF
     * hard-edge filter, so every existing scene and golden frame is unchanged;
     * a positive value widens the filter to a fixed disc of that radius, giving
     * a soft contact shadow. The tap pattern is a fixed integer grid, so the
     * result stays deterministic and identical across the parallel render bands. */
    double shadow_softness;
} ws_render_settings;

typedef struct ws_scene {
    ws_scene_object *objects;
    uint32_t object_count;
    uint32_t object_capacity;
    ws_camera camera;
    ws_light light;
    ws_render_settings settings;
    uint8_t background[3]; /* flat clear colour when sky is disabled */
} ws_scene;

/* Initialise an empty scene over caller-owned object storage. */
ws_result ws_scene_init(ws_scene *scene, ws_scene_object *storage, uint32_t capacity);

/* Append an object. Returns WS_ERR_CAPACITY when the storage is full. */
ws_result ws_scene_add_object(ws_scene *scene, const ws_mesh *mesh,
                              ws_mat4 transform, ws_material material);

/* A camera with sensible defaults; callers override what they need. */
ws_camera ws_camera_default(void);

/* A material with the given linear albedo, roughness, metallic and specular
 * scale and no albedo map. */
ws_material ws_material_make(double r, double g, double b,
                             double roughness, double metallic, double specular);

/* A light with sensible defaults (soft daylight, clear sky). */
ws_light ws_light_default(void);

/* Render settings with sensible defaults (linear, no sky, no fog). */
ws_render_settings ws_render_settings_default(void);

#endif /* WS_RENDER_SCENE_H */
