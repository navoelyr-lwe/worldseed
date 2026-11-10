/* Worldseed Engine — interactive session (P11). */
#include "ws_session.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../audio/ws_audio.h"
#include "../core/ws_hash.h"
#include "../core/ws_serialize.h"
#include "../life/ws_life_event.h"
#include "../life/ws_quadruped.h"
#include "../life/ws_species.h"
#include "../render/ws_env_scene.h"
#include "../render/ws_render.h"
#include "../world/ws_terrain.h"

#define WS_SESSION_VERSION 1u
#define WS_SESSION_MAGIC "WSS1"
#define WS_SESSION_SPEEDS 5
/* 256 m focus patch (32 cells x 8 m): the high-detail ecology covers this, and
 * its terrain mesh and tree set are small enough for the CPU reference renderer
 * to hold an interactive frame rate. The wider 2x2 km world keeps advancing
 * through the host's aggregate model (P8). */
#define WS_SESSION_PATCH_M 256.0
#define WS_SESSION_PATCH_CELLS 32u
#define WS_SESSION_TREE_BUDGET 160u

/* Replay ring: bounded history of periodic whole-world checkpoints (docs/17
 * item 8). 24 samples at one sample every 2 world seconds covers ~48 s of
 * history; at ~64 KB per sample this is ~1.5 MB, cheap enough to keep live. */
#define WS_SESSION_REPLAY_MAX 24u
#define WS_SESSION_REPLAY_INTERVAL_S 2.0

static ws_result session_serialize(ws_session *s, uint8_t **out_buf, size_t *out_len);
static ws_result session_deserialize(ws_session *s, const uint8_t *bytes, size_t len);

typedef struct ws_actor {
    int active;
    int species;
    uint32_t agent_index; /* index into the focus ecology's agent array */
    ws_creature body;
    ws_quad_storage storage;
    ws_gait_state gait;
    ws_quad_parts parts;
    /* Ground profile context: the terrain plus the agent's current world
     * position and heading, so the sagittal ground samples follow the animal. */
    const ws_terrain *terrain;
    double wx, wz, heading;
} ws_actor;

struct ws_session {
    ws_session_config config;
    uint64_t steps;

    ws_world_host host;
    ws_ecology eco;
    ws_director director;
    ws_audio audio;
    uint64_t eco_event_cursor;
    uint64_t host_ev_cursor;

    /* Focus-region render scene (terrain, water, trees) plus the unit cube used
     * for actor bodies. Rebuilt when the focus region changes. */
    ws_env_scene env;
    int env_region_valid;
    int64_t env_region_x, env_region_y;
    uint32_t static_object_count;

    ws_actor actors[WS_SESSION_MAX_ACTORS];
    uint32_t actor_count;
    int selected;

    int view_mode;
    int paused;
    int help_visible;
    int stats_visible;
    int shadows_visible;
    int settings_visible;
    int speed_index;
    double time_of_day_s;

    ws_camera free_camera;
    double free_yaw, free_pitch;

    ws_session_event_view timeline[WS_SESSION_MAX_TIMELINE];
    uint32_t timeline_count;

    double last_frame_ms;
    double avg_frame_ms;
    uint64_t frames;

    char save_path[512]; /* default checkpoint path for the S/L keys */
    double tick_accumulator_s; /* leftover wall time carried between steps */

    /* Replay recording (docs/17 item 8): a ring of whole-world checkpoints. */
    uint8_t *replay_blob[WS_SESSION_REPLAY_MAX];
    size_t replay_len[WS_SESSION_REPLAY_MAX];
    uint64_t replay_step[WS_SESSION_REPLAY_MAX];
    uint32_t replay_head;   /* next write slot */
    uint32_t replay_count;  /* valid samples, <= WS_SESSION_REPLAY_MAX */
    double replay_next_s;   /* world time of the next scheduled sample */
    int replay_active;
    uint32_t replay_cursor; /* slot index shown while replay_active */
    /* Live state captured when replay begins, restored exactly on exit so the
     * live world is never perturbed by entering/leaving replay. */
    uint8_t *replay_live_blob;
    size_t replay_live_len;
};

static const double WS_SPEEDS[WS_SESSION_SPEEDS] = {0.0, 0.5, 1.0, 2.0, 4.0};

const char *ws_view_mode_name(int mode)
{
    switch (mode) {
    case WS_VIEW_AUTO: return "auto";
    case WS_VIEW_FREE: return "free";
    case WS_VIEW_FOLLOW: return "follow";
    case WS_VIEW_MAP: return "map";
    default: return "unknown";
    }
}

ws_session_config ws_session_config_default(uint64_t seed)
{
    ws_session_config c;
    c.seed = seed;
    c.world_uid = 0x7704d5eedULL;
    c.viewport_w = 1280u;
    c.viewport_h = 720u;
    c.start_time_of_day_s = 10.0 * 3600.0;
    c.tick_s = 0.05;
    c.actor_budget = WS_SESSION_MAX_ACTORS;
    return c;
}

const char *ws_session_scene_id(const ws_session *s)
{
    (void)s;
    return "world_live";
}

/* --- ground profile for an actor ---------------------------------------- */

static double actor_ground_cb(void *ctx, double forward)
{
    const ws_actor *a = (const ws_actor *)ctx;
    double wx = a->wx + forward * cos(a->heading);
    double wz = a->wz + forward * sin(a->heading);
    return ws_terrain_height_bilinear(a->terrain, wx, wz);
}

/* --- environment scene (focus region) ----------------------------------- */

static ws_result rebuild_env(ws_session *s, int64_t rx, int64_t ry)
{
    if (s->env_region_valid && s->env_region_x == rx && s->env_region_y == ry) {
        return WS_OK;
    }
    ws_env_scene_free(&s->env);
    ws_env_scene_options o = ws_env_scene_options_default(s->config.seed);
    o.region_x = rx;
    o.region_y = ry;
    o.cells = WS_SESSION_PATCH_CELLS;
    o.cell_size = WS_SESSION_PATCH_M / (double)WS_SESSION_PATCH_CELLS;
    o.scene_kind = WS_ENV_SCENE_DAY;
    o.time_of_day_s = s->time_of_day_s;
    ws_result r = ws_env_scene_create(&s->env, &o);
    if (r != WS_OK) {
        s->env_region_valid = 0;
        return r;
    }
    s->env_region_valid = 1;
    s->env_region_x = rx;
    s->env_region_y = ry;
    s->static_object_count = s->env.scene.object_count;
    return WS_OK;
}

/* --- actor (creature) management ---------------------------------------- */

static void actor_init(ws_session *s, ws_actor *a, int species, uint32_t agent_index)
{
    memset(a, 0, sizeof(*a));
    a->species = species;
    a->agent_index = agent_index;
    a->terrain = &s->env.terrain;
    (void)ws_creature_init(&a->body, 1.0 / 500.0, 9.80665, 30, 0.2,
                           a->storage.bodies, 8u, a->storage.joints, 8u,
                           a->storage.feet, 4u);
    const ws_species_info *si = ws_species_get((ws_species_kind)species);
    const ws_species_profile *sp = &si->body;
    ws_quadruped_build(&a->body, &a->storage, sp, 0.0, &a->parts);
    ws_gait_state_init(&a->gait);
    /* Settle on flat ground so the animal starts standing, then it will follow
     * the real terrain as the agent moves. */
    ws_creature_set_ground(&a->body, actor_ground_cb, a);
    for (int i = 0; i < 200; ++i) {
        ws_quadruped_balance(&a->body, a->parts.torso, 4000.0, 500.0, 0.0);
        ws_creature_step(&a->body);
    }
    a->active = 1;
}

/* Pick up to `budget` live agents from the focus ecology as rendered actors:
 * one per species that has survivors, lowest index first (deterministic). */
