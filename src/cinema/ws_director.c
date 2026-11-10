/* Worldseed Engine — cinematic observer (P9). */
#include "ws_director.h"

#include <math.h>
#include <string.h>

#include "../life/ws_life_event.h"

ws_director_config ws_director_config_default(void)
{
    ws_director_config c;
    c.min_shot_s = 2.0;    /* hold a shot at least two seconds */
    c.max_shot_s = 8.0;    /* re-evaluate a stale shot */
    c.cut_cost = 0.25;     /* a cut must clearly improve on the current shot */
    c.max_speed = 12.0;    /* m/s */
    c.max_accel = 8.0;     /* m/s^2 */
    c.max_turn_rate = 1.2; /* rad/s (~69 deg/s), below motion-sickness range */
    return c;
}

const char *ws_shot_type_name(int shot)
{
    switch (shot) {
    case WS_SHOT_WIDE:     return "wide";
    case WS_SHOT_MEDIUM:   return "medium";
    case WS_SHOT_CLOSE:    return "close";
    case WS_SHOT_TRACKING: return "tracking";
    default:               return "unknown";
    }
}

const char *ws_director_mode_name(int mode)
{
    switch (mode) {
    case WS_DIRECTOR_OFF:  return "off";
    case WS_DIRECTOR_AUTO: return "auto";
    case WS_DIRECTOR_FREE: return "free";
    default:               return "unknown";
    }
}

/* Importance of a real event. Higher means more worth watching. Kinds not in
 * this table cannot occur (the ecology only produces these four). */
static double event_importance(uint8_t kind)
{
    switch (kind) {
    case WS_LIFE_EV_KILLED: return 1.0;
    case WS_LIFE_EV_DIED:   return 0.8;
    case WS_LIFE_EV_BORN:   return 0.6;
    case WS_LIFE_EV_ATE:    return 0.35;
    default:                return 0.0;
    }
}

/* Shot type chosen from the event's nature, not from randomness. */
static int shot_for_kind(uint8_t kind)
{
    switch (kind) {
    case WS_LIFE_EV_KILLED: return WS_SHOT_TRACKING;
    case WS_LIFE_EV_DIED:   return WS_SHOT_CLOSE;
    case WS_LIFE_EV_BORN:   return WS_SHOT_MEDIUM;
    case WS_LIFE_EV_ATE:    return WS_SHOT_WIDE;
    default:                return WS_SHOT_WIDE;
    }
}

static double wrap_pi(double a)
{
    while (a > WS_PI) {
        a -= 2.0 * WS_PI;
    }
    while (a < -WS_PI) {
        a += 2.0 * WS_PI;
    }
    return a;
}

