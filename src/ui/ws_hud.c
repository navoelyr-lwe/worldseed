/* Worldseed Engine — in-world HUD (P11). */
#include "ws_hud.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../life/ws_species.h"
#include "ws_font.h"

/* A small, readable palette. Panels are dark and semi-opaque so the world stays
 * visible behind them; text is near-white with a coloured accent per section. */
static const uint8_t COL_PANEL[4] = {10u, 14u, 18u, 205u};
static const uint8_t COL_BORDER[4] = {70u, 96u, 120u, 235u};
static const uint8_t COL_TEXT[4] = {228u, 232u, 236u, 255u};
static const uint8_t COL_DIM[4] = {150u, 158u, 166u, 255u};
static const uint8_t COL_ACCENT[4] = {120u, 200u, 255u, 255u};
static const uint8_t COL_GOOD[4] = {140u, 220u, 140u, 255u};
static const uint8_t COL_WARN[4] = {240u, 190u, 90u, 255u};
static const uint8_t COL_BAD[4] = {235u, 120u, 110u, 255u};

ws_hud_options ws_hud_options_default(void)
{
    ws_hud_options o;
    o.show_panels = 1;
    o.show_help = 0;
    o.show_stats = 0;
    o.show_settings = 0;
    o.scale = 1;
    return o;
}

static void panel(ws_framebuffer *fb, int x, int y, uint32_t w, uint32_t h)
{
    ws_font_fill_rect(fb, x, y, w, h, COL_PANEL);
    ws_font_stroke_rect(fb, x, y, w, h, 1, COL_BORDER);
}

static void clock_string(double tod_s, char *out, size_t n)
{
    int total = (int)tod_s;
    if (total < 0) { total = 0; }
    int hh = (total / 3600) % 24;
    int mm = (total / 60) % 60;
    snprintf(out, n, "%02d:%02d", hh, mm);
}

/* --- status bar (bottom) ------------------------------------------------ */

static void draw_status_bar(ws_framebuffer *fb, const ws_session *s, int scale)
{
    uint32_t line_h = WS_FONT_GLYPH_H * (uint32_t)scale + 4u;
    int x = 6;
    int y = (int)fb->height - (int)line_h - 4;
    panel(fb, 4, y - 3, fb->width - 8u, line_h + 6u);

    char buf[256];
    char clock[8];
    clock_string(ws_session_time_of_day_s(s), clock, sizeof(clock));
    double speed = ws_session_speed(s);
    const char *paused = ws_session_paused(s) ? "PAUSED" : "RUN";

    snprintf(buf, sizeof(buf),
             "Worldseed  %s  x%.1f  view:%s  t=%s  tick=%llu  fps=%.0f  pop=%u",
             paused, speed, ws_view_mode_name(ws_session_view_mode(s)), clock,
             (unsigned long long)ws_session_tick(s),
             ws_session_perf_fps(s), ws_session_total_population(s));
    ws_font_draw(fb, x, y, buf, scale, COL_TEXT);
}

/* --- info panel (top-left) ---------------------------------------------- */

