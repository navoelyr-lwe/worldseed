/* Worldseed Engine — reference quadruped builder. */
#include "ws_quadruped.h"

#include <math.h>
#include <string.h>

#define WS_PI_D 3.14159265358979323846

ws_species_profile ws_species_red_deer(void)
{
    ws_species_profile p;
    p.name = "red deer (Cervus elaphus)";
    p.total_mass = 110.0;
    p.body_length = 1.6;
    p.shoulder_h = 1.05;
    p.leg_upper = 0.60;
    p.leg_lower = 0.60;
    p.stance = 0.80;
    p.gait_freq = 1.4;
    p.duty_factor = 0.70;
    p.friction = 0.85;
    return p;
}

/* Place a link whose proximal joint is at world (px, py) and whose length axis
 * points at angle `a`; fills centre position and angle. The proximal anchor is
 * at local (-half_len, 0). */
static void place_link(ws_cbody *b, double px, double py, double a)
{
    b->angle = a;
    b->px = px + b->half_len * cos(a);
    b->py = py + b->half_len * sin(a);
    b->vx = 0.0;
    b->vy = 0.0;
    b->av = 0.0;
    b->extra_torque = 0.0;
}

/* Two-link planar IK: place an upper and lower link so the distal end reaches
 * (foot_x, foot_y) from the hip at (hip_x, hip_y). `bend` selects the elbow
 * side (+1/-1). A slight bend at rest avoids the fully-straight singularity,
 * which is what makes an extended leg unable to resist horizontal load. */
static void place_leg_ik(ws_cbody *upper, ws_cbody *lower,
                         double hip_x, double hip_y,
                         double foot_x, double foot_y,
                         double l1, double l2, double bend)
{
    double dx = foot_x - hip_x;
    double dy = foot_y - hip_y;
    double d = sqrt(dx * dx + dy * dy);
    double dmin = fabs(l1 - l2) + 1.0e-3;
    double dmax = l1 + l2 - 1.0e-3;
    if (d < dmin) {
        d = dmin;
    }
    if (d > dmax) {
        d = dmax;
    }
    double base = atan2(dy, dx);
    double ca = (l1 * l1 + d * d - l2 * l2) / (2.0 * l1 * d);
    if (ca > 1.0) {
        ca = 1.0;
    } else if (ca < -1.0) {
        ca = -1.0;
    }
    double alpha = acos(ca);
    double ua = base + bend * alpha;
    double kx = hip_x + l1 * cos(ua);
    double ky = hip_y + l1 * sin(ua);
    double la = atan2(foot_y - ky, foot_x - kx);
    place_link(upper, hip_x, hip_y, ua);
    place_link(lower, kx, ky, la);
}

