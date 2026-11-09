/* Worldseed Engine — small deterministic math primitives. */
#include "ws_math.h"

#include <math.h>
#include <stddef.h>

double ws_clamp_double(double value, double lo, double hi)
{
    if (value < lo) {
        return lo;
    }
    if (value > hi) {
        return hi;
    }
    return value;
}

double ws_lerp(double a, double b, double t)
{
    return a + (b - a) * t;
}

ws_vec2 ws_vec2_make(double x, double y)
{
    ws_vec2 v = {x, y};
    return v;
}

ws_vec2 ws_vec2_add(ws_vec2 a, ws_vec2 b)
{
    return ws_vec2_make(a.x + b.x, a.y + b.y);
}

ws_vec2 ws_vec2_sub(ws_vec2 a, ws_vec2 b)
{
    return ws_vec2_make(a.x - b.x, a.y - b.y);
}

ws_vec2 ws_vec2_scale(ws_vec2 v, double s)
{
    return ws_vec2_make(v.x * s, v.y * s);
}

double ws_vec2_dot(ws_vec2 a, ws_vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

double ws_vec2_length(ws_vec2 v)
{
    return sqrt(ws_vec2_dot(v, v));
}

ws_vec3 ws_vec3_make(double x, double y, double z)
{
    ws_vec3 v = {x, y, z};
    return v;
}

ws_vec3 ws_vec3_add(ws_vec3 a, ws_vec3 b)
{
    return ws_vec3_make(a.x + b.x, a.y + b.y, a.z + b.z);
}

ws_vec3 ws_vec3_sub(ws_vec3 a, ws_vec3 b)
{
    return ws_vec3_make(a.x - b.x, a.y - b.y, a.z - b.z);
}

ws_vec3 ws_vec3_scale(ws_vec3 v, double s)
{
    return ws_vec3_make(v.x * s, v.y * s, v.z * s);
}

double ws_vec3_dot(ws_vec3 a, ws_vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

double ws_vec3_length(ws_vec3 v)
{
    return sqrt(ws_vec3_dot(v, v));
}

int ws_aabb3_is_empty(ws_aabb3 box)
{
    return box.min.x > box.max.x || box.min.y > box.max.y || box.min.z > box.max.z;
}

int ws_mul_size_checked(size_t count, size_t element_size, size_t *out)
{
    if (out == NULL) {
        return 0;
    }
    if (element_size != 0U && count > (SIZE_MAX / element_size)) {
        return 0;
    }
    *out = count * element_size;
    return 1;
}
