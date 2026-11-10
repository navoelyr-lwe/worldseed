/* Worldseed Engine — persistent, multi-scale world host (P8). */
#include "ws_world_host.h"

#include "../core/ws_hash.h"
#include "../core/ws_serialize.h"

#include <math.h>
#include <string.h>

#define WS_HOST_TICK_S 1.0
#define WS_HOST_NEAR_CADENCE_S 1.0
#define WS_HOST_MID_CADENCE_S 4.0
#define WS_HOST_FAR_CADENCE_S 16.0
/* Generous upper bound: slots and the event ring dominate; well under this. */
#define WS_HOST_CHECKPOINT_MAX (64u * 1024u)

ws_world_host_config ws_world_host_config_default(uint64_t seed, uint64_t world_uid)
{
    ws_world_host_config c;
    memset(&c, 0, sizeof(c));
    c.world_uid = world_uid;
    c.seed = seed;
    c.region = ws_region_config_default(seed);
    c.pop = ws_pop_config_default(seed);
    c.radius_near = 1u;
    c.radius_mid = 2u;
    c.radius_load = 3u;
    return c;
}

static double level_cadence(int level)
{
    switch (level) {
    case WS_LEVEL_NEAR: return WS_HOST_NEAR_CADENCE_S;
    case WS_LEVEL_MID:  return WS_HOST_MID_CADENCE_S;
    case WS_LEVEL_FAR:  return WS_HOST_FAR_CADENCE_S;
    default:            return WS_HOST_FAR_CADENCE_S;
    }
}

static int level_for_distance(const ws_world_host_config *cfg, int64_t d)
{
    if (d <= (int64_t)cfg->radius_near) {
        return WS_LEVEL_NEAR;
    }
    if (d <= (int64_t)cfg->radius_mid) {
        return WS_LEVEL_MID;
    }
    if (d <= (int64_t)cfg->radius_load) {
        return WS_LEVEL_FAR;
    }
    return WS_LEVEL_UNLOADED;
}

const char *ws_host_event_kind_name(uint8_t kind)
{
    switch (kind) {
    case WS_HOST_EV_REGION_LOADED: return "region_loaded";
    case WS_HOST_EV_REGION_UNLOADED: return "region_unloaded";
    case WS_HOST_EV_LEVEL_CHANGED: return "level_changed";
    case WS_HOST_EV_SPECIES_EXTINCT: return "species_extinct";
    case WS_HOST_EV_FOCUS_MOVED: return "focus_moved";
    default: return "unknown";
    }
}

/* Append a journal event. Payload layout: kind(1) + int32 x + int32 y +
 * int32 a + int32 b. Fields not used by a kind are zero. */
static void journal(ws_world_host *host, uint8_t kind,
                    int32_t x, int32_t y, int32_t a, int32_t b)
{
    uint8_t payload[WS_EVENT_PAYLOAD_MAX];
    memset(payload, 0, sizeof(payload));
    payload[0] = kind;
    /* little-endian int32 fields at offsets 1, 5, 9, 13 */
    uint32_t v;
    v = (uint32_t)x; memcpy(&payload[1], &v, 4);
    v = (uint32_t)y; memcpy(&payload[5], &v, 4);
    v = (uint32_t)a; memcpy(&payload[9], &v, 4);
    v = (uint32_t)b; memcpy(&payload[13], &v, 4);

    ws_event ev;
    ev.sequence = host->next_sequence++;
    ev.tick = host->tick;
    ev.type = WS_EVENT_CUSTOM;
    ev.payload_size = 17u;
    memcpy(ev.payload, payload, sizeof(ev.payload));
    if (host->event_count < WS_HOST_MAX_EVENTS) {
        host->events[host->event_count++] = ev;
    } else {
        /* Overwrite the oldest retained event; the ring stays ordered by age. */
        memmove(&host->events[0], &host->events[1],
                (WS_HOST_MAX_EVENTS - 1u) * sizeof(ws_event));
        host->events[WS_HOST_MAX_EVENTS - 1u] = ev;
        host->events_dropped += 1u;
    }
}

