/* Worldseed Engine — persistent application settings (P11). */
#include "ws_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WS_SETTINGS_MAX_LINE 256

ws_settings ws_settings_default(void)
{
    ws_settings s;
    s.version = WS_SETTINGS_VERSION;
    s.seed = 20260101ULL;
    s.width = 1280u;
    s.height = 720u;
    s.view = 0;
    s.time_of_day_s = 10.0 * 3600.0;
    s.render_scale = 0;
    s.fullscreen = 0;
    s.shadows = 0;
    s.stats = 0;
    s.help = 0;
    s.hud_scale = 1;
    s.volume = 70;
    s.cam_speed = 100;
    s.look_sens = 100;
    s.cam_collision = 1;
    s.gpu_render = 0;
    return s;
}

void ws_settings_clamp(ws_settings *s)
{
    if (s == NULL) {
        return;
    }
    if (s->width < 16u) { s->width = 16u; }
    if (s->width > 16384u) { s->width = 16384u; }
    if (s->height < 16u) { s->height = 16u; }
    if (s->height > 16384u) { s->height = 16384u; }
    if (s->view < 0 || s->view > 3) { s->view = 0; }
    if (!(s->time_of_day_s >= 0.0) || !(s->time_of_day_s < 86400.0)) {
        s->time_of_day_s = 0.0;
    }
    if (s->render_scale < 0 || s->render_scale > 4) { s->render_scale = 0; }
    s->fullscreen = s->fullscreen ? 1 : 0;
    s->shadows = s->shadows ? 1 : 0;
    s->stats = s->stats ? 1 : 0;
    s->help = s->help ? 1 : 0;
    if (s->hud_scale < 1) { s->hud_scale = 1; }
    if (s->hud_scale > 2) { s->hud_scale = 2; }
    if (s->volume < 0) { s->volume = 0; }
    if (s->volume > 100) { s->volume = 100; }
    if (s->cam_speed < 25) { s->cam_speed = 25; }
    if (s->cam_speed > 400) { s->cam_speed = 400; }
    if (s->look_sens < 25) { s->look_sens = 25; }
    if (s->look_sens > 400) { s->look_sens = 400; }
    s->cam_collision = s->cam_collision ? 1 : 0;
    s->gpu_render = s->gpu_render ? 1 : 0;
}

static int parse_int(const char *v, long *out)
{
    char *end = NULL;
    long n = strtol(v, &end, 10);
    if (end == v || (end != NULL && *end != '\0')) {
        return 0;
    }
    *out = n;
    return 1;
}

static int parse_double(const char *v, double *out)
{
    char *end = NULL;
    double n = strtod(v, &end);
    if (end == v || (end != NULL && *end != '\0')) {
        return 0;
    }
    *out = n;
    return 1;
}

static int parse_u64(const char *v, uint64_t *out)
{
    char *end = NULL;
    unsigned long long n = strtoull(v, &end, 10);
    if (end == v || (end != NULL && *end != '\0')) {
        return 0;
    }
    *out = (uint64_t)n;
    return 1;
}

