/* Worldseed Engine — interactive session (P11).
 *
 * The session is the whole running product behind the window: it owns a living
 * world (the P8 multi-scale host), the high-detail ecology of the focus region
 * (P7), the cinematic observer (P9), the procedural soundscape (P9) and the
 * renderer's camera, and it exposes the user-facing functions the application
 * needs — view modes, live inspection, world map, event timeline, pause/speed,
 * save/load and deterministic replay.
 *
 * Design rules, fixed by docs/00 and docs/09:
 *  - The observer is read-only. View mode, camera, selection, pause, speed and
 *    save/load never change the authoritative simulation. With the director and
 *    the free camera both off, the same seed and the same tick count give a
 *    bit-identical world (asserted by the tests through a SHA-256 digest).
 *  - The world runs on a fixed timestep decoupled from the render frame rate,
 *    so a slow frame does not change the simulation (docs/03 time model).
 *  - Movement is simulated, not played back: each visible animal is an
 *    articulated body driven by the P5 gait controller each tick, placed on the
 *    path its P7 agent decided, never an animation clip (docs/00).
 *
 * The high-detail ecology follows the focus region: the region under the camera
 * runs the agent model, while the rest of the world keeps advancing through the
 * host's aggregate model (P8). Both are deterministic functions of the seed and
 * the focus path.
 *
 * SI units and seconds. The session allocates its world storage once at create.
 */
#ifndef WS_UI_SESSION_H
#define WS_UI_SESSION_H

#include <stdint.h>

#include "../cinema/ws_director.h"
#include "../core/ws_math.h"
#include "../core/ws_result.h"
#include "../life/ws_ecology.h"
#include "../render/ws_framebuffer.h"
#include "../render/ws_scene.h"
#include "../world/ws_region.h"
#include "../world/ws_world_host.h"

/* The reference world is 4x4 regions of 512 m, i.e. about 2 km x 2 km. */
#define WS_SESSION_WORLD_M 2048.0
#define WS_SESSION_MAX_ACTORS 6u
#define WS_SESSION_MAX_TIMELINE 16u
#define WS_SESSION_DIGEST_HEX 65u
#define WS_SESSION_MAX_MAP 49u

typedef enum ws_view_mode {
    WS_VIEW_AUTO = 0,   /* the cinematic observer drives the camera */
    WS_VIEW_FREE = 1,   /* the user flies the camera */
    WS_VIEW_FOLLOW = 2, /* the camera follows the selected animal */
    WS_VIEW_MAP = 3,    /* a top-down view of the loaded region set */
    WS_VIEW_COUNT = 4
} ws_view_mode;

const char *ws_view_mode_name(int mode);

typedef enum ws_session_command {
    WS_CMD_NONE = 0,
    WS_CMD_TOGGLE_PAUSE = 1,
    WS_CMD_SPEED_UP = 2,
    WS_CMD_SPEED_DOWN = 3,
    WS_CMD_CYCLE_VIEW = 4,
    WS_CMD_SET_VIEW_AUTO = 5,
    WS_CMD_SET_VIEW_FREE = 6,
    WS_CMD_SET_VIEW_FOLLOW = 7,
    WS_CMD_SET_VIEW_MAP = 8,
    WS_CMD_SELECT_NEXT = 9,
    WS_CMD_SELECT_PREV = 10,
    WS_CMD_SAVE = 11,
    WS_CMD_LOAD = 12,
    WS_CMD_SET_TIME_OF_DAY = 13, /* arg = seconds since midnight */
    WS_CMD_TOGGLE_HELP = 14,
    WS_CMD_QUIT = 15,
    WS_CMD_TOGGLE_STATS = 16,
    WS_CMD_TOGGLE_SHADOWS = 17
} ws_session_command;

typedef struct ws_session_config {
    uint64_t seed;
    uint64_t world_uid;
    uint32_t viewport_w;
    uint32_t viewport_h;
    double start_time_of_day_s;
    double tick_s;         /* fixed world tick, seconds (default 1/20) */
    uint32_t actor_budget; /* max simultaneously rendered creatures */
} ws_session_config;

ws_session_config ws_session_config_default(uint64_t seed);

typedef struct ws_session ws_session;

ws_result ws_session_create(ws_session **out, const ws_session_config *cfg);
void ws_session_destroy(ws_session *s);

/* Advance the world by `real_dt` seconds of wall time. The world advances in
 * fixed `tick_s` steps scaled by the current speed; a slow frame never changes
 * the result. Safe to call when paused (the camera still updates). */
ws_result ws_session_step(ws_session *s, double real_dt);

/* Apply a discrete user command. */
ws_result ws_session_apply_command(ws_session *s, int command, double arg);

/* Continuous free-camera input, applied at the render frame rate. `forward`,
 * `right`, `up` are -1..1; `look_dx`, `look_dy` are mouse deltas in pixels.
 * Ignored unless the free view is active. */
ws_result ws_session_camera_input(ws_session *s, double forward, double right,
                                  double up, double look_dx, double look_dy,
                                  double dt);

/* --- state queries (all read-only) --------------------------------------- */

