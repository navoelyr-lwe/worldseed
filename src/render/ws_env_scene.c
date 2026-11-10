/* Worldseed Engine — build a renderable scene from a world region. */
#include "ws_env_scene.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "../core/ws_math.h"
#include "../core/ws_noise.h"
#include "../physics/ws_physics.h"
#include "../world/ws_world_host.h"

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
    o.scene_kind = WS_ENV_SCENE_DAY;
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
    case 4: return "creature_walk";
    case 5: return "organism_forage";
    case 6: return "ecosystem";
    case 7: return "world_stream";
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

/* ---- P5 reference creature scene ------------------------------------------
 * The articulated red-deer body is driven by the gait controller for a fixed
 * simulated interval and then rendered mid-stride. The visible picture is the
 * biomechanics result: link poses, contact state and forward progress all come
 * from the simulated body, not a scripted animation. */

/* The creature model is planar (sagittal), so its ground is the terrain profile
 * sampled along the walking line at fixed lateral offset z. */
typedef struct ws_creature_ground {
    const ws_env_scene *e;
    double z;
} ws_creature_ground;

static double creature_profile_cb(void *ctx, double x)
{
    const ws_creature_ground *g = (const ws_creature_ground *)ctx;
    return ws_terrain_height_bilinear(&g->e->terrain, x, g->z);
}

/* Run the reference quadruped walking across the terrain for a fixed simulated
 * interval and keep its final body state on the scene so the renderer can draw
 * it. Returns the distance walked (metres) through *out_forward. */
static ws_result run_creature_walk(ws_env_scene *e, double start_x, double start_z,
                                   double *out_forward)
{
    static ws_cjoint joints[8];
    static ws_cfoot feet[4];
    ws_quad_storage st;
    ws_species_profile sp = ws_species_red_deer();
    ws_creature c;
    ws_quad_parts parts;
    ws_creature_ground ground;
    ground.e = e;
    ground.z = start_z;

    double ground0 = ws_terrain_height_bilinear(&e->terrain, start_x, start_z);
    ws_result r = ws_creature_init(&c, 1.0 / 500.0, 9.80665, 30, 0.2,
                                   e->creature_bodies, 8u, joints, 8u, feet, 4u);
    if (r != WS_OK) {
        return r;
    }
    ws_creature_set_ground(&c, creature_profile_cb, &ground);
    r = ws_quadruped_build(&c, &st, &sp, ground0, &parts);
    if (r != WS_OK) {
        return r;
    }
    /* Shift the assembled body so it starts at the requested spot. */
    double dx = start_x - e->creature_bodies[parts.torso].px;
    double dy = ground0 - e->creature_bodies[parts.torso].py;
    for (uint32_t i = 0; i < c.body_count; ++i) {
        e->creature_bodies[i].px += dx;
        e->creature_bodies[i].py += dy;
    }

    for (int i = 0; i < 1000; ++i) {
        ws_quadruped_balance(&c, parts.torso, 4000.0, 500.0, 0.0);
        ws_creature_step(&c);
    }
    ws_gait_state gs;
    ws_gait_state_init(&gs);
    double x0 = e->creature_bodies[parts.torso].px;
    int steps = (int)(12.0 / c.dt);
    for (int i = 0; i < steps; ++i) {
        ws_quadruped_walk(&c, &parts, &sp, &gs, 0.8);
        ws_quadruped_balance(&c, parts.torso, 4000.0, 500.0, 0.0);
        ws_creature_step(&c);
    }
    e->creature_body_count = c.body_count;
    e->creature_parts = parts;
    e->creature_contact = (int)c.contact_count;
    *out_forward = e->creature_bodies[parts.torso].px - x0;
    return WS_OK;
}

/* Add one creature link as an oriented box. Local x runs along the link. The
 * link's centre is shifted from the region-local frame to the mesh frame, which
 * is shared across all links. `z_off` places the left/right leg pairs. */