static void draw_info_panel(ws_framebuffer *fb, const ws_session *s, int scale)
{
    uint32_t line_h = WS_FONT_GLYPH_H * (uint32_t)scale + 3u;
    int x = 6;
    int y = 6;
    uint32_t w = 250u * (uint32_t)scale;
    uint32_t h = line_h * 12u + 8u;
    panel(fb, x - 3, y - 3, w, h);

    char buf[256];
    int cy = y;
    ws_font_draw(fb, x, cy, "WORLDSEED ENGINE", scale, COL_ACCENT);
    cy += (int)line_h;

    snprintf(buf, sizeof(buf), "seed=%llu  tick=%llu",
             (unsigned long long)ws_session_seed(s),
             (unsigned long long)ws_session_tick(s));
    ws_font_draw(fb, x, cy, buf, scale, COL_DIM); cy += (int)line_h;

    ws_region_coord fr = ws_session_focus_region(s);
    snprintf(buf, sizeof(buf), "focus region=(%lld,%lld) loaded=%u",
             (long long)fr.x, (long long)fr.y, ws_session_loaded_regions(s));
    ws_font_draw(fb, x, cy, buf, scale, COL_DIM); cy += (int)line_h;

    snprintf(buf, sizeof(buf), "population=%u  plant=%.0f",
             ws_session_total_population(s), ws_session_plant_biomass(s));
    ws_font_draw(fb, x, cy, buf, scale, COL_TEXT); cy += (int)line_h;

    for (int sp = 0; sp < WS_SPECIES_COUNT; ++sp) {
        snprintf(buf, sizeof(buf), "  %-14s %u",
                 ws_species_name((ws_species_kind)sp),
                 ws_session_population(s, sp));
        const uint8_t *col = ws_session_population(s, sp) > 0u ? COL_TEXT : COL_BAD;
        ws_font_draw(fb, x, cy, buf, scale, col);
        cy += (int)line_h;
    }

    snprintf(buf, sizeof(buf), "extinctions=%llu  loads=%llu unloads=%llu",
             (unsigned long long)ws_session_extinctions(s),
             (unsigned long long)ws_session_loads(s),
             (unsigned long long)ws_session_unloads(s));
    ws_font_draw(fb, x, cy, buf, scale, COL_DIM); cy += (int)line_h;

    snprintf(buf, sizeof(buf), "sun alt=%.2f  temp=%.1fC  wind=%.1f m/s",
             ws_session_sun_altitude(s), ws_session_temperature_c(s),
             ws_session_wind_speed(s));
    ws_font_draw(fb, x, cy, buf, scale, COL_DIM); cy += (int)line_h;

    snprintf(buf, sizeof(buf), "audio voices=%u events=%llu",
             ws_session_audio_voices(s),
             (unsigned long long)ws_session_audio_events(s));
    ws_font_draw(fb, x, cy, buf, scale, COL_DIM); cy += (int)line_h;

    snprintf(buf, sizeof(buf), "frame=%.1fms  avg=%.1fms  fps=%.0f",
             ws_session_perf_last_ms(s), ws_session_perf_avg_ms(s),
             ws_session_perf_fps(s));
    ws_font_draw(fb, x, cy, buf, scale, COL_DIM);
}

/* --- selected animal panel (top-right) ---------------------------------- */

static void draw_selection_panel(ws_framebuffer *fb, const ws_session *s, int scale)
{
    uint32_t line_h = WS_FONT_GLYPH_H * (uint32_t)scale + 3u;
    uint32_t w = 236u * (uint32_t)scale;
    int x = (int)fb->width - (int)w - 3;
    int y = 6;
    uint32_t h = line_h * 8u + 8u;
    panel(fb, x, y - 3, w, h);

    char buf[256];
    int cy = y;
    ws_font_draw(fb, x + 3, cy, "INSPECT", scale, COL_ACCENT);
    cy += (int)line_h;

    ws_session_agent_view v;
    if (!ws_session_selected(s, &v)) {
        ws_font_draw(fb, x + 3, cy, "no animal selected (N/P)", scale, COL_DIM);
        return;
    }
    snprintf(buf, sizeof(buf), "%s", v.species_name ? v.species_name : "?");
    ws_font_draw(fb, x + 3, cy, buf, scale, COL_TEXT); cy += (int)line_h;

    snprintf(buf, sizeof(buf), "energy=%.2f  age=%.0fs", v.energy, v.age_s);
    const uint8_t *ec = v.energy < 0.25 ? COL_BAD : (v.energy < 0.6 ? COL_WARN : COL_GOOD);
    ws_font_draw(fb, x + 3, cy, buf, scale, ec); cy += (int)line_h;

    snprintf(buf, sizeof(buf), "speed=%.2f m/s  activity=%s", v.speed, v.activity);
    ws_font_draw(fb, x + 3, cy, buf, scale, COL_TEXT); cy += (int)line_h;

    snprintf(buf, sizeof(buf), "pos=(%.0f, %.0f) m", v.x, v.y);
    ws_font_draw(fb, x + 3, cy, buf, scale, COL_DIM); cy += (int)line_h;

    snprintf(buf, sizeof(buf), "travelled=%.1f m", v.distance_m);
    ws_font_draw(fb, x + 3, cy, buf, scale, COL_DIM); cy += (int)line_h;

    snprintf(buf, sizeof(buf), "bites=%llu  kills=%llu",
             (unsigned long long)v.bites, (unsigned long long)v.kills);
    ws_font_draw(fb, x + 3, cy, buf, scale, COL_DIM);
}

