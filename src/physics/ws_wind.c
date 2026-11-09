/* Worldseed Engine — wind field for physics. */
#include "ws_wind.h"

#include <math.h>

ws_vec3 ws_wind_at_height(const ws_environment *env, double height_m)
{
    if (env == NULL) {
        return ws_vec3_make(0.0, 0.0, 0.0);
    }
    ws_vec2 w = env->wind;
    double speed = ws_vec2_length(w);
    if (speed <= 0.0) {
        return ws_vec3_make(0.0, 0.0, 0.0);
    }
    double z = height_m > 0.0 ? height_m : 0.0;
    double scale = pow(z / WS_WIND_REFERENCE_HEIGHT, WS_WIND_SHEAR_EXPONENT);
    ws_vec3 dir = ws_vec3_make(w.x / speed, 0.0, w.y / speed);
    return ws_vec3_scale(dir, speed * scale);
}
