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
#include "../platform/ws_settings.h"
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

/* Fixed seed of the built-in sample world (docs/17 "örnek dünyayı açabilir").
 * A documented constant so "open the sample world" is always reproducible. */
#define WS_SESSION_SAMPLE_SEED 20260214ULL

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
    WS_CMD_TOGGLE_SHADOWS = 17,
    WS_CMD_TOGGLE_SETTINGS = 18,
    WS_CMD_REPLAY_TOGGLE = 19,
    WS_CMD_REPLAY_SEEK = 20,  /* arg = normalized position 0..1, oldest..newest */
    WS_CMD_REPLAY_LIVE = 21,  /* leave replay and jump to the newest state */
    WS_CMD_REPLAY_STEP_BACK = 22,    /* show one older recorded sample */
    WS_CMD_REPLAY_STEP_FORWARD = 23, /* show one newer recorded sample */
    WS_CMD_STEP_ONCE = 24,           /* advance exactly one fixed world tick */
    WS_CMD_SETTINGS_MOVE = 25,       /* arg = signed rows to move the menu cursor */
    WS_CMD_SETTINGS_ADJUST = 26,     /* arg = signed delta on the selected row */
    WS_CMD_SETTINGS_APPLY = 27,      /* apply the selected row's value */
    WS_CMD_SETTINGS_DEFAULTS = 28,   /* reset the menu selection to defaults */
    WS_CMD_WORLD_SELECT_TOGGLE = 29, /* open/close the world-selection screen */
    WS_CMD_WORLD_SELECT_MOVE = 30,   /* arg = signed rows to move its cursor */
    WS_CMD_WORLD_SELECT_CONFIRM = 31 /* confirm the highlighted world choice */
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

/* Minimum clearance in metres the free camera keeps above the ground. The eye
 * is kept at least this far above the terrain height under it, so the camera
 * never sinks into the ground ("Kamera çarpışması ve fiziksel engel
 * farkındalığı", docs/17). */
#define WS_SESSION_CAM_CLEARANCE_M 1.5

/* Horizontal margin kept between the free camera's eye and a tree trunk, on top
 * of the trunk's own radius, so the camera is pushed clear of solid obstacles
 * rather than grazing them (docs/17 "Kamera çarpışması ve fiziksel engel
 * farkındalığı"). */
#define WS_SESSION_CAM_OBSTACLE_MARGIN_M 0.6

/* Whether free-camera ground collision is enabled (the "Camera collision" row).
 * On by default; when off the eye may pass below the terrain, which is useful
 * for inspection. Observer-only: it never changes the world. */
int ws_session_camera_collision(const ws_session *s);

/* Number of solid obstacles (tree trunks) the free camera is pushed out of.
 * Zero when there is no session or the focus patch has no trees. */
uint32_t ws_session_camera_obstacle_count(const ws_session *s);

/* One solid obstacle in region-local metres: the trunk centre and its base
 * radius. `valid` is 0 when the index is out of range. Used by tests and the
 * deterministic tour to aim at a real trunk. */
typedef struct ws_session_obstacle_view {
    int valid;
    double x, z;   /* region-local metres */
    double radius; /* trunk base radius, m */
} ws_session_obstacle_view;

ws_session_obstacle_view ws_session_camera_obstacle(const ws_session *s, uint32_t index);

/* Place the free camera at an explicit pose (region-local metres and radians).
 * Used by evidence and tests to frame a scene deterministically; the pose is
 * still clamped by the collision rules on the next camera input. Observer-only. */
ws_result ws_session_set_free_camera(ws_session *s, double x, double y, double z,
                                     double yaw, double pitch);

/* Terrain height (metres, world) under the free camera's eye, for evidence and
 * the HUD. Returns 0 when there is no session. */
double ws_session_camera_ground_height(const ws_session *s);

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
int ws_session_settings_visible(const ws_session *s);
/* Directional shadows in the live view (off by default; T toggles). */
int ws_session_shadows_visible(const ws_session *s);

/* --- replay (docs/17 item 8) -------------------------------------------- */

