/* Worldseed Engine — logging. */
#include "ws_log.h"

#include <stdarg.h>
#include <stdio.h>

static ws_log_level g_level = WS_LOG_INFO;
static ws_log_sink g_sink = NULL;
static void *g_sink_user = NULL;

static const char *ws_log_level_name(ws_log_level level)
{
    switch (level) {
    case WS_LOG_TRACE: return "TRACE";
    case WS_LOG_DEBUG: return "DEBUG";
    case WS_LOG_INFO:  return "INFO";
    case WS_LOG_WARN:  return "WARN";
    case WS_LOG_ERROR: return "ERROR";
    default:           return "?";
    }
}

void ws_log_set_level(ws_log_level level)
{
    g_level = level;
}

ws_log_level ws_log_get_level(void)
{
    return g_level;
}

void ws_log_set_sink(ws_log_sink sink, void *user)
{
    g_sink = sink;
    g_sink_user = user;
}

void ws_log(ws_log_level level, const char *format, ...)
{
    if (level < g_level || level == WS_LOG_OFF) {
        return;
    }
    char buffer[512];
    int written = 0;
    if (format != NULL) {
        va_list args;
        va_start(args, format);
        written = vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
    }
    if (written < 0) {
        buffer[0] = '\0';
    }

    if (g_sink != NULL) {
        g_sink(level, buffer, g_sink_user);
        return;
    }
    fprintf(stderr, "[worldseed %s] %s\n", ws_log_level_name(level), buffer);
}