/* --- timeline (bottom-right) -------------------------------------------- */

static void draw_timeline(ws_framebuffer *fb, const ws_session *s, int scale)
{
    uint32_t line_h = WS_FONT_GLYPH_H * (uint32_t)scale + 3u;
    uint32_t w = 240u * (uint32_t)scale;
    uint32_t rows = 8u;
    int x = (int)fb->width - (int)w - 3;
    int y = (int)fb->height - (int)(line_h * (rows + 1u)) - 30;
    panel(fb, x, y - 3, w, line_h * (rows + 1u) + 6u);

    char buf[256];
    int cy = y;
    ws_font_draw(fb, x + 3, cy, "EVENTS", scale, COL_ACCENT);
    cy += (int)line_h;

    uint32_t n = ws_session_timeline_count(s);
    for (uint32_t i = 0; i < rows && i < n; ++i) {
        ws_session_event_view ev;
        if (ws_session_timeline_at(s, i, &ev) != WS_OK) {
            break;
        }
        snprintf(buf, sizeof(buf), "#%-5llu %-10s %s",
                 (unsigned long long)ev.sequence,
                 ev.is_life ? "life" : "world",
                 ev.name ? ev.name : "?");
        const uint8_t *col = ev.is_life ? COL_GOOD : COL_DIM;
        ws_font_draw(fb, x + 3, cy, buf, scale, col);
        cy += (int)line_h;
    }
    if (n == 0u) {
        ws_font_draw(fb, x + 3, cy, "(no events yet)", scale, COL_DIM);
    }
}

/* --- world map (top-centre) --------------------------------------------- */

static void draw_map(ws_framebuffer *fb, const ws_session *s, int scale)
{
    ws_session_map_cell cells[WS_SESSION_MAX_MAP];
    uint32_t n = ws_session_map(s, cells, WS_SESSION_MAX_MAP);
    if (n == 0u) {
        return;
    }
    int64_t min_x = cells[0].region_x, max_x = cells[0].region_x;
    int64_t min_y = cells[0].region_y, max_y = cells[0].region_y;
    for (uint32_t i = 1; i < n; ++i) {
        if (cells[i].region_x < min_x) { min_x = cells[i].region_x; }
        if (cells[i].region_x > max_x) { max_x = cells[i].region_x; }
        if (cells[i].region_y < min_y) { min_y = cells[i].region_y; }
        if (cells[i].region_y > max_y) { max_y = cells[i].region_y; }
    }
    int gw = (int)(max_x - min_x) + 1;
    int gh = (int)(max_y - min_y) + 1;
    if (gw > 12) { gw = 12; }
    if (gh > 12) { gh = 12; }

    int cell = 16 * scale;
    uint32_t panel_w = (uint32_t)(gw * cell) + 12u;
    uint32_t panel_h = (uint32_t)(gh * cell) + 12u + WS_FONT_GLYPH_H * (uint32_t)scale;
    int px = ((int)fb->width - (int)panel_w) / 2;
    int py = 6;
    panel(fb, px, py, panel_w, panel_h);
    ws_font_draw(fb, px + 6, py + 4, "WORLD MAP", scale, COL_ACCENT);

    for (uint32_t i = 0; i < n; ++i) {
        int cx = (int)(cells[i].region_x - min_x);
        int cy = (int)(cells[i].region_y - min_y);
        int rx = px + 6 + cx * cell;
        int ry = py + 6 + (int)(WS_FONT_GLYPH_H * (uint32_t)scale) + cy * cell;
        uint8_t col[4];
        switch (cells[i].level) {
        case WS_LEVEL_NEAR: col[0] = 90u; col[1] = 200u; col[2] = 110u; break;
        case WS_LEVEL_MID: col[0] = 90u; col[1] = 150u; col[2] = 210u; break;
        default: col[0] = 90u; col[1] = 96u; col[2] = 120u; break;
        }
        col[3] = 220u;
        if (cells[i].is_focus) {
            col[0] = 250u; col[1] = 210u; col[2] = 90u;
        }
        ws_font_fill_rect(fb, rx + 1, ry + 1, (uint32_t)cell - 2u, (uint32_t)cell - 2u, col);
    }
}