static void add_link(ws_env_scene *e, const ws_cbody *b, double z_off,
                     ws_material m)
{
    double sx = b->half_len * 2.0;
    double sy = b->half_thick * 2.0;
    double sz = b->half_width * 2.0;
    ws_mat4 t = ws_mat4_translation(ws_vec3_make(b->px, b->py, e->creature_z + z_off));
    t = ws_mat4_multiply(t, ws_mat4_rotation_z(b->angle));
    t = ws_mat4_multiply(t, ws_mat4_scale(ws_vec3_make(sx, sy, sz)));
    (void)ws_scene_add_object(&e->scene, &e->cube_mesh, t, m);
}

static void add_creature_objects(ws_env_scene *e)
{
    const ws_quad_parts *p = &e->creature_parts;
    ws_material body = ws_material_make(0.42, 0.26, 0.16, 0.85, 0.0, 0.05);
    ws_material leg = ws_material_make(0.30, 0.19, 0.12, 0.85, 0.0, 0.05);
    ws_material head = ws_material_make(0.48, 0.31, 0.19, 0.85, 0.0, 0.05);
    /* The torso, neck and head lie on the sagittal plane; the legs are drawn
     * twice, offset left and right, so the body reads as a quadruped even
     * though the dynamics are planar. */
    const double leg_z = 0.16;
    for (uint32_t i = 0; i < e->creature_body_count; ++i) {
        const ws_cbody *b = &e->creature_bodies[i];
        ws_material m = body;
        if ((int)i == p->head) {
            m = head;
        }
        int is_leg = ((int)i == p->front_upper || (int)i == p->front_lower ||
                      (int)i == p->rear_upper || (int)i == p->rear_lower);
        if (is_leg) {
            m = leg;
            add_link(e, b, -leg_z, m);
            add_link(e, b, leg_z, m);
        } else {
            add_link(e, b, 0.0, m);
        }
    }
}

/* ---- P6 behaviour organism scene ------------------------------------------
 * The reference animal runs the behaviour agent over resources placed from the
 * world layers. It senses, decides from its internal drives and walks to what
 * it needs; the scene then frames the animal where the decision took it, next
 * to the resource it acted on. The picture is the behaviour result, not a
 * scripted pose. */

typedef struct ws_organism_ground {
    const ws_env_scene *e;
} ws_organism_ground;

/* Build the reference quadruped standing at a region-local spot, facing
 * `heading` (rad, in-plane). Reuses the shared creature body storage. */
static ws_result build_creature_posed(ws_env_scene *e, double lx, double lz,
                                      double heading, double ground_y)
{
    static ws_cjoint joints[8];
    static ws_cfoot feet[4];
    ws_quad_storage st;
    ws_species_profile sp = ws_species_red_deer();
    ws_creature c;
    ws_quad_parts parts;
    ws_result r = ws_creature_init(&c, 1.0 / 500.0, 9.80665, 30, 0.2,
                                   e->creature_bodies, 8u, joints, 8u, feet, 4u);
    if (r != WS_OK) {
        return r;
    }
    /* Ground profile along the sagittal axis through the animal's spot. The
     * profile callback ignores heading because the body is settled before it is
     * rotated into place; on the gentle reference terrain this is well within
     * the model's stated validity. */
    static ws_creature_ground cg;
    cg.e = e;
    cg.z = lz + e->terrain.origin_y;
    ws_creature_set_ground(&c, creature_profile_cb, &cg);
    r = ws_quadruped_build(&c, &st, &sp, ground_y, &parts);
    if (r != WS_OK) {
        return r;
    }
    for (int i = 0; i < 800; ++i) {
        ws_quadruped_balance(&c, parts.torso, 4000.0, 500.0, 0.0);
        ws_creature_step(&c);
    }
    /* Rotate the settled body about its spot into the decided heading and move
     * it to the region-local spot. */
    double ox = e->creature_bodies[parts.torso].px;
    double oy = e->creature_bodies[parts.torso].py;
    double ch = cos(heading);
    double sh = sin(heading);
    for (uint32_t i = 0; i < c.body_count; ++i) {
        ws_cbody *b = &e->creature_bodies[i];
        double rx = b->px - ox;
        double ry = b->py - oy;
        b->px = lx + rx * ch - ry * sh;
        b->py = ground_y + (rx * sh + ry * ch);
        b->angle += heading;
    }
    e->creature_body_count = c.body_count;
    e->creature_parts = parts;
    e->creature_z = lz;
    return WS_OK;
}

