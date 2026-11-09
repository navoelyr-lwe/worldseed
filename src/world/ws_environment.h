/* Worldseed Engine — atmosphere, sun and weather state.
 *
 * One shared environment state feeds both the simulation and the renderer, so a
 * visual rain, wind or fog is tied to the simulated weather rather than being a
 * random post-process (docs/04). The model is a documented approximation:
 *  - solar position from latitude, day-of-year declination and hour angle;
 *  - temperature from a seasonal term, a diurnal term and an altitude lapse;
 *  - wind, humidity and cloud from slow deterministic noise fields;
 *  - fog density from humidity.
 * This is not a full radiative-transfer or pressure-field model; that is later
 * work and is recorded in the model card. All outputs are deterministic.
 */
#ifndef WS_WORLD_ENVIRONMENT_H
#define WS_WORLD_ENVIRONMENT_H

#include <stdint.h>

#include "../core/ws_result.h"
#include "../core/ws_math.h"

#define WS_ENVIRONMENT_VERSION 1u

typedef struct ws_env_config {
    uint64_t seed;
    double latitude_deg;   /* degrees north, positive */
    int day_of_year;       /* 1..365 */
    double mean_temperature_c;
    double daily_swing_c;  /* peak-to-mean diurnal amplitude */
    double seasonal_swing_c;
} ws_env_config;

typedef struct ws_environment {
    ws_env_config config;
    double time_of_day_s;  /* seconds since local midnight */
    ws_vec3 sun_direction; /* unit vector from the scene toward the sun */
    double sun_altitude;   /* sine of elevation angle, -1..1 */
    double daylight;       /* 0 (night) .. 1 (full day) */
    double temperature_c;
    ws_vec2 wind;          /* horizontal wind vector, m/s */
    double wind_speed;
    double humidity;       /* 0..1 */
    double precipitation;  /* 0..1; 0 = dry, 1 = heavy rain */
    double fog_density;    /* exponential fog coefficient */
} ws_environment;

ws_env_config ws_env_config_default(uint64_t seed);

void ws_environment_init(ws_environment *env, const ws_env_config *config);

/* Recompute all fields for a time of day (seconds since midnight) at an
 * altitude in metres. Pure function of the config and inputs. */
void ws_environment_update(ws_environment *env, double time_of_day_s, double altitude_m);

#endif /* WS_WORLD_ENVIRONMENT_H */
