/* Worldseed Engine — event stream. */
#include "ws_event.h"

#include <string.h>

ws_result ws_event_log_init(ws_event_log *log, ws_event *storage, uint32_t capacity)
{
    if (log == NULL || storage == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (capacity == 0u) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    log->events = storage;
    log->capacity = capacity;
    log->head = 0u;
    log->count = 0u;
    log->next_sequence = 1u;
    log->dropped = 0u;
    return WS_OK;
}

void ws_event_log_clear(ws_event_log *log)
{
    if (log != NULL) {
        log->head = 0u;
        log->count = 0u;
        log->dropped = 0u;
    }
}

ws_result ws_event_log_push(ws_event_log *log, uint16_t type, uint64_t tick,
                            const void *payload, uint16_t payload_size)
{
    if (log == NULL || log->events == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (payload_size > WS_EVENT_PAYLOAD_MAX) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    if (payload_size > 0u && payload == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (log->count == log->capacity) {
        log->dropped += 1u;
    }

    ws_event *slot = &log->events[log->head];
    memset(slot, 0, sizeof(*slot));
    slot->sequence = log->next_sequence++;
    slot->tick = tick;
    slot->type = type;
    slot->payload_size = payload_size;
    if (payload_size > 0u) {
        memcpy(slot->payload, payload, payload_size);
    }

    log->head = (log->head + 1u) % log->capacity;
    if (log->count < log->capacity) {
        log->count += 1u;
    }
    return WS_OK;
}

uint32_t ws_event_log_count(const ws_event_log *log)
{
    return log != NULL ? log->count : 0u;
}

uint32_t ws_event_log_capacity(const ws_event_log *log)
{
    return log != NULL ? log->capacity : 0u;
}

uint64_t ws_event_log_dropped(const ws_event_log *log)
{
    return log != NULL ? log->dropped : 0u;
}

ws_result ws_event_log_at(const ws_event_log *log, uint32_t index, ws_event *out)
{
    if (log == NULL || out == NULL || log->events == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (index >= log->count) {
        return WS_ERR_NOT_FOUND;
    }
    /* Oldest retained event sits `count` positions behind the write head. */
    uint32_t start = (log->head + log->capacity - log->count) % log->capacity;
    uint32_t slot = (start + index) % log->capacity;
    *out = log->events[slot];
    return WS_OK;
}

const char *ws_event_type_name(uint16_t type)
{
    switch (type) {
    case WS_EVENT_NONE:              return "NONE";
    case WS_EVENT_WORLD_CREATED:     return "WORLD_CREATED";
    case WS_EVENT_WORLD_TICK:        return "WORLD_TICK";
    case WS_EVENT_ENTITY_CREATED:    return "ENTITY_CREATED";
    case WS_EVENT_ENTITY_DESTROYED:  return "ENTITY_DESTROYED";
    case WS_EVENT_ENTITY_MOVED:      return "ENTITY_MOVED";
    case WS_EVENT_LIFE:              return "LIFE";
    case WS_EVENT_CUSTOM:            return "CUSTOM";
    default:                         return "UNKNOWN";
    }
}