ws_result ws_quadruped_build(ws_creature *c, ws_quad_storage *st,
                             const ws_species_profile *p, double ground_y,
                             ws_quad_parts *out_parts)
{
    if (c == NULL || st == NULL || p == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    memset(st, 0, sizeof(*st));
    const double H = p->shoulder_h + ground_y;
    const double half_stance = p->stance * 0.5;
    const double l1 = p->leg_upper;
    const double l2 = p->leg_lower;
    const double hl_up = l1 * 0.5;
    const double hl_lo = l2 * 0.5;

    ws_cbody *bd = st->bodies;

    /* Torso (index 0): horizontal, 1.1 m long. */
    const uint8_t torso_c[3] = {120u, 88u, 62u};
    ws_cbody_make_box(&bd[0], 0.55, 0.22, 0.20, 310.0, torso_c);
    bd[0].px = 0.0;
    bd[0].py = H;
    bd[0].angle = 0.0;

    /* Neck (index 1) and head (index 2). */
    const uint8_t neck_c[3] = {132u, 100u, 70u};
    ws_cbody_make_box(&bd[1], 0.25, 0.09, 0.10, 700.0, neck_c);
    const double neck_ang = 0.45; /* forward and up */
    place_link(&bd[1], 0.50, H + 0.10, neck_ang);

    const uint8_t head_c[3] = {140u, 108u, 78u};
    ws_cbody_make_box(&bd[2], 0.15, 0.10, 0.09, 700.0, head_c);
    {
        double nx = bd[1].px + bd[1].half_len * cos(bd[1].angle);
        double ny = bd[1].py + bd[1].half_len * sin(bd[1].angle);
        place_link(&bd[2], nx, ny, neck_ang + 0.15);
    }

    /* Legs (index 3,4 front; 5,6 rear). The shoulder height is a little less
     * than the leg length so the knees are bent at rest; a straight leg sits on
     * the singular pose that cannot resist horizontal load. */
    const uint8_t leg_c[3] = {104u, 74u, 52u};
    ws_cbody_make_box(&bd[3], hl_up, 0.05, 0.05, 1450.0, leg_c);
    ws_cbody_make_box(&bd[4], hl_lo, 0.035, 0.035, 1490.0, leg_c);
    place_leg_ik(&bd[3], &bd[4], half_stance, H, half_stance, ground_y,
                 l1, l2, 1.0);

    ws_cbody_make_box(&bd[5], hl_up, 0.05, 0.05, 1450.0, leg_c);
    ws_cbody_make_box(&bd[6], hl_lo, 0.035, 0.035, 1490.0, leg_c);
    place_leg_ik(&bd[5], &bd[6], -half_stance, H, -half_stance, ground_y,
                 l1, l2, 1.0);

    for (uint32_t i = 0; i < 7u; ++i) {
        uint32_t idx;
        if (ws_creature_add_body(c, &bd[i], &idx) != WS_OK) {
            return WS_ERR_CAPACITY;
        }
    }

    /* Joints. */
    struct jdef {
        int a, b;
        double lax, lay, lbx, lby, lo, hi, kp, kd, tmax;
    };
    const struct jdef jd[6] = {
        /* neck base */
        {0, 1, 0.50, 0.10, -0.25, 0.0, -0.5, 0.5, 600.0, 60.0, 250.0},
        /* head */
        {1, 2, 0.25, 0.0, -0.15, 0.0, -0.6, 0.6, 200.0, 20.0, 60.0},
        /* front hip */
        {0, 3, half_stance, 0.0, -hl_up, 0.0, -0.8, 0.8, 6000.0, 600.0, 3000.0},
        /* front knee */
        {3, 4, hl_up, 0.0, -hl_lo, 0.0, -1.2, 0.15, 4000.0, 400.0, 2000.0},
        /* rear hip */
        {0, 5, -half_stance, 0.0, -hl_up, 0.0, -0.8, 0.8, 6000.0, 600.0, 3000.0},
        /* rear knee */
        {5, 6, hl_up, 0.0, -hl_lo, 0.0, -1.2, 0.15, 4000.0, 400.0, 2000.0},
    };
    for (int i = 0; i < 6; ++i) {
        ws_cjoint j;
        memset(&j, 0, sizeof(j));
        j.a = jd[i].a;
        j.b = jd[i].b;
        j.lax = jd[i].lax;
        j.lay = jd[i].lay;
        j.lbx = jd[i].lbx;
        j.lby = jd[i].lby;
        j.min_angle = jd[i].lo;
        j.max_angle = jd[i].hi;
        j.motor_enabled = 1;
        j.motor_target = 0.0;
        j.motor_max_torque = jd[i].tmax;
        j.motor_kp = jd[i].kp;
        j.motor_kd = jd[i].kd;
        if (ws_creature_add_joint(c, &j) != WS_OK) {
            return WS_ERR_CAPACITY;
        }
    }

    /* Feet: the distal end of each lower leg. */
    ws_cfoot f;
    memset(&f, 0, sizeof(f));
    f.body = 4;
    f.lx = hl_lo;
    f.ly = 0.0;
    f.friction = p->friction;
    if (ws_creature_add_foot(c, &f) != WS_OK) {
        return WS_ERR_CAPACITY;
    }
    f.body = 6;
    if (ws_creature_add_foot(c, &f) != WS_OK) {
        return WS_ERR_CAPACITY;
    }

    if (out_parts != NULL) {
        out_parts->torso = 0;
        out_parts->neck = 1;
        out_parts->head = 2;
        out_parts->front_upper = 3;
        out_parts->front_lower = 4;
        out_parts->rear_upper = 5;
        out_parts->rear_lower = 6;
    }
    return WS_OK;
}

/* Smooth one-cycle leg target: hip swings between +A (forward) and -A (back);
 * knee lifts during the swing half of the cycle. */
static void leg_targets(double phase, double duty, double hip_amp, double knee_lift,
                        double *out_hip, double *out_knee)
{
    phase -= floor(phase);
    /* Hip: forward at phase 0, backward at mid-stance, forward again. */
    *out_hip = hip_amp * cos(2.0 * WS_PI_D * phase);
    if (phase < duty) {
        *out_knee = 0.0; /* stance: leg extended */
    } else {
        double s = (phase - duty) / (1.0 - duty); /* 0..1 through swing */
        *out_knee = knee_lift * sin(WS_PI_D * s);
    }
}

void ws_quadruped_gait_targets(ws_creature *c, const ws_quad_parts *parts,
                               const ws_species_profile *p, double phase)
{
    /* Trot: diagonal pairs move together. In the sagittal model the front and
     * rear legs are half a cycle out of phase. Phase is negated so that
     * advancing phase (phase += freq * dt) drives the animal forward (+x), which
     * is the nose direction. */
    double ph = -phase;
    double hip_f, knee_f, hip_r, knee_r;
    const double hip_amp = 0.16;
    const double knee_lift = 0.9;
    leg_targets(ph, p->duty_factor, hip_amp, knee_lift, &hip_f, &knee_f);
    leg_targets(ph + 0.5, p->duty_factor, hip_amp, knee_lift, &hip_r, &knee_r);

    /* Joint indices follow the build order: 2 = front hip, 3 = front knee,
     * 4 = rear hip, 5 = rear knee. */
    c->joints[2].motor_target = hip_f;
    c->joints[3].motor_target = knee_f;
    c->joints[4].motor_target = hip_r;
    c->joints[5].motor_target = knee_r;
    (void)parts;
}

void ws_quadruped_balance(ws_creature *c, int torso, double kp, double kd,
                          double target_angle)
{
    if (c == NULL || torso < 0 || (uint32_t)torso >= c->body_count) {
        return;
    }
    ws_cbody *b = &c->bodies[torso];
    double t = -kp * (b->angle - target_angle) - kd * b->av;
    b->extra_torque = t;
}

/* World joint angles for a two-link leg reaching from the hip to the foot.
 * `bend` selects the elbow side. Mirrors place_leg_ik, but returns angles. */
static void ik_angles(double hip_x, double hip_y, double foot_x, double foot_y,
                      double l1, double l2, double bend,
                      double *out_hip, double *out_knee)
{
    double dx = foot_x - hip_x;
    double dy = foot_y - hip_y;
    double d = sqrt(dx * dx + dy * dy);
    double dmin = fabs(l1 - l2) + 1.0e-3;
    double dmax = l1 + l2 - 1.0e-3;
    if (d < dmin) {
        d = dmin;
    }
    if (d > dmax) {
        d = dmax;
    }
    double base = atan2(dy, dx);
    double ca = (l1 * l1 + d * d - l2 * l2) / (2.0 * l1 * d);
    if (ca > 1.0) {
        ca = 1.0;
    } else if (ca < -1.0) {
        ca = -1.0;
    }
    double alpha = acos(ca);
    double ua = base + bend * alpha;
    double kx = hip_x + l1 * cos(ua);
    double ky = hip_y + l1 * sin(ua);
    *out_hip = ua;
    *out_knee = atan2(foot_y - ky, foot_x - kx);
}

void ws_gait_state_init(ws_gait_state *g)
{
    if (g == NULL) {
        return;
    }
    memset(g, 0, sizeof(*g));
    g->foot_lift = 0.08;
    g->kv = 0.30;
    g->phase = 0.0;
}

void ws_quadruped_walk(ws_creature *c, const ws_quad_parts *parts,
                       const ws_species_profile *p, ws_gait_state *g,
                       double v_des)
{
    if (c == NULL || p == NULL || g == NULL) {
        return;
    }
    if (!g->initialised) {
        /* Preserve caller-set gains/lift; only seed the gait phase. */
        g->phase = 0.0;
        g->stance[0] = 0;
        g->stance[1] = 0;
        g->initialised = 1;
    }
    const int torso = parts->torso;
    const double half_stance = p->stance * 0.5;
    const double l1 = p->leg_upper;
    const double l2 = p->leg_lower;
    const double freq = p->gait_freq;
    const double duty = p->duty_factor;
    const double dt = c->dt;
    const double T_st = duty / freq; /* stance duration, s */
    const double v_act = c->bodies[torso].vx;

    g->phase += freq * dt;
    g->phase -= floor(g->phase);

    /* Joint slots follow the build order: 2 front hip, 3 front knee,
     * 4 rear hip, 5 rear knee. */
    const int hip_j[2] = {2, 4};
    const int knee_j[2] = {3, 5};

    for (int L = 0; L < 2; ++L) {
        double pl = g->phase + (L == 1 ? 0.5 : 0.0);
        pl -= floor(pl);
        int in_stance = (pl < duty);

        double hx, hy;
        ws_cbody_point(&c->bodies[torso], L == 1 ? -half_stance : half_stance,
                       0.0, &hx, &hy);
        (void)hy;
        double ground_hip = (c->ground != NULL) ? c->ground(c->ground_ctx, hx) : 0.0;
        /* Plan the pose from a reference hip height, not the measured one. If
         * the target were recomputed from the current (possibly sagging) hip,
         * a small sag would bend the leg, which lowers the hip further, which
         * bends the leg more: a runaway crouch. Anchoring the plan to the
         * nominal height makes the commanded pose independent of the measured
         * sag, so the motors hold the body up instead of following it down. */
        double hip_ref_y = ground_hip + p->shoulder_h;

        /* Raibert foot placement: the neutral point (hip + half a stance of
         * travel) preserves speed; the kv term corrects it. If the body moves
         * faster than desired the foot is placed further ahead, so the body
         * decelerates over it; if slower, the foot lands behind and the body
         * accelerates. `kv` is therefore a time (foot offset per m/s of error). */
        double neutral_rel = 0.5 * v_act * T_st + g->kv * (v_act - v_des);
        double foot_x;
        double foot_y;
        if (in_stance) {
            if (!g->stance[L]) {
                /* Touchdown: latch the world-fixed plant point. */
                g->td_x[L] = hx + neutral_rel;
                g->stance[L] = 1;
            }
            foot_x = g->td_x[L];
            foot_y = (c->ground != NULL) ? c->ground(c->ground_ctx, foot_x) : 0.0;
        } else {
            g->stance[L] = 0;
            double s = (pl - duty) / (1.0 - duty); /* 0..1 through swing */
            foot_x = hx + neutral_rel;             /* live target */
            double gfx = (c->ground != NULL) ? c->ground(c->ground_ctx, foot_x) : 0.0;
            foot_y = gfx + g->foot_lift * sin(WS_PI_D * s);
        }

        double hip_ang, knee_ang;
        ik_angles(hx, hip_ref_y, foot_x, foot_y, l1, l2, 1.0, &hip_ang, &knee_ang);

        const ws_cjoint *jh = &c->joints[hip_j[L]];
        const ws_cjoint *jk = &c->joints[knee_j[L]];
        double torso_ang = c->bodies[torso].angle;
        c->joints[hip_j[L]].motor_target = (hip_ang - torso_ang) - jh->rest_rel;
        c->joints[knee_j[L]].motor_target = (knee_ang - hip_ang) - jk->rest_rel;
    }
}

ws_result ws_quadruped_walk_steer(ws_creature *c, const ws_quad_parts *parts,
                                  const ws_species_profile *p, ws_gait_state *g,
                                  double v_des, double yaw_rate)
{
    if (c == NULL || p == NULL || g == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (!g->initialised) {
        ws_gait_state_init(g);
    }
    ws_quadruped_walk(c, parts, p, g, v_des);
    g->yaw_rate = yaw_rate;
    g->heading += yaw_rate * c->dt;
    return WS_OK;
}
