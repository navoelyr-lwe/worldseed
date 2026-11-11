/* Worldseed Engine — reference CPU scene renderer.
 *
 * Pipeline: model/view/projection transform, near-plane clipping, perspective
 * divide, viewport map, tiled scanline rasterisation with perspective-correct
 * interpolation of world position, world normal and albedo, then per-pixel
 * shading. Shading is a Lambert diffuse term, a Blinn-Phong specular term and a
 * hemisphere ambient, optionally followed by exponential distance fog and an
 * optional gradient sky background. All shading is linear, then exposure and an
 * optional Reinhard tonemap are applied before sRGB encoding.
 *
 * This is the mandatory correctness reference and CPU fallback path; it is not
 * the final high-quality renderer.
 */
#include "ws_render.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../core/ws_math.h"
#include "../core/ws_sched.h"

typedef struct clip_vertex {
    double x, y, z, w;   /* clip space */
    double wx, wy, wz;   /* world-space position */
    double nx, ny, nz;   /* world-space normal (normalised) */
    double r, g, b;      /* albedo, linear */
} clip_vertex;

typedef struct screen_vertex {
    double x, y;      /* pixels */
    double z;         /* NDC depth in [0, 1], smaller is nearer */
    double inv_w;     /* 1 / clip w */
    double wx, wy, wz;
    double nx, ny, nz;
    double r, g, b;
} screen_vertex;

/* sRGB transfer function (IEC 61966-2-1). */
static double srgb_to_linear(double c8)
{
    double c = c8 / 255.0;
    if (c <= 0.04045) {
        return c / 12.92;
    }
    return pow((c + 0.055) / 1.055, 2.4);
}

static double linear_to_srgb(double c)
{
    if (c <= 0.0) {
        return 0.0;
    }
    if (c <= 0.0031308) {
        return c * 12.92;
    }
    return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
}

/* Encode linear RGB to 8-bit display with a 4096-entry lookup table, linear
 * within each table step. This replaces three pow() calls per channel per pixel
 * with a table lookup and a short lerp. The table is fine enough that the
 * quantised 8-bit result matches the exact sRGB encode for representable
 * values. */
#define ENCODE_LUT_BITS 12
#define ENCODE_LUT_SIZE (1u << ENCODE_LUT_BITS)
#define ENCODE_LUT_MAX 1.0

static double g_encode_lut[ENCODE_LUT_SIZE + 1u];
static int g_encode_lut_ready = 0;

static void encode_lut_init(void)
{
    if (g_encode_lut_ready) {
        return;
    }
    for (unsigned i = 0; i <= ENCODE_LUT_SIZE; ++i) {
        double lin = (double)i / (double)ENCODE_LUT_SIZE * ENCODE_LUT_MAX;
        g_encode_lut[i] = linear_to_srgb(lin) * 255.0;
    }
    g_encode_lut_ready = 1;
}

static unsigned char encode_display_u8(double linear, const ws_render_settings *s)
{
    double v = linear * s->exposure;
    if (s->tonemap == 1) {
        v = v / (1.0 + v); /* Reinhard */
    } else {
        v = ws_clamp_double(v, 0.0, 1.0);
    }
    if (v <= 0.0) {
        return 0u;
    }
    double x = v * (double)ENCODE_LUT_SIZE;
    if (x >= (double)ENCODE_LUT_SIZE) {
        return 255u;
    }
    unsigned i = (unsigned)x;
    double t = x - (double)i;
    double e = g_encode_lut[i] + (g_encode_lut[i + 1u] - g_encode_lut[i]) * t;
    if (e <= 0.0) {
        return 0u;
    }
    if (e >= 255.0) {
        return 255u;
    }
    return (unsigned char)(e + 0.5);
}

static void linear3(const uint8_t srgb[3], double out[3])
{
    out[0] = srgb_to_linear((double)srgb[0]);
    out[1] = srgb_to_linear((double)srgb[1]);
    out[2] = srgb_to_linear((double)srgb[2]);
}

static clip_vertex make_clip_vertex(const ws_scene_object *obj, ws_mat4 mvp,
                                    ws_mat3 normal_mat, const ws_vertex *v)
{
    clip_vertex out;
    ws_vec4 clip = ws_mat4_mul_vec4(mvp, ws_vec4_make(v->position.x, v->position.y, v->position.z, 1.0));
    out.x = clip.x;
    out.y = clip.y;
    out.z = clip.z;
    out.w = clip.w;

    ws_vec4 world = ws_mat4_mul_vec4(obj->transform,
                                     ws_vec4_make(v->position.x, v->position.y, v->position.z, 1.0));
    out.wx = world.x;
    out.wy = world.y;
    out.wz = world.z;

    ws_vec3 n = ws_vec3_normalize(ws_mat3_mul_vec3(normal_mat, v->normal));
    out.nx = n.x;
    out.ny = n.y;
    out.nz = n.z;

    if (obj->material.albedo_map != NULL) {
        double tex[3];
        ws_texture_sample(obj->material.albedo_map, v->u, v->v, tex);
        out.r = tex[0];
        out.g = tex[1];
        out.b = tex[2];
    } else {
        out.r = obj->material.albedo[0];
        out.g = obj->material.albedo[1];
        out.b = obj->material.albedo[2];
    }
    return out;
}

static clip_vertex lerp_clip(clip_vertex a, clip_vertex b, double t)
{
    clip_vertex out;
    out.x = a.x + (b.x - a.x) * t;
    out.y = a.y + (b.y - a.y) * t;
    out.z = a.z + (b.z - a.z) * t;
    out.w = a.w + (b.w - a.w) * t;
    out.wx = a.wx + (b.wx - a.wx) * t;
    out.wy = a.wy + (b.wy - a.wy) * t;
    out.wz = a.wz + (b.wz - a.wz) * t;
    out.nx = a.nx + (b.nx - a.nx) * t;
    out.ny = a.ny + (b.ny - a.ny) * t;
    out.nz = a.nz + (b.nz - a.nz) * t;
    out.r = a.r + (b.r - a.r) * t;
    out.g = a.g + (b.g - a.g) * t;
    out.b = a.b + (b.b - a.b) * t;
    return out;
}

