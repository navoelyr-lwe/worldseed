/* Worldseed Engine — three-dimensional reference quadruped builder (P13). */
#include "ws_quad3d.h"

#include <math.h>
#include <string.h>

#define WS_PI_D 3.14159265358979323846

/* Quaternion for a rotation of `angle` about the world z axis. */
static ws_quat quat_z(double angle)
{
    return ws_quat_make(cos(0.5 * angle), 0.0, 0.0, sin(0.5 * angle));
}

/* Planar two-link IK in the sagittal (x-y) plane. Returns the world angle of
 * the upper link (from +x) and of the lower link. `bend` selects the knee side;
 * a slight bend avoids the straight-leg singularity. Mirrors the planar
 * builder's place_leg_ik. */
static void ik_angles(double hip_x, double hip_y, double foot_x, double foot_y,
                      double l1, double l2, double bend,
                      double *out_upper, double *out_lower)
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
    *out_upper = base + bend * alpha;
    double kx = hip_x + l1 * cos(*out_upper);
    double ky = hip_y + l1 * sin(*out_upper);
    *out_lower = atan2(foot_y - ky, foot_x - kx);
}

/* Suggested fixed step for a species. The contact/joint solver's stability
 * limit scales with the leg length (measured on this solver: a limb up to
 * ~0.0056 x leg_upper stays put; beyond it the stand drifts and the body
 * falls), so the step is proportional to leg length, capped at the reference
 * deer's calibrated 2 ms. The earlier 3/4-power-of-shoulder rule is not
 * physical: it gave the smaller species a step that was far too small, which
 * over-damped the open-loop trot and stalled them (measured: fox ~0.15 m/s).
 * The leg-proportional step lifts the fox to ~0.66 m/s at the same gains and
 * keeps every species standing. */
double ws_quad3_suggested_dt(const ws_species_profile *p)
{
    double leg = 0.60;
    if (p != NULL && p->leg_upper > 0.0) {
        leg = p->leg_upper;
    }
    double dt = 0.0030 * (leg / 0.60);
    if (dt > 0.002) {
        dt = 0.002;
    }
    if (dt < 2.0e-4) {
        dt = 2.0e-4;
    }
    return dt;
}

