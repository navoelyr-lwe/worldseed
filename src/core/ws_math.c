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

double ws_deg_to_rad(double degrees)
{
    return degrees * (WS_PI / 180.0);
}

ws_vec3 ws_vec3_cross(ws_vec3 a, ws_vec3 b)
{
    return ws_vec3_make(a.y * b.z - a.z * b.y,
                        a.z * b.x - a.x * b.z,
                        a.x * b.y - a.y * b.x);
}

ws_vec3 ws_vec3_normalize(ws_vec3 v)
{
    double len = ws_vec3_length(v);
    if (len == 0.0) {
        return ws_vec3_make(0.0, 0.0, 0.0);
    }
    return ws_vec3_scale(v, 1.0 / len);
}

ws_vec3 ws_vec3_lerp(ws_vec3 a, ws_vec3 b, double t)
{
    return ws_vec3_make(ws_lerp(a.x, b.x, t),
                        ws_lerp(a.y, b.y, t),
                        ws_lerp(a.z, b.z, t));
}

ws_vec4 ws_vec4_make(double x, double y, double z, double w)
{
    ws_vec4 v = {x, y, z, w};
    return v;
}

ws_vec4 ws_vec4_add(ws_vec4 a, ws_vec4 b)
{
    return ws_vec4_make(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

ws_vec4 ws_vec4_sub(ws_vec4 a, ws_vec4 b)
{
    return ws_vec4_make(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

ws_vec4 ws_vec4_scale(ws_vec4 v, double s)
{
    return ws_vec4_make(v.x * s, v.y * s, v.z * s, v.w * s);
}

double ws_vec4_dot(ws_vec4 a, ws_vec4 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

ws_vec4 ws_vec4_lerp(ws_vec4 a, ws_vec4 b, double t)
{
    return ws_vec4_make(ws_lerp(a.x, b.x, t), ws_lerp(a.y, b.y, t),
                        ws_lerp(a.z, b.z, t), ws_lerp(a.w, b.w, t));
}

ws_mat4 ws_mat4_identity(void)
{
    ws_mat4 r;
    for (int i = 0; i < 16; ++i) {
        r.m[i] = 0.0;
    }
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0;
    return r;
}

ws_mat4 ws_mat4_multiply(ws_mat4 a, ws_mat4 b)
{
    ws_mat4 r;
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            double sum = 0.0;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[k * 4 + row] * b.m[c * 4 + k];
            }
            r.m[c * 4 + row] = sum;
        }
    }
    return r;
}

ws_vec4 ws_mat4_mul_vec4(ws_mat4 m, ws_vec4 v)
{
    return ws_vec4_make(m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z + m.m[12] * v.w,
                        m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z + m.m[13] * v.w,
                        m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z + m.m[14] * v.w,
                        m.m[3] * v.x + m.m[7] * v.y + m.m[11] * v.z + m.m[15] * v.w);
}

ws_vec4 ws_mat4_mul_vec4_dir(ws_mat4 m, ws_vec4 v)
{
    return ws_vec4_make(m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z,
                        m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z,
                        m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z,
                        0.0);
}

ws_mat4 ws_mat4_transpose(ws_mat4 m)
{
    ws_mat4 r;
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            r.m[c * 4 + row] = m.m[row * 4 + c];
        }
    }
    return r;
}

ws_mat3 ws_mat4_to_mat3(ws_mat4 m)
{
    ws_mat3 r;
    for (int c = 0; c < 3; ++c) {
        for (int row = 0; row < 3; ++row) {
            r.m[c * 3 + row] = m.m[c * 4 + row];
        }
    }
    return r;
}

ws_vec3 ws_mat3_mul_vec3(ws_mat3 m, ws_vec3 v)
{
    return ws_vec3_make(m.m[0] * v.x + m.m[3] * v.y + m.m[6] * v.z,
                        m.m[1] * v.x + m.m[4] * v.y + m.m[7] * v.z,
                        m.m[2] * v.x + m.m[5] * v.y + m.m[8] * v.z);
}

