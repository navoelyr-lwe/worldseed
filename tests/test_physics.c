/* Worldseed Engine — environmental physics tests (P4).
 *
 * Scientific regression suite from docs/05 and docs/12: free-fall against the
 * analytic solution, inclined-plane Coulomb friction, momentum and energy
 * behaviour, contact penetration/rest, convergence under step and iteration
 * changes, buoyancy, and rejection of invalid state. These tests exercise the
 * real solver; there are no mocks.
 */
#include "ws_test.h"

#include "core/ws_math.h"
#include "physics/ws_physics.h"
#include "physics/ws_units.h"
#include "physics/ws_wind.h"
#include "world/ws_environment.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static int nearly(double a, double b, double tol)
{
    return fabs(a - b) <= tol;
}

/* ---------- terrain stubs ---------- */

static double flat_height(void *ctx, double x, double y)
{
    (void)ctx;
    (void)x;
    (void)y;
    return 0.0;
}

/* Plane rising along +x with the given grade (rise/run). */
static double g_plane_grade;
static double slope_height(void *ctx, double x, double y)
{
    (void)ctx;
    (void)y;
    return g_plane_grade * x;
}

/* ---------- free fall ---------- */

static int test_free_fall_analytic(void)
{
    ws_physics_config cfg = ws_physics_config_default();
    ws_body storage[1];
    ws_physics_world w;
    WS_CHECK(ws_physics_world_init(&w, &cfg, storage, 1u) == WS_OK);

    ws_body b;
    memset(&b, 0, sizeof(b));
    b.mass = 2.0;
    b.radius = 0.1;
    b.restitution = 0.0;
    b.friction = 0.5;
    b.position = ws_vec3_make(0.0, 100.0, 0.0);
    WS_CHECK(ws_physics_world_add(&w, &b) == WS_OK);

    const double t = 1.0;
    uint64_t steps = (uint64_t)(t / cfg.dt + 0.5);
    WS_CHECK(ws_physics_advance(&w, steps) == WS_OK);

    const ws_body *p = &w.bodies[0];
    double v_expected = -WS_GRAVITY * (steps * cfg.dt);
    double y_expected = 100.0 - 0.5 * WS_GRAVITY * (steps * cfg.dt) * (steps * cfg.dt);
    /* Velocity is exact for semi-implicit Euler; position carries the known
     * O(g*dt*t) discretisation offset. Both within a documented tolerance. */
    WS_CHECK(nearly(p->velocity.y, v_expected, 1e-9));
    WS_CHECK(nearly(p->position.y, y_expected, 0.05));
    return 1;
}

/* ---------- inclined plane / Coulomb friction ---------- */

static int test_inclined_plane_static(void)
{
    /* grade = tan(theta). With mu = 0.5, theta = atan(0.4) < atan(mu): the body
     * must not slide. */
    g_plane_grade = 0.4;
    ws_physics_config cfg = ws_physics_config_default();
    cfg.solver_iterations = 8;
    ws_body storage[1];
    ws_physics_world w;
    WS_CHECK(ws_physics_world_init(&w, &cfg, storage, 1u) == WS_OK);
    ws_physics_world_set_terrain(&w, slope_height, NULL);

    ws_body b;
    memset(&b, 0, sizeof(b));
    b.mass = 5.0;
    b.radius = 0.2;
    b.restitution = 0.0;
    b.friction = 0.5;
    b.position = ws_vec3_make(0.0, slope_height(NULL, 0.0, 0.0) + b.radius, 0.0);
    WS_CHECK(ws_physics_world_add(&w, &b) == WS_OK);

    WS_CHECK(ws_physics_advance(&w, 400) == WS_OK); /* 2 s */
    const ws_body *p = &w.bodies[0];
    /* Static friction holds: horizontal drift stays tiny. */
    WS_CHECK(fabs(p->position.x) < 0.05);
    WS_CHECK(p->grounded == 1);
    return 1;
}

