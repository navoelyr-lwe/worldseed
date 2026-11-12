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
 * (the standard "ankle strategy").
 *
 * Four feet are tracked independently (front-left, front-right, rear-left,
 * rear-right). Each foot has its own lateral position and stance/swing state.
 * The left and right feet are placed at the capture point on their own stance
 * interval (a lateral-sequence, or pacing, gait): while one side's pair is
 * loaded the other side swings. A stance foot carries an equal share of the
 * body weight, so the distribution conserves total weight exactly; a swing foot
 * carries nothing. The LIPM reference y_sup is the load-weighted support centre,
 * so it moves with the load rather than jumping at each step.
 *
 * The vertical direction is not integrated: this is a reduced-order model and
 * the load is a quasi-static distribution (the model card records this). A turn
 * commands a steady lateral lean y_ref = a_lat / omega^2 and roll
 * phi_ref = a_lat / g, where a_lat = v_forward * yaw_rate is the centripetal
 * acceleration.
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
    double mass = 120.0;
    if (p != NULL) {
        shoulder = p->shoulder_h;
        gait_freq = p->gait_freq;
        mass = p->total_mass;
    }
    if (shoulder <= 0.0) {
        shoulder = 1.05;
    }
    if (gait_freq <= 0.0) {
        gait_freq = 1.4;
    }
    if (mass <= 0.0) {
        mass = 120.0;
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
    out->mass = mass;
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
    /* Front-left/rear-left at +track/2, front-right/rear-right at -track/2. */
    s->foot_y[0] = p->track * 0.5;   /* FL */
    s->foot_y[1] = -p->track * 0.5;  /* FR */
    s->foot_y[2] = p->track * 0.5;   /* RL */
    s->foot_y[3] = -p->track * 0.5;  /* RR */
    for (int i = 0; i < WS_LATERAL_FEET; ++i) {
        s->foot_contact[i] = 1.0;
        s->foot_load[i] = 0.25 * p->mass * WS_LATERAL_G;
    }
    s->y_sup = 0.0;
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

double ws_lateral_support_center(const ws_lateral_state *s)
{
    if (s == NULL) {
        return 0.0;
    }
    double load_sum = 0.0;
    double weighted = 0.0;
    double geo_sum = 0.0;
    int stance = 0;
    for (int i = 0; i < WS_LATERAL_FEET; ++i) {
        if (s->foot_contact[i] > 0.5) {
            load_sum += s->foot_load[i];
            weighted += s->foot_load[i] * s->foot_y[i];
            geo_sum += s->foot_y[i];
            stance++;
        }
    }
    if (load_sum > 1e-9) {
        return weighted / load_sum;
    }
    if (stance > 0) {
        return geo_sum / (double)stance;
    }
    return 0.0;
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

    /* 2. Step timing: alternate the loaded side; place that side's lateral foot
     * position at the capture point (clamped to the lateral reach). The left
     * pair (FL, RL) shares one lateral position, as does the right pair
     * (FR, RR). */
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
        if (s->support > 0) {
            s->foot_y[0] = target; /* FL */
            s->foot_y[2] = target; /* RL */
        } else {
            s->foot_y[1] = target; /* FR */
            s->foot_y[3] = target; /* RR */
        }
        s->step_timer += p->step_period;
    }

    /* 3. Per-foot stance/swing phase: a lateral-sequence walk at the pair
     * level. When the left pair is loaded (support = +1) the right pair swings,
     * and vice versa. Both feet of the loaded pair are in stance. */
    int left_loaded = (s->support > 0);
    s->foot_contact[0] = left_loaded ? 1.0 : 0.0;  /* FL */
    s->foot_contact[1] = left_loaded ? 0.0 : 1.0;  /* FR */
    s->foot_contact[2] = left_loaded ? 1.0 : 0.0;  /* RL */
    s->foot_contact[3] = left_loaded ? 0.0 : 1.0;  /* RR */

    /* 4. Quasi-static ground reaction per foot. The two feet of the loaded pair
     * share the body weight equally; the swing pair carries nothing. The
     * distribution conserves total weight exactly. */
    double half_w = 0.5 * p->mass * WS_LATERAL_G;
    for (int i = 0; i < WS_LATERAL_FEET; ++i) {
        s->foot_load[i] = (s->foot_contact[i] > 0.5) ? half_w : 0.0;
    }

    /* 5. Support centre is the load-weighted foot position (the loaded pair's
     * lateral position); the LIPM is measured against it. */
    s->y_sup = ws_lateral_support_center(s);

    /* 6. Lateral LIPM with the ankle/hip stabiliser. */
    double k_lat = 1.3 * p->omega * p->omega;
    double d_lat = 0.6 * p->omega;
    double a_y = p->omega * p->omega * (s->y - s->y_sup) +
                 k_lat * (y_ref - s->y) - d_lat * s->vy;
    s->vy += a_y * dt;
    s->y += s->vy * dt;

    /* 7. Roll: damped oscillator toward the turn lean, coupled to the CoM
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
    out->contact_changes = 0;
    out->min_stance_feet = WS_LATERAL_FEET;
    out->max_load_error = 0.0;

    double weight = p->mass * WS_LATERAL_G;
    int prev_contact[WS_LATERAL_FEET];
    for (int i = 0; i < WS_LATERAL_FEET; ++i) {
        prev_contact[i] = (s.foot_contact[i] > 0.5) ? 1 : 0;
    }
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
        int stance = 0;
        double load_sum = 0.0;
        for (int f = 0; f < WS_LATERAL_FEET; ++f) {
            int now = (s.foot_contact[f] > 0.5) ? 1 : 0;
            if (now != prev_contact[f]) {
                out->contact_changes++;
                prev_contact[f] = now;
            }
            if (now) {
                stance++;
            }
            load_sum += s.foot_load[f];
        }
        if (stance < out->min_stance_feet) {
            out->min_stance_feet = stance;
        }
        double load_err = fabs(load_sum - weight);
        if (load_err > out->max_load_error) {
            out->max_load_error = load_err;
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
