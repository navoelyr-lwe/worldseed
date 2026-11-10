/* Worldseed Engine — thread primitives (platform boundary). */
#include "ws_threads.h"

#include <stdlib.h>

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct ws_mutex {
    CRITICAL_SECTION cs;
};
struct ws_cond {
    CONDITION_VARIABLE cv;
};
struct ws_thread {
    HANDLE handle;
    ws_thread_fn fn;
    void *arg;
};

ws_result ws_mutex_create(ws_mutex **out)
{
    if (out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    ws_mutex *m = (ws_mutex *)malloc(sizeof(*m));
    if (m == NULL) {
        return WS_ERR_OUT_OF_MEMORY;
    }
    InitializeCriticalSection(&m->cs);
    *out = m;
    return WS_OK;
}

void ws_mutex_destroy(ws_mutex *m)
{
    if (m == NULL) {
        return;
    }
    DeleteCriticalSection(&m->cs);
    free(m);
}

void ws_mutex_lock(ws_mutex *m)
{
    if (m != NULL) {
        EnterCriticalSection(&m->cs);
    }
}

void ws_mutex_unlock(ws_mutex *m)
{
    if (m != NULL) {
        LeaveCriticalSection(&m->cs);
    }
}

ws_result ws_cond_create(ws_cond **out)
{
    if (out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    ws_cond *c = (ws_cond *)malloc(sizeof(*c));
    if (c == NULL) {
        return WS_ERR_OUT_OF_MEMORY;
    }
    InitializeConditionVariable(&c->cv);
    *out = c;
    return WS_OK;
}

void ws_cond_destroy(ws_cond *c)
{
    free(c);
}

void ws_cond_wait(ws_cond *c, ws_mutex *m)
{
    if (c != NULL && m != NULL) {
        SleepConditionVariableCS(&c->cv, &m->cs, INFINITE);
    }
}

void ws_cond_broadcast(ws_cond *c)
{
    if (c != NULL) {
        WakeAllConditionVariable(&c->cv);
    }
}

static DWORD WINAPI ws_thread_trampoline(LPVOID param)
{
    ws_thread *t = (ws_thread *)param;
    t->fn(t->arg);
    return 0;
}

ws_result ws_thread_create(ws_thread **out, ws_thread_fn fn, void *arg)
{
    if (out == NULL || fn == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    ws_thread *t = (ws_thread *)malloc(sizeof(*t));
    if (t == NULL) {
        return WS_ERR_OUT_OF_MEMORY;
    }
    t->fn = fn;
    t->arg = arg;
    t->handle = CreateThread(NULL, 0, ws_thread_trampoline, t, 0, NULL);
    if (t->handle == NULL) {
        free(t);
        return WS_ERR_UNSUPPORTED;
    }
    *out = t;
    return WS_OK;
}

ws_result ws_thread_join(ws_thread *t)
{
    if (t == NULL) {
        return WS_OK;
    }
    WaitForSingleObject(t->handle, INFINITE);
    CloseHandle(t->handle);
    free(t);
    return WS_OK;
}

uint32_t ws_thread_hardware_concurrency(void)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (uint32_t)si.dwNumberOfProcessors;
}

void ws_thread_yield(void)
{
    SwitchToThread();
}

#else /* POSIX */

#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include <pthread.h>
#include <sched.h>
#include <unistd.h>

struct ws_mutex {
    pthread_mutex_t m;
};
struct ws_cond {
    pthread_cond_t c;
};
struct ws_thread {
    pthread_t t;
    ws_thread_fn fn;
    void *arg;
};

ws_result ws_mutex_create(ws_mutex **out)
{
    if (out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    ws_mutex *m = (ws_mutex *)malloc(sizeof(*m));
    if (m == NULL) {
        return WS_ERR_OUT_OF_MEMORY;
    }
    if (pthread_mutex_init(&m->m, NULL) != 0) {
        free(m);
        return WS_ERR_UNSUPPORTED;
    }
    *out = m;
    return WS_OK;
}

void ws_mutex_destroy(ws_mutex *m)
{
    if (m == NULL) {
        return;
    }
    pthread_mutex_destroy(&m->m);
    free(m);
}

void ws_mutex_lock(ws_mutex *m)
{
    if (m != NULL) {
        pthread_mutex_lock(&m->m);
    }
}

void ws_mutex_unlock(ws_mutex *m)
{
    if (m != NULL) {
        pthread_mutex_unlock(&m->m);
    }
}

ws_result ws_cond_create(ws_cond **out)
{
    if (out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    ws_cond *c = (ws_cond *)malloc(sizeof(*c));
    if (c == NULL) {
        return WS_ERR_OUT_OF_MEMORY;
    }
    if (pthread_cond_init(&c->c, NULL) != 0) {
        free(c);
        return WS_ERR_UNSUPPORTED;
    }
    *out = c;
    return WS_OK;
}

void ws_cond_destroy(ws_cond *c)
{
    if (c == NULL) {
        return;
    }
    pthread_cond_destroy(&c->c);
    free(c);
}

void ws_cond_wait(ws_cond *c, ws_mutex *m)
{
    if (c != NULL && m != NULL) {
        pthread_cond_wait(&c->c, &m->m);
    }
}

void ws_cond_broadcast(ws_cond *c)
{
    if (c != NULL) {
        pthread_cond_broadcast(&c->c);
    }
}

static void *ws_thread_trampoline(void *param)
{
    ws_thread *t = (ws_thread *)param;
    t->fn(t->arg);
    return NULL;
}

ws_result ws_thread_create(ws_thread **out, ws_thread_fn fn, void *arg)
{
    if (out == NULL || fn == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    ws_thread *t = (ws_thread *)malloc(sizeof(*t));
    if (t == NULL) {
        return WS_ERR_OUT_OF_MEMORY;
    }
    t->fn = fn;
    t->arg = arg;
    if (pthread_create(&t->t, NULL, ws_thread_trampoline, t) != 0) {
        free(t);
        return WS_ERR_UNSUPPORTED;
    }
    *out = t;
    return WS_OK;
}

ws_result ws_thread_join(ws_thread *t)
{
    if (t == NULL) {
        return WS_OK;
    }
    pthread_join(t->t, NULL);
    free(t);
    return WS_OK;
}

uint32_t ws_thread_hardware_concurrency(void)
{
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (uint32_t)n : 0u;
}

void ws_thread_yield(void)
{
    sched_yield();
}

#endif