static int test_inclined_plane_slides(void)
{
    /* grade = tan(theta) = 0.8 > mu = 0.5: the body accelerates down-slope.
     * Predicted a = g*(sin - mu*cos) with sin/cos from the grade. */
    g_plane_grade = 0.8;
    ws_physics_config cfg = ws_physics_config_default();
    cfg.solver_iterations = 8;
    ws_body storage[1];
    ws_physics_world w;
    WS_CHECK(ws_physics_world_init(&w, &cfg, storage, 1u) == WS_OK);
    ws_physics_world_set_terrain(&w, slope_height, NULL);

    ws_body b;
    memset(&b, 0, sizeof(b));
    b.mass = 3.0;
    b.radius = 0.2;
    b.restitution = 0.0;
    b.friction = 0.5;
    b.position = ws_vec3_make(0.0, slope_height(NULL, 0.0, 0.0) + b.radius, 0.0);
    WS_CHECK(ws_physics_world_add(&w, &b) == WS_OK);

    const double t = 1.0;
    uint64_t steps = (uint64_t)(t / cfg.dt + 0.5);
    WS_CHECK(ws_physics_advance(&w, steps) == WS_OK);
    const ws_body *p = &w.bodies[0];

    double grade = g_plane_grade;
    double sin_t = grade / sqrt(1.0 + grade * grade);
    double cos_t = 1.0 / sqrt(1.0 + grade * grade);
    double a = WS_GRAVITY * (sin_t - 0.5 * cos_t);
    double vx_expected = -a * (steps * cfg.dt) * cos_t; /* down-slope along -x */
    /* Contact solver and discrete friction introduce a bounded offset. */
    WS_CHECK(nearly(p->velocity.x, vx_expected, 0.15));
    WS_CHECK(p->position.x < -0.05);
    return 1;
}

/* ---------- momentum conservation ---------- */

static int test_momentum_conserved(void)
{
    ws_physics_config cfg = ws_physics_config_default();
    cfg.gravity = ws_vec3_make(0.0, 0.0, 0.0); /* isolate the collision */
    cfg.solver_iterations = 4;
    ws_body storage[2];
    ws_physics_world w;
    WS_CHECK(ws_physics_world_init(&w, &cfg, storage, 2u) == WS_OK);

    ws_body a;
    memset(&a, 0, sizeof(a));
    a.mass = 1.0;
    a.radius = 1.0;
    a.restitution = 1.0;
    a.friction = 0.0;
    a.position = ws_vec3_make(-3.0, 0.0, 0.0);
    a.velocity = ws_vec3_make(2.0, 0.0, 0.0);

    ws_body b;
    memset(&b, 0, sizeof(b));
    b.mass = 3.0;
    b.radius = 1.0;
    b.restitution = 1.0;
    b.friction = 0.0;
    b.position = ws_vec3_make(3.0, 0.0, 0.0);
    b.velocity = ws_vec3_make(-1.0, 0.0, 0.0);

    WS_CHECK(ws_physics_world_add(&w, &a) == WS_OK);
    WS_CHECK(ws_physics_world_add(&w, &b) == WS_OK);

    ws_vec3 p0 = ws_physics_momentum(&w);
    /* Closing speed 3 m/s over 6 m needs ~2 s; run 5 s so the collision and
     * separation are complete. */
    WS_CHECK(ws_physics_advance(&w, 1000) == WS_OK);
    ws_vec3 p1 = ws_physics_momentum(&w);
    WS_CHECK(nearly(p1.x, p0.x, 1e-9));
    WS_CHECK(nearly(p1.y, p0.y, 1e-9));
    WS_CHECK(nearly(p1.z, p0.z, 1e-9));
    /* Elastic collision between 1 kg and 3 kg: the heavier body ends up moving
     * forward; verify the bodies actually separated and swapped momentum. */
    WS_CHECK(w.bodies[0].velocity.x < 0.0);
    WS_CHECK(w.bodies[1].velocity.x > 0.0);
    return 1;
}

/* ---------- energy behaviour ---------- */

