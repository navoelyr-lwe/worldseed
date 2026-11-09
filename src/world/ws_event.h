/* Worldseed Engine — event stream.
 *
 * Events are the authoritative, ordered record of discrete world changes. The
 * cinematic director observes events; it never invents them. Payloads are
 * fixed-size and pointer-free so the stream can be journalled and replayed
 * deterministically. Sequence numbers are strictly increasing.
 *
 * Storage is caller-owned so allocation policy stays with the world.
 */
#ifndef WS_WORLD_EVENT_H
#define WS_WORLD_EVENT_H

#include <stddef.h>
#include <stdint.h>

#include "../core/ws_result.h"

#define WS_EVENT_PAYLOAD_MAX 32u

typedef enum ws_event_type {
    WS_EVENT_NONE = 0,
    WS_EVENT_WORLD_CREATED = 1,
    WS_EVENT_WORLD_TICK = 2,
    WS_EVENT_ENTITY_CREATED = 3,
    WS_EVENT_ENTITY_DESTROYED = 4,
    WS_EVENT_ENTITY_MOVED = 5,
    WS_EVENT_CUSTOM = 255
} ws_event_type;

typedef struct ws_event {
    uint64_t sequence;
    uint64_t tick;
    uint16_t type;
    uint16_t payload_size;
    uint8_t payload[WS_EVENT_PAYLOAD_MAX];
} ws_event;

typedef struct ws_event_log {
    ws_event *events;
    uint32_t capacity;
    uint32_t head;   /* next write slot */
    uint32_t count;
    uint64_t next_sequence;
    uint64_t dropped; /* events overwritten since creation */
} ws_event_log;

ws_result ws_event_log_init(ws_event_log *log, ws_event *storage, uint32_t capacity);
void ws_event_log_clear(ws_event_log *log);

/* Append an event. When full, the oldest retained event is overwritten and the
 * dropped counter increases. payload may be NULL only when payload_size == 0. */
ws_result ws_event_log_push(ws_event_log *log, uint16_t type, uint64_t tick,
                            const void *payload, uint16_t payload_size);

uint32_t ws_event_log_count(const ws_event_log *log);
uint32_t ws_event_log_capacity(const ws_event_log *log);
uint64_t ws_event_log_dropped(const ws_event_log *log);

/* Read by age: index 0 is the oldest retained event. */
ws_result ws_event_log_at(const ws_event_log *log, uint32_t index, ws_event *out);

const char *ws_event_type_name(uint16_t type);

#endif /* WS_WORLD_EVENT_H */