/* Clip a polygon against the plane w + z >= 0 (near plane for this projection).
 * `in` has `n` vertices; writes up to `n + 1` to `out` and returns the count. */
static int clip_near(const clip_vertex *in, int n, clip_vertex *out)
{
    int m = 0;
    for (int i = 0; i < n; ++i) {
        const clip_vertex *cur = &in[i];
        const clip_vertex *nxt = &in[(i + 1) % n];
        double dc = cur->w + cur->z;
        double dn = nxt->w + nxt->z;
        int cur_in = dc >= 0.0;
        int nxt_in = dn >= 0.0;
        if (cur_in) {
            out[m++] = *cur;
        }
        if (cur_in != nxt_in) {
            double t = dc / (dc - dn);
            out[m++] = lerp_clip(*cur, *nxt, t);
        }
    }
    return m;
}

static void to_screen(const clip_vertex *c, int fb_w, int fb_h, screen_vertex *s)
{
    double inv_w = 1.0 / c->w;
    double ndc_x = c->x * inv_w;
    double ndc_y = c->y * inv_w;
    double ndc_z = c->z * inv_w;
    s->x = (ndc_x * 0.5 + 0.5) * (double)fb_w;
    s->y = (1.0 - (ndc_y * 0.5 + 0.5)) * (double)fb_h;
    s->z = ndc_z * 0.5 + 0.5;
    s->inv_w = inv_w;
    s->wx = c->wx; s->wy = c->wy; s->wz = c->wz;
    s->nx = c->nx; s->ny = c->ny; s->nz = c->nz;
    s->r = c->r; s->g = c->g; s->b = c->b;
}

