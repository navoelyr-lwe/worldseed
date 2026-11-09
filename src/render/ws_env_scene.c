/* Worldseed Engine — build a renderable scene from a world region. */
#include "ws_env_scene.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "../core/ws_math.h"
#include "../core/ws_noise.h"
#include "../physics/ws_physics.h"

/* Per-surface sRGB colours used by the terrain texture. */
static void surface_color(uint8_t surface, uint8_t out[3])
{
    switch (surface) {
    case WS_SURFACE_SOIL:  out[0] = 108u; out[1] = 86u;  out[2] = 60u;  break;
    case WS_SURFACE_ROCK:  out[0] = 118u; out[1] = 116u; out[2] = 110u; break;
    case WS_SURFACE_SAND:  out[0] = 196u; out[1] = 178u; out[2] = 128u; break;
    case WS_SURFACE_GRASS:
    default:               out[0] = 84u;  out[1] = 120u; out[2] = 64u;  break;
    }
}

/* Shade a surface colour by a signed factor around 1.0, keeping it in range. */
static uint8_t tint(uint8_t base, double factor)
{
    double v = (double)base * factor;
    if (v < 0.0) {
        v = 0.0;
    }
    if (v > 255.0) {
        v = 255.0;
    }
    return (uint8_t)(v + 0.5);
}

ws_env_scene_options ws_env_scene_options_default(uint64_t seed)
{
    ws_env_scene_options o;
    o.seed = seed;
    o.region_x = 0;
    o.region_y = 0;
    o.cells = 64u;
    o.cell_size = 8.0;
    o.scene_kind = 0;
    o.time_of_day_s = 12.0 * 3600.0;
    return o;
}

const char *ws_env_scene_kind_id(int scene_kind)
{
    switch (scene_kind) {
    case 0: return "terrain_day";
    case 1: return "terrain_dusk";
    case 2: return "terrain_weather";
    case 3: return "terrain_physics";
    default: return "terrain";
    }
}

/* Normal at a height-field node from central differences (one-sided at edges). */
static ws_vec3 node_normal(const ws_terrain *t, uint32_t ix, uint32_t iy)
{
    uint32_t n = t->cells;
    double cell = t->config.cell_size;
    uint32_t x0 = ix > 0u ? ix - 1u : ix;
    uint32_t x1 = ix < n ? ix + 1u : ix;
    uint32_t y0 = iy > 0u ? iy - 1u : iy;
    uint32_t y1 = iy < n ? iy + 1u : iy;
    double dhdx = (ws_terrain_node(t, x1, iy) - ws_terrain_node(t, x0, iy)) /
                  ((double)(x1 - x0) * cell);
    double dhdy = (ws_terrain_node(t, ix, y1) - ws_terrain_node(t, ix, y0)) /
                  ((double)(y1 - y0) * cell);
    return ws_vec3_normalize(ws_vec3_make(-dhdx, 1.0, -dhdy));
}

