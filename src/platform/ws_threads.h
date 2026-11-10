/* Worldseed Engine — thread primitives (platform boundary).
 *
 * The only OS-threading code in the engine. The shared core uses these opaque
 * handles; it never includes a platform header. On Windows the implementation
 * uses the Win32 thread/CRITICAL_SECTION/CONDITION_VARIABLE API; elsewhere it
 * uses POSIX threads. Threads never carry simulation state: the authoritative
 * world is advanced on one thread, and worker threads only run deterministic,
 * index-addressed work (see src/core/ws_sched.h).
 */
#ifndef WS_PLATFORM_THREADS_H
#define WS_PLATFORM_THREADS_H

#include <stdint.h>

#include "../core/ws_result.h"

typedef struct ws_mutex ws_mutex;
typedef struct ws_cond ws_cond;
typedef struct ws_thread ws_thread;

typedef void (*ws_thread_fn)(void *arg);

ws_result ws_mutex_create(ws_mutex **out);
void ws_mutex_destroy(ws_mutex *m);
void ws_mutex_lock(ws_mutex *m);
void ws_mutex_unlock(ws_mutex *m);

ws_result ws_cond_create(ws_cond **out);
void ws_cond_destroy(ws_cond *c);
void ws_cond_wait(ws_cond *c, ws_mutex *m);
void ws_cond_broadcast(ws_cond *c);

/* Start a joinable thread running fn(arg). */
ws_result ws_thread_create(ws_thread **out, ws_thread_fn fn, void *arg);
/* Join and free a thread handle. Safe to call with NULL (returns WS_OK). */
ws_result ws_thread_join(ws_thread *t);

/* Number of usable hardware threads; 0 when the platform cannot report it. */
uint32_t ws_thread_hardware_concurrency(void);

/* Cooperative yield hint for spin/settle loops. */
void ws_thread_yield(void);

#endif /* WS_PLATFORM_THREADS_H */