static double edge(double ax, double ay, double bx, double by, double px, double py)
{
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

/* Conservative per-band triangle rejection. Each band otherwise clips and
 * viewport-maps every triangle even though a small triangle touches one band
 * only. The prepass already gives us the projected vertices, so this reports
 * "disjoint" when the triangle's screen-space row range cannot reach rows
 * [band_y0, band_y1). Near-plane clipping only removes area inside the
 * projected triangle, so the unclipped row range is a superset of the
 * rasterised range; a disjoint verdict therefore never drops a pixel, and the
 * image stays bit-identical. Band 0 (which owns the image-wide counters) never
 * rejects. */
static int tri_band_disjoint(const clip_vertex *poly, int fb_h, int band_y0, int band_y1)
{
    double ymin = 1.0e30, ymax = -1.0e30;
    for (int k = 0; k < 3; ++k) {
        if (poly[k].w <= 0.0) {
            return 0; /* behind/at the eye plane: keep, let the clipper decide */
        }
        double sy = (1.0 - (poly[k].y / poly[k].w * 0.5 + 0.5)) * (double)fb_h;
        if (sy < ymin) { ymin = sy; }
        if (sy > ymax) { ymax = sy; }
    }
    if ((int)ymax + 1 < band_y0) {
        return 1;
    }
    if ((int)ymin > band_y1 - 1) {
        return 1;
    }
    return 0;
}

static double ndc_to_linear_depth(double ndc_z, double near_z, double far_z)
{
    return (2.0 * near_z * far_z) / (far_z + near_z - (ndc_z * 2.0 - 1.0) * (far_z - near_z));
}

/* --- Directional shadow map ---------------------------------------------
 *
 * One orthographic depth map rendered from the light's point of view. The
 * world-space bounds of the scene (from the object AABBs) define the light
 * frustum, so it needs no per-frame tuning. Sampling is a 3x3 PCF and is a pure
 * function of the stored depths, so the result is deterministic and identical
 * across the parallel render bands. This is the mandatory CPU shadow reference;
 * a GPU variant can mirror it. */
typedef struct shadow_map {
    const float *depth;      /* size*size, 1.0 = empty (no occluder) */
    unsigned size;
    double strength;         /* 0..1 how dark full shadow is */
    double bias;             /* light-space depth bias to fight acne */
    ws_mat4 light_view_proj;
    int ready;
} shadow_map;

static void scene_world_bounds(const ws_scene *scene, ws_vec3 *lo, ws_vec3 *hi)
{
    double mn[3] = {1.0e30, 1.0e30, 1.0e30};
    double mx[3] = {-1.0e30, -1.0e30, -1.0e30};
    for (uint32_t oi = 0; oi < scene->object_count; ++oi) {
        const ws_scene_object *obj = &scene->objects[oi];
        if (obj->mesh == NULL || obj->mesh->vertices == NULL) {
            continue;
        }
        ws_aabb3 b = ws_mesh_bounds(obj->mesh);
        for (int ci = 0; ci < 8; ++ci) {
            ws_vec3 c = ws_vec3_make((ci & 1) ? b.max.x : b.min.x,
                                     (ci & 2) ? b.max.y : b.min.y,
                                     (ci & 4) ? b.max.z : b.min.z);
            ws_vec4 w = ws_mat4_mul_vec4(obj->transform, ws_vec4_make(c.x, c.y, c.z, 1.0));
            double p[3] = {w.x, w.y, w.z};
            for (int k = 0; k < 3; ++k) {
                if (p[k] < mn[k]) { mn[k] = p[k]; }
                if (p[k] > mx[k]) { mx[k] = p[k]; }
            }
        }
    }
    if (mn[0] > mx[0]) {
        mn[0] = mn[1] = mn[2] = -1.0;
        mx[0] = mx[1] = mx[2] = 1.0;
    }
    *lo = ws_vec3_make(mn[0], mn[1], mn[2]);
    *hi = ws_vec3_make(mx[0], mx[1], mx[2]);
}

/* Light view-projection covering the scene bounds. The light sits one scene
 * radius behind the centre along the light direction and looks at the centre. */
static ws_mat4 shadow_light_matrix(const ws_scene *scene, ws_vec3 lo, ws_vec3 hi)
{
    ws_vec3 center = ws_vec3_scale(ws_vec3_add(lo, hi), 0.5);
    double radius = 0.5 * ws_vec3_length(ws_vec3_sub(hi, lo));
    if (radius < 1.0e-4) {
        radius = 1.0;
    }
    ws_vec3 dir = ws_vec3_normalize(scene->light.direction);
    ws_vec3 eye = ws_vec3_add(center, ws_vec3_scale(dir, -2.0 * radius));
    ws_vec3 up = (fabs(dir.y) > 0.99) ? ws_vec3_make(0.0, 0.0, 1.0)
                                      : ws_vec3_make(0.0, 1.0, 0.0);
    ws_mat4 view = ws_mat4_look_at(eye, center, up);
    ws_mat4 proj = ws_mat4_ortho(-radius, radius, -radius, radius, 0.0, 4.0 * radius);
    return ws_mat4_multiply(proj, view);
}

static clip_vertex make_depth_clip_vertex(ws_mat4 mvp, const ws_vertex *v)
{
    clip_vertex out;
    ws_vec4 clip = ws_mat4_mul_vec4(mvp,
                                    ws_vec4_make(v->position.x, v->position.y, v->position.z, 1.0));
    out.x = clip.x;
    out.y = clip.y;
    out.z = clip.z;
    out.w = clip.w;
    out.wx = out.wy = out.wz = 0.0;
    out.nx = out.ny = out.nz = 0.0;
    out.r = out.g = out.b = 0.0;
    return out;
}

/* Rasterise one light-space triangle writing the nearest depth (smaller wins). */
static void shadow_raster(const screen_vertex *v, float *depth, unsigned size)
{
    int w = (int)size;
    int h = (int)size;
    double area = edge(v[0].x, v[0].y, v[1].x, v[1].y, v[2].x, v[2].y);
    if (area == 0.0) {
        return;
    }
    double min_x = v[0].x, max_x = v[0].x, min_y = v[0].y, max_y = v[0].y;
    for (int i = 1; i < 3; ++i) {
        if (v[i].x < min_x) { min_x = v[i].x; }
        if (v[i].x > max_x) { max_x = v[i].x; }
        if (v[i].y < min_y) { min_y = v[i].y; }
        if (v[i].y > max_y) { max_y = v[i].y; }
    }
    int x0 = (int)min_x; if (x0 < 0) { x0 = 0; }
    int x1 = (int)max_x + 1; if (x1 > w - 1) { x1 = w - 1; }
    int y0 = (int)min_y; if (y0 < 0) { y0 = 0; }
    int y1 = (int)max_y + 1; if (y1 > h - 1) { y1 = h - 1; }
    if (x0 > x1 || y0 > y1) {
        return;
    }
    double inv_area = 1.0 / area;
    int inside_pos = area > 0.0;
    for (int y = y0; y <= y1; ++y) {
        double py = (double)y + 0.5;
        for (int x = x0; x <= x1; ++x) {
            double px = (double)x + 0.5;
            double w0 = edge(v[1].x, v[1].y, v[2].x, v[2].y, px, py);
            double w1 = edge(v[2].x, v[2].y, v[0].x, v[0].y, px, py);
            double w2 = edge(v[0].x, v[0].y, v[1].x, v[1].y, px, py);
            int inside = inside_pos ? (w0 >= 0.0 && w1 >= 0.0 && w2 >= 0.0)
                                    : (w0 <= 0.0 && w1 <= 0.0 && w2 <= 0.0);
            if (!inside) {
                continue;
            }
            double l0 = w0 * inv_area;
            double l1 = w1 * inv_area;
            double l2 = w2 * inv_area;
            double z = l0 * v[0].z + l1 * v[1].z + l2 * v[2].z;
            size_t idx = (size_t)y * (size_t)w + (size_t)x;
            if (z < (double)depth[idx]) {
                depth[idx] = (float)z;
            }
        }
    }
}

static void shadow_render_pass(const ws_scene *scene, ws_mat4 light_vp,
                               float *depth, unsigned size)
{
    size_t count = (size_t)size * (size_t)size;
    for (size_t i = 0; i < count; ++i) {
        depth[i] = 1.0f;
    }
    for (uint32_t oi = 0; oi < scene->object_count; ++oi) {
        const ws_scene_object *obj = &scene->objects[oi];
        if (obj->mesh == NULL || obj->mesh->vertices == NULL || obj->mesh->indices == NULL) {
            continue;
        }
        ws_mat4 mvp = ws_mat4_multiply(light_vp, obj->transform);
        const ws_mesh *mesh = obj->mesh;
        for (uint32_t t = 0u; t + 3u <= mesh->index_count; t += 3u) {
            clip_vertex poly[3];
            int ok = 1;
            for (int k = 0; k < 3; ++k) {
                uint32_t idx = mesh->indices[t + (uint32_t)k];
                if (idx >= mesh->vertex_count) {
                    ok = 0;
                    break;
                }
                poly[k] = make_depth_clip_vertex(mvp, &mesh->vertices[idx]);
            }
            if (!ok) {
                continue;
            }
            clip_vertex clipped[4];
            int cn = clip_near(poly, 3, clipped);
            if (cn < 3) {
                continue;
            }
            screen_vertex scr[4];
            for (int k = 0; k < cn; ++k) {
                to_screen(&clipped[k], (int)size, (int)size, &scr[k]);
            }
            for (int k = 1; k + 1 < cn; ++k) {
                screen_vertex tri[3] = {scr[0], scr[k], scr[k + 1]};
                shadow_raster(tri, depth, size);
            }
        }
    }
}

/* Fraction of the direct light reaching `world`: 1 in light, lower in shadow. */
static double shadow_factor(const shadow_map *sm, ws_vec3 world)
{
    if (sm == NULL || !sm->ready) {
        return 1.0;
    }
    ws_vec4 c = ws_mat4_mul_vec4(sm->light_view_proj, ws_vec4_make(world.x, world.y, world.z, 1.0));
    if (c.w == 0.0) {
        return 1.0;
    }
    double ndc_x = c.x / c.w;
    double ndc_y = c.y / c.w;
    double ndc_z = c.z / c.w;
    if (ndc_x < -1.0 || ndc_x > 1.0 || ndc_y < -1.0 || ndc_y > 1.0 ||
        ndc_z < -1.0 || ndc_z > 1.0) {
        return 1.0; /* outside the light frustum: treat as lit */
    }
    double u = ndc_x * 0.5 + 0.5;
    double v = 1.0 - (ndc_y * 0.5 + 0.5);
    double ref = ndc_z * 0.5 + 0.5;
    double texel = 1.0 / (double)sm->size;
    double lit = 0.0;
    int samples = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int sx = (int)((u + (double)dx * texel) * (double)sm->size);
            int sy = (int)((v + (double)dy * texel) * (double)sm->size);
            if (sx < 0 || sy < 0 || sx >= (int)sm->size || sy >= (int)sm->size) {
                continue;
            }
            double stored = (double)sm->depth[(size_t)sy * sm->size + (size_t)sx];
            lit += (ref - sm->bias > stored) ? 0.0 : 1.0;
            samples += 1;
        }
    }
    if (samples == 0) {
        return 1.0;
    }
    lit /= (double)samples;
    return 1.0 - sm->strength * (1.0 - lit);
}