static ws_result build_terrain_mesh(ws_env_scene *e)
{
    const ws_terrain *t = &e->terrain;
    uint32_t n = t->cells;
    size_t nodes = (size_t)n + 1u;
    size_t vcount = nodes * nodes;
    size_t icount = (size_t)n * (size_t)n * 6u;

    /* calloc: vertex padding must be deterministic for raw-byte hashing. */
    e->terrain_verts = (ws_vertex *)calloc(vcount, sizeof(ws_vertex));
    e->terrain_idx = (uint32_t *)malloc(icount * sizeof(uint32_t));
    if (e->terrain_verts == NULL || e->terrain_idx == NULL) {
        return WS_ERR_OUT_OF_MEMORY;
    }

    /* Texture sized (n+1) so each node samples exactly one texel. */
    ws_result r = ws_texture_create(&e->terrain_tex, n + 1u, n + 1u, 0u, 0u, 0u, 255u);
    if (r != WS_OK) {
        return r;
    }

    for (uint32_t iy = 0; iy <= n; ++iy) {
        for (uint32_t ix = 0; ix <= n; ++ix) {
            ws_vec3 pos = ws_vec3_make((double)ix * t->config.cell_size,
                                       ws_terrain_node(t, ix, iy),
                                       (double)iy * t->config.cell_size);
            ws_vec3 nrm = node_normal(t, ix, iy);
            ws_vertex *v = &e->terrain_verts[(size_t)iy * nodes + ix];
            v->position = pos;
            v->normal = nrm;
            v->u = ((double)ix + 0.5) / (double)(n + 1u);
            v->v = ((double)iy + 0.5) / (double)(n + 1u);
            v->color[0] = 255u; v->color[1] = 255u; v->color[2] = 255u;

            uint32_t cx = ix < n ? ix : n - 1u;
            uint32_t cy = iy < n ? iy : n - 1u;
            uint8_t col[3];
            surface_color(ws_terrain_cell_surface(t, cx, cy), col);
            /* Break up the flat per-cell colour with a deterministic, fine
             * variation so the ground reads as a surface, not solid tiles. */
            double vn = ws_noise_value2(t->config.seed ^ 0x5eedu,
                                        (double)ix * 0.37, (double)iy * 0.37);
            double f = 0.90 + 0.20 * vn;
            ws_texture_set(&e->terrain_tex, ix, iy,
                           tint(col[0], f), tint(col[1], f), tint(col[2], f), 255u);
        }
    }

    uint32_t ii = 0u;
    for (uint32_t cy = 0; cy < n; ++cy) {
        for (uint32_t cx = 0; cx < n; ++cx) {
            uint32_t a = cy * (uint32_t)nodes + cx;
            uint32_t b = a + 1u;
            uint32_t c = a + (uint32_t)nodes;
            uint32_t d = c + 1u;
            e->terrain_idx[ii++] = a; e->terrain_idx[ii++] = c; e->terrain_idx[ii++] = b;
            e->terrain_idx[ii++] = b; e->terrain_idx[ii++] = c; e->terrain_idx[ii++] = d;
        }
    }
    e->terrain_mesh.vertices = e->terrain_verts;
    e->terrain_mesh.vertex_count = (uint32_t)vcount;
    e->terrain_mesh.indices = e->terrain_idx;
    e->terrain_mesh.index_count = ii;
    return WS_OK;
}

static ws_result build_water_mesh(ws_env_scene *e)
{
    const ws_terrain *t = &e->terrain;
    const ws_hydro *h = &e->hydro;
    uint32_t n = t->cells;
    double cell = t->config.cell_size;

    size_t water_cells = 0u;
    for (uint32_t cy = 0; cy < n; ++cy) {
        for (uint32_t cx = 0; cx < n; ++cx) {
            if (h->is_river[(size_t)cy * n + cx]) {
                water_cells += 1u;
            }
        }
    }
    if (water_cells == 0u) {
        e->water_mesh.vertices = NULL;
        e->water_mesh.indices = NULL;
        e->water_mesh.vertex_count = 0u;
        e->water_mesh.index_count = 0u;
        return WS_OK;
    }

    e->water_verts = (ws_vertex *)calloc(water_cells * 4u, sizeof(ws_vertex));
    e->water_idx = (uint32_t *)malloc(water_cells * 6u * sizeof(uint32_t));
    if (e->water_verts == NULL || e->water_idx == NULL) {
        return WS_ERR_OUT_OF_MEMORY;
    }

    uint32_t vi = 0u;
    uint32_t ii = 0u;
    for (uint32_t cy = 0; cy < n; ++cy) {
        for (uint32_t cx = 0; cx < n; ++cx) {
            size_t ci = (size_t)cy * n + cx;
            if (!h->is_river[ci]) {
                continue;
            }
            double y = ws_hydro_surface_height(h, t, cx, cy);
            double x0 = (double)cx * cell;
            double x1 = (double)(cx + 1u) * cell;
            double z0 = (double)cy * cell;
            double z1 = (double)(cy + 1u) * cell;
            ws_vec3 up = ws_vec3_make(0.0, 1.0, 0.0);
            ws_vertex *q = &e->water_verts[vi];
            q[0].position = ws_vec3_make(x0, y, z0); q[0].normal = up; q[0].u = 0.0; q[0].v = 0.0;
            q[1].position = ws_vec3_make(x1, y, z0); q[1].normal = up; q[1].u = 1.0; q[1].v = 0.0;
            q[2].position = ws_vec3_make(x1, y, z1); q[2].normal = up; q[2].u = 1.0; q[2].v = 1.0;
            q[3].position = ws_vec3_make(x0, y, z1); q[3].normal = up; q[3].u = 0.0; q[3].v = 1.0;
            for (int k = 0; k < 4; ++k) {
                q[k].color[0] = 60u; q[k].color[1] = 120u; q[k].color[2] = 160u;
            }
            uint32_t base = vi;
            e->water_idx[ii++] = base; e->water_idx[ii++] = base + 1u; e->water_idx[ii++] = base + 2u;
            e->water_idx[ii++] = base; e->water_idx[ii++] = base + 2u; e->water_idx[ii++] = base + 3u;
            vi += 4u;
        }
    }
    e->water_mesh.vertices = e->water_verts;
    e->water_mesh.vertex_count = vi;
    e->water_mesh.indices = e->water_idx;
    e->water_mesh.index_count = ii;
    return WS_OK;
}

