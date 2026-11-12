/* Worldseed Engine — frontal-plane (lateral) balance and turning model (P13).
 *
 * See ws_lateral.h for the model's scope. The dynamics are a controlled linear
 * inverted pendulum (LIPM) in the lateral plane plus a damped roll oscillator:
 *
 *   a_y   = omega^2 (y - y_sup) + k_lat (y_ref - y) - d_lat v_y
 *   phi'' = roll_omega^2 (phi_ref - phi) - 2 zeta roll_omega phi'
 *           + c_couple (y - y_sup)
 *
 * The first LIPM term is the passive (falling-away-from-support) pendulum; the
 * k_lat/d_lat terms are the ankle/hip stabiliser that keeps the sway bounded
 * (the standard "ankle strategy"). The support position y_sup alternates
 * left/right every step_period and the swing foot is placed at the capture
 * point, clamped to the species' lateral reach. A turn commands a steady
 * lateral lean y_ref = a_lat / omega^2 and roll phi_ref = a_lat / g, where
 * a_lat = v_forward * yaw_rate is the centripetal acceleration.
 */
#include "ws_lateral.h"

#include <math.h>

#define WS_LATERAL_G 9.80665

void ws_lateral_params_for(const ws_species_profile *p, ws_lateral_params *out)
{
    if (out == NULL) {
        return;
    }
    double shoulder = 1.05;
    double gait_freq = 1.4;
    if (p != NULL) {
        shoulder = p->shoulder_h;
        gait_freq = p->gait_freq;
    }
    if (shoulder <= 0.0) {
        shoulder = 1.05;
    }
    if (gait_freq <= 0.0) {
        gait_freq = 1.4;
    }

    out->com_height = 0.85 * shoulder;
    out->track = 0.45 * shoulder;
    out->foot_reach = 0.90 * shoulder;
    out->omega = sqrt(WS_LATERAL_G / out->com_height);
    out->step_period = 0.5 / gait_freq;
    out->roll_omega = 1.2 * out->omega;
    out->zeta = 0.5;
    out->yaw_tau = 0.25;
    /* Smaller species turn faster: the bound scales as sqrt(reference/shoulder). */
    out->yaw_max = 2.0 * sqrt(1.05 / shoulder);
    out->k_capture = 1.0 / out->omega;
}

void ws_lateral_init(ws_lateral_state *s, const ws_lateral_params *p)
{
    if (s == NULL || p == NULL) {
        return;
    }
    s->y = 0.0;
    s->vy = 0.0;
    s->phi = 0.0;
    s->phi_dot = 0.0;
    s->yaw = 0.0;
    s->yaw_rate = 0.0;
    s->y_sup = p->track * 0.5;
    s->foot_y[0] = p->track * 0.5;
    s->foot_y[1] = -p->track * 0.5;
    s->support = 1;
    s->step_timer = p->step_period;
    s->roll_drive = 0.0;
    s->total_yaw = 0.0;
    s->initialised = 1;
}

double ws_lateral_capture_point(const ws_lateral_state *s,
                                const ws_lateral_params *p)
{
    if (s == NULL || p == NULL || p->omega <= 0.0) {
        return 0.0;
    }
    return s->y + s->vy / p->omega;
}

