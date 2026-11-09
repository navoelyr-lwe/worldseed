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
#include <string.h>

#include "../core/ws_math.h"

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

static double encode_display(double linear, const ws_render_settings *s)
{
    double v = linear * s->exposure;
    if (s->tonemap == 1) {
        v = v / (1.0 + v); /* Reinhard */
    } else {
        v = ws_clamp_double(v, 0.0, 1.0);
    }
    return linear_to_srgb(v) * 255.0;
}

static unsigned char to_u8(double v)
{
    if (v <= 0.0) {
        return 0u;
    }
    if (v >= 255.0) {
        return 255u;
    }
    return (unsigned char)(v + 0.5);
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

static double ndc_to_linear_depth(double ndc_z, double near_z, double far_z)
{
    return (2.0 * near_z * far_z) / (far_z + near_z - (ndc_z * 2.0 - 1.0) * (far_z - near_z));
}

/* Shade one pixel from interpolated surface attributes. Writes linear RGB. */
static void shade_pixel(const ws_scene *scene, const ws_material *mat,
                        ws_vec3 world_pos, ws_vec3 normal, const double albedo[3],
                        double out[3])
{
    const ws_light *light = &scene->light;
    ws_vec3 n = ws_vec3_normalize(normal);
    if (ws_vec3_length(n) == 0.0) {
        n = ws_vec3_make(0.0, 1.0, 0.0);
    }
    ws_vec3 view = ws_vec3_normalize(ws_vec3_sub(scene->camera.eye, world_pos));

    double lc[3];
    linear3(light->color, lc);
    double diffuse = -ws_vec3_dot(n, light->direction);
    if (diffuse < 0.0) {
        diffuse = 0.0;
    }
    diffuse *= light->intensity;

    double sky[3], ground[3];
    linear3(light->sky_color, sky);
    linear3(light->ground_color, ground);
    double up = 0.5 * (n.y + 1.0);
    double amb_scale = light->ambient_intensity;

    double mat_diffuse = 1.0 - ws_clamp_double(mat->metallic, 0.0, 1.0);

    double spec = 0.0;
    if (mat->specular > 0.0) {
        ws_vec3 h = ws_vec3_normalize(ws_vec3_sub(view, light->direction));
        double ndoth = ws_vec3_dot(n, h);
        if (ndoth > 0.0) {
            double rough = ws_clamp_double(mat->roughness, 0.02, 1.0);
            double shininess = 2.0 / (rough * rough) - 2.0;
            spec = pow(ndoth, shininess) * mat->specular * light->intensity;
        }
    }

    for (int c = 0; c < 3; ++c) {
        double amb = ws_lerp(ground[c], sky[c], up) * amb_scale;
        double base = albedo[c];
        double diff = base * mat_diffuse * (amb + diffuse * lc[c]);
        double spec_tint = ws_lerp(1.0, base, ws_clamp_double(mat->metallic, 0.0, 1.0));
        out[c] = diff + spec * spec_tint * lc[c];
    }
}

static void raster_triangle(const screen_vertex *v, ws_framebuffer *fb, float *depth,
                            const ws_scene *scene, const ws_material *mat,
                            unsigned tile, ws_render_stats *stats)
{
    int w = (int)fb->width;
    int h = (int)fb->height;

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

    unsigned tile_size = tile == 0u ? (unsigned)((w > h ? w : h) + 1) : tile;
    double inv_area = 1.0 / area;
    int inside_pos = area > 0.0;

    if (stats != NULL) {
        stats->triangles_drawn += 1u;
    }

    const ws_render_settings *settings = &scene->settings;
    int use_fog = settings->fog_density > 0.0;
    double fog_lin[3];
    if (use_fog) {
        if (settings->sky_enabled) {
            linear3(scene->light.sky_horizon, fog_lin);
        } else {
            fog_lin[0] = srgb_to_linear((double)scene->background[0]);
            fog_lin[1] = srgb_to_linear((double)scene->background[1]);
            fog_lin[2] = srgb_to_linear((double)scene->background[2]);
        }
    }
    double near_z = scene->camera.near_z;
    double far_z = scene->camera.far_z;

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
                    shade_pixel(scene, mat, world, normal, albedo, rgb);

                    if (use_fog) {
                        double dist = ndc_to_linear_depth(z, near_z, far_z);
                        double f = 1.0 - exp(-settings->fog_density * dist);
                        if (f < 0.0) { f = 0.0; }
                        if (f > 1.0) { f = 1.0; }
                        for (int c = 0; c < 3; ++c) {
                            rgb[c] = ws_lerp(rgb[c], fog_lin[c], f);
                        }
                    }

                    uint8_t *p = &fb->pixels[idx * 4u];
                    p[0] = to_u8(encode_display(rgb[0], settings));
                    p[1] = to_u8(encode_display(rgb[1], settings));
                    p[2] = to_u8(encode_display(rgb[2], settings));
                    p[3] = 255u;
                    if (stats != NULL) {
                        stats->pixels_shaded += 1u;
                    }
                }
            }
        }
    }
}