/* The session keeps a bounded ring of periodic whole-world checkpoints as the
 * world advances (a real recording, not a scripted demo). Replay mode lets the
 * user re-observe any recorded state. The ring is observation bookkeeping: it
 * never feeds back into the simulation, so the world digest is unaffected.
 * Oldest-to-newest time span of the retained history, in world seconds. */
double ws_session_replay_span_s(const ws_session *s);
/* Number of retained checkpoint samples (0 until the first sample is taken). */
uint32_t ws_session_replay_samples(const ws_session *s);
int ws_session_replay_active(const ws_session *s);
/* World time of the sample the replay cursor currently shows. */
double ws_session_replay_time_s(const ws_session *s);
/* Cursor position in the retained history: 0 = oldest sample, 1 = newest. */
double ws_session_replay_position(const ws_session *s);
/* Newest recorded world time (the live edge of the recording). */
double ws_session_replay_live_time_s(const ws_session *s);

/* Apply a persistent settings snapshot to the observer (start view, time of
 * day, shadow/panel visibility). Read-only for the world: the same seed and
 * tick count still give a bit-identical digest. Returns WS_OK, or
 * WS_ERR_NULL_POINTER when either argument is NULL. */
ws_result ws_session_apply_settings(ws_session *s, const struct ws_settings *settings);
ws_result ws_session_get_settings(const ws_session *s, struct ws_settings *out);
uint64_t ws_session_seed(const ws_session *s);

/* --- interactive settings menu (docs/17 item 9) ------------------------- */

/* Rows of the interactive settings menu, in draw order. */
typedef enum ws_settings_item {
    WS_SET_ITEM_WIDTH = 0,
    WS_SET_ITEM_HEIGHT = 1,
    WS_SET_ITEM_RENDER_SCALE = 2,
    WS_SET_ITEM_VIEW = 3,
    WS_SET_ITEM_TIME = 4,
    WS_SET_ITEM_SPEED = 5,
    WS_SET_ITEM_SHADOWS = 6,
    WS_SET_ITEM_STATS = 7,
    WS_SET_ITEM_HELP = 8,
    WS_SET_ITEM_HUD_SCALE = 9,
    WS_SET_ITEM_VOLUME = 10,
    WS_SET_ITEM_CAM_SPEED = 11,
    WS_SET_ITEM_LOOK_SENS = 12,
    WS_SET_ITEM_CAM_COLLISION = 13,
    WS_SET_ITEM_SAVE = 14,
    WS_SET_ITEM_RELOAD = 15,
    WS_SET_ITEM_COUNT = 16
} ws_settings_item;

/* Row the cursor currently sits on (0 .. WS_SET_ITEM_COUNT-1). */
int ws_session_settings_cursor(const ws_session *s);

/* Label and formatted value of a menu row, for the HUD. Both strings are
 * written into caller buffers (NUL-terminated). Returns WS_OK, or
 * WS_ERR_INVALID_ARGUMENT for an out-of-range row. */
ws_result ws_session_settings_item(const ws_session *s, int item,
                                   char *label, size_t label_cap,
                                   char *value, size_t value_cap);

/* Value of a row as a stable integer token, for evidence/testing: e.g. the
 * resolution width, the view id, the shadow flag. */
int ws_session_settings_item_value(const ws_session *s, int item);

/* Persist the current observer settings to the session's settings file (the
 * "Save now" row and the WS_CMD_SETTINGS_APPLY on it). Returns WS_OK even when
 * there is no settings path configured (a no-op), so the UI never fails. */
ws_result ws_session_save_settings_file(ws_session *s);

/* Set the settings file path used by the menu's save row. Copied; a NULL path
 * disables persistence. Longer paths are rejected with WS_ERR_INVALID_ARGUMENT. */
ws_result ws_session_set_settings_path(ws_session *s, const char *path);

/* Current HUD/UI text scale (1 = small, 2 = large), the "Text size" row. */
int ws_session_hud_scale(const ws_session *s);