ws_result ws_settings_parse(const char *text, ws_settings *out)
{
    if (text == NULL || out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    ws_settings s = ws_settings_default();

    const char *p = text;
    while (*p != '\0') {
        const char *eol = strchr(p, '\n');
        size_t len = eol != NULL ? (size_t)(eol - p) : strlen(p);
        if (len >= WS_SETTINGS_MAX_LINE) {
            len = WS_SETTINGS_MAX_LINE - 1u;
        }
        char line[WS_SETTINGS_MAX_LINE];
        memcpy(line, p, len);
        line[len] = '\0';

        /* Trim a trailing carriage return and surrounding spaces. */
        char *l = line;
        size_t llen = strlen(l);
        while (llen > 0u && (l[llen - 1u] == '\r' || l[llen - 1u] == ' ' ||
                             l[llen - 1u] == '\t')) {
            l[--llen] = '\0';
        }
        while (*l == ' ' || *l == '\t') { ++l; }

        if (*l != '\0' && *l != '#') {
            char *eq = strchr(l, '=');
            if (eq != NULL) {
                *eq = '\0';
                const char *key = l;
                const char *val = eq + 1;
                long n = 0;
                if (strcmp(key, "version") == 0 && parse_int(val, &n)) {
                    s.version = (int)n;
                } else if (strcmp(key, "seed") == 0) {
                    (void)parse_u64(val, &s.seed);
                } else if (strcmp(key, "width") == 0 && parse_int(val, &n)) {
                    s.width = n > 0 ? (uint32_t)n : 0u;
                } else if (strcmp(key, "height") == 0 && parse_int(val, &n)) {
                    s.height = n > 0 ? (uint32_t)n : 0u;
                } else if (strcmp(key, "view") == 0 && parse_int(val, &n)) {
                    s.view = (int)n;
                } else if (strcmp(key, "time_of_day") == 0) {
                    (void)parse_double(val, &s.time_of_day_s);
                } else if (strcmp(key, "render_scale") == 0 && parse_int(val, &n)) {
                    s.render_scale = (int)n;
                } else if (strcmp(key, "fullscreen") == 0 && parse_int(val, &n)) {
                    s.fullscreen = (int)n;
                } else if (strcmp(key, "shadows") == 0 && parse_int(val, &n)) {
                    s.shadows = (int)n;
                } else if (strcmp(key, "stats") == 0 && parse_int(val, &n)) {
                    s.stats = (int)n;
                } else if (strcmp(key, "help") == 0 && parse_int(val, &n)) {
                    s.help = (int)n;
                } else if (strcmp(key, "hud_scale") == 0 && parse_int(val, &n)) {
                    s.hud_scale = (int)n;
                } else if (strcmp(key, "volume") == 0 && parse_int(val, &n)) {
                    s.volume = (int)n;
                } else if (strcmp(key, "cam_speed") == 0 && parse_int(val, &n)) {
                    s.cam_speed = (int)n;
                } else if (strcmp(key, "look_sens") == 0 && parse_int(val, &n)) {
                    s.look_sens = (int)n;
                } else if (strcmp(key, "cam_collision") == 0 && parse_int(val, &n)) {
                    s.cam_collision = (int)n;
                } else if (strcmp(key, "gpu_render") == 0 && parse_int(val, &n)) {
                    s.gpu_render = (int)n;
                }
                /* Unknown keys are ignored. */
            }
        }

        if (eol == NULL) {
            break;
        }
        p = eol + 1;
    }

    ws_settings_clamp(&s);
    *out = s;
    return WS_OK;
}

size_t ws_settings_format(char *buf, size_t cap, const ws_settings *s)
{
    if (s == NULL) {
        return 0u;
    }
    char tmp[512];
    int n = snprintf(tmp, sizeof(tmp),
                     "# Worldseed Engine settings v%d\n"
                     "version=%d\n"
                     "seed=%llu\n"
                     "width=%u\n"
                     "height=%u\n"
                     "view=%d\n"
                     "time_of_day=%.1f\n"
                     "render_scale=%d\n"
                     "fullscreen=%d\n"
                     "shadows=%d\n"
                     "stats=%d\n"
                     "help=%d\n"
                     "hud_scale=%d\n"
                     "volume=%d\n"
                     "cam_speed=%d\n"
                     "look_sens=%d\n"
                     "cam_collision=%d\n"
                     "gpu_render=%d\n",
                     s->version, s->version, (unsigned long long)s->seed,
                     s->width, s->height, s->view, s->time_of_day_s,
                     s->render_scale, s->fullscreen, s->shadows, s->stats,
                     s->help, s->hud_scale, s->volume, s->cam_speed, s->look_sens,
                     s->cam_collision, s->gpu_render);
    if (n < 0) {
        return 0u;
    }
    size_t want = (size_t)n;
    if (buf != NULL && cap > 0u) {
        size_t copy = want < cap - 1u ? want : cap - 1u;
        memcpy(buf, tmp, copy);
        buf[copy] = '\0';
    }
    return want;
}

ws_result ws_settings_load(const char *path, ws_settings *out)
{
    if (path == NULL || out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    *out = ws_settings_default();
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        /* First run: defaults are fine. */
        return WS_OK;
    }
    char text[4096];
    size_t got = fread(text, 1u, sizeof(text) - 1u, f);
    fclose(f);
    text[got] = '\0';
    return ws_settings_parse(text, out);
}

ws_result ws_settings_save(const char *path, const ws_settings *s)
{
    if (path == NULL || s == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        return WS_ERR_IO;
    }
    char buf[512];
    size_t n = ws_settings_format(buf, sizeof(buf), s);
    size_t wrote = n > 0u ? fwrite(buf, 1u, n, f) : 0u;
    int ok = (wrote == n) && (fclose(f) == 0);
    return ok ? WS_OK : WS_ERR_IO;
}