/* Fill the framebuffer with a vertical gradient sky based on the camera ray. */
static void draw_sky(const ws_scene *scene, ws_framebuffer *fb)
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

    for (uint32_t y = 0u; y < fb->height; ++y) {
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
                p[c] = to_u8(encode_display(lin, settings));
            }
            p[3] = 255u;
        }
    }
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

    if (scene->settings.sky_enabled) {
        draw_sky(scene, fb);
    } else {
        ws_framebuffer_clear(fb, scene->background[0], scene->background[1], scene->background[2], 255u);
    }
    if (depth != NULL) {
        size_t count = (size_t)fb->width * (size_t)fb->height;
        for (size_t i = 0; i < count; ++i) {
            depth[i] = 1.0e30f;
        }
    }

    const ws_camera *cam = &scene->camera;
    ws_mat4 view = ws_mat4_look_at(cam->eye, cam->target, cam->up);
    double aspect = (double)fb->width / (double)fb->height;
    ws_mat4 proj = ws_mat4_perspective(cam->fov_y_radians, aspect, cam->near_z, cam->far_z);
    ws_mat4 view_proj = ws_mat4_multiply(proj, view);

    for (uint32_t oi = 0; oi < scene->object_count; ++oi) {
        const ws_scene_object *obj = &scene->objects[oi];
        if (obj->mesh == NULL || obj->mesh->vertices == NULL || obj->mesh->indices == NULL) {
            continue;
        }
        ws_mat4 mvp = ws_mat4_multiply(view_proj, obj->transform);
        ws_mat3 normal_mat = ws_mat4_normal_matrix(obj->transform);
        const ws_mesh *mesh = obj->mesh;

        for (uint32_t t = 0u; t + 3u <= mesh->index_count; t += 3u) {
            clip_vertex poly[3];
            for (int k = 0; k < 3; ++k) {
                uint32_t idx = mesh->indices[t + (uint32_t)k];
                if (idx >= mesh->vertex_count) {
                    continue;
                }
                poly[k] = make_clip_vertex(obj, mvp, normal_mat, &mesh->vertices[idx]);
            }
            if (stats != NULL) {
                stats->triangles_submitted += 1u;
            }

            clip_vertex clipped[4];
            int cn = clip_near(poly, 3, clipped);
            if (cn < 3) {
                if (stats != NULL) {
                    stats->triangles_clipped += 1u;
                }
                continue;
            }

            screen_vertex scr[4];
            for (int k = 0; k < cn; ++k) {
                to_screen(&clipped[k], (int)fb->width, (int)fb->height, &scr[k]);
            }
            for (int k = 1; k + 1 < cn; ++k) {
                screen_vertex tri[3] = {scr[0], scr[k], scr[k + 1]};
                raster_triangle(tri, fb, depth, scene, &obj->material, tile, stats);
            }
        }
    }
    return WS_OK;
}