/* Scene-constant terms hoisted out of the per-pixel shading loop. The light
 * colours and hemisphere ambient are sRGB constants, so decoding them with
 * pow() once per frame instead of once per pixel removes the dominant cost of
 * the inner loop without changing the result. */
typedef struct render_ctx {
    double light_color[3];
    double sky[3];
    double ground[3];
    double amb_scale;
    double light_intensity;
    int use_fog;
    double fog_lin[3];
    double near_z;
    double far_z;
    double fog_density;
    const shadow_map *shadow; /* NULL when shadows are off */
} render_ctx;

/* Per-material terms hoisted out of the per-pixel loop. */
typedef struct mat_ctx {
    double shininess;
    double mat_diffuse;
    double metallic;
    double specular;
} mat_ctx;

static void render_ctx_build(render_ctx *ctx, const ws_scene *scene,
                             const shadow_map *shadow)
{
    const ws_light *light = &scene->light;
    const ws_render_settings *s = &scene->settings;
    linear3(light->color, ctx->light_color);
    linear3(light->sky_color, ctx->sky);
    linear3(light->ground_color, ctx->ground);
    ctx->amb_scale = light->ambient_intensity;
    ctx->light_intensity = light->intensity;
    ctx->near_z = scene->camera.near_z;
    ctx->far_z = scene->camera.far_z;
    ctx->fog_density = s->fog_density;
    ctx->use_fog = s->fog_density > 0.0;
    ctx->shadow = shadow;
    if (ctx->use_fog) {
        if (s->sky_enabled) {
            linear3(light->sky_horizon, ctx->fog_lin);
        } else {
            ctx->fog_lin[0] = srgb_to_linear((double)scene->background[0]);
            ctx->fog_lin[1] = srgb_to_linear((double)scene->background[1]);
            ctx->fog_lin[2] = srgb_to_linear((double)scene->background[2]);
        }
    } else {
        ctx->fog_lin[0] = ctx->fog_lin[1] = ctx->fog_lin[2] = 0.0;
    }
}

static mat_ctx mat_ctx_build(const ws_material *m)
{
    mat_ctx c;
    c.metallic = ws_clamp_double(m->metallic, 0.0, 1.0);
    c.mat_diffuse = 1.0 - c.metallic;
    c.specular = m->specular;
    if (m->specular > 0.0) {
        double rough = ws_clamp_double(m->roughness, 0.02, 1.0);
        c.shininess = 2.0 / (rough * rough) - 2.0;
    } else {
        c.shininess = 0.0;
    }
    return c;
}

/* Shade one pixel from interpolated surface attributes. Writes linear RGB. */
static void shade_pixel(const ws_scene *scene, const render_ctx *ctx, const mat_ctx *mat,
                        ws_vec3 world_pos, ws_vec3 normal, const double albedo[3],
                        double out[3])
{
    const ws_light *light = &scene->light;
    /* Normalise the interpolated normal and the view vector. Computing each
     * length once (rather than through ws_vec3_length after normalising) saves
     * a square root per shaded pixel, which matters at millions of pixels. */
    ws_vec3 n;
    double nlen2 = ws_vec3_dot(normal, normal);
    if (nlen2 > 0.0) {
        n = ws_vec3_scale(normal, 1.0 / sqrt(nlen2));
    } else {
        n = ws_vec3_make(0.0, 1.0, 0.0);
    }
    ws_vec3 to_eye = ws_vec3_sub(scene->camera.eye, world_pos);
    double vlen2 = ws_vec3_dot(to_eye, to_eye);
    ws_vec3 view = vlen2 > 0.0 ? ws_vec3_scale(to_eye, 1.0 / sqrt(vlen2))
                               : ws_vec3_make(0.0, 0.0, 0.0);

    double diffuse = -ws_vec3_dot(n, light->direction);
    if (diffuse < 0.0) {
        diffuse = 0.0;
    }
    diffuse *= ctx->light_intensity;

    double up = 0.5 * (n.y + 1.0);

    double spec = 0.0;
    if (mat->specular > 0.0) {
        ws_vec3 h = ws_vec3_normalize(ws_vec3_sub(view, light->direction));
        double ndoth = ws_vec3_dot(n, h);
        if (ndoth > 0.0) {
            spec = pow(ndoth, mat->shininess) * mat->specular * ctx->light_intensity;
        }
    }

    /* Shadowing attenuates only the direct light; the hemisphere ambient term is
     * left intact. With shadows off this is exactly 1.0, so the multiply is a
     * no-op and the direct-only path stays bit-identical. */
    if (ctx->shadow != NULL) {
        double s = shadow_factor(ctx->shadow, world_pos);
        diffuse *= s;
        spec *= s;
    }

    for (int c = 0; c < 3; ++c) {
        double amb = ws_lerp(ctx->ground[c], ctx->sky[c], up) * ctx->amb_scale;
        double base = albedo[c];
        double diff = base * mat->mat_diffuse * (amb + diffuse * ctx->light_color[c]);
        double spec_tint = ws_lerp(1.0, base, mat->metallic);
        out[c] = diff + spec * spec_tint * ctx->light_color[c];
    }
}