/* Run the behaviour agent over a food patch and a water source placed near the
 * flattest dry spot, then pose the animal where the decision left it. */
static ws_result run_organism_forage(ws_env_scene *e, double spot_x, double spot_z)
{
    /* Two resources: a food patch and a water source, a short walk apart. */
    ws_sense_point pts[2];
    memset(pts, 0, sizeof(pts));
    pts[0].kind = WS_SENSE_FOOD;
    pts[0].x = spot_x + 10.0;
    pts[0].y = spot_z + 3.0;
    pts[0].value = 1.0;
    pts[1].kind = WS_SENSE_WATER;
    pts[1].x = spot_x - 6.0;
    pts[1].y = spot_z - 8.0;
    pts[1].value = 1.0;
    uint32_t count = 2u;

    ws_agent_config cfg = ws_agent_config_default();
    ws_agent_init(&e->organism, &cfg, 0x50u ^ e->terrain.config.seed, spot_x, spot_z, 0.0);
    /* Start hungry so the first decision is to forage for food. */
    e->organism.org.energy = 0.30;

    int steps = (int)(18.0 / 0.02);
    for (int i = 0; i < steps; ++i) {
        (void)ws_agent_step(&e->organism, 0.02, pts, &count, e->env.temperature_c);
    }

    /* Report the resource the animal acted on: the nearer of the two, and its
     * remaining value. */
    double d_food = hypot(pts[0].x - e->organism.x, pts[0].y - e->organism.y);
    double d_water = hypot(pts[1].x - e->organism.x, pts[1].y - e->organism.y);
    if (d_food <= d_water) {
        e->resource_kind = WS_SENSE_FOOD;
        e->resource_x = pts[0].x - e->terrain.origin_x;
        e->resource_z = pts[0].y - e->terrain.origin_y;
        e->resource_value = pts[0].value;
    } else {
        e->resource_kind = WS_SENSE_WATER;
        e->resource_x = pts[1].x - e->terrain.origin_x;
        e->resource_z = pts[1].y - e->terrain.origin_y;
        e->resource_value = pts[1].value;
    }

    e->organism_behavior = e->organism.behavior;
    e->organism_dist_m = e->organism.distance_travelled;
    e->organism_food_found = e->organism.food_found;
    e->organism_water_found = e->organism.water_found;

    double lx = e->organism.x - e->terrain.origin_x;
    double lz = e->organism.y - e->terrain.origin_y;
    double gy = ws_terrain_height_bilinear(&e->terrain, e->organism.x, e->organism.y);
    return build_creature_posed(e, lx, lz, e->organism.heading, gy);
}

/* Add a resource marker: a small coloured box on the ground at its position. */
static void add_resource_marker(ws_env_scene *e, double lx, double lz, ws_material m,
                                double sx, double sy, double sz)
{
    double gy = ws_terrain_height_bilinear(&e->terrain, lx + e->terrain.origin_x,
                                           lz + e->terrain.origin_y);
    ws_mat4 t = ws_mat4_translation(ws_vec3_make(lx, gy + sy * 0.5, lz));
    t = ws_mat4_multiply(t, ws_mat4_scale(ws_vec3_make(sx, sy, sz)));
    (void)ws_scene_add_object(&e->scene, &e->cube_mesh, t, m);
}