static void rebuild_actors(ws_session *s)
{
    uint32_t placed = 0u;
    for (uint32_t i = 0; i < s->eco.alive_count && placed < s->config.actor_budget; ++i) {
        const ws_eco_agent *ag = &s->eco.agents[i];
        if (!ag->alive) {
            continue;
        }
        int species = ag->species;
        int already = 0;
        for (uint32_t k = 0; k < placed; ++k) {
            if (s->actors[k].species == species) {
                already = 1;
                break;
            }
        }
        if (already) {
            continue;
        }
        actor_init(s, &s->actors[placed], species, i);
        placed += 1u;
    }
    s->actor_count = placed;
    if (s->selected >= (int)placed) {
        s->selected = placed > 0u ? 0 : -1;
    }
}

/* Advance every actor's articulated body one tick along its agent's path. */
static void step_actors(ws_session *s, double dt)
{
    for (uint32_t k = 0; k < s->actor_count; ++k) {
        ws_actor *a = &s->actors[k];
        if (!a->active || a->agent_index >= s->eco.alive_count) {
            continue;
        }
        const ws_eco_agent *ag = &s->eco.agents[a->agent_index];
        if (!ag->alive) {
            a->active = 0;
            continue;
        }
        a->wx = ag->x;
        a->wz = ag->y;
        a->heading = ag->heading;
        const ws_species_info *si = ws_species_get((ws_species_kind)a->species);
        double speed = ws_species_walk_speed((ws_species_kind)a->species);
        int sub = (int)(dt / a->body.dt);
        if (sub < 1) {
            sub = 1;
        }
        for (int i = 0; i < sub; ++i) {
            ws_quadruped_walk(&a->body, &a->parts, &si->body, &a->gait, speed);
            ws_quadruped_balance(&a->body, a->parts.torso, 4000.0, 500.0, 0.0);
            ws_creature_step(&a->body);
        }
    }
}

/* --- timeline ----------------------------------------------------------- */

static void timeline_push(ws_session *s, const ws_session_event_view *ev)
{
    /* Newest first. Shift down and insert at 0. */
    uint32_t n = s->timeline_count;
    if (n < WS_SESSION_MAX_TIMELINE) {
        n += 1u;
    }
    for (uint32_t i = n - 1u; i > 0u; --i) {
        s->timeline[i] = s->timeline[i - 1u];
    }
    s->timeline[0] = *ev;
    s->timeline_count = n;
}

static void drain_events(ws_session *s)
{
    /* Host journal events (custom payloads with a host kind). */
    uint32_t hn = ws_world_host_event_count(&s->host);
    for (uint32_t i = 0; i < hn; ++i) {
        ws_event ev;
        if (ws_world_host_event_at(&s->host, i, &ev) != WS_OK) {
            continue;
        }
        if (ev.sequence < s->host_ev_cursor) {
            continue;
        }
        s->host_ev_cursor = ev.sequence + 1u;
        ws_session_event_view v;
        memset(&v, 0, sizeof(v));
        v.sequence = ev.sequence;
        v.tick = ev.tick;
        v.is_life = 0;
        v.kind = ev.payload[0];
        v.name = ws_host_event_kind_name(ev.payload[0]);
        timeline_push(s, &v);
    }
    /* Focus-ecology life events. */
    uint32_t en = ws_ecology_event_count(&s->eco);
    for (uint32_t i = 0; i < en; ++i) {
        ws_event ev;
        if (ws_ecology_event_at(&s->eco, i, &ev) != WS_OK) {
            continue;
        }
        if (ev.sequence < s->eco_event_cursor) {
            continue;
        }
        s->eco_event_cursor = ev.sequence + 1u;
        uint8_t kind = 0u;
        uint32_t actor = 0u, subject = 0u;
        float x = 0.0f, y = 0.0f;
        if (!ws_life_event_decode(ev.payload, ev.payload_size, &kind, &actor,
                                  &subject, &x, &y, NULL, NULL)) {
            continue;
        }
        ws_session_event_view v;
        memset(&v, 0, sizeof(v));
        v.sequence = ev.sequence;
        v.tick = ev.tick;
        v.is_life = 1;
        v.kind = kind;
        v.name = ws_life_event_kind_name(kind);
        v.actor = actor;
        v.subject = subject;
        v.world_x = x;
        v.world_y = y;
        timeline_push(s, &v);

        double pan = (x / s->eco.config.area_m) * 2.0 - 1.0;
        (void)ws_audio_trigger_event(&s->audio, kind, pan, (double)ev.tick * s->config.tick_s);

        /* Offer the real event to the cinematic observer. The director only
         * reads it and plans a camera; it never changes the world. */
        (void)ws_director_observe(&s->director, &ev);
    }
}

/* Point the director at the current position of the species it is following,
 * so the AUTO camera tracks a moving animal rather than a frozen event point. */
static void track_director_subject(ws_session *s)
{
    const ws_director_shot *shot = ws_director_current_shot(&s->director);
    if (shot == NULL) {
        return;
    }
    for (uint32_t i = 0; i < s->eco.alive_count; ++i) {
        const ws_eco_agent *ag = &s->eco.agents[i];
        if (ag->alive && (uint32_t)ag->species == shot->actor_species) {
            (void)ws_director_track(&s->director,
                                    ws_vec3_make(ag->x, 0.0, ag->y));
            return;
        }
    }
}

/* --- replay recording ---------------------------------------------------- */

static void replay_ring_clear(ws_session *s)
{
    for (uint32_t i = 0; i < WS_SESSION_REPLAY_MAX; ++i) {
        free(s->replay_blob[i]);
        s->replay_blob[i] = NULL;
        s->replay_len[i] = 0u;
    }
    free(s->replay_live_blob);
    s->replay_live_blob = NULL;
    s->replay_live_len = 0u;
    s->replay_head = 0u;
    s->replay_count = 0u;
    s->replay_active = 0;
    s->replay_cursor = 0u;
}

/* Record the current whole-world state into the ring. Bounded and best-effort:
 * a failed sample is skipped rather than disrupting the run. */
static void replay_record(ws_session *s)
{
    uint8_t *blob = NULL;
    size_t len = 0u;
    if (session_serialize(s, &blob, &len) != WS_OK) {
        return;
    }
    uint32_t slot = s->replay_head;
    free(s->replay_blob[slot]);
    s->replay_blob[slot] = blob;
    s->replay_len[slot] = len;
    s->replay_step[slot] = s->steps;
    s->replay_head = (slot + 1u) % WS_SESSION_REPLAY_MAX;
    if (s->replay_count < WS_SESSION_REPLAY_MAX) {
        s->replay_count += 1u;
    }
}

/* Slot index of the `age`-th newest sample (age 0 = newest). */
static uint32_t replay_slot_newest(const ws_session *s, uint32_t age)
{
    uint32_t newest = (s->replay_head + WS_SESSION_REPLAY_MAX - 1u) % WS_SESSION_REPLAY_MAX;
    return (newest + WS_SESSION_REPLAY_MAX - (age % WS_SESSION_REPLAY_MAX)) % WS_SESSION_REPLAY_MAX;
}

/* Sample the world on a fixed world-time cadence as it advances. */
static void replay_maybe_record(ws_session *s)
{
    double world_t = (double)s->steps * s->config.tick_s;
    if (s->replay_count == 0u) {
        replay_record(s);
        s->replay_next_s = world_t + WS_SESSION_REPLAY_INTERVAL_S;
        return;
    }
    if (world_t >= s->replay_next_s) {
        replay_record(s);
        s->replay_next_s = world_t + WS_SESSION_REPLAY_INTERVAL_S;
    }
}

static ws_result replay_restore_slot(ws_session *s, uint32_t slot)
{
    if (s->replay_count == 0u || s->replay_blob[slot] == NULL) {
        return WS_ERR_CORRUPT;
    }
    ws_result r = session_deserialize(s, s->replay_blob[slot], s->replay_len[slot]);
    if (r == WS_OK) {
        s->tick_accumulator_s = 0.0;
    }
    return r;
}

static ws_result replay_restore_cursor(ws_session *s)
{
    uint32_t slot = replay_slot_newest(s, s->replay_cursor);
    return replay_restore_slot(s, slot);
}

/* Enter replay: capture the live state so leaving restores it exactly, snapshot
 * the current state as the newest sample (the recording edge), then show the
 * oldest retained sample. */
