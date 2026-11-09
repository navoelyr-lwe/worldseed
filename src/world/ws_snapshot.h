/* Worldseed Engine — read-only world snapshot.
 *
 * The observation layer (renderer, director, viewer) reads the world only
 * through a snapshot. A snapshot is a copy of the authoritative state at a
 * tick; it exposes no mutation path back into the world. This enforces the
 * architectural rule that the observation layer cannot write to the world.
 */
#ifndef WS_WORLD_SNAPSHOT_H
#define WS_WORLD_SNAPSHOT_H

#include <stdint.h>

#include "ws_event.h"

typedef struct ws_world ws_world;

typedef struct ws_snapshot {
    uint64_t world_uid;
    uint64_t seed;
    uint32_t schema_version;
    uint64_t tick;
    double time_seconds;
    uint32_t entity_count;
    uint32_t event_count;
    uint64_t events_dropped;
} ws_snapshot;

/* Copy the current authoritative state into `out`. Read-only. */
void ws_snapshot_capture(const ws_world *world, ws_snapshot *out);

/* Copy the events observed since the previous capture into `out_log`.
 * `out_storage` is caller-owned storage of `capacity` events. Read-only. */
typedef struct ws_snapshot_reader {
    uint64_t last_sequence;
} ws_snapshot_reader;

void ws_snapshot_reader_init(ws_snapshot_reader *reader);
uint64_t ws_snapshot_reader_last_sequence(const ws_snapshot_reader *reader);

ws_result ws_snapshot_read_events(const ws_world *world, ws_snapshot_reader *reader,
                                  ws_event_log *out_log, ws_event *out_storage,
                                  uint32_t capacity);

#endif /* WS_WORLD_SNAPSHOT_H */
