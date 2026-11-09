/* Worldseed Engine — organism internal state and homeostasis (P6). */
#include "ws_organism.h"

#include "../core/ws_math.h"

#include <math.h>

ws_organism_config ws_organism_config_default(void)
{
    ws_organism_config c;
    /* Rates are fractions of the reserve per second (or degrees C), chosen so
     * the reference animal shows a clear foraging cycle on a headless run of a
     * few minutes. They are locally calibrated, not measured; the model card
     * records this and the D1/D2 evidence level. */
    c.comfort_temp_c = 38.5;   /* deer core temperature, degrees C */
    c.thermal_scale_c = 2.0;
    c.basal_energy_rate = 0.0020;
    c.move_energy_rate = 0.0100;
    c.basal_water_rate = 0.0030;
    c.move_water_rate = 0.0120;
    c.thermal_rate = 0.05;
    c.activity_heat_c = 1.5;
    c.fatigue_rate = 0.0200;
    c.recovery_rate = 0.0500;
    c.starve_health_rate = 0.0500;
    return c;
}

const char *ws_need_name(int need)
{
    switch (need) {
    case WS_NEED_ENERGY: return "energy";
    case WS_NEED_WATER: return "water";
    case WS_NEED_WARMTH: return "warmth";
    case WS_NEED_REST: return "rest";
    default: return "unknown";
    }
}

void ws_organism_init(ws_organism *o, const ws_organism_config *config)
{
    if (o == NULL) {
        return;
    }
    o->config = (config != NULL) ? *config : ws_organism_config_default();
    o->energy = 1.0;
    o->hydration = 1.0;
    o->body_temp_c = o->config.comfort_temp_c;
    o->fatigue = 0.0;
    o->health = 1.0;
    o->alive = 1;
    o->hunger = 0.0;
    o->thirst = 0.0;
    o->warmth = 0.0;
}

double ws_organism_drive(const ws_organism *o, int need)
{
    if (o == NULL) {
        return 0.0;
    }
    switch (need) {
    case WS_NEED_ENERGY:
        return o->hunger;
    case WS_NEED_WATER:
        return o->thirst;
    case WS_NEED_WARMTH:
        return o->warmth;
    case WS_NEED_REST:
        return o->fatigue;
    default:
        return 0.0;
    }
}

int ws_organism_most_urgent(const ws_organism *o, double threshold)
{
    if (o == NULL) {
        return -1;
    }
    int best = -1;
    double best_drive = threshold;
    for (int n = 0; n < WS_NEED_COUNT; ++n) {
        double d = ws_organism_drive(o, n);
        if (d > best_drive) {
            best_drive = d;
            best = n;
        }
    }
    return best;
}

static void update_drives(ws_organism *o)
{
    o->hunger = ws_clamp_double(1.0 - o->energy, 0.0, 1.0);
    o->thirst = ws_clamp_double(1.0 - o->hydration, 0.0, 1.0);
    double err = fabs(o->body_temp_c - o->config.comfort_temp_c) / o->config.thermal_scale_c;
    o->warmth = ws_clamp_double(err, 0.0, 1.0);
}

ws_result ws_organism_step(ws_organism *o, double dt, double speed,
                           double ambient_temp_c, int resting)
{
    if (o == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (!(dt > 0.0) || !isfinite(speed) || speed < 0.0) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    if (!o->alive) {
        update_drives(o);
        return WS_OK;
    }
    const ws_organism_config *c = &o->config;

    /* Reserves: basal cost plus a speed-proportional cost. */
    double de = (c->basal_energy_rate + c->move_energy_rate * speed) * dt;
    double dw = (c->basal_water_rate + c->move_water_rate * speed) * dt;
    o->energy = ws_clamp_double(o->energy - de, 0.0, 1.0);
    o->hydration = ws_clamp_double(o->hydration - dw, 0.0, 1.0);

    /* Fatigue rises with activity and recovers at rest. */
    if (resting) {
        o->fatigue = ws_clamp_double(o->fatigue - c->recovery_rate * dt, 0.0, 1.0);
    } else {
        o->fatigue = ws_clamp_double(o->fatigue + c->fatigue_rate * speed * dt, 0.0, 1.0);
    }

    /* Thermoregulation: body temperature relaxes toward a target set by
     * activity heat and a weak pull toward the ambient temperature. */
    double target = c->comfort_temp_c + c->activity_heat_c * speed;
    target += (ambient_temp_c - c->comfort_temp_c) * 0.15;
    o->body_temp_c += (target - o->body_temp_c) * c->thermal_rate * dt;
    o->body_temp_c = ws_clamp_double(o->body_temp_c,
                                     c->comfort_temp_c - 4.0 * c->thermal_scale_c,
                                     c->comfort_temp_c + 4.0 * c->thermal_scale_c);

    /* Health is spent only while a reserve is empty; the animal dies when it
     * runs out. This makes "no free energy" a testable invariant. */
    if (o->energy <= 0.0 || o->hydration <= 0.0) {
        o->health = ws_clamp_double(o->health - c->starve_health_rate * dt, 0.0, 1.0);
    }
    if (o->health <= 0.0) {
        o->alive = 0;
    }

    update_drives(o);
    return WS_OK;
}

double ws_organism_eat(ws_organism *o, double amount)
{
    if (o == NULL || !(amount > 0.0)) {
        return 0.0;
    }
    double room = 1.0 - o->energy;
    double absorbed = ws_clamp_double(amount, 0.0, room);
    o->energy = ws_clamp_double(o->energy + absorbed, 0.0, 1.0);
    update_drives(o);
    return absorbed;
}

double ws_organism_drink(ws_organism *o, double amount)
{
    if (o == NULL || !(amount > 0.0)) {
        return 0.0;
    }
    double room = 1.0 - o->hydration;
    double absorbed = ws_clamp_double(amount, 0.0, room);
    o->hydration = ws_clamp_double(o->hydration + absorbed, 0.0, 1.0);
    update_drives(o);
    return absorbed;
}
