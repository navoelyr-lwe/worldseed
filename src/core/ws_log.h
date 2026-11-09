/* Worldseed Engine — logging.
 *
 * A minimal, dependency-free logger. Output goes to stderr by default; a
 * caller may install a sink. The level is process-global and set once at
 * startup. Logging never allocates and is safe to call from any subsystem.
 */
#ifndef WS_CORE_LOG_H
#define WS_CORE_LOG_H

typedef enum ws_log_level {
    WS_LOG_TRACE = 0,
    WS_LOG_DEBUG = 1,
    WS_LOG_INFO = 2,
    WS_LOG_WARN = 3,
    WS_LOG_ERROR = 4,
    WS_LOG_OFF = 5
} ws_log_level;

typedef void (*ws_log_sink)(ws_log_level level, const char *message, void *user);

void ws_log_set_level(ws_log_level level);
ws_log_level ws_log_get_level(void);

/* Install a custom sink; passing NULL restores stderr output. */
void ws_log_set_sink(ws_log_sink sink, void *user);

void ws_log(ws_log_level level, const char *format, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;

#endif /* WS_CORE_LOG_H */