static ws_result build_cone_mesh(ws_env_scene *e)
{
    unsigned segments = 10u;
    size_t vcount = ws_mesh_cone_vertex_count(segments);
    size_t icount = ws_mesh_cone_index_count(segments);
    e->cone_verts = (ws_vertex *)calloc(vcount, sizeof(ws_vertex));
    e->cone_idx = (uint32_t *)malloc(icount * sizeof(uint32_t));
    if (e->cone_verts == NULL || e->cone_idx == NULL) {
        return WS_ERR_OUT_OF_MEMORY;
    }
    const uint8_t color[3] = {44u, 82u, 46u};
    return ws_mesh_make_cone(&e->cone_mesh, e->cone_verts, vcount, e->cone_idx, icount,
                             segments, color);
}

/* Terrain height callback for the physics solver: world (x, z) -> height. */
static double terrain_height_cb(void *ctx, double x, double y)
{
    const ws_env_scene *e = (const ws_env_scene *)ctx;
    return ws_terrain_height_bilinear(&e->terrain, x, y);
}

/* Choose a drop point: the flattest dry interior spot, so the rock settles
 * quickly and visibly instead of rolling down a slope or sinking in water. */
static void pick_drop_point(const ws_env_scene *e, double *out_x, double *out_z)
{
    double bx = e->extent * 0.5;
    double bz = e->extent * 0.5;
    double best = 1.0e30;
    const int n = 20;
    for (int i = 2; i < n - 1; ++i) {
        for (int j = 2; j < n - 1; ++j) {
            double x = e->extent * (double)i / (double)n;
            double z = e->extent * (double)j / (double)n;
            double h = ws_terrain_height_bilinear(&e->terrain, x, z);
            if (h < e->terrain.config.water_level + 1.5) {
                continue;
            }
            double hx = ws_terrain_height_bilinear(&e->terrain, x + 3.0, z) - h;
            double hz = ws_terrain_height_bilinear(&e->terrain, x, z + 3.0) - h;
            double slope = hx * hx + hz * hz;
            if (slope < best) {
                best = slope;
                bx = x;
                bz = z;
            }
        }
    }
    *out_x = bx;
    *out_z = bz;
}

/* Drop a rigid sphere onto the terrain with the P4 solver and record where it
 * comes to rest. The body is simulated headless; only its final transform is
 * rendered, so the picture is a physics result and not a scripted animation.
 * The simulation runs for a fixed wall-clock budget and then the body must be
 * at rest, otherwise the scene is reported as failed rather than shown wrong. */
static ws_result run_physics_drop(ws_env_scene *e, double cx, double cz)
{
    unsigned segments = 12u;
    unsigned rings = 8u;
    size_t vcount = ws_mesh_sphere_vertex_count(segments, rings);
    size_t icount = ws_mesh_sphere_index_count(segments, rings);
    e->rock_verts = (ws_vertex *)calloc(vcount, sizeof(ws_vertex));
    e->rock_idx = (uint32_t *)malloc(icount * sizeof(uint32_t));
    if (e->rock_verts == NULL || e->rock_idx == NULL) {
        return WS_ERR_OUT_OF_MEMORY;
    }
    const uint8_t rock_color[3] = {116u, 112u, 104u};
    ws_result r = ws_mesh_make_sphere(&e->rock_mesh, e->rock_verts, vcount,
                                      e->rock_idx, icount, segments, rings, rock_color);
    if (r != WS_OK) {
        return r;
    }

    const double radius = 1.2; /* m */

    ws_physics_config cfg = ws_physics_config_default();
    cfg.dt = 1.0 / 200.0;
    cfg.solver_iterations = 8;
    cfg.water_level = e->terrain.config.water_level;
    ws_body storage[1];
    ws_physics_world world;
    r = ws_physics_world_init(&world, &cfg, storage, 1u);
    if (r != WS_OK) {
        return r;
    }
    ws_physics_world_set_terrain(&world, terrain_height_cb, e);

    double drop_y = ws_terrain_height_bilinear(&e->terrain, cx, cz) + radius + 25.0;
    ws_body body;
    memset(&body, 0, sizeof(body));
    body.mass = 60.0;
    body.radius = radius;
    body.restitution = 0.15;
    body.friction = 0.7;
    body.position = ws_vec3_make(cx, drop_y, cz);
    r = ws_physics_world_add(&world, &body);
    if (r != WS_OK) {
        return r;
    }

    /* 6 s budget: enough to fall 25 m and settle for this contact model. */
    r = ws_physics_advance(&world, 1200);
    if (r != WS_OK) {
        return r;
    }
    const ws_body *b = &world.bodies[0];
    if (!b->grounded || ws_vec3_length(b->velocity) > 0.1) {
        return WS_ERR_INVALID_STATE; /* not at rest: refuse to fake a result */
    }
    e->rock_rest_y = b->position.y;
    /* Sphere mesh is unit diameter (radius 0.5), so scale by 2 * radius. */
    e->rock_transform = ws_mat4_translation(ws_vec3_make(
        b->position.x - e->terrain.origin_x, b->position.y,
        b->position.z - e->terrain.origin_y));
    e->rock_transform = ws_mat4_multiply(
        e->rock_transform, ws_mat4_scale(ws_vec3_make(radius * 2.0, radius * 2.0, radius * 2.0)));
    return WS_OK;
}

