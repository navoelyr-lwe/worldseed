/* Worldseed Engine — persistent, multi-scale world host (P8).
 *
 * The host is the authority that owns a set of loaded regions around a moving
 * focus and advances them independently of the camera (docs/04 "kamera
 * taşınırken simülasyon adımı durmaz"). It provides the four things docs/13
 * asks of P8:
 *
 *  - Region streaming: as the focus moves, regions enter and leave a fixed
 *    loaded set; entering/leaving is recorded in the journal. Leaving does not
 *    lose state: a recently unloaded region keeps its evolved aggregate state in
 *    a bounded eviction cache, so re-entering restores it exactly. A region that
 *    has aged out of the cache is re-derived and re-seeded from its
 *    deterministic hash (the documented offline model).
 *  - Simulation levels: loaded regions are assigned NEAR / MID / FAR by
 *    Chebyshev distance from the focus. Each level advances the aggregate
 *    population model (ws_population) at its own cadence (1 s, 4 s, 16 s), so
 *    distant regions cost less to advance while the world still progresses.
 *  - Journal: every load, unload, level change, focus move and species
 *    extinction is appended to a fixed-size, ordered event ring, so the
 *    cinematic director can later observe real events (never invent them).
 *  - Checkpoint / replay: the whole host state serialises field by field with
 *    explicit endianness (ws_serialize); a checkpoint plus the recorded focus
 *    path replays bit-identically, which the tests assert with a SHA-256 digest.
 *
 * Storage is caller-owned and fixed: the slot array has a compile-time size and
 * nothing is allocated, so streaming cost is bounded and deterministic.
 * SI units and seconds.
 */
#ifndef WS_WORLD_HOST_H
#define WS_WORLD_HOST_H

#include <stddef.h>
#include <stdint.h>

#include "../core/ws_result.h"
#include "../life/ws_population.h"
#include "ws_event.h"
#include "ws_region.h"

#define WS_HOST_VERSION 1u
#define WS_HOST_MAX_LOADED 64u
/* Sized to hold two full loaded sets, so an unload/load round trip (the camera
 * leaving and returning) always restores evolved state without a reseed. */
#define WS_HOST_EVICT_CACHE 128u
#define WS_HOST_MAX_EVENTS 512u
#define WS_HOST_DIGEST_HEX 65u /* 64 hex chars + NUL */

/* Simulation level of a loaded region, by Chebyshev distance from the focus. */
typedef enum ws_world_level {
    WS_LEVEL_UNLOADED = 0,
    WS_LEVEL_FAR = 1,
    WS_LEVEL_MID = 2,
    WS_LEVEL_NEAR = 3
} ws_world_level;

/* Journal event kinds. Stored in a ws_event payload's first byte so the shared
 * event stream type stays WS_EVENT_CUSTOM. */
typedef enum ws_host_event_kind {
    WS_HOST_EV_REGION_LOADED = 0,
    WS_HOST_EV_REGION_UNLOADED = 1,
    WS_HOST_EV_LEVEL_CHANGED = 2,
    WS_HOST_EV_SPECIES_EXTINCT = 3,
    WS_HOST_EV_FOCUS_MOVED = 4
} ws_host_event_kind;

typedef struct ws_world_host_config {
    uint64_t world_uid;
    uint64_t seed;
    ws_region_config region;
    ws_pop_config pop;
    uint32_t radius_near; /* Chebyshev radius at which a region is NEAR (>=1) */
    uint32_t radius_mid;  /* outer MID radius; beyond it a region is FAR */
    uint32_t radius_load; /* outer loaded radius; beyond it a region is unloaded */
} ws_world_host_config;

ws_world_host_config ws_world_host_config_default(uint64_t seed, uint64_t world_uid);

typedef struct ws_region_slot {
    int used;
    int level;             /* ws_world_level */
    ws_region_coord coord;
    uint64_t hash;
    ws_pop_state pop;      /* aggregate state (all levels) */
    double last_step_s;    /* host time the slot was last advanced */
    uint32_t extinct_mask; /* bit k set once species k was journalled extinct */
} ws_region_slot;