/* --- statistics panel (left, below the info panel) ---------------------- */

#define STATS_SAMPLES 96u

static void draw_stats_panel(ws_framebuffer *fb, const ws_session *s, int scale)
{
    uint32_t line_h = WS_FONT_GLYPH_H * (uint32_t)scale + 3u;
    int x = 6;
    int y = 6 + (int)(line_h * 12u + 8u) + 6;
    uint32_t w = 250u * (uint32_t)scale;
    uint32_t h = line_h * 10u + 8u;
    panel(fb, x - 3, y - 3, w, h);

    char buf[256];
    int cy = y;
    ws_font_draw(fb, x, cy, "STATISTICS", scale, COL_ACCENT);
    cy += (int)line_h;

    snprintf(buf, sizeof(buf), "births=%llu  deaths=%llu",
             (unsigned long long)ws_session_births(s),
             (unsigned long long)ws_session_deaths(s));
    ws_font_draw(fb, x, cy, buf, scale, COL_TEXT); cy += (int)line_h;

    snprintf(buf, sizeof(buf), "predation=%llu  forage=%llu",
             (unsigned long long)ws_session_predation_events(s),
             (unsigned long long)ws_session_forage_events(s));
    ws_font_draw(fb, x, cy, buf, scale, COL_DIM); cy += (int)line_h;

    snprintf(buf, sizeof(buf), "starve=%llu  age=%llu",
             (unsigned long long)ws_session_starvation_deaths(s),
             (unsigned long long)ws_session_age_deaths(s));
    ws_font_draw(fb, x, cy, buf, scale, COL_DIM); cy += (int)line_h;

    /* Population history: one thin bar per sample, stacked per species so the
     * total height tracks the total population. */
    float hist[STATS_SAMPLES * WS_SPECIES_COUNT];
    uint32_t n = ws_session_population_history(s, hist, STATS_SAMPLES);
    uint32_t chart_h = line_h * 5u;
    uint32_t chart_w = w - 12u;
    int gx = x + 3;
    int gy = cy + (int)chart_h;
    ws_font_stroke_rect(fb, gx, cy, chart_w, chart_h, 1, COL_BORDER);
    if (n > 0u) {
        float peak = 1.0f;
        for (uint32_t i = 0; i < n; ++i) {
            float total = 0.0f;
            for (int sp = 0; sp < WS_SPECIES_COUNT; ++sp) {
                total += hist[i * WS_SPECIES_COUNT + (uint32_t)sp];
            }
            if (total > peak) { peak = total; }
        }
        uint32_t col_w = n < chart_w ? chart_w / n : 1u;
        if (col_w < 1u) { col_w = 1u; }
        static const uint8_t SPEC_COL[WS_SPECIES_COUNT][4] = {
            {150u, 210u, 120u, 255u}, {230u, 150u, 90u, 255u},
            {200u, 190u, 130u, 255u}, {170u, 150u, 200u, 255u},
        };
        for (uint32_t i = 0; i < n; ++i) {
            float base = 0.0f;
            int bx = gx + (int)(i * col_w);
            for (int sp = 0; sp < WS_SPECIES_COUNT; ++sp) {
                float v = hist[i * WS_SPECIES_COUNT + (uint32_t)sp];
                int h0 = (int)(base / peak * (float)(chart_h - 2u));
                base += v;
                int h1 = (int)(base / peak * (float)(chart_h - 2u));
                int bh = h1 - h0;
                if (bh > 0) {
                    ws_font_fill_rect(fb, bx, gy - h1, col_w, (uint32_t)bh,
                                      SPEC_COL[sp]);
                }
            }
        }
    }
    cy += (int)chart_h + 2;
    snprintf(buf, sizeof(buf), "pop history: %u samples (oldest left)", n);
    ws_font_draw(fb, x, cy, buf, scale, COL_DIM);
}