static void replay_enter(ws_session *s)
{
    if (s->replay_active) {
        return;
    }
    /* Make "now" the edge of the recording so the newest sample is the live
     * state; seeking to the end of the span then reproduces the live world. */
    replay_record(s);
    if (s->replay_count == 0u) {
        return;
    }
    uint8_t *live = NULL;
    size_t live_len = 0u;
    if (session_serialize(s, &live, &live_len) != WS_OK) {
        return;
    }
    s->replay_cursor = s->replay_count - 1u;
    if (replay_restore_cursor(s) != WS_OK) {
        free(live);
        return;
    }
    free(s->replay_live_blob);
    s->replay_live_blob = live;
    s->replay_live_len = live_len;
    s->replay_active = 1;
}

/* Leave replay: restore the captured live state bit-exactly and resume. */
static void replay_exit(ws_session *s)
{
    if (!s->replay_active) {
        return;
    }
    if (s->replay_live_blob != NULL) {
        (void)session_deserialize(s, s->replay_live_blob, s->replay_live_len);
    }
    free(s->replay_live_blob);
    s->replay_live_blob = NULL;
    s->replay_live_len = 0u;
    s->replay_active = 0;
    s->replay_cursor = 0u;
}

/* --- world advance ------------------------------------------------------ */

static void world_tick(ws_session *s)
{
    ws_world_host_step(&s->host, s->config.tick_s);
    (void)ws_ecology_step(&s->eco, s->config.tick_s);
    s->steps += 1u;
    replay_maybe_record(s);

    /* Follow the focus region: the focus region's centre defines the world
     * point the camera observes, and the high-detail ecology covers it. */
    drain_events(s);
    step_actors(s, s->config.tick_s);
    track_director_subject(s);
    (void)ws_director_update(&s->director, s->config.tick_s);
}

/* --- lifecycle ---------------------------------------------------------- */