/* Rasterise one screen triangle, but only the rows inside [band_y0, band_y1).
 * Returns the number of pixels actually shaded, so the caller can count drawn
 * triangles and shaded pixels without the bands stepping on each other. */
static uint64_t raster_triangle(const screen_vertex *v, ws_framebuffer *fb, float *depth,
                                const ws_scene *scene, const render_ctx *ctx, const mat_ctx *mat,
                                unsigned tile, int band_y0, int band_y1)
{
    int w = (int)fb->width;
    int h = (int)fb->height;

    double area = edge(v[0].x, v[0].y, v[1].x, v[1].y, v[2].x, v[2].y);
    if (area == 0.0) {
        return 0u;
    }

    double min_x = v[0].x, max_x = v[0].x, min_y = v[0].y, max_y = v[0].y;
    for (int i = 1; i < 3; ++i) {
        if (v[i].x < min_x) { min_x = v[i].x; }
        if (v[i].x > max_x) { max_x = v[i].x; }
        if (v[i].y < min_y) { min_y = v[i].y; }
        if (v[i].y > max_y) { max_y = v[i].y; }
    }

    int x0 = (int)min_x; if (x0 < 0) { x0 = 0; }
    int x1 = (int)max_x + 1; if (x1 > w - 1) { x1 = w - 1; }
    int y0 = (int)min_y; if (y0 < 0) { y0 = 0; }
    int y1 = (int)max_y + 1; if (y1 > h - 1) { y1 = h - 1; }
    /* Restrict to the caller's row band so parallel bands never overlap. */
    if (y0 < band_y0) { y0 = band_y0; }
    if (y1 > band_y1 - 1) { y1 = band_y1 - 1; }
    if (x0 > x1 || y0 > y1) {
        return 0u;
    }

    unsigned tile_size = tile == 0u ? (unsigned)((w > h ? w : h) + 1) : tile;
    double inv_area = 1.0 / area;
    int inside_pos = area > 0.0;
    uint64_t shaded = 0u;

    const ws_render_settings *settings = &scene->settings;
    int use_fog = ctx->use_fog;
    const double *fog_lin = ctx->fog_lin;
    double near_z = ctx->near_z;
    double far_z = ctx->far_z;

    for (int ty = y0; ty <= y1; ty += (int)tile_size) {
        int ty1 = ty + (int)tile_size - 1;
        if (ty1 > y1) { ty1 = y1; }
        for (int tx = x0; tx <= x1; tx += (int)tile_size) {
            int tx1 = tx + (int)tile_size - 1;
            if (tx1 > x1) { tx1 = x1; }
            for (int y = ty; y <= ty1; ++y) {
                double py = (double)y + 0.5;
                for (int x = tx; x <= tx1; ++x) {
                    double px = (double)x + 0.5;
                    double w0 = edge(v[1].x, v[1].y, v[2].x, v[2].y, px, py);
                    double w1 = edge(v[2].x, v[2].y, v[0].x, v[0].y, px, py);
                    double w2 = edge(v[0].x, v[0].y, v[1].x, v[1].y, px, py);
                    int inside = inside_pos ? (w0 >= 0.0 && w1 >= 0.0 && w2 >= 0.0)
                                            : (w0 <= 0.0 && w1 <= 0.0 && w2 <= 0.0);
                    if (!inside) {
                        continue;
                    }
                    double l0 = w0 * inv_area;
                    double l1 = w1 * inv_area;
                    double l2 = w2 * inv_area;

                    double iw = l0 * v[0].inv_w + l1 * v[1].inv_w + l2 * v[2].inv_w;
                    if (iw <= 0.0) {
                        continue;
                    }
                    double p0 = l0 * v[0].inv_w / iw;
                    double p1 = l1 * v[1].inv_w / iw;
                    double p2 = l2 * v[2].inv_w / iw;

                    double z = p0 * v[0].z + p1 * v[1].z + p2 * v[2].z;
                    size_t idx = (size_t)y * fb->width + (size_t)x;
                    if (depth != NULL) {
                        if (z >= (double)depth[idx]) {
                            continue;
                        }
                        depth[idx] = (float)z;
                    }

                    ws_vec3 world = ws_vec3_make(p0 * v[0].wx + p1 * v[1].wx + p2 * v[2].wx,
                                                 p0 * v[0].wy + p1 * v[1].wy + p2 * v[2].wy,
                                                 p0 * v[0].wz + p1 * v[1].wz + p2 * v[2].wz);
                    ws_vec3 normal = ws_vec3_make(p0 * v[0].nx + p1 * v[1].nx + p2 * v[2].nx,
                                                  p0 * v[0].ny + p1 * v[1].ny + p2 * v[2].ny,
                                                  p0 * v[0].nz + p1 * v[1].nz + p2 * v[2].nz);
                    double albedo[3];
                    albedo[0] = p0 * v[0].r + p1 * v[1].r + p2 * v[2].r;
                    albedo[1] = p0 * v[0].g + p1 * v[1].g + p2 * v[2].g;
                    albedo[2] = p0 * v[0].b + p1 * v[1].b + p2 * v[2].b;

                    double rgb[3];
                    shade_pixel(scene, ctx, mat, world, normal, albedo, rgb);

                    if (use_fog) {
                        double dist = ndc_to_linear_depth(z, near_z, far_z);
                        double f = 1.0 - exp(-ctx->fog_density * dist);
                        if (f < 0.0) { f = 0.0; }
                        if (f > 1.0) { f = 1.0; }
                        for (int c = 0; c < 3; ++c) {
                            rgb[c] = ws_lerp(rgb[c], fog_lin[c], f);
                        }
                    }

                    uint8_t *p = &fb->pixels[idx * 4u];
                    p[0] = encode_display_u8(rgb[0], settings);
                    p[1] = encode_display_u8(rgb[1], settings);
                    p[2] = encode_display_u8(rgb[2], settings);
                    p[3] = 255u;
                    shaded += 1u;
                }
            }
        }
    }
    return shaded;
}