ws_result ws_lateral_step(ws_lateral_state *s, const ws_lateral_params *p,
                          double dt, double v_forward, double yaw_cmd)
{
    if (s == NULL || p == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (dt <= 0.0 || p->omega <= 0.0 || p->com_height <= 0.0) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    if (!s->initialised) {
        ws_lateral_init(s, p);
    }

    /* 1. Turn model: first-order lag toward the commanded yaw rate, bounded by
     * the species' maximum turn rate. */
    double yaw_rate = s->yaw_rate + (yaw_cmd - s->yaw_rate) * (dt / p->yaw_tau);
    if (yaw_rate > p->yaw_max) {
        yaw_rate = p->yaw_max;
    } else if (yaw_rate < -p->yaw_max) {
        yaw_rate = -p->yaw_max;
    }
    s->yaw_rate = yaw_rate;
    s->yaw += yaw_rate * dt;
    s->total_yaw = s->yaw;

    /* Centripetal acceleration and the steady lean it demands. */
    double a_lat = v_forward * yaw_rate;
    double y_ref = a_lat / (p->omega * p->omega);
    double phi_ref = a_lat / WS_LATERAL_G;

    /* 2. Step timing: alternate the loaded side; place the swing foot at the
     * capture point (clamped to the lateral reach). */
    s->step_timer -= dt;
    if (s->step_timer <= 0.0) {
        s->support = -s->support;
        double xi = ws_lateral_capture_point(s, p);
        double target = xi;
        if (target > p->foot_reach) {
            target = p->foot_reach;
        } else if (target < -p->foot_reach) {
            target = -p->foot_reach;
        }
        int idx = (s->support > 0) ? 0 : 1;
        s->foot_y[idx] = target;
        s->y_sup = target;
        s->step_timer += p->step_period;
    }

    /* 3. Lateral LIPM with the ankle/hip stabiliser. */
    double k_lat = 1.3 * p->omega * p->omega;
    double d_lat = 0.6 * p->omega;
    double a_y = p->omega * p->omega * (s->y - s->y_sup) +
                 k_lat * (y_ref - s->y) - d_lat * s->vy;
    s->vy += a_y * dt;
    s->y += s->vy * dt;

    /* 4. Roll: damped oscillator toward the turn lean, coupled to the CoM
     * offset from the support. */
    double c_couple = 0.3 * p->roll_omega * p->roll_omega / (p->track > 0.0 ? p->track : 1.0);
    double phi_acc = p->roll_omega * p->roll_omega * (phi_ref - s->phi) -
                     2.0 * p->zeta * p->roll_omega * s->phi_dot +
                     c_couple * (s->y - s->y_sup);
    s->phi_dot += phi_acc * dt;
    s->phi += s->phi_dot * dt;
    s->roll_drive = phi_acc;

    return WS_OK;
}

ws_result ws_lateral_measure(const ws_lateral_params *p, double dt,
                             double v_forward, double yaw_cmd, double seconds,
                             ws_lateral_metrics *out)
{
    if (p == NULL || out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (dt <= 0.0 || seconds <= 0.0 || p->omega <= 0.0) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    ws_lateral_state s;
    ws_lateral_init(&s, p);

    out->frames = (uint64_t)(seconds / dt + 0.5);
    out->max_abs_y = 0.0;
    out->max_abs_vy = 0.0;
    out->max_abs_phi = 0.0;
    out->max_abs_yaw_rate = 0.0;
    out->final_yaw = 0.0;
    out->turn_track_err = 0.0;
    out->support_changes = 0;

    int prev_support = s.support;
    double err_sum = 0.0;
    uint64_t err_n = 0u;
    uint64_t half = out->frames / 2u;
    for (uint64_t i = 0; i < out->frames; ++i) {
        if (ws_lateral_step(&s, p, dt, v_forward, yaw_cmd) != WS_OK) {
            return WS_ERR_INVALID_STATE;
        }
        double ay = fabs(s.y);
        double av = fabs(s.vy);
        double ap = fabs(s.phi);
        double ar = fabs(s.yaw_rate);
        if (ay > out->max_abs_y) {
            out->max_abs_y = ay;
        }
        if (av > out->max_abs_vy) {
            out->max_abs_vy = av;
        }
        if (ap > out->max_abs_phi) {
            out->max_abs_phi = ap;
        }
        if (ar > out->max_abs_yaw_rate) {
            out->max_abs_yaw_rate = ar;
        }
        if (s.support != prev_support) {
            out->support_changes++;
            prev_support = s.support;
        }
        if (i >= half) {
            err_sum += fabs(s.yaw_rate - yaw_cmd);
            err_n++;
        }
    }
    out->final_yaw = s.yaw;
    out->turn_track_err = err_n > 0u ? err_sum / (double)err_n : 0.0;
    return WS_OK;
}