ws_result ws_session_create(ws_session **out, const ws_session_config *cfg)
{
    if (out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    *out = NULL;
    ws_session *s = (ws_session *)calloc(1u, sizeof(*s));
    if (s == NULL) {
        return WS_ERR_OUT_OF_MEMORY;
    }
    s->config = cfg != NULL ? *cfg : ws_session_config_default(20260101ULL);
    if (s->config.actor_budget == 0u) {
        s->config.actor_budget = WS_SESSION_MAX_ACTORS;
    }
    if (s->config.actor_budget > WS_SESSION_MAX_ACTORS) {
        s->config.actor_budget = WS_SESSION_MAX_ACTORS;
    }
    s->view_mode = WS_VIEW_AUTO;
    s->paused = 0;
    s->speed_index = 2; /* 1.0x */
    s->selected = -1;
    s->time_of_day_s = s->config.start_time_of_day_s;
    snprintf(s->save_path, sizeof(s->save_path), "worldseed-checkpoint.wss");

    ws_world_host_config hc = ws_world_host_config_default(s->config.seed, s->config.world_uid);
    if (ws_world_host_init(&s->host, &hc) != WS_OK) {
        ws_session_destroy(s);
        return WS_ERR_OUT_OF_MEMORY;
    }
    /* Start at the world centre so the loaded set covers the 2x2 km region. */
    (void)ws_world_host_set_focus_world(&s->host, WS_SESSION_WORLD_M * 0.5,
                                        WS_SESSION_WORLD_M * 0.5);

    if (rebuild_env(s, s->host.focus.x, s->host.focus.y) != WS_OK) {
        ws_session_destroy(s);
        return WS_ERR_OUT_OF_MEMORY;
    }

    ws_eco_config ecfg = ws_eco_config_default(0x7ec0u ^ s->config.seed);
    ecfg.area_m = WS_SESSION_PATCH_M;
    if (ws_ecology_init(&s->eco, &ecfg) != WS_OK) {
        ws_session_destroy(s);
        return WS_ERR_OUT_OF_MEMORY;
    }
    ws_director_init(&s->director, NULL);
    ws_director_set_ground(&s->director, NULL, NULL);
    ws_audio_init(&s->audio, WS_AUDIO_SAMPLE_RATE);

    rebuild_actors(s);

    /* Free camera: a raised overview of the focus region. */
    s->free_camera = ws_camera_default();
    s->free_yaw = 0.6;
    s->free_pitch = -0.35;
    double cx = WS_SESSION_PATCH_M * 0.5;
    s->free_camera.eye = ws_vec3_make(cx - 40.0, 30.0, cx - 40.0);
    s->free_camera.target = ws_vec3_make(cx, 0.0, cx);

    *out = s;
    return WS_OK;
}

void ws_session_destroy(ws_session *s)
{
    if (s == NULL) {
        return;
    }
    replay_ring_clear(s);
    ws_env_scene_free(&s->env);
    ws_ecology_free(&s->eco);
    free(s);
}

/* --- commands ----------------------------------------------------------- */

ws_result ws_session_apply_command(ws_session *s, int command, double arg)
{
    if (s == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    switch (command) {
    case WS_CMD_TOGGLE_PAUSE:
        s->paused = !s->paused;
        break;
    case WS_CMD_SPEED_UP:
        if (s->speed_index + 1 < WS_SESSION_SPEEDS) {
            s->speed_index += 1;
        }
        break;
    case WS_CMD_SPEED_DOWN:
        if (s->speed_index > 0) {
            s->speed_index -= 1;
        }
        break;
    case WS_CMD_CYCLE_VIEW:
        s->view_mode = (s->view_mode + 1) % WS_VIEW_COUNT;
        break;
    case WS_CMD_SET_VIEW_AUTO: s->view_mode = WS_VIEW_AUTO; break;
    case WS_CMD_SET_VIEW_FREE: s->view_mode = WS_VIEW_FREE; break;
    case WS_CMD_SET_VIEW_FOLLOW: s->view_mode = WS_VIEW_FOLLOW; break;
    case WS_CMD_SET_VIEW_MAP: s->view_mode = WS_VIEW_MAP; break;
    case WS_CMD_SELECT_NEXT:
        if (s->actor_count > 0u) {
            s->selected = (s->selected + 1) % (int)s->actor_count;
        }
        break;
    case WS_CMD_SELECT_PREV:
        if (s->actor_count > 0u) {
            s->selected = (s->selected - 1 + (int)s->actor_count) % (int)s->actor_count;
        }
        break;
    case WS_CMD_SET_TIME_OF_DAY:
        if (arg >= 0.0 && arg < 86400.0) {
            s->time_of_day_s = arg;
        }
        break;
    case WS_CMD_SAVE:
        (void)ws_session_save(s, s->save_path);
        break;
    case WS_CMD_LOAD:
        (void)ws_session_load(s, s->save_path);
        break;
    case WS_CMD_TOGGLE_HELP:
        s->help_visible = !s->help_visible;
        break;
    case WS_CMD_TOGGLE_STATS:
        s->stats_visible = !s->stats_visible;
        break;
    case WS_CMD_TOGGLE_SHADOWS:
        s->shadows_visible = !s->shadows_visible;
        break;
    case WS_CMD_TOGGLE_SETTINGS:
        s->settings_visible = !s->settings_visible;
        break;
    case WS_CMD_REPLAY_TOGGLE:
        if (s->replay_active) {
            replay_exit(s);
        } else {
            replay_enter(s);
        }
        break;
    case WS_CMD_REPLAY_SEEK: {
        if (!s->replay_active || s->replay_count == 0u) {
            break;
        }
        /* arg is normalized time 0..1 over the retained span (0 = oldest,
         * 1 = newest). The cursor counts samples back from the newest. */
        double t = arg < 0.0 ? 0.0 : (arg > 1.0 ? 1.0 : arg);
        double age = (1.0 - t) * (double)(s->replay_count - 1u);
        s->replay_cursor = (uint32_t)(age + 0.5);
        (void)replay_restore_cursor(s);
        break;
    }
    case WS_CMD_REPLAY_STEP_BACK:
        if (s->replay_active && s->replay_cursor + 1u < s->replay_count) {
            s->replay_cursor += 1u;
            (void)replay_restore_cursor(s);
        }
        break;
    case WS_CMD_REPLAY_STEP_FORWARD:
        if (s->replay_active && s->replay_cursor > 0u) {
            s->replay_cursor -= 1u;
            (void)replay_restore_cursor(s);
        }
        break;
    case WS_CMD_REPLAY_LIVE:
        replay_exit(s);
        break;
    case WS_CMD_STEP_ONCE:
        /* Time control (docs/17): advance exactly one fixed world tick. While
         * replaying, the same key means "show the next recorded sample", so one
         * key steps forward in both the live and the replay timeline. */
        if (s->replay_active) {
            if (s->replay_cursor > 0u) {
                s->replay_cursor -= 1u;
                (void)replay_restore_cursor(s);
            }
        } else {
            world_tick(s);
            s->tick_accumulator_s = 0.0;
        }
        break;
    case WS_CMD_QUIT:
        break;
    default:
        return WS_ERR_INVALID_ARGUMENT;
    }
    return WS_OK;
}

ws_result ws_session_apply_settings(ws_session *s, const struct ws_settings *settings)
{
    if (s == NULL || settings == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    /* Observer-only: view, clock and panel/shadow visibility. None of this
     * reaches the host, the ecology or any event stream, so the world digest is
     * unaffected (docs/00 read-only observer). */
    if (settings->view >= 0 && settings->view < WS_VIEW_COUNT) {
        s->view_mode = settings->view;
    }
    if (settings->time_of_day_s >= 0.0 && settings->time_of_day_s < 86400.0) {
        s->time_of_day_s = settings->time_of_day_s;
    }
    s->shadows_visible = settings->shadows ? 1 : 0;
    s->stats_visible = settings->stats ? 1 : 0;
    s->help_visible = settings->help ? 1 : 0;
    return WS_OK;
}

ws_result ws_session_get_settings(const ws_session *s, struct ws_settings *out)
{
    if (s == NULL || out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    /* Start from the defaults so the window/scale/seed fields, which belong to
     * the application, keep sensible values; overwrite the observer fields with
     * the live state. */
    ws_settings v = ws_settings_default();
    v.seed = s->config.seed;
    v.view = s->view_mode;
    v.time_of_day_s = s->time_of_day_s;
    v.shadows = s->shadows_visible ? 1 : 0;
    v.stats = s->stats_visible ? 1 : 0;
    v.help = s->help_visible ? 1 : 0;
    *out = v;
    return WS_OK;
}

ws_result ws_session_step(ws_session *s, double real_dt)
{
    if (s == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (!(real_dt > 0.0) || !isfinite(real_dt)) {
        real_dt = 0.0;
    }
    double speed = WS_SPEEDS[s->speed_index];
    if (!s->paused && !s->replay_active && speed > 0.0) {
        /* Cap the catch-up so a very long frame does not spiral, and carry the
         * sub-tick remainder so the world advances at the right rate. */
        s->tick_accumulator_s += real_dt * speed;
        if (s->tick_accumulator_s > 0.5) {
            s->tick_accumulator_s = 0.5;
        }
        while (s->tick_accumulator_s >= s->config.tick_s) {
            world_tick(s);
            s->tick_accumulator_s -= s->config.tick_s;
        }
        s->time_of_day_s += real_dt * speed;
        while (s->time_of_day_s >= 86400.0) {
            s->time_of_day_s -= 86400.0;
        }
    }
    return WS_OK;
}

ws_result ws_session_camera_input(ws_session *s, double forward, double right,
                                  double up, double look_dx, double look_dy,
                                  double dt)
{
    if (s == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (s->view_mode != WS_VIEW_FREE) {
        return WS_OK;
    }
    s->free_yaw += look_dx * 0.0035;
    s->free_pitch -= look_dy * 0.0035;
    if (s->free_pitch > 1.3) { s->free_pitch = 1.3; }
    if (s->free_pitch < -1.3) { s->free_pitch = -1.3; }

    double cp = cos(s->free_pitch);
    ws_vec3 dir = ws_vec3_make(cos(s->free_yaw) * cp, sin(s->free_pitch),
                               sin(s->free_yaw) * cp);
    ws_vec3 rightv = ws_vec3_make(-sin(s->free_yaw), 0.0, cos(s->free_yaw));
    double move = 40.0 * dt;
    ws_vec3 eye = s->free_camera.eye;
    eye = ws_vec3_add(eye, ws_vec3_scale(dir, forward * move));
    eye = ws_vec3_add(eye, ws_vec3_scale(rightv, right * move));
    eye.y += up * move;
    if (eye.y < 1.5) {
        eye.y = 1.5;
    }
    s->free_camera.eye = eye;
    s->free_camera.target = ws_vec3_add(eye, dir);
    return WS_OK;
}

/* --- queries ------------------------------------------------------------ */

int ws_session_view_mode(const ws_session *s) { return s != NULL ? s->view_mode : 0; }
int ws_session_paused(const ws_session *s) { return s != NULL ? s->paused : 0; }
double ws_session_speed(const ws_session *s) { return s != NULL ? WS_SPEEDS[s->speed_index] : 0.0; }
double ws_session_time_s(const ws_session *s) { return s != NULL ? s->host.time_s : 0.0; }
double ws_session_time_of_day_s(const ws_session *s) { return s != NULL ? s->time_of_day_s : 0.0; }
uint64_t ws_session_tick(const ws_session *s) { return s != NULL ? s->host.tick : 0u; }
uint64_t ws_session_steps(const ws_session *s) { return s != NULL ? s->steps : 0u; }
int ws_session_help_visible(const ws_session *s) { return s != NULL ? s->help_visible : 0; }
int ws_session_stats_visible(const ws_session *s) { return s != NULL ? s->stats_visible : 0; }
int ws_session_settings_visible(const ws_session *s) { return s != NULL ? s->settings_visible : 0; }
int ws_session_shadows_visible(const ws_session *s) { return s != NULL ? s->shadows_visible : 0; }

/* --- replay queries ----------------------------------------------------- */

uint32_t ws_session_replay_samples(const ws_session *s)
{
    return s != NULL ? s->replay_count : 0u;
}

int ws_session_replay_active(const ws_session *s)
{
    return s != NULL ? s->replay_active : 0;
}

double ws_session_replay_live_time_s(const ws_session *s)
{
    if (s == NULL || s->replay_count == 0u) {
        return 0.0;
    }
    uint32_t newest = replay_slot_newest(s, 0u);
    return (double)s->replay_step[newest] * s->config.tick_s;
}

double ws_session_replay_time_s(const ws_session *s)
{
    if (s == NULL || s->replay_count == 0u) {
        return 0.0;
    }
    uint32_t slot = s->replay_active ? replay_slot_newest(s, s->replay_cursor)
                                     : replay_slot_newest(s, 0u);
    return (double)s->replay_step[slot] * s->config.tick_s;
}

double ws_session_replay_position(const ws_session *s)
{
    if (s == NULL || s->replay_count <= 1u) {
        return 1.0;
    }
    uint32_t age = s->replay_active ? s->replay_cursor : 0u;
    if (age >= s->replay_count) {
        age = s->replay_count - 1u;
    }
    /* 0 = oldest, 1 = newest. */
    return 1.0 - (double)age / (double)(s->replay_count - 1u);
}

double ws_session_replay_span_s(const ws_session *s)
{
    if (s == NULL || s->replay_count == 0u) {
        return 0.0;
    }
    uint32_t newest = replay_slot_newest(s, 0u);
    uint32_t oldest = replay_slot_newest(s, s->replay_count - 1u);
    double span = (double)(s->replay_step[newest] - s->replay_step[oldest]) *
                  s->config.tick_s;
    return span > 0.0 ? span : 0.0;
}
uint64_t ws_session_seed(const ws_session *s) { return s != NULL ? s->config.seed : 0u; }

double ws_session_perf_last_ms(const ws_session *s) { return s != NULL ? s->last_frame_ms : 0.0; }
double ws_session_perf_avg_ms(const ws_session *s) { return s != NULL ? s->avg_frame_ms : 0.0; }
double ws_session_perf_fps(const ws_session *s)
{
    return (s != NULL && s->avg_frame_ms > 0.0) ? 1000.0 / s->avg_frame_ms : 0.0;
}

void ws_session_perf(const ws_session *s, double *out_last_frame_ms,
                     double *out_avg_frame_ms, double *out_fps, uint64_t *out_frames)
{
    if (s == NULL) {
        return;
    }
    if (out_last_frame_ms) { *out_last_frame_ms = s->last_frame_ms; }
    if (out_avg_frame_ms) { *out_avg_frame_ms = s->avg_frame_ms; }
    if (out_fps) { *out_fps = s->avg_frame_ms > 0.0 ? 1000.0 / s->avg_frame_ms : 0.0; }
    if (out_frames) { *out_frames = s->frames; }
}

void ws_session_note_frame(ws_session *s, double frame_ms)
{
    if (s == NULL) {
        return;
    }
    s->last_frame_ms = frame_ms;
    s->frames += 1u;
    /* Exponential moving average over the first frames, then a windowed mean. */
    if (s->frames == 1u) {
        s->avg_frame_ms = frame_ms;
    } else {
        s->avg_frame_ms = s->avg_frame_ms * 0.9 + frame_ms * 0.1;
    }
}

uint32_t ws_session_population(const ws_session *s, int species)
{
    if (s == NULL || species < 0 || species >= WS_SPECIES_COUNT) {
        return 0u;
    }
    return ws_ecology_population(&s->eco, species);
}

uint32_t ws_session_total_population(const ws_session *s)
{
    if (s == NULL) {
        return 0u;
    }
    return s->eco.alive_count;
}

double ws_session_plant_biomass(const ws_session *s)
{
    return s != NULL ? ws_ecology_plant_biomass(&s->eco) : 0.0;
}

uint32_t ws_session_loaded_regions(const ws_session *s)
{
    return s != NULL ? ws_world_host_loaded_count(&s->host) : 0u;
}

uint64_t ws_session_loads(const ws_session *s) { return s != NULL ? s->host.total_loads : 0u; }
uint64_t ws_session_unloads(const ws_session *s) { return s != NULL ? s->host.total_unloads : 0u; }
uint64_t ws_session_extinctions(const ws_session *s) { return s != NULL ? s->host.extinctions : 0u; }

uint64_t ws_session_births(const ws_session *s) { return s != NULL ? s->eco.births : 0u; }
uint64_t ws_session_deaths(const ws_session *s) { return s != NULL ? s->eco.deaths : 0u; }
uint64_t ws_session_predation_events(const ws_session *s) { return s != NULL ? s->eco.predation_events : 0u; }
uint64_t ws_session_forage_events(const ws_session *s) { return s != NULL ? s->eco.forage_events : 0u; }
uint64_t ws_session_starvation_deaths(const ws_session *s) { return s != NULL ? s->eco.starvation_deaths : 0u; }
uint64_t ws_session_age_deaths(const ws_session *s) { return s != NULL ? s->eco.age_deaths : 0u; }

uint32_t ws_session_population_history(const ws_session *s, float *out, uint32_t capacity)
{
    if (s == NULL || out == NULL || capacity == 0u) {
        return 0u;
    }
    const ws_ecology *e = &s->eco;
    uint32_t n = e->sample_count;
    if (n > capacity) {
        n = capacity;
    }
    /* The ring holds the most recent n samples; oldest is (head - count) mod. */
    uint32_t start = (e->sample_head + WS_ECOLOGY_MAX_SAMPLES - e->sample_count) %
                     WS_ECOLOGY_MAX_SAMPLES;
    for (uint32_t i = 0u; i < n; ++i) {
        uint32_t idx = (start + (e->sample_count - n) + i) % WS_ECOLOGY_MAX_SAMPLES;
        for (int sp = 0; sp < WS_SPECIES_COUNT; ++sp) {
            out[i * (uint32_t)WS_SPECIES_COUNT + (uint32_t)sp] = e->pop_series[idx][sp];
        }
    }
    return n;
}

ws_region_coord ws_session_focus_region(const ws_session *s)
{
    return s != NULL ? s->host.focus : ws_region_coord_make(0, 0);
}

double ws_session_focus_x(const ws_session *s)
{
    if (s == NULL) { return 0.0; }
    double wx, wy;
    ws_region_center_world(&s->host.config.region, s->host.focus, &wx, &wy);
    return wx;
}

double ws_session_focus_y(const ws_session *s)
{
    if (s == NULL) { return 0.0; }
    double wx, wy;
    ws_region_center_world(&s->host.config.region, s->host.focus, &wx, &wy);
    return wy;
}

double ws_session_sun_altitude(const ws_session *s) { return s != NULL ? s->env.env.sun_altitude : 0.0; }
double ws_session_temperature_c(const ws_session *s) { return s != NULL ? s->env.env.temperature_c : 0.0; }
double ws_session_wind_speed(const ws_session *s) { return s != NULL ? s->env.env.wind_speed : 0.0; }
uint32_t ws_session_audio_voices(const ws_session *s) { return s != NULL ? ws_audio_active_voices(&s->audio) : 0u; }
uint64_t ws_session_audio_events(const ws_session *s) { return s != NULL ? s->audio.events_played : 0u; }

static const char *agent_activity(const ws_eco_agent *ag)
{
    if (ag->energy < 0.25) {
        return "starving";
    }
    if (ag->hunt_cooldown > 0.0) {
        return "hunting/resting";
    }
    const ws_species_info *si = ws_species_get((ws_species_kind)ag->species);
    if (si != NULL && si->diet == WS_DIET_CARNIVORE) {
        return "hunting";
    }
    if (ag->energy < 0.6) {
        return "foraging";
    }
    return "ranging";
}

int ws_session_selected(const ws_session *s, ws_session_agent_view *out)
{
    if (s == NULL || out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));
    if (s->selected < 0 || s->selected >= (int)s->actor_count) {
        return 0;
    }
    const ws_actor *a = &s->actors[s->selected];
    if (a->agent_index >= s->eco.alive_count) {
        return 0;
    }
    const ws_eco_agent *ag = &s->eco.agents[a->agent_index];
    if (!ag->alive) {
        return 0;
    }
    out->valid = 1;
    out->species = ag->species;
    out->species_name = ws_species_name((ws_species_kind)ag->species);
    out->energy = ag->energy;
    out->age_s = ag->age_s;
    out->x = ag->x;
    out->y = ag->y;
    out->heading = ag->heading;
    out->distance_m = ag->dist_travelled;
    out->bites = ag->bites;
    out->kills = ag->kills;
    out->activity = agent_activity(ag);
    /* Horizontal speed from the articulated body's torso velocity. */
    const ws_cbody *torso = &a->body.bodies[a->parts.torso];
    out->speed = sqrt(torso->vx * torso->vx + torso->vy * torso->vy);
    return 1;
}

uint32_t ws_session_timeline_count(const ws_session *s)
{
    return s != NULL ? s->timeline_count : 0u;
}

ws_result ws_session_timeline_at(const ws_session *s, uint32_t index,
                                 ws_session_event_view *out)
{
    if (s == NULL || out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (index >= s->timeline_count) {
        return WS_ERR_NOT_FOUND;
    }
    *out = s->timeline[index];
    return WS_OK;
}

uint32_t ws_session_map(const ws_session *s, ws_session_map_cell *out, uint32_t capacity)
{
    if (s == NULL || out == NULL) {
        return 0u;
    }
    uint32_t n = 0u;
    for (uint32_t i = 0; i < s->host.slot_count && n < capacity; ++i) {
        const ws_region_slot *sl = &s->host.slots[i];
        if (!sl->used) {
            continue;
        }
        out[n].region_x = sl->coord.x;
        out[n].region_y = sl->coord.y;
        out[n].level = sl->level;
        double pop = 0.0;
        for (int k = 0; k < WS_SPECIES_COUNT; ++k) {
            pop += sl->pop.count[k];
        }
        out[n].population = pop;
        out[n].is_focus = ws_region_coord_equal(sl->coord, s->host.focus);
        n += 1u;
    }
    return n;
}

/* --- camera ------------------------------------------------------------- */

static ws_camera camera_for_view(const ws_session *s)
{
    if (s->view_mode == WS_VIEW_FREE) {
        return s->free_camera;
    }
    if (s->view_mode == WS_VIEW_MAP) {
        double cx = WS_SESSION_PATCH_M * 0.5;
        ws_camera c = ws_camera_default();
        c.eye = ws_vec3_make(cx, 900.0, cx + 1.0);
        c.target = ws_vec3_make(cx, 0.0, cx);
        c.far_z = 4000.0;
        return c;
    }
    if (s->view_mode == WS_VIEW_FOLLOW && s->selected >= 0 &&
        s->selected < (int)s->actor_count) {
        const ws_actor *a = &s->actors[s->selected];
        double h = a->heading;
        double lx = a->wx - s->env.terrain.origin_x;
        double lz = a->wz - s->env.terrain.origin_y;
        double gy = ws_terrain_height_bilinear(&s->env.terrain, a->wx, a->wz);
        ws_camera c = ws_camera_default();
        c.eye = ws_vec3_make(lx - cos(h) * 5.0, gy + 2.6, lz - sin(h) * 5.0);
        c.target = ws_vec3_make(lx, gy + 1.0, lz);
        c.far_z = s->env.extent * 8.0;
        return c;
    }
    /* AUTO: the director's camera, lifted onto the terrain. When it has no
     * shot yet, fall back to a fixed overview of the focus region. */
    const ws_camera *dc = ws_director_camera(&s->director);
    if (dc != NULL && ws_director_current_shot(&s->director) != NULL) {
        ws_camera c = *dc;
        double eh = ws_terrain_height_bilinear(&s->env.terrain, dc->eye.x, dc->eye.z);
        double th = ws_terrain_height_bilinear(&s->env.terrain, dc->target.x, dc->target.z);
        c.eye.y = eh + dc->eye.y;
        c.target.y = th + dc->target.y;
        c.far_z = s->env.extent * 8.0;
        return c;
    }
    double mid = WS_SESSION_PATCH_M * 0.5;
    double gy = ws_terrain_height_bilinear(&s->env.terrain, mid, mid);
    ws_camera c = ws_camera_default();
    c.eye = ws_vec3_make(mid - 60.0, gy + 28.0, mid - 60.0);
    c.target = ws_vec3_make(mid, gy, mid);
    c.far_z = s->env.extent * 8.0;
    return c;
}

ws_camera ws_session_camera(const ws_session *s)
{
    if (s == NULL) {
        return ws_camera_default();
    }
    return camera_for_view(s);
}

/* --- render ------------------------------------------------------------- */

static void add_actor_objects(ws_session *s, ws_actor *a)
{
    const ws_quad_parts *p = &a->parts;
    /* Body colour per species (matches the reference scene palette). */
    uint8_t base[3];
    switch (a->species) {
    case WS_SPECIES_DEER: base[0] = 122u; base[1] = 82u; base[2] = 52u; break;
    case WS_SPECIES_FOX: base[0] = 176u; base[1] = 96u; base[2] = 44u; break;
    case WS_SPECIES_RABBIT: base[0] = 150u; base[1] = 138u; base[2] = 112u; break;
    case WS_SPECIES_BOAR: base[0] = 78u; base[1] = 70u; base[2] = 60u; break;
    default: base[0] = 120u; base[1] = 100u; base[2] = 80u; break;
    }
    ws_material body = ws_material_make(base[0] / 255.0, base[1] / 255.0,
                                        base[2] / 255.0, 0.85, 0.0, 0.05);
    ws_material dark = ws_material_make(base[0] / 320.0, base[1] / 320.0,
                                        base[2] / 320.0, 0.85, 0.0, 0.05);

    const ws_cbody *torso = &a->body.bodies[p->torso];
    double ox = torso->px;
    double oy = torso->py;
    double lx = a->wx - s->env.terrain.origin_x;
    double lz = a->wz - s->env.terrain.origin_y;
    double gy = ws_terrain_height_bilinear(&s->env.terrain, a->wx, a->wz);
    double ch = cos(a->heading);
    double sh = sin(a->heading);
    const double leg_z = 0.12;

    for (uint32_t i = 0; i < a->body.body_count; ++i) {
        const ws_cbody *b = &a->body.bodies[i];
        int is_leg = ((int)i == p->front_upper || (int)i == p->front_lower ||
                      (int)i == p->rear_upper || (int)i == p->rear_lower);
        ws_material m = is_leg ? dark : body;
        double sx = b->half_len * 2.0;
        double sy = b->half_thick * 2.0;
        double sz = b->half_width * 2.0;
        /* Sagittal forward offset of this link's centre from the torso. */
        double fwd = b->px - ox;
        double lift = b->py - oy;
        double px = lx + fwd * ch;
        double pz = lz + fwd * sh;
        double py = gy + lift;
        int sides = is_leg ? 2 : 1;
        for (int sgn = 0; sgn < sides; ++sgn) {
            double side = is_leg ? (sgn == 0 ? -1.0 : 1.0) : 0.0;
            /* Yaw about the vertical, then the link's own sagittal pitch. */
            ws_mat4 t = ws_mat4_translation(ws_vec3_make(px, py, pz));
            t = ws_mat4_multiply(t, ws_mat4_rotation_y(-a->heading));
            t = ws_mat4_multiply(t, ws_mat4_translation(ws_vec3_make(0.0, 0.0, side * leg_z)));
            t = ws_mat4_multiply(t, ws_mat4_rotation_z(b->angle));
            t = ws_mat4_multiply(t, ws_mat4_scale(ws_vec3_make(sx, sy, sz)));
            if (ws_scene_add_object(&s->env.scene, &s->env.cube_mesh, t, m) != WS_OK) {
                return;
            }
        }
    }
}

ws_result ws_session_render(ws_session *s, ws_framebuffer *fb, float *depth)
{
    if (s == NULL || fb == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    /* The focus region's scene supplies terrain, water and trees; rebuild it if
     * the camera focus moved to a new region. */
    ws_result r = rebuild_env(s, s->host.focus.x, s->host.focus.y);
    if (r != WS_OK) {
        return r;
    }
    /* Terrain is the ground for every actor's sagittal profile. */
    for (uint32_t k = 0; k < s->actor_count; ++k) {
        s->actors[k].terrain = &s->env.terrain;
    }

    /* Reset the dynamic part of the object list, then add the actors. */
    s->env.scene.object_count = s->static_object_count;
    for (uint32_t k = 0; k < s->actor_count; ++k) {
        if (s->actors[k].active) {
            add_actor_objects(s, &s->actors[k]);
        }
    }
    s->env.scene.camera = camera_for_view(s);
    /* Day/night follows the session clock. */
    ws_environment_update(&s->env.env, s->time_of_day_s,
                          s->env.terrain.config.base_height);
    /* The user toggles directional shadows with T; off by default so the live
     * view matches the golden references unless asked otherwise. */
    s->env.scene.settings.shadows_enabled = s->shadows_visible ? 1 : 0;
    return ws_render_scene(&s->env.scene, fb, depth, 32u, NULL);
}

/* --- checkpoint --------------------------------------------------------- */

static ws_result write_ecology(ws_writer *w, const ws_ecology *e)
{
    ws_result r;
    r = ws_writer_u64(w, e->config.seed); if (r) return r;
    r = ws_writer_f64(w, e->config.area_m); if (r) return r;
    r = ws_writer_u32(w, e->config.resource_cells); if (r) return r;
    r = ws_writer_f64(w, e->time_s); if (r) return r;
    r = ws_writer_u32(w, e->alive_count); if (r) return r;
    r = ws_writer_u32(w, e->capacity); if (r) return r;
    r = ws_writer_u64(w, e->births); if (r) return r;
    r = ws_writer_u64(w, e->deaths); if (r) return r;
    r = ws_writer_u64(w, e->predation_events); if (r) return r;
    r = ws_writer_u64(w, e->forage_events); if (r) return r;
    r = ws_writer_u64(w, e->starvation_deaths); if (r) return r;
    r = ws_writer_u64(w, e->age_deaths); if (r) return r;
    r = ws_writer_u64(w, e->next_event_sequence); if (r) return r;
    r = ws_writer_u64(w, e->events_total); if (r) return r;
    r = ws_writer_u64(w, e->events_dropped); if (r) return r;
    r = ws_writer_u32(w, e->event_count); if (r) return r;
    r = ws_writer_u64(w, e->rng.state); if (r) return r;
    r = ws_writer_u64(w, e->rng.draws); if (r) return r;

    /* Plant grid. */
    size_t cells = (size_t)e->config.resource_cells * e->config.resource_cells;
    r = ws_writer_u32(w, (uint32_t)cells); if (r) return r;
    for (size_t i = 0; i < cells; ++i) {
        r = ws_writer_f64(w, e->plants[i]); if (r) return r;
    }
    /* Agents (field by field, deterministic and endianness-explicit). */
    r = ws_writer_u32(w, e->capacity); if (r) return r;
    for (uint32_t i = 0; i < e->capacity; ++i) {
        const ws_eco_agent *a = &e->agents[i];
        r = ws_writer_u8(w, (uint8_t)(a->alive ? 1 : 0)); if (r) return r;
        r = ws_writer_u32(w, (uint32_t)a->species); if (r) return r;
        r = ws_writer_u32(w, (uint32_t)a->sex); if (r) return r;
        r = ws_writer_f64(w, a->x); if (r) return r;
        r = ws_writer_f64(w, a->y); if (r) return r;
        r = ws_writer_f64(w, a->heading); if (r) return r;
        r = ws_writer_f64(w, a->energy); if (r) return r;
        r = ws_writer_f64(w, a->age_s); if (r) return r;
        r = ws_writer_f64(w, a->dist_travelled); if (r) return r;
        r = ws_writer_f64(w, a->hunt_cooldown); if (r) return r;
        r = ws_writer_u64(w, a->bites); if (r) return r;
        r = ws_writer_u64(w, a->kills); if (r) return r;
        r = ws_writer_u32(w, a->offspring); if (r) return r;
    }
    /* Retained life events. */
    r = ws_writer_u32(w, e->event_count); if (r) return r;
    for (uint32_t i = 0; i < e->event_count; ++i) {
        const ws_event *ev = &e->events[i];
        r = ws_writer_u64(w, ev->sequence); if (r) return r;
        r = ws_writer_u64(w, ev->tick); if (r) return r;
        r = ws_writer_u16(w, ev->type); if (r) return r;
        r = ws_writer_u16(w, ev->payload_size); if (r) return r;
        r = ws_writer_bytes(w, ev->payload, WS_EVENT_PAYLOAD_MAX); if (r) return r;
    }
    return WS_OK;
}

static ws_result read_ecology(ws_reader *rd, ws_ecology *e)
{
    ws_result r;
    uint64_t seed = 0u;
    r = ws_reader_u64(rd, &seed); if (r) return r;
    ws_eco_config cfg = ws_eco_config_default(seed);
    r = ws_reader_f64(rd, &cfg.area_m); if (r) return r;
    r = ws_reader_u32(rd, &cfg.resource_cells); if (r) return r;
    memset(e, 0, sizeof(*e));
    e->config = cfg;
    e->capacity = WS_ECOLOGY_MAX_AGENTS;
    e->cell_size = cfg.area_m / (double)cfg.resource_cells;
    r = ws_reader_f64(rd, &e->time_s); if (r) return r;
    r = ws_reader_u32(rd, &e->alive_count); if (r) return r;
    r = ws_reader_u32(rd, &e->capacity); if (r) return r;
    r = ws_reader_u64(rd, &e->births); if (r) return r;
    r = ws_reader_u64(rd, &e->deaths); if (r) return r;
    r = ws_reader_u64(rd, &e->predation_events); if (r) return r;
    r = ws_reader_u64(rd, &e->forage_events); if (r) return r;
    r = ws_reader_u64(rd, &e->starvation_deaths); if (r) return r;
    r = ws_reader_u64(rd, &e->age_deaths); if (r) return r;
    r = ws_reader_u64(rd, &e->next_event_sequence); if (r) return r;
    r = ws_reader_u64(rd, &e->events_total); if (r) return r;
    r = ws_reader_u64(rd, &e->events_dropped); if (r) return r;
    r = ws_reader_u32(rd, &e->event_count); if (r) return r;
    r = ws_reader_u64(rd, &e->rng.state); if (r) return r;
    r = ws_reader_u64(rd, &e->rng.draws); if (r) return r;
    e->rng.seed = seed;
    e->rng.stream_id = 0x7007u;

    uint32_t cells = 0u;
    r = ws_reader_u32(rd, &cells); if (r) return r;
    if (cells != (uint32_t)(cfg.resource_cells * cfg.resource_cells)) {
        return WS_ERR_CORRUPT;
    }
    e->plants = (double *)malloc((size_t)cells * sizeof(double));
    if (e->plants == NULL) {
        return WS_ERR_OUT_OF_MEMORY;
    }
    for (uint32_t i = 0; i < cells; ++i) {
        r = ws_reader_f64(rd, &e->plants[i]); if (r) return r;
    }
    uint32_t cap = 0u;
    r = ws_reader_u32(rd, &cap); if (r) return r;
    if (cap > WS_ECOLOGY_MAX_AGENTS) {
        return WS_ERR_CORRUPT;
    }
    for (uint32_t i = 0; i < cap; ++i) {
        ws_eco_agent *a = &e->agents[i];
        uint8_t alive = 0u;
        uint32_t sp = 0u, sx = 0u;
        r = ws_reader_u8(rd, &alive); if (r) return r;
        r = ws_reader_u32(rd, &sp); if (r) return r;
        r = ws_reader_u32(rd, &sx); if (r) return r;
        a->alive = alive;
        a->species = (int)sp;
        a->sex = (int)sx;
        r = ws_reader_f64(rd, &a->x); if (r) return r;
        r = ws_reader_f64(rd, &a->y); if (r) return r;
        r = ws_reader_f64(rd, &a->heading); if (r) return r;
        r = ws_reader_f64(rd, &a->energy); if (r) return r;
        r = ws_reader_f64(rd, &a->age_s); if (r) return r;
        r = ws_reader_f64(rd, &a->dist_travelled); if (r) return r;
        r = ws_reader_f64(rd, &a->hunt_cooldown); if (r) return r;
        r = ws_reader_u64(rd, &a->bites); if (r) return r;
        r = ws_reader_u64(rd, &a->kills); if (r) return r;
        r = ws_reader_u32(rd, &a->offspring); if (r) return r;
    }
    uint32_t evc = 0u;
    r = ws_reader_u32(rd, &evc); if (r) return r;
    if (evc > WS_ECOLOGY_MAX_EVENTS) {
        return WS_ERR_CORRUPT;
    }
    for (uint32_t i = 0; i < evc; ++i) {
        ws_event *ev = &e->events[i];
        r = ws_reader_u64(rd, &ev->sequence); if (r) return r;
        r = ws_reader_u64(rd, &ev->tick); if (r) return r;
        r = ws_reader_u16(rd, &ev->type); if (r) return r;
        r = ws_reader_u16(rd, &ev->payload_size); if (r) return r;
        r = ws_reader_bytes(rd, ev->payload, WS_EVENT_PAYLOAD_MAX); if (r) return r;
    }
    return WS_OK;
}

/* Serialize the whole session into a freshly allocated buffer. The caller owns
 * the buffer. Shared by the file checkpoint (ws_session_save) and the replay
 * ring, so a recorded state and a saved checkpoint are byte-identical. */
static ws_result session_serialize(ws_session *s, uint8_t **out_buf, size_t *out_len)
{
    *out_buf = NULL;
    *out_len = 0u;
    size_t cap = 8u * 1024u * 1024u;
    uint8_t *buf = (uint8_t *)malloc(cap);
    if (buf == NULL) {
        return WS_ERR_OUT_OF_MEMORY;
    }
    ws_writer w;
    ws_writer_init(&w, buf, cap);
    ws_result r;
    r = ws_writer_bytes(&w, WS_SESSION_MAGIC, 4u); if (r) { free(buf); return r; }
    r = ws_writer_u32(&w, WS_SESSION_VERSION); if (r) { free(buf); return r; }
    r = ws_writer_u64(&w, s->config.seed); if (r) { free(buf); return r; }
    r = ws_writer_u64(&w, s->config.world_uid); if (r) { free(buf); return r; }
    r = ws_writer_u64(&w, s->steps); if (r) { free(buf); return r; }
    r = ws_writer_f64(&w, s->time_of_day_s); if (r) { free(buf); return r; }
    r = ws_writer_u32(&w, (uint32_t)s->speed_index); if (r) { free(buf); return r; }
    r = ws_writer_u32(&w, (uint32_t)(s->paused ? 1 : 0)); if (r) { free(buf); return r; }
    r = ws_writer_u32(&w, (uint32_t)s->view_mode); if (r) { free(buf); return r; }
    r = ws_writer_u32(&w, (uint32_t)(s->selected < 0 ? 0xFFFFFFFFu : (uint32_t)s->selected));
    if (r) { free(buf); return r; }

    /* Host state, length-prefixed. */
    size_t hcap = 4u * 1024u * 1024u;
    uint8_t *hbuf = (uint8_t *)malloc(hcap);
    if (hbuf == NULL) { free(buf); return WS_ERR_OUT_OF_MEMORY; }
    size_t hlen = 0u;
    r = ws_world_host_save(&s->host, hbuf, hcap, &hlen);
    if (r) { free(hbuf); free(buf); return r; }
    r = ws_writer_u32(&w, (uint32_t)hlen);
    if (!r) { r = ws_writer_bytes(&w, hbuf, hlen); }
    free(hbuf);
    if (r) { free(buf); return r; }

    r = write_ecology(&w, &s->eco);
    if (r) { free(buf); return r; }

    *out_buf = buf;
    *out_len = ws_writer_size(&w);
    return WS_OK;
}

ws_result ws_session_save(ws_session *s, const char *path)
{
    if (s == NULL || path == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    uint8_t *buf = NULL;
    size_t len = 0u;
    ws_result r = session_serialize(s, &buf, &len);
    if (r != WS_OK) {
        return r;
    }
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        free(buf);
        return WS_ERR_IO;
    }
    size_t wrote = fwrite(buf, 1u, len, f);
    fclose(f);
    free(buf);
    return wrote == len ? WS_OK : WS_ERR_IO;
}

ws_result ws_session_load(ws_session *s, const char *path)
{
    if (s == NULL || path == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return WS_ERR_IO;
    }
    if (fseek(f, 0L, SEEK_END) != 0) { fclose(f); return WS_ERR_IO; }
    long sz = ftell(f);
    if (sz <= 0) { fclose(f); return WS_ERR_CORRUPT; }
    rewind(f);
    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (buf == NULL) { fclose(f); return WS_ERR_OUT_OF_MEMORY; }
    size_t got = fread(buf, 1u, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz) { free(buf); return WS_ERR_IO; }

    ws_result r = session_deserialize(s, buf, (size_t)sz);
    free(buf);
    if (r == WS_OK) {
        /* A loaded checkpoint is a new world state; drop history recorded before
         * the load and start a fresh recording from here. */
        replay_ring_clear(s);
        s->replay_next_s = (double)s->steps * s->config.tick_s +
                           WS_SESSION_REPLAY_INTERVAL_S;
    }
    return r;
}

/* Restore a session from a serialized buffer (the inverse of session_serialize).
 * Does not free `bytes`; the caller owns it. Shared by the file checkpoint
 * (ws_session_load) and the replay ring. */
static ws_result session_deserialize(ws_session *s, const uint8_t *bytes, size_t len)
{
    ws_reader rd;
    ws_reader_init(&rd, bytes, len);
    char magic[4];
    ws_result r;
    r = ws_reader_bytes(&rd, magic, 4u);
    if (r || memcmp(magic, WS_SESSION_MAGIC, 4u) != 0) { return WS_ERR_CORRUPT; }
    uint32_t version = 0u;
    r = ws_reader_u32(&rd, &version);
    if (r || version != WS_SESSION_VERSION) { return WS_ERR_CORRUPT; }

    uint64_t seed = 0u, uid = 0u;
    r = ws_reader_u64(&rd, &seed); if (r) { return r; }
    r = ws_reader_u64(&rd, &uid); if (r) { return r; }
    r = ws_reader_u64(&rd, &s->steps); if (r) { return r; }
    r = ws_reader_f64(&rd, &s->time_of_day_s); if (r) { return r; }
    uint32_t speed = 0u, paused = 0u, view = 0u, sel = 0u;
    r = ws_reader_u32(&rd, &speed); if (r) { return r; }
    r = ws_reader_u32(&rd, &paused); if (r) { return r; }
    r = ws_reader_u32(&rd, &view); if (r) { return r; }
    r = ws_reader_u32(&rd, &sel); if (r) { return r; }
    s->config.seed = seed;
    s->config.world_uid = uid;
    s->speed_index = (int)(speed < WS_SESSION_SPEEDS ? speed : 2u);
    s->paused = (int)paused;
    s->view_mode = (int)(view < WS_VIEW_COUNT ? view : 0u);
    s->selected = (sel == 0xFFFFFFFFu) ? -1 : (int)sel;

    uint32_t hlen = 0u;
    r = ws_reader_u32(&rd, &hlen);
    if (r || hlen > ws_reader_remaining(&rd)) { return WS_ERR_CORRUPT; }
    r = ws_world_host_load(&s->host, rd.buffer + rd.offset, hlen);
    if (r) { return r; }
    rd.offset += hlen;

    /* The previous ecology owns a plant grid; free it before overwriting. */
    if (s->eco.plants != NULL) {
        ws_ecology_free(&s->eco);
    }
    r = read_ecology(&rd, &s->eco);
    if (r) { return r; }

    s->env_region_valid = 0;
    rebuild_env(s, s->host.focus.x, s->host.focus.y);
    rebuild_actors(s);
    s->host_ev_cursor = 0u;
    s->eco_event_cursor = 0u;
    s->timeline_count = 0u;
    return WS_OK;
}

ws_result ws_session_set_checkpoint_path(ws_session *s, const char *path)
{
    if (s == NULL || path == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    size_t n = strlen(path);
    if (n + 1u > sizeof(s->save_path)) {
        return WS_ERR_CAPACITY;
    }
    memcpy(s->save_path, path, n + 1u);
    return WS_OK;
}

ws_result ws_session_digest(ws_session *s, char out_hex[WS_SESSION_DIGEST_HEX])
{
    if (s == NULL || out_hex == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    char host_hex[WS_HOST_DIGEST_HEX];
    ws_result r = ws_world_host_digest(&s->host, host_hex);
    if (r != WS_OK) {
        return r;
    }
    /* Combine the host digest with the focus-ecology state so both the
     * multi-scale world and the high-detail patch are covered. */
    ws_sha256 ctx;
    ws_sha256_init(&ctx);
    ws_sha256_update(&ctx, host_hex, strlen(host_hex));
    uint64_t fields[4] = {s->eco.alive_count, s->eco.births, s->eco.deaths,
                          s->eco.rng.state};
    ws_sha256_update(&ctx, fields, sizeof(fields));
    for (uint32_t i = 0; i < s->eco.alive_count; ++i) {
        const ws_eco_agent *a = &s->eco.agents[i];
        double v[5] = {a->x, a->y, a->heading, a->energy, a->age_s};
        uint8_t sp = (uint8_t)a->species;
        ws_sha256_update(&ctx, &sp, 1u);
        ws_sha256_update(&ctx, v, sizeof(v));
    }
    uint8_t digest[WS_SHA256_DIGEST_SIZE];
    ws_sha256_final(&ctx, digest);
    ws_sha256_hex(digest, out_hex);
    return WS_OK;
}