static void add_organism_objects(ws_env_scene *e)
{
    add_creature_objects(e);
    /* Food marker (green) and water marker (blue) so the acted-on resource is
     * visible next to the animal. */
    add_resource_marker(e, e->resource_x, e->resource_z,
                        ws_material_make(0.16, 0.55, 0.18, 0.85, 0.0, 0.05),
                        2.4, 0.9, 2.4);
}

/* ---- P7 ecosystem scene ---------------------------------------------------
 * The ecosystem runs headless over the world region; the scene then renders a
 * representative body for each species whose population survived, at a spot
 * derived from the simulated survivor, so the picture summarises the population
 * result rather than a scripted arrangement. */

/* Per-species runner body colours, so the four species read apart in one frame. */
static void species_color(int kind, uint8_t out[3])
{
    switch (kind) {
    case WS_SPECIES_DEER: out[0] = 122u; out[1] = 82u;  out[2] = 52u;  break;
    case WS_SPECIES_FOX:  out[0] = 176u; out[1] = 96u;  out[2] = 44u;  break;
    case WS_SPECIES_RABBIT: out[0] = 150u; out[1] = 138u; out[2] = 112u; break;
    case WS_SPECIES_BOAR: out[0] = 78u;  out[1] = 70u;  out[2] = 60u;  break;
    default:              out[0] = 120u; out[1] = 100u; out[2] = 80u;  break;
    }
}

/* Build a species body, settle it, walk it a short interval so it is caught
 * mid-stride, then rotate/translate it to a region-local spot facing `heading`.
 * Writes the final link poses into `out`. */
static ws_result build_species_walk(ws_env_scene *e, int kind,
                                    double lx, double lz, double heading,
                                    struct ws_rendered_creature *out)
{
    static ws_cjoint joints[8];
    static ws_cfoot feet[4];
    static ws_creature_ground cg;
    ws_quad_storage st;
    const ws_species_info *si = ws_species_get((ws_species_kind)kind);
    if (si == NULL) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    const ws_species_profile *sp = &si->body;
    ws_creature c;
    ws_quad_parts parts;

    double ground0 = 0.0; /* body is built above 0, then seated on the terrain */
    ws_result r = ws_creature_init(&c, 1.0 / 500.0, 9.80665, 30, 0.2,
                                   out->bodies, 8u, joints, 8u, feet, 4u);
    if (r != WS_OK) {
        return r;
    }
    cg.e = e;
    cg.z = lz + e->terrain.origin_y;
    ws_creature_set_ground(&c, creature_profile_cb, &cg);
    r = ws_quadruped_build(&c, &st, sp, ground0, &parts);
    if (r != WS_OK) {
        return r;
    }
    for (int i = 0; i < 600; ++i) {
        ws_quadruped_balance(&c, parts.torso, 4000.0, 500.0, 0.0);
        ws_creature_step(&c);
    }
    ws_gait_state gs;
    ws_gait_state_init(&gs);
    double speed = ws_species_walk_speed((ws_species_kind)kind);
    int steps = (int)(3.0 / c.dt);
    for (int i = 0; i < steps; ++i) {
        ws_quadruped_walk(&c, &parts, sp, &gs, speed);
        ws_quadruped_balance(&c, parts.torso, 4000.0, 500.0, 0.0);
        ws_creature_step(&c);
    }
    /* Re-seat on the terrain at the requested spot and rotate to the heading. */
    double gy = ws_terrain_height_bilinear(&e->terrain, lx + e->terrain.origin_x,
                                           lz + e->terrain.origin_y);
    double ox = c.bodies[parts.torso].px;
    double oy = c.bodies[parts.torso].py;
    double ch = cos(heading);
    double sh = sin(heading);
    for (uint32_t i = 0; i < c.body_count; ++i) {
        ws_cbody *b = &out->bodies[i];
        double rx = b->px - ox;
        double ry = b->py - oy;
        b->px = lx + rx * ch - ry * sh;
        b->py = gy + (rx * sh + ry * ch);
        b->angle += heading;
    }
    out->body_count = c.body_count;
    out->parts = parts;
    out->z = lz;
    out->species = kind;
    return WS_OK;
}

