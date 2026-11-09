/* Worldseed Engine — read-only world snapshot. */
#include "ws_snapshot.h"

#include "ws_world.h"

#include <stddef.h>

void ws_snapshot_capture(const ws_world *world, ws_snapshot *out)
{
    if (world == NULL || out == NULL) {
        return;
    }
    out->world_uid = world->world_uid;
    out->seed = world->seed;
    out->schema_version = world->schema_version;
    out->tick = world->tick;
    out->time_seconds = world->time_seconds;
    out->entity_count = ws_entity_count(&world->entities);
    out->event_count = ws_event_log_count(&world->events);
    out->events_dropped = ws_event_log_dropped(&world->events);
}

void ws_snapshot_reader_init(ws_snapshot_reader *reader)
{
    if (reader != NULL) {
        reader->last_sequence = 0u;
    }
}

uint64_t ws_snapshot_reader_last_sequence(const ws_snapshot_reader *reader)
{
    return reader != NULL ? reader->last_sequence : 0u;
}

ws_result ws_snapshot_read_events(const ws_world *world, ws_snapshot_reader *reader,
                                  ws_event_log *out_log, ws_event *out_storage,
                                  uint32_t capacity)
{
    if (world == NULL || reader == NULL || out_log == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    ws_result r = ws_event_log_init(out_log, out_storage, capacity);
    if (r != WS_OK) {
        return r;
    }

    uint32_t count = ws_event_log_count(&world->events);
    for (uint32_t i = 0; i < count; ++i) {
        ws_event event;
        if (ws_event_log_at(&world->events, i, &event) != WS_OK) {
            continue;
        }
        if (event.sequence <= reader->last_sequence) {
            continue;
        }
        r = ws_event_log_push(out_log, event.type, event.tick, event.payload,
                              event.payload_size);
        if (r != WS_OK) {
            return r;
        }
        if (event.sequence > reader->last_sequence) {
            reader->last_sequence = event.sequence;
        }
    }
    return WS_OK;
}
