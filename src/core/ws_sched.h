/* Worldseed Engine — multi-core work scheduler (P10).
 *
 * A fixed worker pool that runs a caller-supplied "parallel for" over an index
 * range. The engine's determinism rule (docs/11 "Çok çekirdek ve GPU") is that
 * parallel work must not change the result: each index is an independent,
 * self-contained job that writes only to its own slot, and the caller's body
 * must not depend on the order or thread that runs it. The pool then only
 * changes wall-clock time, never the produced data.
 *
 * The pool is created once and reused; parallel_for is re-entrant from a single
 * calling thread (the main thread blocks until every worker has finished, then
 * runs a share of the range itself). There is no nested parallelism.
 */
#ifndef WS_CORE_SCHED_H
#define WS_CORE_SCHED_H

#include <stdint.h>

#include "../core/ws_result.h"

typedef struct ws_sched ws_sched;

/* Body run once per index in [begin, end). It must be a pure function of the
 * index and the shared read-only context; it must not touch other indices. */
typedef void (*ws_sched_body)(void *ctx, uint32_t index);

/* Create a pool with `worker_count` worker threads. worker_count is clamped to
 * [0, hardware_concurrency-1]; 0 means the pool runs every job on the calling
 * thread (deterministic serial fallback). */
ws_result ws_sched_create(ws_sched **out, uint32_t worker_count);

/* Create a pool sized to leave one core for the calling thread. */
ws_result ws_sched_create_default(ws_sched **out);

void ws_sched_destroy(ws_sched *s);

/* Run body(ctx, i) for i in [begin, end) across the pool and return only after
 * every index is done. Indices are partitioned into contiguous chunks; the
 * result is identical to a serial loop because bodies are independent. */
void ws_sched_parallel_for(ws_sched *s, uint32_t begin, uint32_t end,
                           ws_sched_body body, void *ctx);

/* Number of worker threads in the pool (not counting the calling thread). */
uint32_t ws_sched_worker_count(const ws_sched *s);

/* Number of parallel_for calls dispatched so far (for tests/telemetry). */
uint64_t ws_sched_jobs_dispatched(const ws_sched *s);

#endif /* WS_CORE_SCHED_H */
