/* Worldseed Engine — small deterministic math primitives.
 *
 * Only operations the engine needs today live here. All values are double
 * precision unless a type says otherwise; integer helpers are exact.
 */
#ifndef WS_CORE_MATH_H
#define WS_CORE_MATH_H

#include <stddef.h>
#include <stdint.h>

typedef struct ws_vec2 {
    double x;
    double y;
} ws_vec2;

typedef struct ws_vec3 {
    double x;
    double y;
    double z;
} ws_vec3;

typedef struct ws_aabb3 {
    ws_vec3 min;
    ws_vec3 max;
} ws_aabb3;

double ws_clamp_double(double value, double lo, double hi);
double ws_lerp(double a, double b, double t);

ws_vec2 ws_vec2_make(double x, double y);
ws_vec2 ws_vec2_add(ws_vec2 a, ws_vec2 b);
ws_vec2 ws_vec2_sub(ws_vec2 a, ws_vec2 b);
ws_vec2 ws_vec2_scale(ws_vec2 v, double s);
double ws_vec2_dot(ws_vec2 a, ws_vec2 b);
double ws_vec2_length(ws_vec2 v);

ws_vec3 ws_vec3_make(double x, double y, double z);
ws_vec3 ws_vec3_add(ws_vec3 a, ws_vec3 b);
ws_vec3 ws_vec3_sub(ws_vec3 a, ws_vec3 b);
ws_vec3 ws_vec3_scale(ws_vec3 v, double s);
double ws_vec3_dot(ws_vec3 a, ws_vec3 b);
double ws_vec3_length(ws_vec3 v);

/* Returns 1 if the box is empty or inverted on any axis, else 0. */
int ws_aabb3_is_empty(ws_aabb3 box);

/* Checked size multiplication: returns 0 on success and writes product,
 * or returns 0 and leaves *out untouched when the multiply overflows. */
int ws_mul_size_checked(size_t count, size_t element_size, size_t *out);

#endif /* WS_CORE_MATH_H */