/* Fill rows [y_begin, y_end) of the framebuffer with the vertical gradient sky.
 * Rows are independent, so the bands can draw their own sky slice in parallel;
 * together they cover the whole image exactly once. */
static void draw_sky_band(const ws_scene *scene, ws_framebuffer *fb,
                          uint32_t y_begin, uint32_t y_end)
{
    const ws_camera *cam = &scene->camera;
    const ws_render_settings *settings = &scene->settings;
    ws_vec3 forward = ws_vec3_normalize(ws_vec3_sub(cam->target, cam->eye));
    ws_vec3 right = ws_vec3_normalize(ws_vec3_cross(forward, cam->up));
    ws_vec3 up = ws_vec3_cross(right, forward);
    double tan_half = tan(cam->fov_y_radians * 0.5);
    double aspect = (double)fb->width / (double)fb->height;
    double top[3], horizon[3];
    linear3(scene->light.sky_top, top);
    linear3(scene->light.sky_horizon, horizon);

    if (y_end > fb->height) {
        y_end = fb->height;
    }
    for (uint32_t y = y_begin; y < y_end; ++y) {
        double ndc_y = 1.0 - 2.0 * ((double)y + 0.5) / (double)fb->height;
        for (uint32_t x = 0u; x < fb->width; ++x) {
            double ndc_x = 2.0 * ((double)x + 0.5) / (double)fb->width - 1.0;
            ws_vec3 dir = ws_vec3_make(forward.x + right.x * ndc_x * aspect * tan_half + up.x * ndc_y * tan_half,
                                       forward.y + right.y * ndc_x * aspect * tan_half + up.y * ndc_y * tan_half,
                                       forward.z + right.z * ndc_x * aspect * tan_half + up.z * ndc_y * tan_half);
            dir = ws_vec3_normalize(dir);
            double t = ws_clamp_double((dir.y + 0.15) / 0.9, 0.0, 1.0);
            uint8_t *p = &fb->pixels[((size_t)y * fb->width + x) * 4u];
            for (int c = 0; c < 3; ++c) {
                double lin = ws_lerp(horizon[c], top[c], t);
                p[c] = encode_display_u8(lin, settings);
            }
            p[3] = 255u;
        }
    }
}

/* Render the scene's geometry into one horizontal band of rows [band_y0,
 * band_y1). Because the bands partition the image and every pixel writes only
 * to its own depth/colour slot, running the bands on different threads is
 * bit-identical to a single-threaded pass; only wall-clock time changes. */
typedef struct band_job {
    const ws_scene *scene;
    ws_framebuffer *fb;
    float *depth;
    unsigned tile;
    ws_mat4 view_proj;
    const shadow_map *shadow;
    const clip_vertex *prep;    /* per-frame vertex prepass (shared, read-only) */
    const screen_vertex *prep_scr; /* prepass screen coords (viewport-mapped) */
    const uint32_t *obj_base;   /* prep[] offset of each object's first vertex */
    int band_y0;
    int band_y1;
    int count_triangles;  /* only band 0 counts triangle-level stats */
    uint64_t triangles_drawn;
    uint64_t pixels_shaded;
    uint64_t triangles_clipped;
    uint64_t triangles_submitted;
} band_job;