ws_result ws_quad3_build(ws_body3d *c, ws_quad3_storage *st,
                         const ws_species_profile *p, double ground_y,
                         ws_quad3_parts *out_parts)
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

    const double scale = p->shoulder_h / 1.05;
    double mass_scale = p->total_mass / 110.0;
    if (mass_scale <= 0.0) {
        mass_scale = 1.0;
    }
    const double density_scale = mass_scale / (scale * scale * scale);
    const double torque_scale = mass_scale * scale;

    /* Lateral half-track: planning value proportional to body width. */
    const double track_half = 0.20 * scale;

    ws_body3 *bd = st->bodies;

    /* Torso (index 0). */
    const uint8_t torso_c[3] = {120u, 88u, 62u};
    ws_body3_make_box(&bd[0], 0.55 * scale, 0.22 * scale, 0.20 * scale,
                      310.0 * density_scale, torso_c);
    bd[0].pos = ws_vec3_make(0.0, H, 0.0);
    bd[0].orient = ws_quat_make(1.0, 0.0, 0.0, 0.0);

    /* Neck (1) and head (2), pitched forward and up in the sagittal plane. */
    const uint8_t neck_c[3] = {132u, 100u, 70u};
    ws_body3_make_box(&bd[1], 0.25 * scale, 0.09 * scale, 0.10 * scale,
                      700.0 * density_scale, neck_c);
    const double neck_ang = 0.45;
    {
        double ax = 0.50 * scale;
        double ay = 0.10 * scale;
        bd[1].orient = quat_z(neck_ang);
        bd[1].pos = ws_vec3_make(ax + bd[1].half_len * cos(neck_ang),
                                 H + ay + bd[1].half_len * sin(neck_ang), 0.0);
    }
    const uint8_t head_c[3] = {140u, 108u, 78u};
    ws_body3_make_box(&bd[2], 0.15 * scale, 0.10 * scale, 0.09 * scale,
                      700.0 * density_scale, head_c);
    {
        double nx = bd[1].pos.x + bd[1].half_len * cos(neck_ang);
        double ny = bd[1].pos.y + bd[1].half_len * sin(neck_ang);
        double ha = neck_ang + 0.15;
        bd[2].orient = quat_z(ha);
        bd[2].pos = ws_vec3_make(nx + bd[2].half_len * cos(ha),
                                 ny + bd[2].half_len * sin(ha), 0.0);
    }

    /* Legs (indices 3..10): upper, lower for FL, FR, RL, RR. */
    const uint8_t leg_c[3] = {104u, 74u, 52u};
    for (int leg = 0; leg < WS_LEG_COUNT; ++leg) {
        int upper = 3 + leg * 2;
        int lower = upper + 1;
        ws_body3_make_box(&bd[upper], hl_up, 0.05 * scale, 0.05 * scale,
                          1450.0 * density_scale, leg_c);
        ws_body3_make_box(&bd[lower], hl_lo, 0.035 * scale, 0.035 * scale,
                          1490.0 * density_scale, leg_c);
        double hip_x = (leg == WS_LEG_FL || leg == WS_LEG_FR) ? half_stance
                                                              : -half_stance;
        double hip_z = (leg == WS_LEG_FL || leg == WS_LEG_RL) ? track_half
                                                              : -track_half;
        double ua, la;
        ik_angles(hip_x, H, hip_x, ground_y, l1, l2, 1.0, &ua, &la);
        bd[upper].orient = quat_z(ua);
        bd[upper].pos = ws_vec3_make(hip_x + hl_up * cos(ua),
                                     H + hl_up * sin(ua), hip_z);
        double kx = hip_x + l1 * cos(ua);
        double ky = H + l1 * sin(ua);
        bd[lower].orient = quat_z(la);
        bd[lower].pos = ws_vec3_make(kx + hl_lo * cos(la),
                                     ky + hl_lo * sin(la), hip_z);
    }

    for (uint32_t i = 0; i < 11u; ++i) {
        uint32_t idx;
        if (ws_body3d_add_body(c, &bd[i], &idx) != WS_OK) {
            return WS_ERR_CAPACITY;
        }
    }

    /* Neck and head joints (hinge about z). */
    struct jdef {
        int a, b;
        double lax, lay, laz, lbx, lby, lbz;
        double lo, hi, kp, kd, tmax;
    };
    const struct jdef neck_jd[2] = {
        {0, 1, 0.50, 0.10, 0.0, -0.25, 0.0, 0.0, -0.5, 0.5, 600.0, 60.0, 250.0},
        {1, 2, 0.25, 0.0, 0.0, -0.15, 0.0, 0.0, -0.6, 0.6, 200.0, 20.0, 60.0},
    };
    for (int i = 0; i < 2; ++i) {
        ws_joint3 j;
        memset(&j, 0, sizeof(j));
        j.a = neck_jd[i].a;
        j.b = neck_jd[i].b;
        j.la = ws_vec3_make(neck_jd[i].lax * scale, neck_jd[i].lay * scale,
                            neck_jd[i].laz * scale);
        j.lb = ws_vec3_make(neck_jd[i].lbx * scale, neck_jd[i].lby * scale,
                            neck_jd[i].lbz * scale);
        j.hinge_axis = 2;
        j.hinge_lock = 1;
        j.limit_enabled = 1;
        j.min_angle = neck_jd[i].lo;
        j.max_angle = neck_jd[i].hi;
        j.motor_enabled = 1;
        j.motor_target = 0.0;
        j.motor_max_torque = neck_jd[i].tmax * torque_scale;
        j.motor_kp = neck_jd[i].kp * torque_scale;
        j.motor_kd = neck_jd[i].kd * torque_scale;
        if (ws_body3d_add_joint(c, &j) != WS_OK) {
            return WS_ERR_CAPACITY;
        }
    }

    /* Leg joints: hip (torso->upper) and knee (upper->lower), hinge about z. */
    for (int leg = 0; leg < WS_LEG_COUNT; ++leg) {
        int upper = 3 + leg * 2;
        int lower = upper + 1;
        double hip_x = (leg == WS_LEG_FL || leg == WS_LEG_FR) ? half_stance
                                                              : -half_stance;
        double hip_z = (leg == WS_LEG_FL || leg == WS_LEG_RL) ? track_half
                                                              : -track_half;
        ws_joint3 hip;
        memset(&hip, 0, sizeof(hip));
        hip.a = 0;
        hip.b = upper;
        hip.la = ws_vec3_make(hip_x, 0.0, hip_z);
        hip.lb = ws_vec3_make(-hl_up, 0.0, 0.0);
        hip.hinge_axis = 2;
        hip.hinge_lock = 1;
        hip.limit_enabled = 1;
        hip.min_angle = -0.8;
        hip.max_angle = 0.8;
        hip.motor_enabled = 1;
        hip.motor_target = 0.0;
        hip.motor_max_torque = 3000.0 * torque_scale;
        hip.motor_kp = 6000.0 * torque_scale;
        hip.motor_kd = 600.0 * torque_scale;
        if (ws_body3d_add_joint(c, &hip) != WS_OK) {
            return WS_ERR_CAPACITY;
        }

        ws_joint3 knee;
        memset(&knee, 0, sizeof(knee));
        knee.a = upper;
        knee.b = lower;
        knee.la = ws_vec3_make(hl_up, 0.0, 0.0);
        knee.lb = ws_vec3_make(-hl_lo, 0.0, 0.0);
        knee.hinge_axis = 2;
        knee.hinge_lock = 1;
        knee.limit_enabled = 1;
        knee.min_angle = -1.2;
        knee.max_angle = 0.15;
        knee.motor_enabled = 1;
        knee.motor_target = 0.0;
        knee.motor_max_torque = 2000.0 * torque_scale;
        knee.motor_kp = 4000.0 * torque_scale;
        knee.motor_kd = 400.0 * torque_scale;
        if (ws_body3d_add_joint(c, &knee) != WS_OK) {
            return WS_ERR_CAPACITY;
        }
    }

    /* Feet: distal end of each lower leg. */
    for (int leg = 0; leg < WS_LEG_COUNT; ++leg) {
        ws_foot3 f;
        memset(&f, 0, sizeof(f));
        f.body = 3 + leg * 2 + 1;
        f.local = ws_vec3_make(hl_lo, 0.0, 0.0);
        f.friction = p->friction;
        if (ws_body3d_add_foot(c, &f) != WS_OK) {
            return WS_ERR_CAPACITY;
        }
    }

    if (out_parts != NULL) {
        out_parts->torso = 0;
        out_parts->neck = 1;
        out_parts->head = 2;
        for (int leg = 0; leg < WS_LEG_COUNT; ++leg) {
            out_parts->leg_upper[leg] = 3 + leg * 2;
            out_parts->leg_lower[leg] = 3 + leg * 2 + 1;
        }
    }
    return WS_OK;
}

