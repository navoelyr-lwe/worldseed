/* Worldseed Engine — multi-core work scheduler (P10). */
#include "ws_sched.h"

#include "../platform/ws_threads.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct ws_sched {
    ws_thread **workers;
    uint32_t worker_count;

    ws_mutex *mutex;
    ws_cond *work_ready;
    ws_cond *job_done;

    uint64_t generation;      /* bumped once per dispatched job */
    uint64_t *last_gen;       /* per worker: last generation it ran */

    /* Current job parameters, valid while a job is in flight. */
    uint32_t job_begin;
    uint32_t job_end;
    uint32_t job_chunk;
    uint32_t job_parts;       /* participant count = worker_count + 1 */
    ws_sched_body job_body;
    void *job_ctx;

    int stop;
    int remaining;            /* participants still working on the job */

    uint64_t jobs_dispatched;
};

static uint32_t ceil_div_u32(uint32_t a, uint32_t b)
{
    return b == 0u ? a : (a + b - 1u) / b;
}

/* Run the chunks assigned to participant `part` (0..job_parts-1). Chunk c
 * covers [begin + c*chunk, min(begin + (c+1)*chunk, end)). Participant `part`
 * takes every job_parts-th chunk, which balances uneven worker wakeups. */
static void run_participant(ws_sched *s, uint32_t part)
{
    uint32_t begin = s->job_begin;
    uint32_t end = s->job_end;
    uint32_t chunk = s->job_chunk;
    uint32_t parts = s->job_parts;
    ws_sched_body body = s->job_body;
    void *ctx = s->job_ctx;

    if (chunk == 0u || body == NULL) {
        return;
    }
    uint32_t nchunks = ceil_div_u32(end - begin, chunk);
    for (uint32_t c = part; c < nchunks; c += parts) {
        uint32_t lo = begin + c * chunk;
        uint32_t hi = lo + chunk;
        if (hi > end) {
            hi = end;
        }
        for (uint32_t i = lo; i < hi; ++i) {
            body(ctx, i);
        }
    }
}

typedef struct ws_worker_arg {
    ws_sched *s;
    uint32_t index;
} ws_worker_arg;

static void worker_main(void *arg)
{
    ws_worker_arg *wa = (ws_worker_arg *)arg;
    ws_sched *s = wa->s;
    uint32_t index = wa->index;
    free(wa);

    for (;;) {
        ws_mutex_lock(s->mutex);
        while (!s->stop && s->last_gen[index] == s->generation) {
            ws_cond_wait(s->work_ready, s->mutex);
        }
        if (s->stop) {
            ws_mutex_unlock(s->mutex);
            break;
        }
        s->last_gen[index] = s->generation;
        ws_mutex_unlock(s->mutex);

        run_participant(s, index);

        ws_mutex_lock(s->mutex);
        s->remaining -= 1;
        if (s->remaining == 0) {
            ws_cond_broadcast(s->job_done);
        }
        ws_mutex_unlock(s->mutex);
    }
}

ws_result ws_sched_create(ws_sched **out, uint32_t worker_count)
{
    if (out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    *out = NULL;

    uint32_t hw = ws_thread_hardware_concurrency();
    uint32_t max_workers = hw > 0u ? hw - 1u : 0u;
    if (worker_count > max_workers) {
        worker_count = max_workers;
    }

    ws_sched *s = (ws_sched *)calloc(1u, sizeof(*s));
    if (s == NULL) {
        return WS_ERR_OUT_OF_MEMORY;
    }
    s->worker_count = worker_count;

    if (ws_mutex_create(&s->mutex) != WS_OK ||
        ws_cond_create(&s->work_ready) != WS_OK ||
        ws_cond_create(&s->job_done) != WS_OK) {
        ws_sched_destroy(s);
        return WS_ERR_OUT_OF_MEMORY;
    }

    if (worker_count > 0u) {
        s->workers = (ws_thread **)calloc(worker_count, sizeof(*s->workers));
        s->last_gen = (uint64_t *)calloc(worker_count, sizeof(*s->last_gen));
        if (s->workers == NULL || s->last_gen == NULL) {
            ws_sched_destroy(s);
            return WS_ERR_OUT_OF_MEMORY;
        }
    }

    for (uint32_t i = 0; i < worker_count; ++i) {
        ws_worker_arg *wa = (ws_worker_arg *)malloc(sizeof(*wa));
        if (wa == NULL) {
            ws_sched_destroy(s);
            return WS_ERR_OUT_OF_MEMORY;
        }
        wa->s = s;
        wa->index = i;
        if (ws_thread_create(&s->workers[i], worker_main, wa) != WS_OK) {
            free(wa);
            ws_sched_destroy(s);
            return WS_ERR_UNSUPPORTED;
        }
    }

    *out = s;
    return WS_OK;
}

ws_result ws_sched_create_default(ws_sched **out)
{
    uint32_t hw = ws_thread_hardware_concurrency();
    uint32_t workers = hw > 0u ? hw - 1u : 0u;
    return ws_sched_create(out, workers);
}

void ws_sched_destroy(ws_sched *s)
{
    if (s == NULL) {
        return;
    }
    if (s->mutex != NULL) {
        ws_mutex_lock(s->mutex);
        s->stop = 1;
        ws_cond_broadcast(s->work_ready);
        ws_mutex_unlock(s->mutex);
    }
    if (s->workers != NULL) {
        for (uint32_t i = 0; i < s->worker_count; ++i) {
            ws_thread_join(s->workers[i]);
        }
    }
    free(s->workers);
    free(s->last_gen);
    ws_cond_destroy(s->work_ready);
    ws_cond_destroy(s->job_done);
    ws_mutex_destroy(s->mutex);
    free(s);
}

void ws_sched_parallel_for(ws_sched *s, uint32_t begin, uint32_t end,
                           ws_sched_body body, void *ctx)
{
    if (s == NULL || body == NULL || end <= begin) {
        return;
    }

    uint32_t total = end - begin;
    uint32_t parts = s->worker_count + 1u;
    /* ~4 chunks per participant for load balance; never more chunks than items. */
    uint32_t target_chunks = parts * 4u;
    uint32_t chunk = ceil_div_u32(total, target_chunks);
    if (chunk == 0u) {
        chunk = 1u;
    }

    ws_mutex_lock(s->mutex);
    s->job_begin = begin;
    s->job_end = end;
    s->job_chunk = chunk;
    s->job_parts = parts;
    s->job_body = body;
    s->job_ctx = ctx;
    s->remaining = (int)parts;
    s->generation += 1u;
    s->jobs_dispatched += 1u;
    ws_cond_broadcast(s->work_ready);
    ws_mutex_unlock(s->mutex);

    /* The calling thread is the last participant (index worker_count). */
    run_participant(s, s->worker_count);

    ws_mutex_lock(s->mutex);
    s->remaining -= 1;
    if (s->remaining == 0) {
        ws_cond_broadcast(s->job_done);
    }
    while (s->remaining > 0) {
        ws_cond_wait(s->job_done, s->mutex);
    }
    ws_mutex_unlock(s->mutex);
}

uint32_t ws_sched_worker_count(const ws_sched *s)
{
    return s != NULL ? s->worker_count : 0u;
}

uint64_t ws_sched_jobs_dispatched(const ws_sched *s)
{
    return s != NULL ? s->jobs_dispatched : 0u;
}