static void render_band(band_job *job)
{
    const ws_scene *scene = job->scene;
    ws_framebuffer *fb = job->fb;
    float *depth = job->depth;
    unsigned tile = job->tile;
    int band_y0 = job->band_y0;
    int band_y1 = job->band_y1;
    int w = (int)fb->width;
    int h = (int)fb->height;

    /* Background and depth for this band's rows: independent of other bands, so
     * it runs in parallel with them. */
    if (scene->settings.sky_enabled) {
        draw_sky_band(scene, fb, (uint32_t)band_y0, (uint32_t)band_y1);
    } else {
        for (int y = band_y0; y < band_y1; ++y) {
            uint8_t *row = &fb->pixels[(size_t)y * fb->width * 4u];
            for (int x = 0; x < w; ++x) {
                row[x * 4 + 0] = scene->background[0];
                row[x * 4 + 1] = scene->background[1];
                row[x * 4 + 2] = scene->background[2];
                row[x * 4 + 3] = 255u;
            }
        }
    }
    if (depth != NULL) {
        for (int y = band_y0; y < band_y1; ++y) {
            float *drow = &depth[(size_t)y * fb->width];
            for (int x = 0; x < w; ++x) {
                drow[x] = 1.0e30f;
            }
        }
    }

    render_ctx ctx;
    render_ctx_build(&ctx, scene, job->shadow);

    for (uint32_t oi = 0; oi < scene->object_count; ++oi) {
        const ws_scene_object *obj = &scene->objects[oi];
        if (obj->mesh == NULL || obj->mesh->vertices == NULL || obj->mesh->indices == NULL) {
            continue;
        }
        ws_mat4 mvp = ws_mat4_multiply(job->view_proj, obj->transform);
        ws_mat3 normal_mat = ws_mat4_normal_matrix(obj->transform);
        const ws_mesh *mesh = obj->mesh;
        mat_ctx mctx = mat_ctx_build(&obj->material);
        const clip_vertex *prepv =
            job->prep != NULL ? &job->prep[job->obj_base[oi]] : NULL;
        int band_may_reject = !job->count_triangles && prepv != NULL;

        for (uint32_t t = 0u; t + 3u <= mesh->index_count; t += 3u) {
            clip_vertex poly[3];
            int have_all = 1;
            for (int k = 0; k < 3; ++k) {
                uint32_t idx = mesh->indices[t + (uint32_t)k];
                if (idx >= mesh->vertex_count) {
                    have_all = 0;
                    continue;
                }
                poly[k] = prepv != NULL ? prepv[idx]
                                        : make_clip_vertex(obj, mvp, normal_mat, &mesh->vertices[idx]);
            }
            if (band_may_reject && have_all && tri_band_disjoint(poly, h, band_y0, band_y1)) {
                continue;
            }
            if (job->count_triangles) {
                job->triangles_submitted += 1u;
            }

            clip_vertex clipped[4];
            int cn;
            screen_vertex scr[4];
            if (prepv != NULL && have_all &&
                poly[0].w + poly[0].z >= 0.0 && poly[1].w + poly[1].z >= 0.0 &&
                poly[2].w + poly[2].z >= 0.0) {
                /* Fast path: no vertex is behind the near plane, so clipping is
                 * the identity and the screen coords come straight from the
                 * prepass (identical to_screen arithmetic). */
                cn = 3;
                scr[0] = job->prep_scr[job->obj_base[oi] + mesh->indices[t + 0u]];
                scr[1] = job->prep_scr[job->obj_base[oi] + mesh->indices[t + 1u]];
                scr[2] = job->prep_scr[job->obj_base[oi] + mesh->indices[t + 2u]];
            } else {
                cn = clip_near(poly, 3, clipped);
                if (cn < 3) {
                    if (job->count_triangles) {
                        job->triangles_clipped += 1u;
                    }
                    continue;
                }
                for (int k = 0; k < cn; ++k) {
                    to_screen(&clipped[k], w, h, &scr[k]);
                }
            }
            for (int k = 1; k + 1 < cn; ++k) {
                screen_vertex tri[3] = {scr[0], scr[k], scr[k + 1]};
                /* "Drawn" is the image-wide bounding-box test, so it is
                 * band-invariant and only band 0 counts it. */
                if (job->count_triangles) {
                    double mnx = tri[0].x, mxx = tri[0].x, mny = tri[0].y, mxy = tri[0].y;
                    for (int e = 1; e < 3; ++e) {
                        if (tri[e].x < mnx) { mnx = tri[e].x; }
                        if (tri[e].x > mxx) { mxx = tri[e].x; }
                        if (tri[e].y < mny) { mny = tri[e].y; }
                        if (tri[e].y > mxy) { mxy = tri[e].y; }
                    }
                    if ((int)mnx <= w - 1 && (int)mxx + 1 >= 0 &&
                        (int)mny <= h - 1 && (int)mxy + 1 >= 0) {
                        job->triangles_drawn += 1u;
                    }
                }
                job->pixels_shaded += raster_triangle(tri, fb, depth, scene, &ctx, &mctx,
                                                      tile, band_y0, band_y1);
            }
        }
    }
}

static void band_body(void *vctx, uint32_t index)
{
    band_job *jobs = (band_job *)vctx;
    render_band(&jobs[index]);
}