/* Run the ecosystem headless and pick one survivor per species (the lowest
 * index, for determinism). Returns the number of species represented. The
 * representatives are laid out around the scene's drop point (cre_x, cre_z) so
 * the camera that frames that point sees them. */
static ws_result run_ecosystem(ws_env_scene *e, double cre_x, double cre_z)
{
    ws_eco_config cfg = ws_eco_config_default(0x7ec0u ^ e->terrain.config.seed);
    ws_ecology eco;
    ws_result r = ws_ecology_init(&eco, &cfg);
    if (r != WS_OK) {
        return r;
    }
    /* A fixed simulated interval: enough generations for the population to sort
     * itself out, short enough to keep a scene build snappy. */
    int steps = (int)(600.0 / 0.05);
    for (int i = 0; i < steps; ++i) {
        (void)ws_ecology_step(&eco, 0.05);
    }

    uint32_t placed = 0u;
    for (int s = 0; s < WS_SPECIES_COUNT && placed < WS_ENV_MAX_CREATURES; ++s) {
        int survivor = -1;
        for (uint32_t i = 0; i < eco.alive_count; ++i) {
            if (eco.agents[i].alive && eco.agents[i].species == s) {
                survivor = (int)i;
                break;
            }
        }
        if (survivor < 0) {
            continue; /* species died out; not rendered */
        }
        /* Lay the representatives in a row perpendicular to the camera's
         * horizontal view direction, so all four read across the frame rather
         * than receding along the diagonal. */
        double vx = 1.0, vz = 0.6;         /* camera horizontal view direction */
        double vlen = sqrt(vx * vx + vz * vz);
        double px = vz / vlen, pz = -vx / vlen;   /* screen-right in world xz */
        double off = ((double)placed - 1.5) * 3.6;
        double lx = cre_x - e->terrain.origin_x + px * off;
        double lz = cre_z - e->terrain.origin_y + pz * off;
        /* Face the body toward the camera (opposite the view direction) with a
         * small per-species turn so the row does not look cloned. */
        double heading = atan2(-vz, -vx) + (double)(placed - 1) * 0.25;
        struct ws_rendered_creature *rc = &e->ecosystem_creatures[placed];
        r = build_species_walk(e, s, lx, lz, heading, rc);
        if (r != WS_OK) {
            ws_ecology_free(&eco);
            return r;
        }
        ++placed;
    }
    e->ecosystem_count = placed;
    ws_ecology_free(&eco);
    if (placed == 0u) {
        return WS_ERR_INVALID_STATE;
    }
    return WS_OK;
}

static void add_ecosystem_objects(ws_env_scene *e)
{
    for (uint32_t k = 0; k < e->ecosystem_count; ++k) {
        const struct ws_rendered_creature *rc = &e->ecosystem_creatures[k];
        const ws_quad_parts *p = &rc->parts;
        uint8_t col[3];
        species_color(rc->species, col);
        ws_material body = ws_material_make(col[0] / 255.0, col[1] / 255.0,
                                            col[2] / 255.0, 0.85, 0.0, 0.05);
        ws_material dark = ws_material_make(col[0] / 320.0, col[1] / 320.0,
                                            col[2] / 320.0, 0.85, 0.0, 0.05);
        const double leg_z = 0.12;
        for (uint32_t i = 0; i < rc->body_count; ++i) {
            const ws_cbody *b = &rc->bodies[i];
            int is_leg = ((int)i == p->front_upper || (int)i == p->front_lower ||
                          (int)i == p->rear_upper || (int)i == p->rear_lower);
            ws_material m = is_leg ? dark : body;
            double sx = b->half_len * 2.0;
            double sy = b->half_thick * 2.0;
            double sz = b->half_width * 2.0;
            double base_z = rc->z;
            if (is_leg) {
                for (int side = -1; side <= 1; side += 2) {
                    ws_mat4 t = ws_mat4_translation(
                        ws_vec3_make(b->px, b->py, base_z + side * leg_z));
                    t = ws_mat4_multiply(t, ws_mat4_rotation_z(b->angle));
                    t = ws_mat4_multiply(t, ws_mat4_scale(ws_vec3_make(sx, sy, sz)));
                    (void)ws_scene_add_object(&e->scene, &e->cube_mesh, t, m);
                }
            } else {
                ws_mat4 t = ws_mat4_translation(ws_vec3_make(b->px, b->py, base_z));
                t = ws_mat4_multiply(t, ws_mat4_rotation_z(b->angle));
                t = ws_mat4_multiply(t, ws_mat4_scale(ws_vec3_make(sx, sy, sz)));
                (void)ws_scene_add_object(&e->scene, &e->cube_mesh, t, m);
            }
        }
    }
}