/* Current master audio volume percent (0..100), the "Master volume" row. */
int ws_session_volume(const ws_session *s);

/* Current free-camera move speed percent (25..400) and mouse-look sensitivity
 * percent (25..400), the "Camera speed" and "Look sensitivity" rows. */
int ws_session_camera_speed_pct(const ws_session *s);
int ws_session_look_sensitivity_pct(const ws_session *s);

/* --- world selection (docs/17 "Ana deneyim") ---------------------------- */

/* The world-selection screen the application shows when it opens (docs/17:
 * "uygulama açıldığında kullanıcı yeni dünya oluşturabilir, mevcut dünyayı
 * yükleyebilir veya örnek dünyayı açabilir"). Three choices, in draw order. */
typedef enum ws_world_choice {
    WS_WORLD_NEW = 0,    /* create a fresh world from a (seed-derived) seed */
    WS_WORLD_LOAD = 1,   /* load the existing checkpoint, if one is present */
    WS_WORLD_SAMPLE = 2, /* open the built-in sample world */
    WS_WORLD_CHOICE_COUNT = 3
} ws_world_choice;

/* Is the world-selection screen currently shown (application start, or `M`)? */
int ws_session_world_select_visible(const ws_session *s);
/* Row the selection cursor sits on (0 .. WS_WORLD_CHOICE_COUNT-1). */
int ws_session_world_select_cursor(const ws_session *s);
/* Does a checkpoint exist on disk for the "load existing" choice? 0/1. */
int ws_session_world_select_can_load(const ws_session *s);
/* Label and formatted detail of a choice row, for the HUD. */
ws_result ws_session_world_choice_item(const ws_session *s, int choice,
                                       char *label, size_t label_cap,
                                       char *value, size_t value_cap);
/* Whether the "load existing" row is currently selectable. */
int ws_session_world_choice_enabled(const ws_session *s, int choice);
/* Set the seed the next "create a new world" confirmation will use. The
 * application derives it from the wall clock so each new world differs, while
 * the session still rebuilds deterministically from whatever seed it is given.
 * 0 restores the default (config.seed + config.world_uid), which keeps the
 * headless/unit runs reproducible. */
void ws_session_world_set_new_seed(ws_session *s, uint64_t seed);
/* Confirm a choice directly (the same action the confirm command takes):
 * "new" and "sample" build a fresh world, "load" restores the checkpoint.
 * Returns WS_OK, WS_ERR_NOT_FOUND when the choice is not selectable, or a
 * deserialize error from a corrupt checkpoint. Closes the selection screen. */
ws_result ws_session_world_confirm(ws_session *s, int choice);

/* --- mouse-driven menu selection (docs/17 item 9 / "Ana deneyim") ------- */

/* The modal menus can be driven with the mouse as well as the keyboard: a left
 * click selects the row under the pointer, and a click on the highlighted row
 * activates it (the settings "Save now"/"Reload" rows act, the world-selection
 * screen opens the clicked world). This is observer-only, exactly like the
 * keyboard path, so it never changes the world digest.
 *
 * Row ids are the very enums the HUD draws: `ws_settings_item` for the settings
 * menu, `ws_world_choice` for the world-selection screen. Selecting a row moves
 * the menu cursor to it; activating performs the same action as the keyboard.
 * Both return WS_OK, or WS_ERR_INVALID_ARGUMENT when no menu is open or the row
 * id is out of range. */
ws_result ws_session_settings_select_row(ws_session *s, int row);
ws_result ws_session_world_select_row(ws_session *s, int row);

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

/* --- export (docs/17 "Dışa aktarma") ----------------------------------- */

/* Mix `frames` stereo sample frames of the live soundscape into `out` (2*frames
 * int16, interleaved), at WS_AUDIO_SAMPLE_RATE Hz. This is the same procedural
 * audio the observer hears, driven by the run's real life events, so a
 * timestamped frame-sequence export can ship the matching WAV. Mixing advances
 * the voice state but never the world: the digest is unchanged. */
ws_result ws_session_render_audio(ws_session *s, int16_t *out, uint32_t frames);

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