int ws_session_view_mode(const ws_session *s);
int ws_session_paused(const ws_session *s);
double ws_session_speed(const ws_session *s);
double ws_session_time_s(const ws_session *s);      /* world time, s */
double ws_session_time_of_day_s(const ws_session *s);
uint64_t ws_session_tick(const ws_session *s);
uint64_t ws_session_steps(const ws_session *s);     /* world ticks advanced */
int ws_session_help_visible(const ws_session *s);
int ws_session_stats_visible(const ws_session *s);
/* Directional shadows in the live view (off by default; T toggles). */
int ws_session_shadows_visible(const ws_session *s);
uint64_t ws_session_seed(const ws_session *s);

/* Frames and wall time of the most recent render, for the performance HUD. */
void ws_session_perf(const ws_session *s, double *out_last_frame_ms,
                     double *out_avg_frame_ms, double *out_fps, uint64_t *out_frames);
double ws_session_perf_last_ms(const ws_session *s);
double ws_session_perf_avg_ms(const ws_session *s);
double ws_session_perf_fps(const ws_session *s);
void ws_session_note_frame(ws_session *s, double frame_ms);

/* Population and resource summary. */
uint32_t ws_session_population(const ws_session *s, int species);
uint32_t ws_session_total_population(const ws_session *s);
double ws_session_plant_biomass(const ws_session *s);
uint32_t ws_session_loaded_regions(const ws_session *s);
uint64_t ws_session_loads(const ws_session *s);
uint64_t ws_session_unloads(const ws_session *s);
uint64_t ws_session_extinctions(const ws_session *s);

/* Demography of the focus ecology, for the statistics panel. */
uint64_t ws_session_births(const ws_session *s);
uint64_t ws_session_deaths(const ws_session *s);
uint64_t ws_session_predation_events(const ws_session *s);
uint64_t ws_session_forage_events(const ws_session *s);
uint64_t ws_session_starvation_deaths(const ws_session *s);
uint64_t ws_session_age_deaths(const ws_session *s);

/* Down-sampled population history, oldest first. Writes up to `capacity`
 * per-species counts (WS_SPECIES_COUNT per sample) into `out` and returns the
 * number of samples written. Read-only. */
uint32_t ws_session_population_history(const ws_session *s, float *out, uint32_t capacity);
ws_region_coord ws_session_focus_region(const ws_session *s);
double ws_session_focus_x(const ws_session *s);
double ws_session_focus_y(const ws_session *s);
double ws_session_sun_altitude(const ws_session *s);
double ws_session_temperature_c(const ws_session *s);
double ws_session_wind_speed(const ws_session *s);
uint32_t ws_session_audio_voices(const ws_session *s);
uint64_t ws_session_audio_events(const ws_session *s);

/* The observer camera (world metres). */
ws_camera ws_session_camera(const ws_session *s);

/* Live inspection of the selected animal. */
typedef struct ws_session_agent_view {
    int valid;
    int species;          /* ws_species_kind */
    const char *species_name;
    double energy;        /* 0..1 reserve */
    double age_s;
    double x, y;          /* world metres */
    double heading;       /* rad */
    double speed;         /* m/s, from the body's horizontal velocity */
    double distance_m;    /* distance travelled since birth */
    uint64_t bites;
    uint64_t kills;
    const char *activity; /* coarse label derived from the agent's state */
} ws_session_agent_view;

int ws_session_selected(const ws_session *s, ws_session_agent_view *out);

/* Event timeline: the most recent real world events, newest first. */
typedef struct ws_session_event_view {
    uint64_t sequence;
    uint64_t tick;
    double world_x;
    double world_y;
    int is_life;      /* 1 = life event, 0 = host journal event */
    uint8_t kind;
    const char *name;
    uint32_t actor;
    uint32_t subject;
} ws_session_event_view;

uint32_t ws_session_timeline_count(const ws_session *s);
ws_result ws_session_timeline_at(const ws_session *s, uint32_t index,
                                 ws_session_event_view *out);

/* World map: one entry per loaded region, for the map overlay. */
typedef struct ws_session_map_cell {
    int64_t region_x;
    int64_t region_y;
    int level;         /* ws_world_level */
    double population; /* total aggregate population */
    int is_focus;
} ws_session_map_cell;

uint32_t ws_session_map(const ws_session *s, ws_session_map_cell *out, uint32_t capacity);
const char *ws_session_scene_id(const ws_session *s);

/* Render the current frame into `fb` (RGBA8, `depth` optional). The world is not
 * modified. */
ws_result ws_session_render(ws_session *s, ws_framebuffer *fb, float *depth);

/* Checkpoint the whole session to `path`; restore it with ws_session_load. The
 * checkpoint is explicit little-endian bytes (ws_serialize), not a raw struct. */
ws_result ws_session_save(ws_session *s, const char *path);
ws_result ws_session_load(ws_session *s, const char *path);

/* Set the path used by the WS_CMD_SAVE / WS_CMD_LOAD commands (the S/L keys).
 * The string is copied; longer paths are rejected. */
ws_result ws_session_set_checkpoint_path(ws_session *s, const char *path);

/* SHA-256 of the authoritative world state (host + focus ecology), lowercase
 * hex plus NUL. Two sessions that ran the same seed and tick count agree. */
ws_result ws_session_digest(ws_session *s, char out_hex[WS_SESSION_DIGEST_HEX]);

#endif /* WS_UI_SESSION_H */
