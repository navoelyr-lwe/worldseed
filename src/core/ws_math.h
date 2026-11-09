/* Worldseed Engine — small deterministic math primitives.
 *
 * Only operations the engine needs today live here. All values are double
 * precision unless a type says otherwise; integer helpers are exact.
 */
#ifndef WS_CORE_MATH_H
#define WS_CORE_MATH_H

#include <stddef.h>
#include <stdint.h>

#define WS_PI 3.14159265358979323846

typedef struct ws_vec2 {
    double x;
    double y;
} ws_vec2;

typedef struct ws_vec3 {
    double x;
    double y;
    double z;
} ws_vec3;

typedef struct ws_vec4 {
    double x;
    double y;
    double z;
    double w;
} ws_vec4;

typedef struct ws_aabb3 {
    ws_vec3 min;
    ws_vec3 max;
} ws_aabb3;

/* Column-major 4x4 matrix: m[col * 4 + row]. */
typedef struct ws_mat4 {
    double m[16];
} ws_mat4;

/* Column-major 3x3 matrix: m[col * 3 + row]. */
typedef struct ws_mat3 {
    double m[9];
} ws_mat3;

double ws_clamp_double(double value, double lo, double hi);
double ws_lerp(double a, double b, double t);
double ws_deg_to_rad(double degrees);

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
ws_vec3 ws_vec3_cross(ws_vec3 a, ws_vec3 b);
double ws_vec3_length(ws_vec3 v);
/* Unit vector in the direction of v; returns the zero vector for v == 0. */
ws_vec3 ws_vec3_normalize(ws_vec3 v);
ws_vec3 ws_vec3_lerp(ws_vec3 a, ws_vec3 b, double t);

ws_vec4 ws_vec4_make(double x, double y, double z, double w);
ws_vec4 ws_vec4_add(ws_vec4 a, ws_vec4 b);
ws_vec4 ws_vec4_sub(ws_vec4 a, ws_vec4 b);
ws_vec4 ws_vec4_scale(ws_vec4 v, double s);
double ws_vec4_dot(ws_vec4 a, ws_vec4 b);
ws_vec4 ws_vec4_lerp(ws_vec4 a, ws_vec4 b, double t);

ws_mat4 ws_mat4_identity(void);
ws_mat4 ws_mat4_multiply(ws_mat4 a, ws_mat4 b);
ws_vec4 ws_mat4_mul_vec4(ws_mat4 m, ws_vec4 v);
/* Transform a direction: ignore translation, do not divide by w. */
ws_vec4 ws_mat4_mul_vec4_dir(ws_mat4 m, ws_vec4 v);
ws_mat4 ws_mat4_transpose(ws_mat4 m);
/* Upper-left 3x3 of a matrix. */
ws_mat3 ws_mat4_to_mat3(ws_mat4 m);
/* Inverse-transpose of the upper-left 3x3, for transforming normals. Falls
 * back to the plain 3x3 when the matrix is singular. */
ws_mat3 ws_mat4_normal_matrix(ws_mat4 m);
ws_vec3 ws_mat3_mul_vec3(ws_mat3 m, ws_vec3 v);
ws_mat4 ws_mat4_translation(ws_vec3 t);
ws_mat4 ws_mat4_scale(ws_vec3 s);
/* Rotation about a principal axis. Angle in radians. */
ws_mat4 ws_mat4_rotation_x(double radians);
ws_mat4 ws_mat4_rotation_y(double radians);
ws_mat4 ws_mat4_rotation_z(double radians);
/* Right-handed look-at view matrix. */
ws_mat4 ws_mat4_look_at(ws_vec3 eye, ws_vec3 target, ws_vec3 up);
/* Perspective projection, right-handed, mapping depth to [-1, 1] (OpenGL-style
 * clip space, since the engine performs its own divide and viewport map). */
ws_mat4 ws_mat4_perspective(double fov_y_radians, double aspect, double near_z, double far_z);

/* Returns 1 if the box is empty or inverted on any axis, else 0. */
int ws_aabb3_is_empty(ws_aabb3 box);

/* Checked size multiplication: returns 1 and writes the product, or returns 0
 * and leaves *out untouched when the multiply overflows or out is NULL. */
int ws_mul_size_checked(size_t count, size_t element_size, size_t *out);

#endif /* WS_CORE_MATH_H */
