/* Worldseed Engine — environmental rigid-body physics. */
#include "ws_physics.h"

#include <math.h>
#include <string.h>

/* Default physics step: 1/200 s. Chosen well below the contact timescale of
 * the reference bodies and independent of the render frame rate (docs/05). */
#define WS_PHYSICS_DEFAULT_DT 0.005

ws_physics_config ws_physics_config_default(void)
{
    ws_physics_config c;
    c.dt = WS_PHYSICS_DEFAULT_DT;
    c.solver_iterations = 4;
    c.gravity = ws_vec3_make(0.0, -WS_GRAVITY, 0.0);
    c.wind = ws_vec3_make(0.0, 0.0, 0.0);
    c.air_density = WS_AIR_DENSITY_SEA_LEVEL;
    c.water_level = -1.0e30; /* no water by default */
    c.water_density = WS_WATER_DENSITY;
    return c;
}

ws_result ws_physics_world_init(ws_physics_world *world,
                                const ws_physics_config *config,
                                ws_body *storage, uint32_t capacity)
{
    if (world == NULL || config == NULL || storage == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (!(config->dt > 0.0) || config->solver_iterations < 1) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    memset(world, 0, sizeof(*world));
    world->config = *config;
    world->bodies = storage;
    world->capacity = capacity;
    return WS_OK;
}

ws_result ws_physics_world_add(ws_physics_world *world, const ws_body *body)
{
    if (world == NULL || body == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (world->count >= world->capacity) {
        return WS_ERR_CAPACITY;
    }
    ws_body copy = *body;
    /* Derive the cached inverse mass so a static body (mass 0) is well defined
     * and the integration never divides by zero. */
    copy.inv_mass = (copy.mass > 0.0) ? 1.0 / copy.mass : 0.0;
    world->bodies[world->count++] = copy;
    return WS_OK;
}

void ws_physics_world_set_terrain(ws_physics_world *world,
                                  ws_height_fn fn, void *ctx)
{
    if (world == NULL) {
        return;
    }
    world->height_fn = fn;
    world->height_ctx = ctx;
}

void ws_body_apply_impulse(ws_body *body, ws_vec3 impulse)
{
    if (body == NULL || body->inv_mass <= 0.0) {
        return;
    }
    body->velocity = ws_vec3_add(body->velocity, ws_vec3_scale(impulse, body->inv_mass));
}

double ws_physics_kinetic_energy(const ws_physics_world *world)
{
    if (world == NULL) {
        return 0.0;
    }
    double e = 0.0;
    for (uint32_t i = 0; i < world->count; ++i) {
        const ws_body *b = &world->bodies[i];
        if (b->inv_mass <= 0.0) {
            continue;
        }
        e += 0.5 * b->mass * ws_vec3_dot(b->velocity, b->velocity);
    }
    return e;
}

ws_vec3 ws_physics_momentum(const ws_physics_world *world)
{
    ws_vec3 p = ws_vec3_make(0.0, 0.0, 0.0);
    if (world == NULL) {
        return p;
    }
    for (uint32_t i = 0; i < world->count; ++i) {
        const ws_body *b = &world->bodies[i];
        if (b->inv_mass <= 0.0) {
            continue;
        }
        p = ws_vec3_add(p, ws_vec3_scale(b->velocity, b->mass));
    }
    return p;
}

/* Height-field gradient by central differences in metres per metre. */
static ws_vec3 terrain_normal(ws_physics_world *world, double x, double y)
{
    const double h = 0.5; /* sample offset, metres */
    double z0 = world->height_fn(world->height_ctx, x - h, y);
    double z1 = world->height_fn(world->height_ctx, x + h, y);
    double w0 = world->height_fn(world->height_ctx, x, y - h);
    double w1 = world->height_fn(world->height_ctx, x, y + h);
    double dhdx = (z1 - z0) / (2.0 * h);
    double dhdy = (w1 - w0) / (2.0 * h);
    /* Surface z = f(x, y) has normal (-df/dx, 1, -df/dy). */
    return ws_vec3_normalize(ws_vec3_make(-dhdx, 1.0, -dhdy));
}

/* Apply environmental body forces: aerodynamic drag and buoyancy. Both act
 * relative to the medium and are documented approximations (docs/physics). */
static void apply_environmental_forces(ws_physics_world *world, ws_body *b)
{
    const ws_physics_config *c = &world->config;
    if (b->inv_mass <= 0.0) {
        return;
    }
    if (b->drag_area > 0.0) {
        ws_vec3 rel = ws_vec3_sub(b->velocity, c->wind);
        double speed = ws_vec3_length(rel);
        if (speed > 1e-9) {
            /* F = -0.5 * rho * Cd * A * |v| * v  (drag_area = 0.5*Cd*A). */
            double k = c->air_density * b->drag_area * speed;
            b->force = ws_vec3_sub(b->force, ws_vec3_scale(rel, k));
        }
    }
    if (b->volume > 0.0 && b->position.y < c->water_level) {
        /* Fully submerged approximation: buoyant force = rho * V * g, upward. */
        double fb = c->water_density * b->volume * (-c->gravity.y);
        b->force = ws_vec3_add(b->force, ws_vec3_make(0.0, fb, 0.0));
    }
}

/* Resolve a body against the terrain height field and against the world
 * boundary planes. Impulse-based with restitution and Coulomb friction. */
static void resolve_terrain_contact(ws_physics_world *world, ws_body *b)
{
    b->grounded = 0;
    if (world->height_fn == NULL || b->inv_mass <= 0.0) {
        return;
    }
    double gy = world->height_fn(world->height_ctx, b->position.x, b->position.z);
    double penetration = (gy + b->radius) - b->position.y;
    /* A small contact band so a body resting exactly on the surface (where the
     * positional correction has driven penetration to zero) still reports as
     * grounded instead of flickering between contact and free fall. */
    if (penetration < -1e-6) {
        return;
    }
    b->grounded = 1;
    if (penetration <= 0.0) {
        return; /* in contact band but not penetrating: nothing to correct */
    }
    ws_vec3 n = terrain_normal(world, b->position.x, b->position.z);

    /* Remove the vertical penetration directly so the resting height is
     * exactly gy + radius and no slope-induced drift appears. The surface
     * normal is used only for the velocity response below. */
    b->position.y += penetration;

    double vn = ws_vec3_dot(b->velocity, n);
    if (vn < 0.0) {
        ws_vec3 vn_vec = ws_vec3_scale(n, vn);
        ws_vec3 vt = ws_vec3_sub(b->velocity, vn_vec);
        /* Normal response with restitution (clamped to a valid range). */
        double e = ws_clamp_double(b->restitution, 0.0, 1.0);
        vn = -e * vn;
        /* Coulomb friction opposes the tangential velocity; mu * |dn| limit. */
        double vt_len = ws_vec3_length(vt);
        double max_dv = b->friction * fabs(vn - ws_vec3_dot(b->velocity, n));
        double dv = vt_len < max_dv ? vt_len : max_dv;
        if (vt_len > 1e-12) {
            ws_vec3 vt_dir = ws_vec3_scale(vt, 1.0 / vt_len);
            vt = ws_vec3_sub(vt, ws_vec3_scale(vt_dir, dv));
        }
        b->velocity = ws_vec3_add(vt, ws_vec3_scale(n, vn));
    }
}

/* Resolve a moving body against a static body (sphere-sphere). Used for the
 * "drop onto a fixed sphere" reference and, later, creature-vs-world contacts. */
static void resolve_body_pair(ws_body *a, ws_body *b)
{
    ws_vec3 d = ws_vec3_sub(b->position, a->position);
    double dist = ws_vec3_length(d);
    double rsum = a->radius + b->radius;
    if (dist >= rsum || dist < 1e-12) {
        return;
    }
    double inv_sum = a->inv_mass + b->inv_mass;
    if (inv_sum <= 0.0) {
        return;
    }
    ws_vec3 n = ws_vec3_scale(d, 1.0 / dist);
    double penetration = rsum - dist;
    double corr = penetration / inv_sum;
    a->position = ws_vec3_sub(a->position, ws_vec3_scale(n, corr * a->inv_mass));
    b->position = ws_vec3_add(b->position, ws_vec3_scale(n, corr * b->inv_mass));

    ws_vec3 rel = ws_vec3_sub(b->velocity, a->velocity);
    double vn = ws_vec3_dot(rel, n);
    if (vn >= 0.0) {
        return;
    }
    double e = ws_clamp_double(a->restitution < b->restitution ? a->restitution
                                                              : b->restitution,
                               0.0, 1.0);
    double jn = -(1.0 + e) * vn / inv_sum;
    ws_vec3 impulse = ws_vec3_scale(n, jn);
    a->velocity = ws_vec3_sub(a->velocity, ws_vec3_scale(impulse, a->inv_mass));
    b->velocity = ws_vec3_add(b->velocity, ws_vec3_scale(impulse, b->inv_mass));

    /* Tangential friction impulse along the relative tangent. */
    ws_vec3 tangent = ws_vec3_sub(rel, ws_vec3_scale(n, vn));
    double tlen = ws_vec3_length(tangent);
    if (tlen > 1e-12) {
        ws_vec3 tdir = ws_vec3_scale(tangent, 1.0 / tlen);
        double mu = a->friction < b->friction ? a->friction : b->friction;
        double jt = -tlen / inv_sum;
        double max_jt = mu * jn;
        if (jt < -max_jt) {
            jt = -max_jt;
        }
        ws_vec3 fimp = ws_vec3_scale(tdir, jt);
        a->velocity = ws_vec3_sub(a->velocity, ws_vec3_scale(fimp, a->inv_mass));
        b->velocity = ws_vec3_add(b->velocity, ws_vec3_scale(fimp, b->inv_mass));
    }
}

ws_result ws_physics_step(ws_physics_world *world)
{
    if (world == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    const double dt = world->config.dt;

    /* 1. Accumulate forces: gravity plus environment. */
    for (uint32_t i = 0; i < world->count; ++i) {
        ws_body *b = &world->bodies[i];
        if (b->inv_mass <= 0.0) {
            continue;
        }
        b->force = ws_vec3_scale(world->config.gravity, b->mass);
        apply_environmental_forces(world, b);
    }

    /* 2. Integrate velocities and positions (semi-implicit Euler). */
    for (uint32_t i = 0; i < world->count; ++i) {
        ws_body *b = &world->bodies[i];
        if (b->inv_mass <= 0.0) {
            continue;
        }
        ws_vec3 accel = ws_vec3_scale(b->force, b->inv_mass);
        b->velocity = ws_vec3_add(b->velocity, ws_vec3_scale(accel, dt));
        b->position = ws_vec3_add(b->position, ws_vec3_scale(b->velocity, dt));
    }

    /* 3. Contact solve: iterate so stacked/resting contacts settle. */
    for (int it = 0; it < world->config.solver_iterations; ++it) {
        for (uint32_t i = 0; i < world->count; ++i) {
            resolve_terrain_contact(world, &world->bodies[i]);
        }
        for (uint32_t i = 0; i < world->count; ++i) {
            for (uint32_t j = i + 1; j < world->count; ++j) {
                resolve_body_pair(&world->bodies[i], &world->bodies[j]);
            }
        }
    }
    return WS_OK;
}

ws_result ws_physics_advance(ws_physics_world *world, uint64_t steps)
{
    if (world == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    for (uint64_t s = 0; s < steps; ++s) {
        ws_result r = ws_physics_step(world);
        if (r != WS_OK) {
            return r;
        }
    }
    return WS_OK;
}