void ws_quad3_hold_rest(ws_body3d *c, const ws_quad3_parts *parts)
{
    if (c == NULL || parts == NULL) {
        return;
    }
    for (int leg = 0; leg < WS_LEG_COUNT; ++leg) {
        (void)parts;
        /* Joint index: 2 neck + leg*2 hip, +1 knee. */
        c->joints[2 + leg * 2].motor_target = 0.0;
        c->joints[2 + leg * 2 + 1].motor_target = 0.0;
    }
}

/* One-cycle leg target: hip swings forward at phase 0 and back at mid-stance;
 * the knee lifts only during the swing half. */
static void leg_target(double phase, double duty, double hip_amp,
                       double knee_lift, double *out_hip, double *out_knee)
{
    phase -= floor(phase);
    *out_hip = hip_amp * cos(2.0 * WS_PI_D * phase);
    if (phase < duty) {
        *out_knee = 0.0;
    } else {
        double s = (phase - duty) / (1.0 - duty);
        *out_knee = knee_lift * sin(WS_PI_D * s);
    }
}

void ws_quad3_gait_targets(ws_body3d *c, const ws_quad3_parts *parts,
                           const ws_species_profile *p, double phase)
{
    ws_quad3_gait g = ws_quad3_gait_walk(p);
    ws_quad3_gait_apply(c, parts, &g, phase);
}

