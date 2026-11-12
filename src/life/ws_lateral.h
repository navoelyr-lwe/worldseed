/* Worldseed Engine — frontal-plane (lateral) balance and turning model (P13).
 *
 * The reference creature solver (ws_creature) is a planar sagittal chain: it
 * models forward motion, vertical motion and body pitch, with a single left/
 * right leg solution. The P13 slice adds the missing third dimension at the
 * level of a reduced-order frontal-plane model rather than a full 3D rigid-body
 * solver: a lateral linear inverted pendulum (LIPM) for the centre of mass, a
 * capture-point foot-placement law with independent left/right stance, a
 * damped roll stabiliser, and a turn model that couples the commanded yaw rate
 * to the lateral foot placement. This is the standard reduced-order approach
 * used to study lateral balance and turning, and its scope is recorded in the
 * model card (docs/life/MODEL-KARTI-FRONTAL-DENGE.md).
 *
 * It is deterministic, allocates nothing and uses SI units throughout. The
 * lateral CoM is measured along the body's lateral axis (left positive), roll
 * is rotation about the forward axis, and yaw is the heading in the ground
 * plane. The model never writes into the planar creature solver, so the
 * sagittal golden scenes stay bit-identical.
 */
#ifndef WS_LIFE_LATERAL_H
#define WS_LIFE_LATERAL_H

#include "ws_quadruped.h"

/* Bump when the model's dynamics change, so a recorded motion is detected as
 * incompatible. */
#define WS_LATERAL_VERSION 2u

/* Number of independent feet: front-left, front-right, rear-left, rear-right.
 * The P13 slice promotes the planar model's single left/right pair into four
 * separately tracked feet with their own lateral position, contact state and
 * vertical load. */
#define WS_LATERAL_FEET 4

typedef struct ws_lateral_params {
    double com_height;  /* m, CoM height above the ground */
    double track;       /* m, left-right foot separation (stance width) */
    double foot_reach;  /* m, max lateral foot placement from the midline */
    double omega;       /* 1/s, sqrt(g / com_height) for the LIPM */
    double step_period; /* s, half a stride (one lateral support interval) */
    double roll_omega;  /* 1/s, natural frequency of the roll righting reflex */
    double zeta;        /* roll damping ratio (0..1) */
    double yaw_tau;     /* s, first-order lag of achieved yaw rate */
    double yaw_max;     /* rad/s, bound on the achieved turn rate */
    double k_capture;   /* s, velocity-error gain of the foot-placement law */
    double mass;        /* kg, body mass (sets the ground reaction load) */
} ws_lateral_params;

/* Fill the reduced-order lateral parameters for a species from its profile.
 * The CoM height and track scale from the species' shoulder height; the gains
 * come from the reference deer and are recorded in the model card. NULL gives
 * the reference deer. */
void ws_lateral_params_for(const ws_species_profile *p, ws_lateral_params *out);

typedef struct ws_lateral_state {
    double y;             /* m, CoM lateral offset from the midline */
    double vy;            /* m/s, lateral CoM velocity */
    double phi;           /* rad, torso roll about the forward axis */
    double phi_dot;       /* rad/s, roll rate */
    double yaw;           /* rad, heading in the ground plane */
    double yaw_rate;      /* rad/s, achieved turn rate */
    double y_sup;         /* m, load-weighted support centre (lateral) */
    double foot_y[WS_LATERAL_FEET];  /* m, per-foot lateral position:
                                      * 0 FL, 1 FR, 2 RL, 3 RR */
    double foot_contact[WS_LATERAL_FEET]; /* 1 in stance, 0 in swing */
    double foot_load[WS_LATERAL_FEET];    /* N, vertical ground reaction */
    int support;          /* +1 left pair loaded, -1 right pair, 0 double */
    double step_timer;    /* s, time left in the current lateral support */
    double roll_drive;    /* rad/s^2, normalised roll drive applied this step */
    double total_yaw;     /* rad, integrated heading (for evidence) */
    int initialised;
} ws_lateral_state;

/* Initialise a state at rest on the midline with both feet at half track. */
void ws_lateral_init(ws_lateral_state *s, const ws_lateral_params *p);

/* Lateral capture point xi = y + vy / omega (m): the position the CoM reaches
 * if the pendulum is left uncontrolled. */
double ws_lateral_capture_point(const ws_lateral_state *s,
                                const ws_lateral_params *p);

/* Load-weighted support centre (m): sum(foot_load * foot_y) / sum(foot_load),
 * or the geometric mean of the stance feet when the total load is zero. This is
 * the ground reference the LIPM is measured against; with four independent feet
 * it moves smoothly as load shifts, instead of jumping at each step. */
double ws_lateral_support_center(const ws_lateral_state *s);

/* Advance one fixed step of dt seconds at forward speed `v_forward` (m/s) with
 * commanded turn rate `yaw_cmd` (rad/s). Deterministic; returns WS_OK or an
 * error for invalid input. */
ws_result ws_lateral_step(ws_lateral_state *s, const ws_lateral_params *p,
                          double dt, double v_forward, double yaw_cmd);

/* Objective metrics of one lateral run, all from the simulated state. */
typedef struct ws_lateral_metrics {
    uint64_t frames;            /* fixed steps in the run */
    double max_abs_y;           /* peak lateral CoM offset from the midline, m */
    double max_abs_vy;          /* peak lateral CoM speed, m/s */
    double max_abs_phi;         /* peak torso roll, rad */
    double max_abs_yaw_rate;    /* peak achieved turn rate, rad/s */
    double final_yaw;           /* integrated heading, rad */
    double turn_track_err;      /* mean |achieved - commanded| rate over the
                                 * second half (settled), rad/s */
    int support_changes;        /* lateral support switches (steps taken) */
    int contact_changes;        /* total per-foot stance/swing transitions */
    int min_stance_feet;        /* fewest feet in contact at any step */
    double max_load_error;      /* peak |sum(foot_load) - m*g| in steady stance, N */
} ws_lateral_metrics;

/* Run the lateral model for `seconds` at a fixed forward speed and turn
 * command and fill `out` with the run's metrics. Returns WS_OK, or an error for
 * invalid input. The model is deterministic, so two calls with equal arguments
 * produce identical metrics. */
ws_result ws_lateral_measure(const ws_lateral_params *p, double dt,
                             double v_forward, double yaw_cmd, double seconds,
                             ws_lateral_metrics *out);

#endif /* WS_LIFE_LATERAL_H */
