/* Worldseed Engine — atmosphere, sun and weather state. */
#include "ws_environment.h"

#include <math.h>

#include "../core/ws_noise.h"

#define ENV_CLOUD_SALT 0x6666666666666666ULL
#define ENV_WIND_SALT 0x7777777777777777ULL

ws_env_config ws_env_config_default(uint64_t seed)
{
    ws_env_config c;
    c.seed = seed;
    c.latitude_deg = 48.0; /* temperate northern reference ecosystem */
    c.day_of_year = 172;   /* late June: long days */
    c.mean_temperature_c = 12.0;
    c.daily_swing_c = 7.0;
    c.seasonal_swing_c = 9.0;
    return c;
}

void ws_environment_init(ws_environment *env, const ws_env_config *config)
{
    if (env == NULL) {
        return;
    }
    if (config != NULL) {
        env->config = *config;
    } else {
        env->config = ws_env_config_default(0ULL);
    }
    ws_environment_update(env, 6.0 * 3600.0, 0.0);
}

void ws_environment_update(ws_environment *env, double time_of_day_s, double altitude_m)
{
    if (env == NULL) {
        return;
    }
    const double day_seconds = 86400.0;
    double t = fmod(time_of_day_s, day_seconds);
    if (t < 0.0) {
        t += day_seconds;
    }
    env->time_of_day_s = t;

    /* Solar declination and hour angle (standard low-precision model). */
    double lat = ws_deg_to_rad(env->config.latitude_deg);
    double decl = 23.44 * ws_deg_to_rad(sin(2.0 * WS_PI *
                     ((double)env->config.day_of_year - 81.0) / 365.0));
    double hour_angle = ws_deg_to_rad((t / 3600.0 - 12.0) * 15.0);

    double sin_alt = sin(lat) * sin(decl) + cos(lat) * cos(decl) * cos(hour_angle);
    double cos_alt = sqrt(fmax(0.0, 1.0 - sin_alt * sin_alt));
    /* Azimuth measured from north, clockwise; east is +x, north is -z. */
    double cos_az = (sin(decl) - sin(lat) * sin_alt) / (cos(lat) * cos_alt + 1.0e-9);
    cos_az = ws_clamp_double(cos_az, -1.0, 1.0);
    double az = acos(cos_az);
    if (hour_angle > 0.0) {
        az = 2.0 * WS_PI - az;
    }
    env->sun_altitude = sin_alt;
    env->sun_direction = ws_vec3_normalize(ws_vec3_make(cos_alt * sin(az),
                                                        sin_alt,
                                                        -cos_alt * cos(az)));
    env->daylight = ws_clamp_double((sin_alt + 0.15) / 0.35, 0.0, 1.0);

    /* Temperature: seasonal term from the day of year, diurnal term peaking in
     * the early afternoon, and a lapse rate with altitude. */
    double seasonal = -cos(2.0 * WS_PI * ((double)env->config.day_of_year - 15.0) / 365.0);
    double diurnal = sin(2.0 * WS_PI * (t - 9.0 * 3600.0) / day_seconds);
    env->temperature_c = env->config.mean_temperature_c +
                         env->config.seasonal_swing_c * seasonal +
                         env->config.daily_swing_c * diurnal -
                         0.0065 * altitude_m;

    /* Slow wind and cloud fields, plus humidity and fog. */
    double hours = t / 3600.0;
    double wx = ws_noise_fbm2(env->config.seed ^ ENV_WIND_SALT, hours / 6.0, 0.0, 3, 2.0, 0.5);
    double wy = ws_noise_fbm2(env->config.seed ^ ENV_WIND_SALT, 0.0, hours / 6.0, 3, 2.0, 0.5);
    env->wind = ws_vec2_make((wx - 0.5) * 12.0, (wy - 0.5) * 12.0);
    env->wind_speed = ws_vec2_length(env->wind);

    double cloud = ws_noise_fbm2(env->config.seed ^ ENV_CLOUD_SALT, hours / 8.0, 0.0, 3, 2.0, 0.5);
    env->humidity = ws_clamp_double(0.35 + 0.5 * cloud, 0.0, 1.0);
    double rain = ws_clamp_double((cloud - 0.72) / 0.28, 0.0, 1.0);
    env->precipitation = rain * (1.0 - 0.4 * env->daylight);
    env->fog_density = 0.02 * env->humidity;
}