ws_result ws_render_scene(const ws_scene *scene, ws_framebuffer *fb,
                          float *depth, unsigned tile, ws_render_stats *stats)
{
    if (scene == NULL || fb == NULL || fb->pixels == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (scene->objects == NULL) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    if (stats != NULL) {
        memset(stats, 0, sizeof(*stats));
    }

    encode_lut_init();

    const ws_camera *cam = &scene->camera;
    ws_mat4 view = ws_mat4_look_at(cam->eye, cam->target, cam->up);
    double aspect = (double)fb->width / (double)fb->height;
    ws_mat4 proj = ws_mat4_perspective(cam->fov_y_radians, aspect, cam->near_z, cam->far_z);
    ws_mat4 view_proj = ws_mat4_multiply(proj, view);

    /* Optional directional shadow pass: build the light-space depth map once,
     * before the parallel image pass, so every band samples the same data. */
    shadow_map shadow;
    float *shadow_depth = NULL;
    shadow.depth = NULL;
    shadow.size = 0u;
    shadow.strength = 0.0;
    shadow.bias = 0.0;
    shadow.light_view_proj = ws_mat4_identity();
    shadow.ready = 0;
    const shadow_map *shadow_ptr = NULL;
    if (scene->settings.shadows_enabled) {
        ws_vec3 lo, hi;
        scene_world_bounds(scene, &lo, &hi);
        ws_mat4 light_vp = shadow_light_matrix(scene, lo, hi);
        unsigned ssize = scene->settings.shadow_map_size ? scene->settings.shadow_map_size : 1024u;
        size_t scount = (size_t)ssize * (size_t)ssize;
        shadow_depth = (float *)malloc(scount * sizeof(float));
        if (shadow_depth != NULL) {
            shadow_render_pass(scene, light_vp, shadow_depth, ssize);
            shadow.depth = shadow_depth;
            shadow.size = ssize;
            shadow.strength = ws_clamp_double(scene->settings.shadow_strength, 0.0, 1.0);
            shadow.bias = scene->settings.shadow_bias;
            shadow.light_view_proj = light_vp;
            shadow.ready = 1;
            shadow_ptr = &shadow;
        }
        /* Allocation failure falls back to unshadowed shading rather than
         * failing the frame. */
    }

    /* Per-frame vertex prepass: each vertex's clip transform, world position,
     * normal and albedo are computed once here and shared read-only by every
     * band. This is the same arithmetic make_clip_vertex would run inside each
     * band, so the shaded result is bit-identical; it removes the per-band
     * duplication that dominated setup for high-vertex-count meshes. */
    clip_vertex *prep = NULL;
    screen_vertex *prep_scr = NULL;
    uint32_t *obj_base = NULL;
    size_t prep_count = 0u;
    for (uint32_t oi = 0; oi < scene->object_count; ++oi) {
        const ws_scene_object *obj = &scene->objects[oi];
        if (obj->mesh != NULL && obj->mesh->vertices != NULL) {
            prep_count += obj->mesh->vertex_count;
        }
    }
    if (prep_count > 0u) {
        prep = (clip_vertex *)malloc(prep_count * sizeof(clip_vertex));
        prep_scr = (screen_vertex *)malloc(prep_count * sizeof(screen_vertex));
        obj_base = (uint32_t *)malloc((size_t)scene->object_count * sizeof(uint32_t));
    }
    if (prep != NULL && prep_scr != NULL && obj_base != NULL) {
        size_t cursor = 0u;
        for (uint32_t oi = 0; oi < scene->object_count; ++oi) {
            const ws_scene_object *obj = &scene->objects[oi];
            obj_base[oi] = (uint32_t)cursor;
            if (obj->mesh == NULL || obj->mesh->vertices == NULL) {
                continue;
            }
            ws_mat4 mvp = ws_mat4_multiply(view_proj, obj->transform);
            ws_mat3 normal_mat = ws_mat4_normal_matrix(obj->transform);
            const ws_mesh *mesh = obj->mesh;
            for (uint32_t vi = 0u; vi < mesh->vertex_count; ++vi) {
                clip_vertex c = make_clip_vertex(obj, mvp, normal_mat, &mesh->vertices[vi]);
                prep[cursor + vi] = c;
                /* Viewport map once, shared by every band. Equal to the per-band
                 * to_screen() arithmetic, so the image is unchanged. */
                to_screen(&c, (int)fb->width, (int)fb->height, &prep_scr[cursor + vi]);
            }
            cursor += mesh->vertex_count;
        }
    } else {
        free(prep);
        prep = NULL;
        free(prep_scr);
        prep_scr = NULL;
        free(obj_base);
        obj_base = NULL;
    }

    /* Split the image into row bands, one per worker. Vertex setup is shared
     * across bands (see the prepass above) and every band writes only its own
     * rows, so the split is the same for every frame and the result is
     * deterministic. */
    enum { MAX_BANDS = 64 };
    ws_sched *sched = NULL;
    (void)ws_sched_create_default(&sched);
    uint32_t band_count = 1u;
    if (sched != NULL) {
        /* Many narrow bands give the scheduler enough work items to balance
         * the uneven per-row cost (near rows shade far more pixels than far
         * rows). Vertex setup is shared via the prepass, so extra bands are
         * cheap. */
        band_count = (ws_sched_worker_count(sched) + 1u) * 8u;
    }
    if (band_count > MAX_BANDS) {
        band_count = MAX_BANDS;
    }
    if (band_count > fb->height) {
        band_count = fb->height > 0u ? fb->height : 1u;
    }

    band_job jobs[MAX_BANDS];
    uint32_t rows_per_band = (fb->height + band_count - 1u) / band_count;
    uint32_t active = 0u;
    for (uint32_t b = 0u; b < band_count; ++b) {
        uint32_t y0 = b * rows_per_band;
        uint32_t y1 = y0 + rows_per_band;
        if (y0 >= fb->height) {
            break;
        }
        if (y1 > fb->height) {
            y1 = fb->height;
        }
        band_job *j = &jobs[active];
        j->scene = scene;
        j->fb = fb;
        j->depth = depth;
        j->tile = tile;
        j->view_proj = view_proj;
        j->shadow = shadow_ptr;
        j->prep = prep;
        j->prep_scr = prep_scr;
        j->obj_base = obj_base;
        j->band_y0 = (int)y0;
        j->band_y1 = (int)y1;
        j->count_triangles = (active == 0u);
        j->triangles_drawn = 0u;
        j->pixels_shaded = 0u;
        j->triangles_clipped = 0u;
        j->triangles_submitted = 0u;
        active++;
    }

    if (sched != NULL && active > 1u) {
        ws_sched_parallel_for(sched, 0u, active, band_body, jobs);
    } else {
        for (uint32_t i = 0u; i < active; ++i) {
            render_band(&jobs[i]);
        }
    }
    ws_sched_destroy(sched);

    free(prep);
    free(prep_scr);
    free(obj_base);

    if (stats != NULL) {
        /* Triangle-level counters come from band 0 alone (they are image-wide);
         * shaded pixels are partitioned across the bands and simply summed. */
        stats->triangles_submitted = jobs[0].triangles_submitted;
        stats->triangles_clipped = jobs[0].triangles_clipped;
        stats->triangles_drawn = jobs[0].triangles_drawn;
        for (uint32_t i = 0u; i < active; ++i) {
            stats->pixels_shaded += jobs[i].pixels_shaded;
        }
    }
    free(shadow_depth);
    return WS_OK;
}