/* --- settings panel (docs/17 item 9) ------------------------------------ */

static void draw_settings_panel(ws_framebuffer *fb, const ws_session *s, int scale)
{
    uint32_t line_h = WS_FONT_GLYPH_H * (uint32_t)scale + 3u;
    int x = 6;
    int y = 6 + (int)(line_h * 12u + 8u) + 6;
    uint32_t w = 250u * (uint32_t)scale;
    uint32_t h = line_h * 10u + 8u;
    panel(fb, x - 3, y - 3, w, h);

    struct ws_settings v;
    if (ws_session_get_settings(s, &v) != WS_OK) {
        return;
    }

    char buf[256];
    int cy = y;
    ws_font_draw(fb, x, cy, "SETTINGS (persisted)", scale, COL_ACCENT);
    cy += (int)line_h;

    snprintf(buf, sizeof(buf), "resolution %ux%u", v.width, v.height);
    ws_font_draw(fb, x, cy, buf, scale, COL_TEXT); cy += (int)line_h;

    snprintf(buf, sizeof(buf), "render scale %s",
             v.render_scale == 0 ? "auto" : "fixed");
    ws_font_draw(fb, x, cy, buf, scale, COL_DIM); cy += (int)line_h;

    snprintf(buf, sizeof(buf), "start view %s", ws_view_mode_name(v.view));
    ws_font_draw(fb, x, cy, buf, scale, COL_DIM); cy += (int)line_h;

    char clock[8];
    clock_string(v.time_of_day_s, clock, sizeof(clock));
    snprintf(buf, sizeof(buf), "start time %s", clock);
    ws_font_draw(fb, x, cy, buf, scale, COL_DIM); cy += (int)line_h;

    snprintf(buf, sizeof(buf), "shadows %s  fullscreen %s",
             v.shadows ? "on" : "off", v.fullscreen ? "on" : "off");
    ws_font_draw(fb, x, cy, buf, scale, COL_DIM); cy += (int)line_h;

    snprintf(buf, sizeof(buf), "seed %llu", (unsigned long long)v.seed);
    ws_font_draw(fb, x, cy, buf, scale, COL_DIM); cy += (int)line_h;

    ws_font_draw(fb, x, cy, "Saved on clean exit; edit the", scale, COL_TEXT);
    cy += (int)line_h;
    ws_font_draw(fb, x, cy, "config file to change directly.", scale, COL_TEXT);
}

/* --- replay panel (docs/17 item 8) -------------------------------------- */