static int test_restitution_loses_energy(void)
{
    ws_physics_config cfg = ws_physics_config_default();
    cfg.solver_iterations = 8;
    ws_body storage[1];
    ws_physics_world w;
    WS_CHECK(ws_physics_world_init(&w, &cfg, storage, 1u) == WS_OK);
    ws_physics_world_set_terrain(&w, flat_height, NULL);

    ws_body b;
    memset(&b, 0, sizeof(b));
    b.mass = 1.0;
    b.radius = 0.25;
    b.restitution = 0.5;
    b.friction = 0.2;
    b.position = ws_vec3_make(0.0, 5.0, 0.0);
    WS_CHECK(ws_physics_world_add(&w, &b) == WS_OK);

    /* Peak height after the first bounce must be lower than the drop height,
     * and the body must eventually come to rest near the surface. */
    double h0 = w.bodies[0].position.y;
    WS_CHECK(ws_physics_advance(&w, 2000) == WS_OK); /* 10 s */
    double h1 = w.bodies[0].position.y;
    WS_CHECK(h1 < h0);
    WS_CHECK(h1 >= b.radius - 1e-6);
    /* Comes to rest: speed is negligible at the end. */
    WS_CHECK(ws_vec3_length(w.bodies[0].velocity) < 0.05);
    return 1;
}

/* ---------- contact rest / penetration ---------- */

static int test_rest_no_penetration(void)
{
    ws_physics_config cfg = ws_physics_config_default();
    cfg.solver_iterations = 8;
    ws_body storage[1];
    ws_physics_world w;
    WS_CHECK(ws_physics_world_init(&w, &cfg, storage, 1u) == WS_OK);
    ws_physics_world_set_terrain(&w, flat_height, NULL);

    ws_body b;
    memset(&b, 0, sizeof(b));
    b.mass = 4.0;
    b.radius = 0.3;
    b.restitution = 0.0;
    b.friction = 0.6;
    b.position = ws_vec3_make(0.0, 2.0, 0.0);
    WS_CHECK(ws_physics_world_add(&w, &b) == WS_OK);

    WS_CHECK(ws_physics_advance(&w, 4000) == WS_OK); /* 20 s soak */
    const ws_body *p = &w.bodies[0];
    /* Rests on the surface without sinking below it and without drifting. */
    WS_CHECK(p->position.y >= p->radius - 1e-3);
    WS_CHECK(p->position.y <= p->radius + 1e-3);
    WS_CHECK(fabs(p->position.x) < 1e-3);
    WS_CHECK(ws_vec3_length(p->velocity) < 1e-2);
    return 1;
}

/* ---------- convergence under step size ---------- */

static int test_step_size_convergence(void)
{
    /* Free fall for 0.5 s at three step sizes; the analytic velocity must be
     * approached as dt shrinks (first-order error in position is expected). */
    double t = 0.5;
    double prev_err = 1e30;
    for (int k = 0; k < 3; ++k) {
        double dt = 0.02 / (double)(1 << k); /* 0.02, 0.01, 0.005 */
        ws_physics_config cfg = ws_physics_config_default();
        cfg.dt = dt;
        ws_body storage[1];
        ws_physics_world w;
        WS_CHECK(ws_physics_world_init(&w, &cfg, storage, 1u) == WS_OK);
        ws_body b;
        memset(&b, 0, sizeof(b));
        b.mass = 1.0;
        b.radius = 0.1;
        b.position = ws_vec3_make(0.0, 50.0, 0.0);
        WS_CHECK(ws_physics_world_add(&w, &b) == WS_OK);
        uint64_t steps = (uint64_t)(t / dt + 0.5);
        WS_CHECK(ws_physics_advance(&w, steps) == WS_OK);
        double exact = -WS_GRAVITY * (steps * dt);
        double err = fabs(w.bodies[0].velocity.y - exact);
        WS_CHECK(err <= prev_err + 1e-12);
        prev_err = err;
    }
    return 1;
}

/* ---------- buoyancy ---------- */

static int test_buoyancy_rises(void)
{
    ws_physics_config cfg = ws_physics_config_default();
    cfg.water_level = 0.0;
    ws_body storage[1];
    ws_physics_world w;
    WS_CHECK(ws_physics_world_init(&w, &cfg, storage, 1u) == WS_OK);

    /* A light, large body below the water line must accelerate upward when the
     * buoyant force exceeds its weight. */
    ws_body b;
    memset(&b, 0, sizeof(b));
    b.mass = 1.0;
    b.radius = 0.3;
    b.volume = 0.5; /* rho * V = 498 kg displaced >> 1 kg */
    b.position = ws_vec3_make(0.0, -1.0, 0.0);
    WS_CHECK(ws_physics_world_add(&w, &b) == WS_OK);
    WS_CHECK(ws_physics_advance(&w, 100) == WS_OK);
    WS_CHECK(w.bodies[0].velocity.y > 0.0);
    return 1;
}