/* Run the P8 world host: stream a loaded set of regions around a focus that
 * walks a short path, so the picture shows a world that has progressed while
 * the camera moved. Record the loaded set as level-tinted tiles and keep the
 * run summary (loads/unloads/extinctions and the replay digest) for evidence. */
static ws_result run_world_stream(ws_env_scene *e, double extent)
{
    ws_world_host_config cfg = ws_world_host_config_default(e->terrain.config.seed,
                                                            e->terrain.config.seed);
    /* The region grid here is coarser than the render region: a region is the
     * scene extent, so the tiles map cleanly onto the frame. */
    cfg.region.cells = (uint32_t)(extent / cfg.region.cell_size);
    ws_world_host host;
    ws_result r = ws_world_host_init(&host, &cfg);
    if (r != WS_OK) {
        return r;
    }
    /* Walk the focus along a fixed path so the streaming and level structure
     * are exercised, then settle so the NEAR region advances every tick. */
    const ws_region_coord path[5] = {
        {0, 0}, {1, 0}, {1, 1}, {0, 1}, {-1, 0}
    };
    for (uint32_t i = 0; i < 5u; ++i) {
        r = ws_world_host_set_focus(&host, path[i]);
        if (r != WS_OK) {
            return r;
        }
        r = ws_world_host_run(&host, 90.0);
        if (r != WS_OK) {
            return r;
        }
    }

    double half = extent * 0.5;
    /* Size the tiles so the whole loaded set spans the render region. */
    double tile = extent / (double)(2u * cfg.radius_load + 1u);
    e->stream_tile_count = 0u;
    for (uint32_t i = 0; i < ws_world_host_loaded_count(&host) &&
                         e->stream_tile_count < WS_ENV_MAX_STREAM_TILES; ++i) {
        const ws_region_slot *s = &host.slots[i];
        struct ws_stream_tile *t = &e->stream_tiles[e->stream_tile_count++];
        /* Region-local offset from the focus region centre, then shifted so the
         * focus region sits at the middle of the scene. */
        double rx = (double)(s->coord.x - host.focus.x);
        double ry = (double)(s->coord.y - host.focus.y);
        t->x = half + rx * tile;
        t->z = half + ry * tile;
        t->size = tile;
        t->level = s->level;
        t->pop = (float)ws_pop_total(&s->pop);
        t->is_focus = ws_region_coord_equal(s->coord, host.focus) ? 1 : 0;
    }
    e->stream_loaded = ws_world_host_loaded_count(&host);
    e->stream_loads = host.total_loads;
    e->stream_unloads = host.total_unloads;
    e->stream_extinctions = host.extinctions;
    e->stream_time_s = host.time_s;
    (void)ws_world_host_digest(&host, e->stream_digest);
    if (e->stream_tile_count == 0u) {
        return WS_ERR_INVALID_STATE;
    }
    return WS_OK;
}