ws_quad3_gait ws_quad3_gait_walk(const ws_species_profile *p)
{
    ws_quad3_gait g;
    g.freq = 1.4;
    g.hip_amp = 0.10;
    g.knee_lift = 0.6;
    g.duty = 0.6;
    if (p != NULL) {
        g.freq = p->gait_freq;
        g.duty = p->duty_factor;
    }
    return g;
}

ws_quad3_gait ws_quad3_gait_run(const ws_species_profile *p)
{
    ws_quad3_gait g = ws_quad3_gait_walk(p);
    /* A run raises the cadence and roughly doubles the stride, which is what
     * lifts the reference deer from ~1.8 m/s to ~2.1 m/s while it stays
     * upright. The stance fraction is unchanged: shortening it below the
     * trot's diagonal overlap destabilises the open-loop driver at this step. */
    g.freq *= 1.4;
    g.hip_amp *= 2.0;
    return g;
}

void ws_quad3_gait_apply(ws_body3d *c, const ws_quad3_parts *parts,
                         const ws_quad3_gait *g, double phase)
{
    if (c == NULL || parts == NULL || g == NULL) {
        return;
    }
    const double duty = g->duty;
    const double hip_amp = g->hip_amp;
    const double knee_lift = g->knee_lift;
    /* Diagonal pairs move together: FL & RR in phase, FR & RL half a cycle
     * later. */
    const double leg_phase[WS_LEG_COUNT] = {
        phase, phase + 0.5, phase + 0.5, phase,
    };
    for (int leg = 0; leg < WS_LEG_COUNT; ++leg) {
        double hip, knee;
        leg_target(-leg_phase[leg], duty, hip_amp, knee_lift, &hip, &knee);
        c->joints[2 + leg * 2].motor_target = hip;
        c->joints[2 + leg * 2 + 1].motor_target = knee;
    }
    (void)parts;
}

void ws_quad3_gait_state_init(ws_quad3_gait_state *gs,
                              const ws_species_profile *p)
{
    if (gs == NULL) {
        return;
    }
    memset(gs, 0, sizeof(*gs));
    gs->kv = 0.30;
    gs->foot_lift = 0.08;
    gs->initialised = 1;
    if (p == NULL || p->shoulder_h <= 0.0) {
        return;
    }
    /* Swing apex scales with body size, like the planar controller. The
     * foot-placement gain is a time, but the offset it commands (kv x speed
     * error) must stay inside the leg's horizontal reach, so scale it by the
     * reach relative to the reference deer. The deer maps to exactly 1.0 and
     * keeps the 0.08 m apex and 0.30 s gain. */
    double scale = p->shoulder_h / 1.05;
    gs->foot_lift = 0.08 * scale;
    double s = p->leg_upper + p->leg_lower;
    double reach = (s > p->shoulder_h)
                       ? sqrt(s * s - p->shoulder_h * p->shoulder_h)
                       : 0.0;
    const double deer_reach = sqrt(1.2 * 1.2 - 1.05 * 1.05);
    if (reach > 0.0) {
        gs->kv = 0.30 * (reach / deer_reach);
    }
}