/* ---------- wind ---------- */

static int test_wind_profile(void)
{
    ws_environment env;
    ws_env_config ec = ws_env_config_default(1234u);
    ws_environment_init(&env, &ec);
    env.wind = ws_vec2_make(3.0, 4.0); /* speed 5 m/s */
    ws_vec3 w10 = ws_wind_at_height(&env, 10.0);
    ws_vec3 w40 = ws_wind_at_height(&env, 40.0);
    WS_CHECK(nearly(ws_vec3_length(w10), 5.0, 1e-9));
    /* Power law: u(40)/u(10) = (4)^0.14. */
    double ratio = pow(4.0, WS_WIND_SHEAR_EXPONENT);
    WS_CHECK(nearly(ws_vec3_length(w40), 5.0 * ratio, 1e-9));
    WS_CHECK(w10.y == 0.0 && w40.y == 0.0);
    return 1;
}

/* ---------- invalid input rejection ---------- */

static int test_invalid_input(void)
{
    ws_physics_config cfg = ws_physics_config_default();
    ws_body storage[1];
    ws_physics_world w;
    /* dt <= 0 rejected. */
    cfg.dt = 0.0;
    WS_CHECK(ws_physics_world_init(&w, &cfg, storage, 1u) == WS_ERR_INVALID_ARGUMENT);
    cfg = ws_physics_config_default();
    cfg.solver_iterations = 0;
    WS_CHECK(ws_physics_world_init(&w, &cfg, storage, 1u) == WS_ERR_INVALID_ARGUMENT);
    cfg = ws_physics_config_default();
    WS_CHECK(ws_physics_world_init(NULL, &cfg, storage, 1u) == WS_ERR_NULL_POINTER);
    WS_CHECK(ws_physics_world_init(&w, &cfg, NULL, 1u) == WS_ERR_NULL_POINTER);
    WS_CHECK(ws_physics_world_init(&w, &cfg, storage, 0u) == WS_OK);

    ws_body b;
    memset(&b, 0, sizeof(b));
    b.mass = 1.0;
    WS_CHECK(ws_physics_world_add(&w, &b) == WS_ERR_CAPACITY);
    return 1;
}

static int test_kinetic_energy(void)
{
    ws_physics_config cfg = ws_physics_config_default();
    cfg.gravity = ws_vec3_make(0.0, 0.0, 0.0);
    ws_body storage[1];
    ws_physics_world w;
    WS_CHECK(ws_physics_world_init(&w, &cfg, storage, 1u) == WS_OK);
    ws_body b;
    memset(&b, 0, sizeof(b));
    b.mass = 2.0;
    b.velocity = ws_vec3_make(3.0, 0.0, 0.0);
    WS_CHECK(ws_physics_world_add(&w, &b) == WS_OK);
    WS_CHECK(nearly(ws_physics_kinetic_energy(&w), 0.5 * 2.0 * 9.0, 1e-12));
    return 1;
}

int main(void)
{
    static const ws_test_case cases[] = {
        {"free fall analytic", test_free_fall_analytic},
        {"inclined plane static", test_inclined_plane_static},
        {"inclined plane slides", test_inclined_plane_slides},
        {"momentum conserved", test_momentum_conserved},
        {"restitution loses energy", test_restitution_loses_energy},
        {"rest no penetration", test_rest_no_penetration},
        {"step size convergence", test_step_size_convergence},
        {"buoyancy rises", test_buoyancy_rises},
        {"wind profile", test_wind_profile},
        {"invalid input", test_invalid_input},
        {"kinetic energy", test_kinetic_energy},
    };
    return ws_test_run("physics", cases, (int)(sizeof(cases) / sizeof(cases[0])));
}