static void draw_replay_panel(ws_framebuffer *fb, const ws_session *s, int scale)
{
    uint32_t line_h = WS_FONT_GLYPH_H * (uint32_t)scale + 3u;
    uint32_t w = 300u * (uint32_t)scale;
    uint32_t h = line_h * 6u + 8u;
    int x = ((int)fb->width - (int)w) / 2;
    int y = (int)fb->height - (int)h - (int)(line_h * 3u + 12u);
    panel(fb, x - 3, y - 3, w, h);

    char buf[256];
    int cx = x;
    int cy = y;
    ws_font_draw(fb, cx, cy, "REPLAY", scale, COL_WARN);
    cy += (int)line_h;

    double pos = ws_session_replay_position(s);
    int bar_w = (int)w - 16;
    ws_font_fill_rect(fb, cx, cy + 4, (uint32_t)bar_w, 5u, COL_DIM);
    int fill = (int)(pos * (double)bar_w);
    if (fill > bar_w) { fill = bar_w; }
    ws_font_fill_rect(fb, cx, cy + 4, (uint32_t)fill, 5u, COL_ACCENT);
    cy += (int)line_h;

    char clock[8];
    char live_clock[8];
    clock_string(ws_session_replay_time_s(s), clock, sizeof(clock));
    clock_string(ws_session_replay_live_time_s(s), live_clock, sizeof(live_clock));
    snprintf(buf, sizeof(buf), "recorded %s / live %s", clock, live_clock);
    ws_font_draw(fb, cx, cy, buf, scale, COL_TEXT);
    cy += (int)line_h;

    snprintf(buf, sizeof(buf), "history %.0fs  %u samples",
             ws_session_replay_span_s(s), ws_session_replay_samples(s));
    ws_font_draw(fb, cx, cy, buf, scale, COL_DIM);
    cy += (int)line_h;

    ws_font_draw(fb, cx, cy, ", / .  step back / forward", scale, COL_TEXT);
    cy += (int)line_h;
    ws_font_draw(fb, cx, cy, "R leave replay (back to live)", scale, COL_TEXT);
}

/* --- help overlay ------------------------------------------------------- */

static const char *HELP_LINES[] = {
    "Worldseed Engine - live world observer",
    "",
    "  Space      pause / resume",
    "  + / -      speed up / down (0.5x .. 4x)",
    "  V          cycle view (auto / free / follow / map)",
    "  N / P      select next / previous animal",
    "  G          toggle statistics panel",
    "  T          toggle directional shadows",
    "  O          toggle settings panel",
    "  Arrow keys / WASD  fly the free camera",
    "  Mouse drag look (free view)",
    "  [ / ]      time of day -/+ 1 hour",
    "  S / L      save / load checkpoint",
    "  R          toggle replay of recorded history",
    "  , / .      (in replay) step back / forward",
    "  H          toggle this help",
    "  Esc / Q    quit",
    "",
    "The observer never changes the world. The camera,",
    "selection, pause and save are read-only.",
};

static void draw_help(ws_framebuffer *fb, int scale)
{
    uint32_t line_h = WS_FONT_GLYPH_H * (uint32_t)scale + 4u;
    int n = (int)(sizeof(HELP_LINES) / sizeof(HELP_LINES[0]));
    uint32_t w = 340u * (uint32_t)scale;
    uint32_t h = line_h * (uint32_t)n + 12u;
    int x = ((int)fb->width - (int)w) / 2;
    int y = ((int)fb->height - (int)h) / 2;
    panel(fb, x, y, w, h);
    for (int i = 0; i < n; ++i) {
        const uint8_t *col = (i == 0) ? COL_ACCENT : COL_TEXT;
        ws_font_draw(fb, x + 8, y + 6 + (int)line_h * i, HELP_LINES[i], scale, col);
    }
}

void ws_hud_draw(ws_framebuffer *fb, const ws_session *s, const ws_hud_options *opts)
{
    if (fb == NULL || s == NULL) {
        return;
    }
    ws_hud_options o = opts != NULL ? *opts : ws_hud_options_default();
    int scale = o.scale < 1 ? 1 : o.scale;

    draw_status_bar(fb, s, scale);
    if (o.show_panels) {
        draw_info_panel(fb, s, scale);
        draw_selection_panel(fb, s, scale);
        draw_timeline(fb, s, scale);
    }
    if (o.show_stats || ws_session_stats_visible(s)) {
        draw_stats_panel(fb, s, scale);
    }
    if (o.show_settings || ws_session_settings_visible(s)) {
        draw_settings_panel(fb, s, scale);
    }
    if (ws_session_replay_active(s)) {
        draw_replay_panel(fb, s, scale);
    }
    if (ws_session_view_mode(s) == WS_VIEW_MAP) {
        draw_map(fb, s, scale);
    }
    if (o.show_help || ws_session_help_visible(s)) {
        draw_help(fb, scale);
    }
}