static void sun_and_sky(const ws_environment *env, ws_light *light,
                        double *out_intensity, double *out_ambient)
{
    double alt = ws_clamp_double(env->sun_altitude, -1.0, 1.0);
    double day = env->daylight;

    /* Sun colour warms as it drops toward the horizon. */
    double warm = 1.0 - ws_clamp_double((alt - 0.05) / 0.35, 0.0, 1.0);
    light->color[0] = (uint8_t)(255.0);
    light->color[1] = (uint8_t)(230.0 - 90.0 * warm);
    light->color[2] = (uint8_t)(190.0 - 150.0 * warm);
    light->direction = ws_vec3_normalize(ws_vec3_scale(env->sun_direction, -1.0));

    light->sky_color[0] = (uint8_t)(90.0 + 70.0 * day);
    light->sky_color[1] = (uint8_t)(120.0 + 80.0 * day);
    light->sky_color[2] = (uint8_t)(160.0 + 80.0 * day);
    light->ground_color[0] = 70u; light->ground_color[1] = 62u; light->ground_color[2] = 50u;

    light->sky_top[0] = (uint8_t)(60.0 + 40.0 * day);
    light->sky_top[1] = (uint8_t)(95.0 + 60.0 * day);
    light->sky_top[2] = (uint8_t)(150.0 + 90.0 * day);
    light->sky_horizon[0] = (uint8_t)(170.0 + 60.0 * warm);
    light->sky_horizon[1] = (uint8_t)(160.0 - 20.0 * warm);
    light->sky_horizon[2] = (uint8_t)(150.0 - 40.0 * warm);

    *out_intensity = 0.25 + 0.85 * day;
    *out_ambient = 0.15 + 0.5 * day;
}

/* Scan a day for the time matching a scene kind, so dusk and weather scenes are
 * produced by the environment model rather than by hard-coded numbers. */
static double pick_time_of_day(const ws_env_config *cfg, int scene_kind)
{
    if (scene_kind == WS_ENV_SCENE_DAY || scene_kind == WS_ENV_SCENE_PHYSICS) {
        return 12.0 * 3600.0;
    }
    ws_environment probe;
    probe.config = *cfg;
    double best_t = 12.0 * 3600.0;
    double best_score = -1.0e30;
    for (int m = 0; m < 24 * 60; m += 5) {
        double t = (double)m * 60.0;
        ws_environment_update(&probe, t, 0.0);
        double score;
        if (scene_kind == WS_ENV_SCENE_DUSK) {
            /* Dusk: sun low but still up. */
            score = -fabs(probe.sun_altitude - 0.10);
        } else {
            /* Weather: heaviest precipitation, then most fog, but kept in
             * daylight so it reads as an overcast day rather than night. */
            score = probe.precipitation * 4.0 + probe.fog_density + probe.daylight;
        }
        if (score > best_score) {
            best_score = score;
            best_t = t;
        }
    }
    return best_t;
}