typedef struct ws_world_host {
    ws_world_host_config config;
    uint64_t tick;
    double time_s;
    ws_region_coord focus;

    ws_region_slot slots[WS_HOST_MAX_LOADED];
    uint32_t slot_count;

    /* Eviction cache: recently unloaded regions keep their evolved aggregate
     * state so that leaving and re-entering the loaded set does not lose state
     * (docs/04 "bölge boşalt/yükle çevrimi durum kaybettirmez"). Evicted regions
     * that fall out of this small cache are re-derived from their deterministic
     * hash plus the host's elapsed time, which is the documented offline model. */
    ws_region_slot evict[WS_HOST_EVICT_CACHE];
    uint32_t evict_count;

    ws_event events[WS_HOST_MAX_EVENTS];
    uint32_t event_count;
    uint64_t next_sequence;
    uint64_t events_dropped;

    uint64_t total_loads;
    uint64_t total_unloads;
    uint64_t extinctions;
} ws_world_host;

ws_result ws_world_host_init(ws_world_host *host, const ws_world_host_config *config);

/* Move the focus to the region containing world position (wx, wy). Loads and
 * unloads regions to keep the loaded set around the focus, reassigns levels and
 * journals every change. Idempotent when the focus region does not change. */
ws_result ws_world_host_set_focus_world(ws_world_host *host, double wx, double wy);

/* Move the focus to an explicit region coordinate. */
ws_result ws_world_host_set_focus(ws_world_host *host, ws_region_coord coord);

/* Advance the host by `dt` seconds: every loaded region is advanced at its
 * level's cadence, and extinctions are journalled. dt must be positive. */
ws_result ws_world_host_step(ws_world_host *host, double dt);

/* Advance by `seconds` in fixed 1 s authority ticks. */
ws_result ws_world_host_run(ws_world_host *host, double seconds);

/* Look up a loaded slot by coordinate, or NULL when not loaded. */
const ws_region_slot *ws_world_host_find(const ws_world_host *host, ws_region_coord coord);

/* Level of a region (WS_LEVEL_UNLOADED when not loaded). */
int ws_world_host_level(const ws_world_host *host, ws_region_coord coord);

uint32_t ws_world_host_loaded_count(const ws_world_host *host);

/* Journal access. Index 0 is the oldest retained event. */
uint32_t ws_world_host_event_count(const ws_world_host *host);
ws_result ws_world_host_event_at(const ws_world_host *host, uint32_t index, ws_event *out);
uint64_t ws_world_host_events_dropped(const ws_world_host *host);
const char *ws_host_event_kind_name(uint8_t kind);

/* Checkpoint. Serialise the whole host into `buffer` (caller-owned). On success
 * `*out_length` is the number of bytes written. WS_ERR_CAPACITY when the buffer
 * is too small. */
ws_result ws_world_host_save(const ws_world_host *host, uint8_t *buffer,
                             size_t capacity, size_t *out_length);

/* Restore a host from a checkpoint produced by ws_world_host_save. `host` must
 * be zeroed or previously initialised; its config is taken from the checkpoint.
 * Returns WS_ERR_CORRUPT when the bytes are damaged or truncated. */
ws_result ws_world_host_load(ws_world_host *host, const uint8_t *buffer, size_t length);

/* SHA-256 of the host's serialised state, lowercase hex plus NUL. Two hosts in
 * the same state produce the same digest, which is how replay equality is
 * asserted. */
ws_result ws_world_host_digest(const ws_world_host *host, char out_hex[WS_HOST_DIGEST_HEX]);

/* Replay helper: build a host, walk the focus along `focus_path` (one region per
 * `leg_seconds`), then write the final digest. Used by tests and the CLI to show
 * that a checkpoint plus the focus path reproduces a run exactly. */
ws_result ws_world_host_replay(const ws_world_host_config *config,
                               const ws_region_coord *focus_path, uint32_t path_length,
                               double leg_seconds, double settle_seconds,
                               char out_hex[WS_HOST_DIGEST_HEX]);

#endif /* WS_WORLD_HOST_H */
