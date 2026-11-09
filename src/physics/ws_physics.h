/* Worldseed Engine — environmental rigid-body physics.
 *
 * A compact, deterministic, headless physics core for the environmental
 * processes named in docs/05: gravity, rigid-body kinematics/dynamics, sphere
 * contact, Coulomb friction and restitution, and environmental forces (wind
 * drag, buoyancy). It is deliberately not a general-purpose scientific solver;
 * each sub-model states its scope in the model cards (docs/physics).
 *
 * Integration: semi-implicit (symplectic) Euler at a fixed time step with a
 * fixed number of solver iterations. The step is decoupled from the render
 * frame rate: callers advance the world by whole fixed steps only. This keeps
 * energy behaviour bounded and replay deterministic (docs/05, docs/12).
 *
 * All quantities are SI: position metres, velocity m/s, mass kg, force N,
 * energy J. State storage is caller-owned via an explicit array so the module
 * allocates nothing on the hot path and is easy to snapshot.
 */
#ifndef WS_PHYSICS_H
#define WS_PHYSICS_H

#include <stddef.h>
#include <stdint.h>

#include "../core/ws_math.h"
#include "../core/ws_result.h"
#include "ws_units.h"

/* Bump when integration or contact changes so saved worlds are detected. */
#define WS_PHYSICS_VERSION 1u

/* A body is a sphere: the simplest collision proxy that still exercises
 * contact, friction and restitution. Multi-segment bodies for creatures are
 * built later (P5) on top of this primitive. */
typedef struct ws_body {
    double mass;       /* kg; 0 marks a static body */
    double inv_mass;   /* 1/kg; 0 for static */
    ws_vec3 position;  /* m */
    ws_vec3 velocity;  /* m/s */
    ws_vec3 force;     /* N accumulator, cleared each step */
    double radius;     /* m, collision sphere radius */
    double restitution;/* 0 (inelastic) .. 1 (elastic) */
    double friction;   /* Coulomb coefficient mu >= 0 */
    double drag_area;  /* lumped 0.5 * Cd * A, m^2; 0 disables drag */
    double volume;     /* m^3; used for buoyancy */
    int grounded;      /* set by the last contact pass */
} ws_body;

/* Height-field query for terrain contact. Returns height in metres at world
 * (x, y); the normal is filled from central differences of the same field.
 * Passing a NULL height_fn disables terrain contact. */
typedef double (*ws_height_fn)(void *ctx, double x, double y);

typedef struct ws_physics_config {
    double dt;             /* fixed step, seconds (must be > 0) */
    int solver_iterations; /* contact iterations per step (>= 1) */
    ws_vec3 gravity;       /* m/s^2, typically (0, -g, 0) */
    ws_vec3 wind;          /* m/s, horizontal wind velocity */
    double air_density;    /* kg/m^3 */
    double water_level;    /* absolute metres; buoyancy below this */
    double water_density;  /* kg/m^3 */
} ws_physics_config;

typedef struct ws_physics_world {
    ws_physics_config config;
    ws_body *bodies;
    uint32_t count;
    uint32_t capacity;
    ws_height_fn height_fn;
    void *height_ctx;
} ws_physics_world;

/* Default configuration: standard gravity, still air, sea-level densities. */
ws_physics_config ws_physics_config_default(void);

/* Initialise a world over caller-owned body storage. */
ws_result ws_physics_world_init(ws_physics_world *world,
                                const ws_physics_config *config,
                                ws_body *storage, uint32_t capacity);

/* Append a body. Returns WS_ERR_CAPACITY when full. */
ws_result ws_physics_world_add(ws_physics_world *world, const ws_body *body);

/* Set the terrain height function used for ground contact. */
void ws_physics_world_set_terrain(ws_physics_world *world,
                                  ws_height_fn fn, void *ctx);

/* Advance exactly one fixed step: apply gravity and accumulated forces,
 * integrate, then resolve contacts. */
ws_result ws_physics_step(ws_physics_world *world);

/* Advance `steps` fixed steps (steps == 0 is a no-op). */
ws_result ws_physics_advance(ws_physics_world *world, uint64_t steps);

/* Total kinetic energy (J) and linear momentum (kg*m/s) over all bodies. */
double ws_physics_kinetic_energy(const ws_physics_world *world);
ws_vec3 ws_physics_momentum(const ws_physics_world *world);

/* Add an impulse (N*s) to a body's velocity, respecting static bodies. */
void ws_body_apply_impulse(ws_body *body, ws_vec3 impulse);

#endif /* WS_PHYSICS_H */