ws_result ws_env_scene_create(ws_env_scene *e, const ws_env_scene_options *opts)
{
    if (e == NULL || opts == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    memset(e, 0, sizeof(*e));

    ws_terrain_config tcfg = ws_terrain_config_default(opts->seed);
    tcfg.cell_size = opts->cell_size;
    tcfg.region_cells = opts->cells;

    ws_result r = ws_terrain_create(&e->terrain, &tcfg, opts->region_x, opts->region_y);
    if (r != WS_OK) {
        ws_env_scene_free(e);
        return r;
    }
    ws_hydro_config hcfg = ws_hydro_config_default(&tcfg);
    r = ws_hydro_create(&e->hydro, &hcfg, &e->terrain);
    if (r != WS_OK) {
        ws_env_scene_free(e);
        return r;
    }
    r = ws_habitat_create(&e->habitat, &e->terrain, &e->hydro);
    if (r != WS_OK) {
        ws_env_scene_free(e);
        return r;
    }
    r = ws_vegetation_create(&e->veg, &e->terrain, &e->habitat);
    if (r != WS_OK) {
        ws_env_scene_free(e);
        return r;
    }

    ws_env_config ecfg = ws_env_config_default(opts->seed);
    double tod = opts->time_of_day_s;
    if ((opts->scene_kind == WS_ENV_SCENE_DUSK ||
         opts->scene_kind == WS_ENV_SCENE_WEATHER) && tod == 12.0 * 3600.0) {
        tod = pick_time_of_day(&ecfg, opts->scene_kind);
    }
    ws_environment_init(&e->env, &ecfg);
    ws_environment_update(&e->env, tod, tcfg.base_height);

    e->extent = (double)opts->cells * opts->cell_size;

    /* Physics prop first: drop the rock on the flattest dry spot, then aim the
     * camera at where it actually came to rest, so the visible picture is the
     * solver's result and not a scripted pose. */
    double drop_x = e->extent * 0.5;
    double drop_z = e->extent * 0.5;
    if (opts->scene_kind == WS_ENV_SCENE_PHYSICS) {
        pick_drop_point(e, &drop_x, &drop_z);
        ws_result pr = run_physics_drop(e, drop_x, drop_z);
        if (pr != WS_OK) {
            ws_env_scene_free(e);
            return pr;
        }
    }

    /* Camera. Default environment scenes use a low, near-ground view from
     * inside the region looking across it with a fixed gentle downward pitch,
     * which is the scale at which terrain relief, water and individual trees are
     * visible. The physics scene instead frames the resting rock from a close,
     * slightly elevated angle so the contact with the ground is legible. */
    double eye_x;
    double eye_y;
    double eye_z;
    double tgt_x;
    double tgt_y;
    double tgt_z;
    if (opts->scene_kind == WS_ENV_SCENE_PHYSICS) {
        double rx = drop_x - e->terrain.origin_x;
        double rz = drop_z - e->terrain.origin_y;
        double ry = e->rock_rest_y;
        double back = 12.0;
        double up = 4.0;
        eye_x = rx - back;
        eye_y = ry + up;
        eye_z = rz - back * 0.6;
        tgt_x = rx;
        tgt_y = ry;
        tgt_z = rz;
    } else {
        double mid_x = e->extent * 0.5;
        eye_x = mid_x - e->extent * 0.08;
        eye_z = e->extent * 0.06;
        eye_y = ws_terrain_height_bilinear(&e->terrain, eye_x, eye_z) + 4.0;
        double fwd_x = 0.16;
        double fwd_z = 0.99;
        double flen = sqrt(fwd_x * fwd_x + fwd_z * fwd_z);
        fwd_x /= flen;
        fwd_z /= flen;
        double look = e->extent * 0.9;
        double pitch = -5.0 * (3.14159265358979323846 / 180.0);
        tgt_x = eye_x + fwd_x * look;
        tgt_y = eye_y + look * tan(pitch);
        tgt_z = eye_z + fwd_z * look;
    }
    ws_camera camera = ws_camera_default();
    camera.eye = ws_vec3_make(eye_x, eye_y, eye_z);
    camera.target = ws_vec3_make(tgt_x, tgt_y, tgt_z);
    camera.far_z = e->extent * 8.0;

    if (build_terrain_mesh(e) != WS_OK || build_water_mesh(e) != WS_OK ||
        build_cone_mesh(e) != WS_OK) {
        ws_env_scene_free(e);
        return WS_ERR_OUT_OF_MEMORY;
    }

    e->object_capacity = WS_ENV_MAX_OBJECTS;
    e->objects = (ws_scene_object *)malloc((size_t)e->object_capacity * sizeof(ws_scene_object));
    if (e->objects == NULL) {
        ws_env_scene_free(e);
        return WS_ERR_OUT_OF_MEMORY;
    }
    r = ws_scene_init(&e->scene, e->objects, e->object_capacity);
    if (r != WS_OK) {
        ws_env_scene_free(e);
        return r;
    }

    /* Terrain: textured mesh carrying the surface classes. */
    ws_material ground = ws_material_make(1.0, 1.0, 1.0, 0.92, 0.0, 0.02);
    ground.albedo_map = &e->terrain_tex;
    (void)ws_scene_add_object(&e->scene, &e->terrain_mesh, ws_mat4_identity(), ground);

    /* Water surface at the simulated level. */
    if (e->water_mesh.vertex_count > 0u) {
        ws_material water = ws_material_make(0.06, 0.20, 0.30, 0.08, 0.0, 0.9);
        (void)ws_scene_add_object(&e->scene, &e->water_mesh, ws_mat4_identity(), water);
    }

    /* Trees: instanced cones placed from the vegetation layer. If there are
     * more trees than the object budget, take an even stride so the sample is
     * spread across the whole region instead of one corner. */
    uint32_t tree_total = 0u;
    for (uint32_t i = 0; i < e->veg.count; ++i) {
        if (e->veg.plants[i].kind == WS_PLANT_TREE) {
            tree_total += 1u;
        }
    }
    uint32_t stride = (tree_total > WS_ENV_MAX_TREES) ? (tree_total / WS_ENV_MAX_TREES) : 1u;
    if (stride == 0u) {
        stride = 1u;
    }
    uint32_t seen = 0u;
    uint32_t trees = 0u;
    for (uint32_t i = 0; i < e->veg.count && trees < WS_ENV_MAX_TREES; ++i) {
        const ws_plant *p = &e->veg.plants[i];
        if (p->kind != WS_PLANT_TREE) {
            continue;
        }
        if ((seen++ % stride) != 0u) {
            continue;
        }
        double lx = p->position.x - e->terrain.origin_x;
        double lz = p->position.y - e->terrain.origin_y;
        double gy = ws_terrain_height_bilinear(&e->terrain, p->position.x, p->position.y);
        double h = p->height;
        ws_mat4 t = ws_mat4_multiply(
            ws_mat4_translation(ws_vec3_make(lx, gy, lz)),
            ws_mat4_scale(ws_vec3_make(h * 0.42, h, h * 0.42)));
        ws_material m = ws_material_make(0.09, 0.22, 0.11, 0.85, 0.0, 0.05);
        if (ws_scene_add_object(&e->scene, &e->cone_mesh, t, m) == WS_OK) {
            trees += 1u;
        }
    }
    e->tree_count = trees;

    /* Physics prop: the rock where the solver left it. */
    if (opts->scene_kind == WS_ENV_SCENE_PHYSICS) {
        ws_material rock = ws_material_make(0.62, 0.60, 0.56, 0.9, 0.0, 0.05);
        (void)ws_scene_add_object(&e->scene, &e->rock_mesh, e->rock_transform, rock);
    }

    e->scene.camera = camera;
    e->scene.light = ws_light_default();
    double intensity = 1.0;
    double ambient = 0.4;
    sun_and_sky(&e->env, &e->scene.light, &intensity, &ambient);
    e->scene.light.intensity = intensity;
    e->scene.light.ambient_intensity = ambient;

    e->scene.settings = ws_render_settings_default();
    e->scene.settings.sky_enabled = 1;
    e->scene.settings.tonemap = 1;
    e->scene.settings.exposure = 1.1;
    /* Exponential coefficient per metre: a clear day stays sharp for hundreds of
     * metres, humid weather closes in. Values are tuned to the region scale. */
    e->scene.settings.fog_density = 0.00025 + e->env.humidity * 0.00075;
    return WS_OK;
}

void ws_env_scene_free(ws_env_scene *e)
{
    if (e == NULL) {
        return;
    }
    ws_texture_free(&e->terrain_tex);
    free(e->terrain_verts);
    free(e->terrain_idx);
    free(e->water_verts);
    free(e->water_idx);
    free(e->cone_verts);
    free(e->cone_idx);
    free(e->rock_verts);
    free(e->rock_idx);
    free(e->objects);
    ws_vegetation_free(&e->veg);
    ws_habitat_free(&e->habitat);
    ws_hydro_free(&e->hydro);
    ws_terrain_free(&e->terrain);
    memset(e, 0, sizeof(*e));
}