/* One flat tile per loaded region, raised just above the terrain so it reads as
 * a level-tinted map: NEAR bright green, MID amber, FAR dim blue-grey. The focus
 * region is outlined by a brighter tile. */
static void add_world_stream_objects(ws_env_scene *e)
{
    for (uint32_t i = 0; i < e->stream_tile_count; ++i) {
        const struct ws_stream_tile *t = &e->stream_tiles[i];
        double gy = ws_terrain_height_bilinear(&e->terrain, t->x, t->z) + 0.4;
        uint8_t col[3];
        switch (t->level) {
        case WS_LEVEL_NEAR: col[0] = 90u;  col[1] = 170u; col[2] = 90u;  break;
        case WS_LEVEL_MID:  col[0] = 190u; col[1] = 150u; col[2] = 70u;  break;
        default:            col[0] = 90u;  col[1] = 110u; col[2] = 150u; break;
        }
        if (t->is_focus) {
            col[0] = 235u; col[1] = 235u; col[2] = 150u;
        }
        double s = t->size * 0.5;
        ws_material m = ws_material_make(col[0] / 255.0, col[1] / 255.0,
                                         col[2] / 255.0, 0.95, 0.0, 0.02);
        ws_mat4 tr = ws_mat4_translation(ws_vec3_make(t->x, gy, t->z));
        tr = ws_mat4_multiply(tr, ws_mat4_scale(ws_vec3_make(s * 1.9, 0.6, s * 1.9)));
        (void)ws_scene_add_object(&e->scene, &e->cube_mesh, tr, m);
    }
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

    /* Creature: walk the reference quadruped across the flattest dry spot, then
     * frame it where the simulation left it. */
    double cre_x = e->extent * 0.35;
    double cre_z = e->extent * 0.5;
    if (opts->scene_kind == WS_ENV_SCENE_CREATURE ||
        opts->scene_kind == WS_ENV_SCENE_ORGANISM ||
        opts->scene_kind == WS_ENV_SCENE_ECOSYSTEM ||
        opts->scene_kind == WS_ENV_SCENE_WORLD_STREAM) {
        pick_drop_point(e, &cre_x, &cre_z);
        /* Build the shared unit-cube mesh used for every link. */
        size_t vc = ws_mesh_cube_vertex_count();
        size_t ic = ws_mesh_cube_index_count();
        e->cube_verts = (ws_vertex *)calloc(vc, sizeof(ws_vertex));
        e->cube_idx = (uint32_t *)malloc(ic * sizeof(uint32_t));
        if (e->cube_verts == NULL || e->cube_idx == NULL) {
            ws_env_scene_free(e);
            return WS_ERR_OUT_OF_MEMORY;
        }
        const uint8_t cube_color[3] = {150u, 120u, 90u};
        ws_result cr = ws_mesh_make_cube(&e->cube_mesh, e->cube_verts, vc,
                                         e->cube_idx, ic, cube_color);
        if (cr != WS_OK) {
            ws_env_scene_free(e);
            return cr;
        }
        if (opts->scene_kind == WS_ENV_SCENE_CREATURE) {
            double fwd = 0.0;
            cr = run_creature_walk(e, cre_x, cre_z, &fwd);
            if (cr != WS_OK) {
                ws_env_scene_free(e);
                return cr;
            }
            e->creature_z = cre_z - e->terrain.origin_y;
            e->creature_forward_m = fwd;
        } else if (opts->scene_kind == WS_ENV_SCENE_ORGANISM) {
            e->resource_kind = -1;
            cr = run_organism_forage(e, cre_x, cre_z);
            if (cr != WS_OK) {
                ws_env_scene_free(e);
                return cr;
            }
        } else if (opts->scene_kind == WS_ENV_SCENE_WORLD_STREAM) {
            cr = run_world_stream(e, e->extent);
            if (cr != WS_OK) {
                ws_env_scene_free(e);
                return cr;
            }
        } else {
            cr = run_ecosystem(e, cre_x, cre_z);
            if (cr != WS_OK) {
                ws_env_scene_free(e);
                return cr;
            }
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
    } else if (opts->scene_kind == WS_ENV_SCENE_ECOSYSTEM) {
        /* Frame the group of representative animals on a raised three-quarter
         * view so all species and the surrounding terrain read together. */
        double cx = cre_x - e->terrain.origin_x;
        double cz = cre_z - e->terrain.origin_y;
        double cy = ws_terrain_height_bilinear(&e->terrain, cre_x, cre_z);
        double back = 11.0;
        double up = 4.5;
        eye_x = cx - back;
        eye_y = cy + up;
        eye_z = cz - back * 0.6;
        tgt_x = cx;
        tgt_y = cy + 0.9;
        tgt_z = cz;
    } else if (opts->scene_kind == WS_ENV_SCENE_WORLD_STREAM) {
        /* High three-quarter view of the whole loaded set so the near/mid/far
         * level structure and the focus tile all read at once. */
        double mid = e->extent * 0.5;
        double cy = ws_terrain_height_bilinear(&e->terrain, mid, mid);
        eye_x = mid - e->extent * 0.55;
        eye_y = cy + e->extent * 0.95;
        eye_z = mid - e->extent * 0.75;
        tgt_x = mid;
        tgt_y = cy;
        tgt_z = mid;
    } else if (opts->scene_kind == WS_ENV_SCENE_CREATURE ||
               opts->scene_kind == WS_ENV_SCENE_ORGANISM) {
        /* Side-on view of the walking animal: the sagittal gait reads best from
         * the side, so the camera looks across the direction of travel and is
         * framed on the middle of the body. For the behaviour scene the frame is
         * widened a little and biased toward the acted-on resource so both the
         * animal and the resource it chose are visible. */
        const ws_cbody *torso = &e->creature_bodies[e->creature_parts.torso];
        double rx = torso->px - e->terrain.origin_x;
        double rz = cre_z - e->terrain.origin_y;
        double back = 4.2;
        double up = 1.6;
        eye_x = rx + 0.2;
        eye_y = torso->py + up;
        eye_z = rz - back;
        tgt_x = rx + 0.3;
        tgt_y = torso->py - 0.15;
        tgt_z = rz;
        if (opts->scene_kind == WS_ENV_SCENE_ORGANISM) {
            /* Look from a slightly raised angle toward the resource marker. */
            tgt_x = (torso->px - e->terrain.origin_x) * 0.4 + e->resource_x * 0.6;
            tgt_z = rz * 0.4 + e->resource_z * 0.6;
            back = 6.0;
            up = 2.4;
            eye_x = tgt_x - 1.5;
            eye_y = torso->py + up;
            eye_z = tgt_z - back;
        }
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

    /* Reference creature: the articulated body where the gait controller left it. */
    if (opts->scene_kind == WS_ENV_SCENE_CREATURE) {
        add_creature_objects(e);
    }

    /* Behaviour organism: the animal where its decision took it, plus the
     * resource marker it acted on. */
    if (opts->scene_kind == WS_ENV_SCENE_ORGANISM) {
        add_organism_objects(e);
    }

    /* Ecosystem: one representative body per surviving species. */
    if (opts->scene_kind == WS_ENV_SCENE_ECOSYSTEM) {
        add_ecosystem_objects(e);
    }

    /* World stream: the loaded region set as level-tinted tiles. */
    if (opts->scene_kind == WS_ENV_SCENE_WORLD_STREAM) {
        add_world_stream_objects(e);
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
    free(e->cube_verts);
    free(e->cube_idx);
    free(e->objects);
    ws_vegetation_free(&e->veg);
    ws_habitat_free(&e->habitat);
    ws_hydro_free(&e->hydro);
    ws_terrain_free(&e->terrain);
    memset(e, 0, sizeof(*e));
}