static double vec_len3(ws_vec3 v)
{
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

/* Plan a camera for a shot in the ground-relative frame. The framing distance
 * and height come from the shot type; the azimuth is a fixed function of the
 * shot so the plan is deterministic (no per-frame jitter). When a ground
 * sampler is set, the eye rises until the straight line to the subject clears
 * the terrain, so the subject is not hidden behind a ridge; the terrain itself
 * is never changed. */
static void plan_camera(const ws_director *d, const ws_director_shot *shot,
                        ws_camera *out, int *out_lifted)
{
    double dist;
    double height;
    double lift;
    double azimuth;
    switch (shot->type) {
    case WS_SHOT_CLOSE:
        dist = 6.0;  height = 2.4; lift = 1.0; azimuth = 2.6;
        break;
    case WS_SHOT_TRACKING:
        dist = 20.0; height = 3.2; lift = 1.2; azimuth = 3.6;
        break;
    case WS_SHOT_MEDIUM:
        dist = 14.0; height = 4.0; lift = 1.1; azimuth = 2.2;
        break;
    case WS_SHOT_WIDE:
    default:
        dist = 30.0; height = 7.0; lift = 1.0; azimuth = 2.0;
        break;
    }
    ws_vec3 s = shot->subject;
    double ex = s.x + cos(azimuth) * dist;
    double ez = s.z + sin(azimuth) * dist;
    double ey = s.y + height;

    int lifted = 0;
    if (d->ground_height != NULL) {
        /* Sample the sight line; if the ground rises above it at any sample,
         * raise the eye just enough to clear the highest obstruction. */
        double max_need = 0.0;
        const int samples = 16;
        for (int i = 1; i < samples; ++i) {
            double t = (double)i / (double)samples;
            double sx = ex + (s.x - ex) * t;
            double sz = ez + (s.z - ez) * t;
            double line_y = ey + ((s.y + lift) - ey) * t;
            double gy = d->ground_height(d->ground_ctx, sx, sz) + 1.5;
            double need = gy - line_y;
            if (need > max_need) {
                max_need = need;
            }
        }
        if (max_need > 0.0) {
            ey += max_need;
            lifted = 1;
        }
    }
    if (out_lifted != NULL) {
        *out_lifted = lifted;
    }

    ws_camera cam = ws_camera_default();
    cam.eye = ws_vec3_make(ex, ey, ez);
    cam.target = ws_vec3_make(s.x, s.y + lift, s.z);
    cam.up = ws_vec3_make(0.0, 1.0, 0.0);
    *out = cam;
}

static void overview_camera(ws_camera *out)
{
    ws_camera cam = ws_camera_default();
    cam.eye = ws_vec3_make(0.0, 34.0, -46.0);
    cam.target = ws_vec3_make(0.0, 0.0, 0.0);
    cam.up = ws_vec3_make(0.0, 1.0, 0.0);
    *out = cam;
}

void ws_director_init(ws_director *d, const ws_director_config *cfg)
{
    if (d == NULL) {
        return;
    }
    memset(d, 0, sizeof(*d));
    d->config = (cfg != NULL) ? *cfg : ws_director_config_default();
    d->mode = WS_DIRECTOR_AUTO;
    d->has_shot = 0;
    d->next_sequence = 0u;
    overview_camera(&d->camera);
}

void ws_director_set_mode(ws_director *d, int mode)
{
    if (d == NULL) {
        return;
    }
    d->mode = mode;
}

void ws_director_set_ground(ws_director *d,
                            double (*fn)(void *ctx, double x, double z),
                            void *ctx)
{
    if (d == NULL) {
        return;
    }
    d->ground_height = fn;
    d->ground_ctx = ctx;
}

int ws_director_last_occlusion_lift(const ws_director *d)
{
    return d != NULL ? d->last_occlusion_lift : 0;
}

int ws_director_mode_of(const ws_director *d)
{
    return d != NULL ? d->mode : WS_DIRECTOR_OFF;
}

/* Cut to a new shot: this is a deliberate camera change, so the eye jumps and
 * the motion history resets. The jump is counted as a cut, not as motion. */
static void cut_to(ws_director *d, const ws_director_shot *shot)
{
    d->shot = *shot;
    d->has_shot = 1;
    d->shot_time_s = 0.0;
    d->metrics.cuts += 1u;
    plan_camera(d, &d->shot, &d->camera, &d->last_occlusion_lift);
    d->prev_eye = d->camera.eye;
    d->prev_vel = ws_vec3_make(0.0, 0.0, 0.0);
    d->prev_yaw = atan2(d->camera.target.z - d->camera.eye.z,
                        d->camera.target.x - d->camera.eye.x);
    d->has_prev = 1;
}

ws_result ws_director_observe(ws_director *d, const ws_event *event)
{
    if (d == NULL || event == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (event->type != WS_EVENT_LIFE) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    /* Only new events count. The caller may feed every retained event each
     * frame; anything already seen is ignored. */
    if (event->sequence < d->next_sequence) {
        return WS_OK;
    }
    d->next_sequence = event->sequence + 1u;

    uint8_t kind;
    uint32_t actor;
    float x, y, value;
    if (!ws_life_event_decode(event->payload, event->payload_size, &kind, &actor,
                              NULL, &x, &y, &value, NULL)) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    d->metrics.events_observed += 1u;

    ws_director_shot cand;
    memset(&cand, 0, sizeof(cand));
    cand.type = shot_for_kind(kind);
    cand.event_sequence = event->sequence;
    cand.event_tick = event->tick;
    cand.event_kind = kind;
    cand.actor_species = actor;
    cand.subject = ws_vec3_make((double)x, 0.0, (double)y);
    cand.importance = event_importance(kind);
    plan_camera(d, &cand, &cand.camera, NULL);

    int take = 0;
    if (!d->has_shot) {
        take = 1;
    } else if (d->shot_time_s >= d->config.max_shot_s) {
        take = 1; /* the current shot is stale */
    } else if (d->shot_time_s >= d->config.min_shot_s &&
               cand.importance > d->shot.importance + d->config.cut_cost) {
        take = 1; /* a clearly better event arrived after the hold window */
    }

    if (take) {
        cut_to(d, &cand);
    }
    return WS_OK;
}

ws_result ws_director_track(ws_director *d, ws_vec3 subject)
{
    if (d == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (!d->has_shot) {
        return WS_OK; /* nothing to track yet */
    }
    d->shot.subject = subject;
    return WS_OK;
}

/* Clamp a vector's magnitude to `limit` (>= 0). */
static ws_vec3 clamp_mag(ws_vec3 v, double limit)
{
    double len = vec_len3(v);
    if (len <= limit || len <= 1e-12) {
        return v;
    }
    double s = limit / len;
    return ws_vec3_make(v.x * s, v.y * s, v.z * s);
}

ws_result ws_director_update(ws_director *d, double dt)
{
    if (d == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (!(dt > 0.0) || !isfinite(dt)) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    if (d->mode != WS_DIRECTOR_AUTO) {
        return WS_OK; /* observer control or off; the director holds */
    }

    ws_camera desired;
    if (d->has_shot) {
        plan_camera(d, &d->shot, &desired, &d->last_occlusion_lift);
    } else {
        overview_camera(&desired);
    }

    d->metrics.frames += 1u;

    /* The first frame, or a frame right after a cut, is a hard set: the cut
     * already placed the camera, so we only advance the hold timer. */
    if (!d->has_prev) {
        d->camera = desired;
        d->prev_eye = desired.eye;
        d->prev_vel = ws_vec3_make(0.0, 0.0, 0.0);
        d->prev_yaw = atan2(desired.target.z - desired.eye.z,
                            desired.target.x - desired.eye.x);
        d->has_prev = 1;
        d->shot_time_s += dt;
        return WS_OK;
    }

    /* Translate the eye toward the planned eye under a speed cap and an
     * acceleration cap. This bounds both the velocity and its rate of change,
     * so the picture cannot lurch. */
    ws_vec3 eye = d->camera.eye;
    ws_vec3 delta = ws_vec3_sub(desired.eye, eye);
    ws_vec3 desired_vel = ws_vec3_scale(delta, 1.0 / dt);
    desired_vel = clamp_mag(desired_vel, d->config.max_speed);
    ws_vec3 accel = ws_vec3_scale(ws_vec3_sub(desired_vel, d->prev_vel), 1.0 / dt);
    accel = clamp_mag(accel, d->config.max_accel);
    ws_vec3 vel = ws_vec3_add(d->prev_vel, ws_vec3_scale(accel, dt));
    vel = clamp_mag(vel, d->config.max_speed);
    eye = ws_vec3_add(eye, ws_vec3_scale(vel, dt));

    /* Turn the view toward the planned target under a yaw-rate cap. */
    double desired_yaw = atan2(desired.target.z - eye.z,
                               desired.target.x - eye.x);
    double yaw_delta = wrap_pi(desired_yaw - d->prev_yaw);
    double max_yaw_step = d->config.max_turn_rate * dt;
    double yaw_step = ws_clamp_double(yaw_delta, -max_yaw_step, max_yaw_step);
    double yaw = d->prev_yaw + yaw_step;

    double view_dist = vec_len3(ws_vec3_sub(desired.target, desired.eye));
    if (view_dist < 1e-6) {
        view_dist = 1.0;
    }
    ws_camera cam = d->camera;
    cam.eye = eye;
    cam.target = ws_vec3_make(eye.x + cos(yaw) * view_dist,
                              desired.target.y,
                              eye.z + sin(yaw) * view_dist);
    d->camera = cam;

    /* Metrics: observed motion and tracking error, measured only on bounded
     * (non-cut) frames. */
    double speed = vec_len3(vel);
    double acc = vec_len3(accel);
    double turn = fabs(yaw_step) / dt;
    if (speed > d->metrics.speed_max) d->metrics.speed_max = speed;
    if (acc > d->metrics.accel_max) d->metrics.accel_max = acc;
    if (turn > d->metrics.turn_rate_max) d->metrics.turn_rate_max = turn;
    double err = vec_len3(ws_vec3_sub(desired.eye, eye));
    d->metrics.track_error = err;
    if (err > d->metrics.track_error_max) d->metrics.track_error_max = err;

    d->prev_eye = eye;
    d->prev_vel = vel;
    d->prev_yaw = yaw;
    d->shot_time_s += dt;
    return WS_OK;
}

const ws_director_shot *ws_director_current_shot(const ws_director *d)
{
    if (d == NULL || !d->has_shot) {
        return NULL;
    }
    return &d->shot;
}

const ws_camera *ws_director_camera(const ws_director *d)
{
    return d != NULL ? &d->camera : NULL;
}

const ws_director_metrics *ws_director_metrics_of(const ws_director *d)
{
    return d != NULL ? &d->metrics : NULL;
}