ws_mat3 ws_mat4_normal_matrix(ws_mat4 m)
{
    ws_mat3 a = ws_mat4_to_mat3(m);
    double det = a.m[0] * (a.m[4] * a.m[8] - a.m[5] * a.m[7])
               - a.m[3] * (a.m[1] * a.m[8] - a.m[2] * a.m[7])
               + a.m[6] * (a.m[1] * a.m[5] - a.m[2] * a.m[4]);
    if (det > -1e-12 && det < 1e-12) {
        return a;
    }
    double inv = 1.0 / det;
    ws_mat3 cof;
    cof.m[0] = (a.m[4] * a.m[8] - a.m[5] * a.m[7]) * inv;
    cof.m[1] = (a.m[2] * a.m[7] - a.m[1] * a.m[8]) * inv;
    cof.m[2] = (a.m[1] * a.m[5] - a.m[2] * a.m[4]) * inv;
    cof.m[3] = (a.m[5] * a.m[6] - a.m[3] * a.m[8]) * inv;
    cof.m[4] = (a.m[0] * a.m[8] - a.m[2] * a.m[6]) * inv;
    cof.m[5] = (a.m[2] * a.m[3] - a.m[0] * a.m[5]) * inv;
    cof.m[6] = (a.m[3] * a.m[7] - a.m[4] * a.m[6]) * inv;
    cof.m[7] = (a.m[1] * a.m[6] - a.m[0] * a.m[7]) * inv;
    cof.m[8] = (a.m[0] * a.m[4] - a.m[1] * a.m[3]) * inv;
    /* Normal matrix = transpose(inverse) = cofactor matrix as written above. */
    return cof;
}

ws_mat4 ws_mat4_translation(ws_vec3 t)
{
    ws_mat4 r = ws_mat4_identity();
    r.m[12] = t.x;
    r.m[13] = t.y;
    r.m[14] = t.z;
    return r;
}

ws_mat4 ws_mat4_scale(ws_vec3 s)
{
    ws_mat4 r = ws_mat4_identity();
    r.m[0] = s.x;
    r.m[5] = s.y;
    r.m[10] = s.z;
    return r;
}

ws_mat4 ws_mat4_rotation_x(double radians)
{
    double c = cos(radians);
    double s = sin(radians);
    ws_mat4 r = ws_mat4_identity();
    r.m[5] = c;
    r.m[6] = s;
    r.m[9] = -s;
    r.m[10] = c;
    return r;
}

ws_mat4 ws_mat4_rotation_y(double radians)
{
    double c = cos(radians);
    double s = sin(radians);
    ws_mat4 r = ws_mat4_identity();
    r.m[0] = c;
    r.m[2] = -s;
    r.m[8] = s;
    r.m[10] = c;
    return r;
}

ws_mat4 ws_mat4_rotation_z(double radians)
{
    double c = cos(radians);
    double s = sin(radians);
    ws_mat4 r = ws_mat4_identity();
    r.m[0] = c;
    r.m[1] = s;
    r.m[4] = -s;
    r.m[5] = c;
    return r;
}

ws_mat4 ws_mat4_look_at(ws_vec3 eye, ws_vec3 target, ws_vec3 up)
{
    ws_vec3 f = ws_vec3_normalize(ws_vec3_sub(target, eye));
    ws_vec3 s = ws_vec3_normalize(ws_vec3_cross(f, up));
    ws_vec3 u = ws_vec3_cross(s, f);

    ws_mat4 r = ws_mat4_identity();
    r.m[0] = s.x;
    r.m[4] = s.y;
    r.m[8] = s.z;
    r.m[1] = u.x;
    r.m[5] = u.y;
    r.m[9] = u.z;
    r.m[2] = -f.x;
    r.m[6] = -f.y;
    r.m[10] = -f.z;
    r.m[12] = -ws_vec3_dot(s, eye);
    r.m[13] = -ws_vec3_dot(u, eye);
    r.m[14] = ws_vec3_dot(f, eye);
    return r;
}

ws_mat4 ws_mat4_perspective(double fov_y_radians, double aspect, double near_z, double far_z)
{
    ws_mat4 r;
    for (int i = 0; i < 16; ++i) {
        r.m[i] = 0.0;
    }
    double f = 1.0 / tan(fov_y_radians * 0.5);
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (far_z + near_z) / (near_z - far_z);
    r.m[11] = -1.0;
    r.m[14] = (2.0 * far_z * near_z) / (near_z - far_z);
    return r;
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
