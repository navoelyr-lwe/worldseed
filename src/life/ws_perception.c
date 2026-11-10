/* Worldseed Engine — organism perception implementation (P6). */
#include "ws_perception.h"

#include <math.h>

ws_perception_config ws_perception_config_default(void)
{
    ws_perception_config cfg;
    /* Reference deer: a wide, forward-facing field of view and a range large
     * enough to cross a meadow, both literature-plausible and documented in the
     * model card (docs/life/MODEL-KARTI-ALGI.md). */
    cfg.range = 60.0;
    cfg.fov = 4.0 * (3.14159265358979323846 / 6.0); /* ~240 degrees total */
    return cfg;
}

/* Wrap an angle into [-pi, pi]. */
static double wrap_pi(double a)
{
    const double two_pi = 2.0 * 3.14159265358979323846;
    a = fmod(a + 3.14159265358979323846, two_pi);
    if (a < 0.0) {
        a += two_pi;
    }
    return a - 3.14159265358979323846;
}

static void sense_channel(ws_perception_target *out, const ws_perception_config *cfg,
                          double px, double py, double heading, uint8_t kind,
                          const ws_sense_point *points, uint32_t count)
{
    out->seen = 0;
    out->dist = 0.0;
    out->bearing = 0.0;
    out->wx = 0.0;
    out->wy = 0.0;
    out->value = 0.0;

    double best_d2 = 0.0;
    for (uint32_t i = 0; i < count; ++i) {
        const ws_sense_point *p = &points[i];
        if (p->kind != kind) {
            continue;
        }
        /* A depleted resource (no value left) is not sensed; the world layer
         * regrows it later. This keeps a grazed patch from attracting the
         * animal forever. */
        if (!(p->value > 0.0)) {
            continue;
        }
        double dx = p->x - px;
        double dy = p->y - py;
        double d2 = dx * dx + dy * dy;
        if (d2 > cfg->range * cfg->range) {
            continue;
        }
        /* Bearing is the angle of the direction to the target relative to the
         * heading; targets outside the field of view are not seen. */
        double bearing = wrap_pi(atan2(dy, dx) - heading);
        if (fabs(bearing) > cfg->fov * 0.5) {
            continue;
        }
        if (!out->seen || d2 < best_d2) {
            out->seen = 1;
            best_d2 = d2;
            out->dist = sqrt(d2);
            out->bearing = bearing;
            out->wx = p->x;
            out->wy = p->y;
            out->value = p->value;
        }
    }
}

void ws_perception_update(ws_perception *out, const ws_perception_config *cfg,
                          double px, double py, double heading,
                          const ws_sense_point *points, uint32_t count,
                          const ws_perception_internal *internal)
{
    if (out == NULL) {
        return;
    }
    ws_perception_config c = (cfg != NULL) ? *cfg : ws_perception_config_default();
    if (points == NULL) {
        count = 0u;
    }

    sense_channel(&out->food, &c, px, py, heading, WS_SENSE_FOOD, points, count);
    sense_channel(&out->water, &c, px, py, heading, WS_SENSE_WATER, points, count);
    sense_channel(&out->threat, &c, px, py, heading, WS_SENSE_THREAT, points, count);

    if (internal != NULL) {
        out->internal = *internal;
    } else {
        out->internal.energy = 0.0;
        out->internal.hydration = 0.0;
        out->internal.body_temp_c = 0.0;
        out->internal.fatigue = 0.0;
    }
}