void ws_quad3_gait_closed_loop(ws_body3d *c, const ws_quad3_parts *parts,
                               const ws_species_profile *p,
                               ws_quad3_gait_state *gs,
                               const ws_quad3_gait *g, double phase,
                               double v_des)
{
    if (c == NULL || parts == NULL || p == NULL || gs == NULL || g == NULL) {
        return;
    }
    if (!gs->initialised) {
        ws_quad3_gait_state_init(gs, p);
    }
    const double duty = g->duty;
    const double l1 = p->leg_upper;
    const double l2 = p->leg_lower;
    const double half_stance = p->stance * 0.5;
    const double T_stance = (g->freq > 0.0) ? duty / g->freq : 0.0;

    const ws_body3 *torso = &c->bodies[parts->torso];
    ws_vec3 fwd = ws_quat_rotate(torso->orient, ws_vec3_make(1.0, 0.0, 0.0));
    double v_act = ws_vec3_dot(torso->vel, fwd);

    for (int leg = 0; leg < WS_LEG_COUNT; ++leg) {
        int front = (leg == WS_LEG_FL || leg == WS_LEG_FR);
        double hip_x_local = front ? half_stance : -half_stance;
        /* Hip anchor in the torso frame, exactly as the builder placed it, so
         * the foot target tracks the body through any pose. */
        ws_vec3 hip_w = ws_body3_point(torso, ws_vec3_make(hip_x_local, 0.0, 0.0));
        /* The IK vertical reference is the nominal standing hip height above
         * the local ground, not the instantaneous (possibly sagging) hip
         * height. Feeding the sagged height back into the target would shorten
         * the commanded leg exactly when the body dips, which is positive
         * feedback and collapses the stance; the open-loop driver holds the leg
         * at its standing extension for the same reason. */
        double ground_hip = c->ground(c->ground_ctx, hip_w.x, hip_w.z);
        double hip_y_ref = ground_hip + p->shoulder_h;

        double leg_phase = phase + ((leg == WS_LEG_FL || leg == WS_LEG_RR)
                                        ? 0.0
                                        : 0.5);
        leg_phase -= floor(leg_phase);

        /* Raibert foot placement: the stance foot is planted at a neutral point
         * ahead of the hip (half the distance the body travels in one stance,
         * so the body vaults over it) plus a velocity-error term. Using v_des in
         * the neutral term drives the body to the command; holding the planted
         * foot fixed in the world during stance is what keeps the gait stable
         * (a stance foot that sweeps backward always pushes forward and runs
         * away).
         *
         * A real limb cannot place its foot past its own reach. The neutral
         * offset 0.5 * v * T_stance must stay inside the leg's horizontal reach,
         * so the *feedforward* speed is capped at the fastest stride the leg
         * allows (v_stride = 2 * reach / T_stance). The velocity-error term still
         * uses the true v_des, so an unreachable command keeps pulling the foot
         * behind the hip (a forward push) instead of planting it always ahead,
         * which would walk the body backward. */
        double s_leg = l1 + l2;
        double reach = (s_leg > p->shoulder_h)
                           ? sqrt(s_leg * s_leg - p->shoulder_h * p->shoulder_h)
                           : 0.0;
        double max_off = 0.8 * reach;
        double v_stride = (T_stance > 0.0) ? (2.0 * max_off / T_stance) : 0.0;
        double v_ff = (v_des < v_stride) ? v_des : v_stride;
        double neutral = 0.5 * v_ff * T_stance + gs->kv * (v_act - v_des);
        if (neutral > max_off) {
            neutral = max_off;
        } else if (neutral < -max_off) {
            neutral = -max_off;
        }

        double foot_x;
        double foot_y;
        if (leg_phase < duty) {
            if (!gs->stance[leg]) {
                gs->td_x[leg] = hip_w.x + neutral;
                gs->stance[leg] = 1;
            }
            foot_x = gs->td_x[leg];
            foot_y = c->ground(c->ground_ctx, foot_x, hip_w.z);
        } else {
            gs->stance[leg] = 0;
            double s = (leg_phase - duty) / (1.0 - duty);
            foot_x = hip_w.x + neutral;
            foot_y = c->ground(c->ground_ctx, foot_x, hip_w.z) +
                     gs->foot_lift * sin(WS_PI_D * s);
        }

        double ua, la;
        ik_angles(hip_w.x, hip_y_ref, foot_x, foot_y, l1, l2, 1.0, &ua, &la);
        ws_joint3 *hj = &c->joints[2 + leg * 2];
        ws_joint3 *kj = &c->joints[2 + leg * 2 + 1];
        hj->motor_target = ua - hj->rest_angle;
        kj->motor_target = (la - ua) - kj->rest_angle;
    }
}

