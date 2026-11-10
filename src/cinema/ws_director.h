/* Worldseed Engine — cinematic observer (P9).
 *
 * The director is NOT an authority over the world. It consumes the ordered,
 * real event stream the simulation produced (ws_life_event) and the read-only
 * world positions those events carry, and from them it plans and drives a
 * camera. It has no call that writes to the world: it never advances time,
 * never moves an animal and never invents an event. If the world produced no
 * event, the director has no shot to follow (docs/09 "Yönetmen etkin/kapalı
 * iken aynı seed aynı simülasyon sonucunu üretir").
 *
 * Pipeline (docs/09 "Yönetmen katmanları"):
 *   1. event discovery — read new events from the log,
 *   2. prioritisation  — score each event's importance,
 *   3. subject choice  — keep the current subject or cut to a better event,
 *   4. shot planning   — pick wide/medium/close/tracking from importance,
 *   5. camera control  — drive the eye toward the planned view under speed,
 *      acceleration and turn-rate limits, so no jump or sickening motion, and
 *   6. continuity      — hold a shot for a minimum time (hysteresis) so the
 *      picture does not strobe between events.
 *
 * A deliberate shot change is a cut: the eye jumps to the new view, the motion
 * history resets and the cut is counted. Between cuts the motion is bounded and
 * measured (ws_director_metrics), which is what the P9 smoothness tests check.
 *
 * Frames: the world is a ground plane; the director plans in a ground-relative
 * frame (y = 0) and the renderer lifts the eye to the terrain. SI units.
 */
#ifndef WS_CINEMA_DIRECTOR_H
#define WS_CINEMA_DIRECTOR_H

#include <stdint.h>

#include "../core/ws_math.h"
#include "../core/ws_result.h"
#include "../render/ws_scene.h"
#include "../world/ws_event.h"

typedef enum ws_director_mode {
    WS_DIRECTOR_OFF = 0,  /* no camera control; the caller keeps its own */
    WS_DIRECTOR_AUTO = 1, /* the director plans and drives the camera */
    WS_DIRECTOR_FREE = 2  /* the observer took manual control; the director
                           * holds, the world is unaffected either way */
} ws_director_mode;

typedef enum ws_shot_type {
    WS_SHOT_WIDE = 0,
    WS_SHOT_MEDIUM = 1,
    WS_SHOT_CLOSE = 2,
    WS_SHOT_TRACKING = 3
} ws_shot_type;

const char *ws_shot_type_name(int shot);
const char *ws_director_mode_name(int mode);

typedef struct ws_director_config {
    double min_shot_s;    /* hold a shot at least this long */
    double max_shot_s;    /* re-evaluate a shot after this long */
    double cut_cost;      /* a new event must beat the current shot's importance
                           * by this much to force a cut */
    double max_speed;     /* m/s cap on camera translation */
    double max_accel;     /* m/s^2 cap on camera acceleration */
    double max_turn_rate; /* rad/s cap on camera yaw rate */
} ws_director_config;

ws_director_config ws_director_config_default(void);

typedef struct ws_director_shot {
    int type;                /* ws_shot_type */
    uint64_t event_sequence; /* the real event this shot follows */
    uint64_t event_tick;
    uint8_t event_kind;      /* ws_life_event_kind */
    uint32_t actor_species;
    ws_vec3 subject;         /* world position (ground-relative) being observed */
    double importance;
    ws_camera camera;        /* planned camera in the ground-relative frame */
} ws_director_shot;

typedef struct ws_director_metrics {
    double speed_max;     /* observed max eye speed, m/s (non-cut frames) */
    double accel_max;     /* observed max eye acceleration, m/s^2 */
    double turn_rate_max; /* observed max yaw rate, rad/s */
    double track_error;   /* current distance from eye to the planned eye, m */
    double track_error_max;
    uint32_t cuts;        /* deliberate shot changes so far */
    uint64_t events_observed;
    uint64_t frames;
} ws_director_metrics;

typedef struct ws_director {
    ws_director_config config;
    int mode;
    int has_shot;          /* 0 until the first real event is observed */
    ws_director_shot shot;
    double shot_time_s;    /* time the current shot has been held */

    /* Camera motion state (ground-relative). */
    ws_camera camera;      /* the current, limited camera */
    ws_vec3 prev_eye;
    ws_vec3 prev_vel;
    double prev_yaw;
    int has_prev;

    ws_director_metrics metrics;
    uint64_t next_sequence; /* next event sequence the director will read */

    /* Optional terrain height sampler so the director can keep its subject in
     * line of sight over a ridge. It only ever moves the camera; it never
     * removes terrain or vegetation to reveal the subject (docs/09). */
    double (*ground_height)(void *ctx, double x, double z);
    void *ground_ctx;
    int last_occlusion_lift; /* 1 when the last plan had to rise for sight */
} ws_director;

void ws_director_init(ws_director *d, const ws_director_config *cfg);

/* Provide a ground-height sampler. Passing NULL disables occlusion handling. */
void ws_director_set_ground(ws_director *d,
                            double (*fn)(void *ctx, double x, double z),
                            void *ctx);

/* 1 when the most recent camera plan had to rise to clear an occluder. */
int ws_director_last_occlusion_lift(const ws_director *d);

void ws_director_set_mode(ws_director *d, int mode);
int ws_director_mode_of(const ws_director *d);

/* Offer one event to the director. Events must be offered in increasing
 * sequence order; a repeated or older sequence is ignored so the caller can
 * simply feed every retained event each frame. Returns WS_OK, or
 * WS_ERR_INVALID_ARGUMENT when the payload is not a life event. */
ws_result ws_director_observe(ws_director *d, const ws_event *event);

/* Give the director the current world position of the subject it is following,
 * so the camera tracks a moving animal instead of a frozen event point. This is
 * a read-only observation: the director updates only its own shot and camera,
 * never the entity. Does nothing when the director has no shot. */
ws_result ws_director_track(ws_director *d, ws_vec3 subject);

/* Advance the camera toward the current planned view by `dt` seconds under the
 * motion limits, and update the metrics. Safe to call with no shot. */
ws_result ws_director_update(ws_director *d, double dt);

/* The current shot (NULL when the director has no shot, e.g. no event seen). */
const ws_director_shot *ws_director_current_shot(const ws_director *d);

/* The current camera in the ground-relative frame. When there is no shot this
 * is a fixed, deterministic overview, not a fabricated event. */
const ws_camera *ws_director_camera(const ws_director *d);

const ws_director_metrics *ws_director_metrics_of(const ws_director *d);

#endif /* WS_CINEMA_DIRECTOR_H */
