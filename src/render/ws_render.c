/* Worldseed Engine — reference CPU scene renderer. */
#include "ws_render.h"

#include <stddef.h>
#include <string.h>

#include "../core/ws_math.h"

typedef struct clip_vertex {
    double x, y, z, w;   /* clip space */
    double r, g, b;      /* lit colour, 0..255 */
} clip_vertex;

typedef struct screen_vertex {
    double x, y;         /* pixels */
    double z;            /* NDC depth in [0, 1], smaller is nearer */
    double inv_w;        /* 1 / clip w */
    double r, g, b;      /* lit colour, 0..255 */
} screen_vertex;

static double shade_channel(double albedo, double ndotl, double ambient, double light_color)
{
    double lit = albedo * (ambient + (1.0 - ambient) * ndotl) * light_color;
    return lit * 255.0;
}

static clip_vertex make_clip_vertex(const ws_scene_object *obj, const ws_light *light,
                                    ws_mat4 mvp, const ws_vertex *v)
{
    clip_vertex out;
    ws_vec4 clip = ws_mat4_mul_vec4(mvp, ws_vec4_make(v->position.x, v->position.y, v->position.z, 1.0));
    out.x = clip.x;
    out.y = clip.y;
    out.z = clip.z;
    out.w = clip.w;

    double ndotl = -ws_vec3_dot(v->normal, light->direction);
    if (ndotl < 0.0) {
        ndotl = 0.0;
    }
    double amb = (double)light->ambient / 255.0;
    double lr = (double)light->color[0] / 255.0;
    double lg = (double)light->color[1] / 255.0;
    double lb = (double)light->color[2] / 255.0;
    out.r = shade_channel(obj->material.albedo[0], ndotl * light->intensity, amb, lr);
    out.g = shade_channel(obj->material.albedo[1], ndotl * light->intensity, amb, lg);
    out.b = shade_channel(obj->material.albedo[2], ndotl * light->intensity, amb, lb);
    return out;
}

static clip_vertex lerp_clip(clip_vertex a, clip_vertex b, double t)
{
    clip_vertex out;
    out.x = a.x + (b.x - a.x) * t;
    out.y = a.y + (b.y - a.y) * t;
    out.z = a.z + (b.z - a.z) * t;
    out.w = a.w + (b.w - a.w) * t;
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
    s->r = c->r;
    s->g = c->g;
    s->b = c->b;
}

static double edge(double ax, double ay, double bx, double by, double px, double py)
{
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

static void raster_triangle(const screen_vertex *v, ws_framebuffer *fb, float *depth,
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

                    /* Perspective-correct weights. */
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
                    double r = p0 * v[0].r + p1 * v[1].r + p2 * v[2].r;
                    double g = p0 * v[0].g + p1 * v[1].g + p2 * v[2].g;
                    double b = p0 * v[0].b + p1 * v[1].b + p2 * v[2].b;
                    uint8_t *p = &fb->pixels[idx * 4u];
                    p[0] = to_u8(r);
                    p[1] = to_u8(g);
                    p[2] = to_u8(b);
                    p[3] = 255u;
                    if (stats != NULL) {
                        stats->pixels_shaded += 1u;
                    }
                }
            }
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

    ws_framebuffer_clear(fb, scene->background[0], scene->background[1], scene->background[2], 255u);
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
        const ws_mesh *mesh = obj->mesh;

        for (uint32_t t = 0u; t + 3u <= mesh->index_count; t += 3u) {
            clip_vertex poly[3];
            for (int k = 0; k < 3; ++k) {
                uint32_t idx = mesh->indices[t + (uint32_t)k];
                if (idx >= mesh->vertex_count) {
                    continue;
                }
                poly[k] = make_clip_vertex(obj, &scene->light, mvp, &mesh->vertices[idx]);
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
                raster_triangle(tri, fb, depth, tile, stats);
            }
        }
    }
    return WS_OK;
}