ws_result ws_world_host_init(ws_world_host *host, const ws_world_host_config *config)
{
    if (host == NULL || config == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (config->radius_near == 0u || config->radius_mid < config->radius_near ||
        config->radius_load < config->radius_mid) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    memset(host, 0, sizeof(*host));
    host->config = *config;
    host->tick = 0u;
    host->time_s = 0.0;
    host->focus = ws_region_coord_make(0, 0);
    host->next_sequence = 1u;
    return ws_world_host_set_focus(host, ws_region_coord_make(0, 0));
}

static ws_region_slot *find_slot(ws_world_host *host, ws_region_coord coord)
{
    for (uint32_t i = 0; i < host->slot_count; ++i) {
        if (host->slots[i].used && ws_region_coord_equal(host->slots[i].coord, coord)) {
            return &host->slots[i];
        }
    }
    return NULL;
}

const ws_region_slot *ws_world_host_find(const ws_world_host *host, ws_region_coord coord)
{
    if (host == NULL) {
        return NULL;
    }
    for (uint32_t i = 0; i < host->slot_count; ++i) {
        if (host->slots[i].used && ws_region_coord_equal(host->slots[i].coord, coord)) {
            return &host->slots[i];
        }
    }
    return NULL;
}

int ws_world_host_level(const ws_world_host *host, ws_region_coord coord)
{
    const ws_region_slot *s = ws_world_host_find(host, coord);
    return (s != NULL) ? s->level : WS_LEVEL_UNLOADED;
}

uint32_t ws_world_host_loaded_count(const ws_world_host *host)
{
    return host != NULL ? host->slot_count : 0u;
}

uint32_t ws_world_host_event_count(const ws_world_host *host)
{
    return host != NULL ? host->event_count : 0u;
}

ws_result ws_world_host_event_at(const ws_world_host *host, uint32_t index, ws_event *out)
{
    if (host == NULL || out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (index >= host->event_count) {
        return WS_ERR_NOT_FOUND;
    }
    *out = host->events[index];
    return WS_OK;
}

uint64_t ws_world_host_events_dropped(const ws_world_host *host)
{
    return host != NULL ? host->events_dropped : 0u;
}

/* Push an unloaded slot into the eviction cache (bounded FIFO). */
static void evict_push(ws_world_host *host, const ws_region_slot *slot)
{
    if (host->evict_count >= WS_HOST_EVICT_CACHE) {
        memmove(&host->evict[0], &host->evict[1],
                (WS_HOST_EVICT_CACHE - 1u) * sizeof(ws_region_slot));
        host->evict_count = WS_HOST_EVICT_CACHE - 1u;
    }
    host->evict[host->evict_count++] = *slot;
}

static ws_region_slot *evict_take(ws_world_host *host, ws_region_coord coord,
                                  ws_region_slot *out)
{
    for (uint32_t i = 0; i < host->evict_count; ++i) {
        if (ws_region_coord_equal(host->evict[i].coord, coord)) {
            *out = host->evict[i];
            memmove(&host->evict[i], &host->evict[i + 1u],
                    (host->evict_count - i - 1u) * sizeof(ws_region_slot));
            host->evict_count -= 1u;
            return out;
        }
    }
    return NULL;
}

/* Recompute the loaded set around the focus: unload out-of-range regions, load
 * newly in-range ones, and reassign levels. Journals each change. */
static ws_result reconcile(ws_world_host *host)
{
    int64_t r_load = (int64_t)host->config.radius_load;

    /* Unload regions now outside the loaded radius. Compact the array and keep
     * the evolved state in the eviction cache so a re-entry is seamless. */
    uint32_t w = 0u;
    for (uint32_t i = 0; i < host->slot_count; ++i) {
        ws_region_slot *s = &host->slots[i];
        if (!s->used) {
            continue;
        }
        int64_t d = ws_region_distance(s->coord, host->focus);
        if (d > r_load) {
            journal(host, WS_HOST_EV_REGION_UNLOADED,
                    (int32_t)s->coord.x, (int32_t)s->coord.y, s->level, 0);
            host->total_unloads += 1u;
            evict_push(host, s);
            continue; /* drop from the loaded set */
        }
        host->slots[w++] = *s;
    }
    host->slot_count = w;

    /* Load any in-range region that is missing. Iterate the square in a fixed
     * order so loads are deterministic. */
    for (int64_t dy = -r_load; dy <= r_load; ++dy) {
        for (int64_t dx = -r_load; dx <= r_load; ++dx) {
            ws_region_coord c = ws_region_coord_make(host->focus.x + dx, host->focus.y + dy);
            int64_t d = ws_region_distance(c, host->focus);
            int level = level_for_distance(&host->config, d);
            if (level == WS_LEVEL_UNLOADED) {
                continue;
            }
            ws_region_slot *s = find_slot(host, c);
            if (s == NULL) {
                if (host->slot_count >= WS_HOST_MAX_LOADED) {
                    return WS_ERR_CAPACITY;
                }
                s = &host->slots[host->slot_count++];
                memset(s, 0, sizeof(*s));
                ws_region_slot cached;
                if (evict_take(host, c, &cached) != NULL) {
                    /* Restore the evolved state; a region that re-enters keeps
                     * everything it accumulated while unloaded from the cache. */
                    *s = cached;
                    s->used = 1;
                } else {
                    s->used = 1;
                    s->coord = c;
                    s->hash = ws_region_hash(&host->config.region, c);
                    s->pop = ws_pop_seed(&host->config.pop, s->hash);
                    s->last_step_s = host->time_s;
                }
                s->level = level;
                journal(host, WS_HOST_EV_REGION_LOADED,
                        (int32_t)c.x, (int32_t)c.y, level, 0);
                host->total_loads += 1u;
            } else if (s->level != level) {
                journal(host, WS_HOST_EV_LEVEL_CHANGED,
                        (int32_t)c.x, (int32_t)c.y, s->level, level);
                s->level = level;
            }
        }
    }
    return WS_OK;
}

ws_result ws_world_host_set_focus(ws_world_host *host, ws_region_coord coord)
{
    if (host == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (ws_region_coord_equal(host->focus, coord) && host->slot_count > 0u) {
        return WS_OK;
    }
    host->focus = coord;
    journal(host, WS_HOST_EV_FOCUS_MOVED, (int32_t)coord.x, (int32_t)coord.y, 0, 0);
    return reconcile(host);
}

ws_result ws_world_host_set_focus_world(ws_world_host *host, double wx, double wy)
{
    if (host == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    ws_region_pos p = ws_region_from_world(&host->config.region, wx, wy);
    return ws_world_host_set_focus(host, p.region);
}

/* Advance one loaded region by the time elapsed since its last step, at its
 * level's cadence, and journal any species extinction. */
static ws_result advance_slot(ws_world_host *host, ws_region_slot *s)
{
    double cadence = level_cadence(s->level);
    double elapsed = host->time_s - s->last_step_s;
    if (elapsed + 1e-9 < cadence) {
        return WS_OK;
    }
    /* Step in fixed 1 s sub-steps so the result does not depend on the cadence
     * grid (a NEAR region stepping every second and the same region seen later
     * as FAR both advance by whole seconds). */
    double remaining = elapsed;
    while (remaining > 1e-9) {
        double sub = (remaining > 1.0) ? 1.0 : remaining;
        ws_result r = ws_pop_step(&host->config.pop, &s->pop, sub);
        if (r != WS_OK) {
            return r;
        }
        remaining -= sub;
    }
    s->last_step_s = host->time_s;

    for (int k = 0; k < WS_SPECIES_COUNT; ++k) {
        uint32_t bit = 1u << (uint32_t)k;
        if ((s->extinct_mask & bit) == 0u && s->pop.count[k] <= 0.0) {
            s->extinct_mask |= bit;
            host->extinctions += 1u;
            journal(host, WS_HOST_EV_SPECIES_EXTINCT,
                    (int32_t)s->coord.x, (int32_t)s->coord.y, k, 0);
        }
    }
    return WS_OK;
}

ws_result ws_world_host_step(ws_world_host *host, double dt)
{
    if (host == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (!(dt > 0.0) || !isfinite(dt)) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    host->time_s += dt;
    host->tick += 1u;
    for (uint32_t i = 0; i < host->slot_count; ++i) {
        ws_result r = advance_slot(host, &host->slots[i]);
        if (r != WS_OK) {
            return r;
        }
    }
    return WS_OK;
}

ws_result ws_world_host_run(ws_world_host *host, double seconds)
{
    if (host == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (!(seconds >= 0.0) || !isfinite(seconds)) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    double remaining = seconds;
    while (remaining > 1e-9) {
        double sub = (remaining > WS_HOST_TICK_S) ? WS_HOST_TICK_S : remaining;
        ws_result r = ws_world_host_step(host, sub);
        if (r != WS_OK) {
            return r;
        }
        remaining -= sub;
    }
    return WS_OK;
}

/* --- checkpoint ------------------------------------------------------- */

static ws_result write_pop(ws_writer *w, const ws_pop_state *s)
{
    ws_result r;
    r = ws_writer_f64(w, s->time_s); if (r != WS_OK) return r;
    r = ws_writer_f64(w, s->plant); if (r != WS_OK) return r;
    for (int k = 0; k < WS_SPECIES_COUNT; ++k) {
        r = ws_writer_f64(w, s->count[k]); if (r != WS_OK) return r;
    }
    r = ws_writer_f64(w, s->births); if (r != WS_OK) return r;
    r = ws_writer_f64(w, s->deaths); if (r != WS_OK) return r;
    return ws_writer_u64(w, s->extinctions);
}

static ws_result read_pop(ws_reader *r, ws_pop_state *s)
{
    ws_result res;
    res = ws_reader_f64(r, &s->time_s); if (res != WS_OK) return res;
    res = ws_reader_f64(r, &s->plant); if (res != WS_OK) return res;
    for (int k = 0; k < WS_SPECIES_COUNT; ++k) {
        res = ws_reader_f64(r, &s->count[k]); if (res != WS_OK) return res;
    }
    res = ws_reader_f64(r, &s->births); if (res != WS_OK) return res;
    res = ws_reader_f64(r, &s->deaths); if (res != WS_OK) return res;
    return ws_reader_u64(r, &s->extinctions);
}

ws_result ws_world_host_save(const ws_world_host *host, uint8_t *buffer,
                             size_t capacity, size_t *out_length)
{
    if (host == NULL || buffer == NULL || out_length == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    ws_writer w;
    ws_writer_init(&w, buffer, capacity);
    ws_result r;
    r = ws_writer_u32(&w, WS_HOST_VERSION); if (r != WS_OK) return r;
    r = ws_writer_u64(&w, host->config.world_uid); if (r != WS_OK) return r;
    r = ws_writer_u64(&w, host->config.seed); if (r != WS_OK) return r;
    r = ws_writer_u32(&w, host->config.region.cells); if (r != WS_OK) return r;
    r = ws_writer_f64(&w, host->config.region.cell_size); if (r != WS_OK) return r;
    r = ws_writer_u32(&w, host->config.radius_near); if (r != WS_OK) return r;
    r = ws_writer_u32(&w, host->config.radius_mid); if (r != WS_OK) return r;
    r = ws_writer_u32(&w, host->config.radius_load); if (r != WS_OK) return r;
    r = ws_writer_u64(&w, host->tick); if (r != WS_OK) return r;
    r = ws_writer_f64(&w, host->time_s); if (r != WS_OK) return r;
    r = ws_writer_u64(&w, (uint64_t)host->focus.x); if (r != WS_OK) return r;
    r = ws_writer_u64(&w, (uint64_t)host->focus.y); if (r != WS_OK) return r;
    r = ws_writer_u64(&w, host->total_loads); if (r != WS_OK) return r;
    r = ws_writer_u64(&w, host->total_unloads); if (r != WS_OK) return r;
    r = ws_writer_u64(&w, host->extinctions); if (r != WS_OK) return r;
    r = ws_writer_u32(&w, host->slot_count); if (r != WS_OK) return r;
    for (uint32_t i = 0; i < host->slot_count; ++i) {
        const ws_region_slot *s = &host->slots[i];
        r = ws_writer_u32(&w, (uint32_t)s->level); if (r != WS_OK) return r;
        r = ws_writer_u64(&w, (uint64_t)s->coord.x); if (r != WS_OK) return r;
        r = ws_writer_u64(&w, (uint64_t)s->coord.y); if (r != WS_OK) return r;
        r = ws_writer_u64(&w, s->hash); if (r != WS_OK) return r;
        r = ws_writer_f64(&w, s->last_step_s); if (r != WS_OK) return r;
        r = ws_writer_u32(&w, s->extinct_mask); if (r != WS_OK) return r;
        r = write_pop(&w, &s->pop); if (r != WS_OK) return r;
    }
    r = ws_writer_u32(&w, host->evict_count); if (r != WS_OK) return r;
    for (uint32_t i = 0; i < host->evict_count; ++i) {
        const ws_region_slot *s = &host->evict[i];
        r = ws_writer_u32(&w, (uint32_t)s->level); if (r != WS_OK) return r;
        r = ws_writer_u64(&w, (uint64_t)s->coord.x); if (r != WS_OK) return r;
        r = ws_writer_u64(&w, (uint64_t)s->coord.y); if (r != WS_OK) return r;
        r = ws_writer_u64(&w, s->hash); if (r != WS_OK) return r;
        r = ws_writer_f64(&w, s->last_step_s); if (r != WS_OK) return r;
        r = ws_writer_u32(&w, s->extinct_mask); if (r != WS_OK) return r;
        r = write_pop(&w, &s->pop); if (r != WS_OK) return r;
    }
    r = ws_writer_u32(&w, host->event_count); if (r != WS_OK) return r;
    r = ws_writer_u64(&w, host->next_sequence); if (r != WS_OK) return r;
    r = ws_writer_u64(&w, host->events_dropped); if (r != WS_OK) return r;
    for (uint32_t i = 0; i < host->event_count; ++i) {
        const ws_event *e = &host->events[i];
        r = ws_writer_u64(&w, e->sequence); if (r != WS_OK) return r;
        r = ws_writer_u64(&w, e->tick); if (r != WS_OK) return r;
        r = ws_writer_u16(&w, e->type); if (r != WS_OK) return r;
        r = ws_writer_u16(&w, e->payload_size); if (r != WS_OK) return r;
        r = ws_writer_bytes(&w, e->payload, WS_EVENT_PAYLOAD_MAX); if (r != WS_OK) return r;
    }
    *out_length = ws_writer_size(&w);
    return WS_OK;
}

static ws_result read_slot(ws_reader *r, ws_region_slot *s)
{
    uint32_t u32v = 0u;
    uint64_t u64v = 0u;
    ws_result res;
    s->used = 1;
    res = ws_reader_u32(r, &u32v); if (res != WS_OK) return res;
    s->level = (int)u32v;
    res = ws_reader_u64(r, &u64v); if (res != WS_OK) return res;
    s->coord.x = (int64_t)u64v;
    res = ws_reader_u64(r, &u64v); if (res != WS_OK) return res;
    s->coord.y = (int64_t)u64v;
    res = ws_reader_u64(r, &s->hash); if (res != WS_OK) return res;
    res = ws_reader_f64(r, &s->last_step_s); if (res != WS_OK) return res;
    res = ws_reader_u32(r, &s->extinct_mask); if (res != WS_OK) return res;
    return read_pop(r, &s->pop);
}

ws_result ws_world_host_load(ws_world_host *host, const uint8_t *buffer, size_t length)
{
    if (host == NULL || buffer == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    ws_reader r;
    ws_reader_init(&r, buffer, length);
    ws_result res;
    uint32_t version = 0u;
    res = ws_reader_u32(&r, &version); if (res != WS_OK) return WS_ERR_CORRUPT;
    if (version != WS_HOST_VERSION) {
        return WS_ERR_UNSUPPORTED;
    }

    ws_world_host h;
    memset(&h, 0, sizeof(h));
    uint64_t u64v = 0u;
    uint32_t u32v = 0u;
    res = ws_reader_u64(&r, &h.config.world_uid); if (res != WS_OK) return WS_ERR_CORRUPT;
    res = ws_reader_u64(&r, &h.config.seed); if (res != WS_OK) return WS_ERR_CORRUPT;
    res = ws_reader_u32(&r, &h.config.region.cells); if (res != WS_OK) return WS_ERR_CORRUPT;
    res = ws_reader_f64(&r, &h.config.region.cell_size); if (res != WS_OK) return WS_ERR_CORRUPT;
    res = ws_reader_u32(&r, &h.config.radius_near); if (res != WS_OK) return WS_ERR_CORRUPT;
    res = ws_reader_u32(&r, &h.config.radius_mid); if (res != WS_OK) return WS_ERR_CORRUPT;
    res = ws_reader_u32(&r, &h.config.radius_load); if (res != WS_OK) return WS_ERR_CORRUPT;
    h.config.region.seed = h.config.seed;
    h.config.pop = ws_pop_config_default(h.config.seed);
    res = ws_reader_u64(&r, &h.tick); if (res != WS_OK) return WS_ERR_CORRUPT;
    res = ws_reader_f64(&r, &h.time_s); if (res != WS_OK) return WS_ERR_CORRUPT;
    res = ws_reader_u64(&r, &u64v); if (res != WS_OK) return WS_ERR_CORRUPT;
    h.focus.x = (int64_t)u64v;
    res = ws_reader_u64(&r, &u64v); if (res != WS_OK) return WS_ERR_CORRUPT;
    h.focus.y = (int64_t)u64v;
    res = ws_reader_u64(&r, &h.total_loads); if (res != WS_OK) return WS_ERR_CORRUPT;
    res = ws_reader_u64(&r, &h.total_unloads); if (res != WS_OK) return WS_ERR_CORRUPT;
    res = ws_reader_u64(&r, &h.extinctions); if (res != WS_OK) return WS_ERR_CORRUPT;
    res = ws_reader_u32(&r, &u32v); if (res != WS_OK) return WS_ERR_CORRUPT;
    if (u32v > WS_HOST_MAX_LOADED) {
        return WS_ERR_CORRUPT;
    }
    h.slot_count = u32v;
    for (uint32_t i = 0; i < h.slot_count; ++i) {
        res = read_slot(&r, &h.slots[i]);
        if (res != WS_OK) return WS_ERR_CORRUPT;
    }
    res = ws_reader_u32(&r, &u32v); if (res != WS_OK) return WS_ERR_CORRUPT;
    if (u32v > WS_HOST_EVICT_CACHE) {
        return WS_ERR_CORRUPT;
    }
    h.evict_count = u32v;
    for (uint32_t i = 0; i < h.evict_count; ++i) {
        res = read_slot(&r, &h.evict[i]);
        if (res != WS_OK) return WS_ERR_CORRUPT;
    }
    res = ws_reader_u32(&r, &u32v); if (res != WS_OK) return WS_ERR_CORRUPT;
    if (u32v > WS_HOST_MAX_EVENTS) {
        return WS_ERR_CORRUPT;
    }
    h.event_count = u32v;
    res = ws_reader_u64(&r, &h.next_sequence); if (res != WS_OK) return WS_ERR_CORRUPT;
    res = ws_reader_u64(&r, &h.events_dropped); if (res != WS_OK) return WS_ERR_CORRUPT;
    for (uint32_t i = 0; i < h.event_count; ++i) {
        ws_event *e = &h.events[i];
        res = ws_reader_u64(&r, &e->sequence); if (res != WS_OK) return WS_ERR_CORRUPT;
        res = ws_reader_u64(&r, &e->tick); if (res != WS_OK) return WS_ERR_CORRUPT;
        res = ws_reader_u16(&r, &e->type); if (res != WS_OK) return WS_ERR_CORRUPT;
        res = ws_reader_u16(&r, &e->payload_size); if (res != WS_OK) return WS_ERR_CORRUPT;
        res = ws_reader_bytes(&r, e->payload, WS_EVENT_PAYLOAD_MAX);
        if (res != WS_OK) return WS_ERR_CORRUPT;
    }

    *host = h;
    return WS_OK;
}

ws_result ws_world_host_digest(const ws_world_host *host, char out_hex[WS_HOST_DIGEST_HEX])
{
    if (host == NULL || out_hex == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    uint8_t buf[WS_HOST_CHECKPOINT_MAX];
    size_t len = 0u;
    ws_result r = ws_world_host_save(host, buf, sizeof(buf), &len);
    if (r != WS_OK) {
        return r;
    }
    uint8_t digest[WS_SHA256_DIGEST_SIZE];
    ws_sha256_buffer(buf, len, digest);
    ws_sha256_hex(digest, out_hex);
    return WS_OK;
}

ws_result ws_world_host_replay(const ws_world_host_config *config,
                               const ws_region_coord *focus_path, uint32_t path_length,
                               double leg_seconds, double settle_seconds,
                               char out_hex[WS_HOST_DIGEST_HEX])
{
    if (config == NULL || out_hex == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (path_length > 0u && focus_path == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    ws_world_host host;
    ws_result r = ws_world_host_init(&host, config);
    if (r != WS_OK) {
        return r;
    }
    if (path_length > 0u) {
        r = ws_world_host_set_focus(&host, focus_path[0]);
        if (r != WS_OK) {
            return r;
        }
        r = ws_world_host_run(&host, settle_seconds);
        if (r != WS_OK) {
            return r;
        }
        for (uint32_t i = 1u; i < path_length; ++i) {
            r = ws_world_host_set_focus(&host, focus_path[i]);
            if (r != WS_OK) {
                return r;
            }
            r = ws_world_host_run(&host, leg_seconds);
            if (r != WS_OK) {
                return r;
            }
        }
    }
    return ws_world_host_digest(&host, out_hex);
}