/* Body x is forward, y is up, z is lateral (right-handed). Roll is a rotation
 * about the forward (x) axis; pitch is a rotation about the lateral (z) axis. */
void ws_quad3_balance(ws_body3d *c, const ws_quad3_parts *parts,
                      const ws_species_profile *p, double heading_target)
{
    if (c == NULL || parts == NULL) {
        return;
    }
    int torso = parts->torso;
    if (torso < 0 || (uint32_t)torso >= c->body_count) {
        return;
    }
    double torque_scale = 1.0;
    if (p != NULL) {
        double scale = p->shoulder_h / 1.05;
        double mass_scale = p->total_mass / 110.0;
        if (mass_scale > 0.0 && scale > 0.0) {
            torque_scale = mass_scale * scale;
        }
    }
    const double kp = 4000.0 * torque_scale;
    const double kd = 500.0 * torque_scale;
    const double kyaw = 400.0 * torque_scale;
    const double khead = 2500.0 * torque_scale;
    ws_body3 *b = &c->bodies[torso];
    /* World up and forward, then resolved into the body frame. */
    ws_quat qc = ws_quat_make(b->orient.w, -b->orient.x, -b->orient.y,
                              -b->orient.z);
    ws_vec3 up_body = ws_quat_rotate(qc, ws_vec3_make(0.0, 1.0, 0.0));
    ws_vec3 fwd_body = ws_quat_rotate(qc, ws_vec3_make(1.0, 0.0, 0.0));
    ws_vec3 w_body = ws_quat_rotate(qc, b->omega);
    /* Heading (yaw) of the torso's forward in the world x-z plane. The servo
     * tracks an absolute heading target (zero for straight-line walking, a
     * growing value for a commanded turn); the heading term also removes the
     * slow curve an asymmetric gait would otherwise accumulate. */
    ws_vec3 fwd_world = ws_quat_rotate(b->orient, ws_vec3_make(1.0, 0.0, 0.0));
    double heading = atan2(-fwd_world.z, fwd_world.x);
    double heading_err = heading - heading_target;
    /* Wrap the error to (-pi, pi] so a target that crosses the atan2 seam does
     * not demand a full spin. */
    heading_err = atan2(sin(heading_err), cos(heading_err));
    /* Pitch (about z): level means world forward maps to body (1,0,0). A nose-up
     * pitch of +theta makes fwd_body.y = -sin(theta), so the corrective torque
     * is +kp*fwd_body.y. Roll (about x): a roll of +phi makes up_body.z =
     * -sin(phi), so the corrective torque is +kp*up_body.z. */
    ws_vec3 t_body = ws_vec3_make(kp * up_body.z - kd * w_body.x,
                                  -kyaw * w_body.y - khead * heading_err,
                                  kp * fwd_body.y - kd * w_body.z);
    b->extra_torque = ws_quat_rotate(b->orient, t_body);
}
