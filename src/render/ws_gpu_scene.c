/* Worldseed Engine — full-scene OpenCL render host (P12).
 *
 * Marshals a ws_scene into the flat arrays the ws_scene kernel consumes and
 * runs it on the OpenCL device, producing an RGBA8 frame that matches the CPU
 * reference renderer (src/render/ws_render.c) within a documented per-channel
 * tolerance. The CPU renderer stays the bit-identical golden reference; this
 * path is the GPU accelerator (ADR-001) and is used only when a device is
 * present and the frame succeeds, otherwise the caller falls back to CPU.
 *
 * The transform/clip/screen arithmetic here mirrors ws_render.c exactly (same
 * matrices, same near-plane clip, same viewport map) so the two paths agree to
 * within float rounding. Texture albedo is sampled on the host with the same
 * ws_texture_sample the CPU prepass uses, so texture bilinear rounding is shared
 * and not a source of divergence.
 */
#include "ws_render.h"
#include "ws_render_internal.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../core/ws_math.h"
#include "../gpu/ws_opencl.h"

/* Scene-constant layout must match kernels/ws_scene.cl. */
enum {
    C_LIGHT_R = 0, C_LIGHT_G, C_LIGHT_B,
    C_SKY_R, C_SKY_G, C_SKY_B,
    C_GROUND_R, C_GROUND_G, C_GROUND_B,
    C_AMB_SCALE, C_LIGHT_INT, C_USE_FOG,
    C_FOG_R, C_FOG_G, C_FOG_B,
    C_NEAR, C_FAR, C_FOG_DENSITY,
    C_EYE_X, C_EYE_Y, C_EYE_Z,
    C_LIGHT_DIR_X, C_LIGHT_DIR_Y, C_LIGHT_DIR_Z,
    C_EXPOSURE, C_TONEMAP, C_SHADOW_ACTIVE,
    C_SHADOW_STRENGTH, C_SHADOW_BIAS, C_SHADOW_SOFTNESS, C_SHADOW_SIZE,
    C_SKY_ENABLED, C_BG_R, C_BG_G, C_BG_B,
    C_SKYTOP_R, C_SKYTOP_G, C_SKYTOP_B,
    C_SKYHOR_R, C_SKYHOR_G, C_SKYHOR_B,
    C_FWD_X, C_FWD_Y, C_FWD_Z,
    C_RIGHT_X, C_RIGHT_Y, C_RIGHT_Z,
    C_UP_X, C_UP_Y, C_UP_Z,
    C_TAN_HALF, C_ASPECT, C_COUNT
};

#define GC_STRIDE 13 /* floats per vertex in the kernel vertex array */
#define GC_TRI (3u * GC_STRIDE)

typedef struct gc_clip {
    double x, y, z, w;
    double wx, wy, wz;
    double nx, ny, nz;
    double r, g, b;
} gc_clip;

static double gc_srgb_to_linear(double c8)
{
    double c = c8 / 255.0;
    if (c <= 0.04045) {
        return c / 12.92;
    }
    return pow((c + 0.055) / 1.055, 2.4);
}

static void gc_linear3(const uint8_t srgb[3], double out[3])
{
    out[0] = gc_srgb_to_linear((double)srgb[0]);
    out[1] = gc_srgb_to_linear((double)srgb[1]);
    out[2] = gc_srgb_to_linear((double)srgb[2]);
}

static gc_clip gc_make_clip(const ws_scene_object *obj, ws_mat4 mvp, ws_mat3 nmat,
                            const ws_vertex *v)
{
    gc_clip out;
    ws_vec4 clip = ws_mat4_mul_vec4(mvp, ws_vec4_make(v->position.x, v->position.y,
                                                       v->position.z, 1.0));
    out.x = clip.x; out.y = clip.y; out.z = clip.z; out.w = clip.w;
    ws_vec4 world = ws_mat4_mul_vec4(obj->transform, ws_vec4_make(v->position.x,
                                                                  v->position.y,
                                                                  v->position.z, 1.0));
    out.wx = world.x; out.wy = world.y; out.wz = world.z;
    ws_vec3 n = ws_vec3_normalize(ws_mat3_mul_vec3(nmat, v->normal));
    out.nx = n.x; out.ny = n.y; out.nz = n.z;
    if (obj->material.albedo_map != NULL) {
        double tex[3];
        ws_texture_sample(obj->material.albedo_map, v->u, v->v, tex);
        out.r = tex[0]; out.g = tex[1]; out.b = tex[2];
    } else {
        out.r = obj->material.albedo[0];
        out.g = obj->material.albedo[1];
        out.b = obj->material.albedo[2];
    }
    return out;
}

static gc_clip gc_lerp(gc_clip a, gc_clip b, double t)
{
    gc_clip o;
    o.x = a.x + (b.x - a.x) * t;
    o.y = a.y + (b.y - a.y) * t;
    o.z = a.z + (b.z - a.z) * t;
    o.w = a.w + (b.w - a.w) * t;
    o.wx = a.wx + (b.wx - a.wx) * t;
    o.wy = a.wy + (b.wy - a.wy) * t;
    o.wz = a.wz + (b.wz - a.wz) * t;
    o.nx = a.nx + (b.nx - a.nx) * t;
    o.ny = a.ny + (b.ny - a.ny) * t;
    o.nz = a.nz + (b.nz - a.nz) * t;
    o.r = a.r + (b.r - a.r) * t;
    o.g = a.g + (b.g - a.g) * t;
    o.b = a.b + (b.b - a.b) * t;
    return o;
}

static int gc_clip_near(const gc_clip *in, int n, gc_clip *out)
{
    int m = 0;
    for (int i = 0; i < n; ++i) {
        const gc_clip *cur = &in[i];
        const gc_clip *nxt = &in[(i + 1) % n];
        double dc = cur->w + cur->z;
        double dn = nxt->w + nxt->z;
        int cur_in = dc >= 0.0;
        int nxt_in = dn >= 0.0;
        if (cur_in) {
            out[m++] = *cur;
        }
        if (cur_in != nxt_in) {
            double t = dc / (dc - dn);
            out[m++] = gc_lerp(*cur, *nxt, t);
        }
    }
    return m;
}

static void gc_to_screen(const gc_clip *c, int w, int h, float *v)
{
    double inv_w = 1.0 / c->w;
    double ndc_x = c->x * inv_w;
    double ndc_y = c->y * inv_w;
    double ndc_z = c->z * inv_w;
    v[0] = (float)((ndc_x * 0.5 + 0.5) * (double)w);
    v[1] = (float)((1.0 - (ndc_y * 0.5 + 0.5)) * (double)h);
    v[2] = (float)(ndc_z * 0.5 + 0.5);
    v[3] = (float)inv_w;
    v[4] = (float)c->wx; v[5] = (float)c->wy; v[6] = (float)c->wz;
    v[7] = (float)c->nx; v[8] = (float)c->ny; v[9] = (float)c->nz;
    v[10] = (float)c->r; v[11] = (float)c->g; v[12] = (float)c->b;
}

static void gc_build_consts(const ws_scene *scene, const shadow_map_view *shadow,
                            int w, int h, float *c)
{
    const ws_light *light = &scene->light;
    const ws_render_settings *s = &scene->settings;
    double lc[3], sky[3], gr[3];
    gc_linear3(light->color, lc);
    gc_linear3(light->sky_color, sky);
    gc_linear3(light->ground_color, gr);
    c[C_LIGHT_R] = (float)lc[0]; c[C_LIGHT_G] = (float)lc[1]; c[C_LIGHT_B] = (float)lc[2];
    c[C_SKY_R] = (float)sky[0]; c[C_SKY_G] = (float)sky[1]; c[C_SKY_B] = (float)sky[2];
    c[C_GROUND_R] = (float)gr[0]; c[C_GROUND_G] = (float)gr[1]; c[C_GROUND_B] = (float)gr[2];
    c[C_AMB_SCALE] = (float)light->ambient_intensity;
    c[C_LIGHT_INT] = (float)light->intensity;
    int use_fog = s->fog_density > 0.0;
    c[C_USE_FOG] = (float)use_fog;
    if (use_fog) {
        if (s->sky_enabled) {
            double f[3];
            gc_linear3(light->sky_horizon, f);
            c[C_FOG_R] = (float)f[0]; c[C_FOG_G] = (float)f[1]; c[C_FOG_B] = (float)f[2];
        } else {
            c[C_FOG_R] = (float)gc_srgb_to_linear((double)scene->background[0]);
            c[C_FOG_G] = (float)gc_srgb_to_linear((double)scene->background[1]);
            c[C_FOG_B] = (float)gc_srgb_to_linear((double)scene->background[2]);
        }
    } else {
        c[C_FOG_R] = c[C_FOG_G] = c[C_FOG_B] = 0.0f;
    }
    c[C_NEAR] = (float)scene->camera.near_z;
    c[C_FAR] = (float)scene->camera.far_z;
    c[C_FOG_DENSITY] = (float)s->fog_density;
    c[C_EYE_X] = (float)scene->camera.eye.x;
    c[C_EYE_Y] = (float)scene->camera.eye.y;
    c[C_EYE_Z] = (float)scene->camera.eye.z;
    ws_vec3 dir = ws_vec3_normalize(light->direction);
    c[C_LIGHT_DIR_X] = (float)dir.x;
    c[C_LIGHT_DIR_Y] = (float)dir.y;
    c[C_LIGHT_DIR_Z] = (float)dir.z;
    c[C_EXPOSURE] = (float)s->exposure;
    c[C_TONEMAP] = (float)s->tonemap;
    c[C_SHADOW_ACTIVE] = (float)(shadow != NULL && shadow->ready ? 1 : 0);
    c[C_SHADOW_STRENGTH] = (float)(shadow != NULL ? shadow->strength : 0.0);
    c[C_SHADOW_BIAS] = (float)(shadow != NULL ? shadow->bias : 0.0);
    c[C_SHADOW_SOFTNESS] = (float)(shadow != NULL ? shadow->softness : 0.0);
    c[C_SHADOW_SIZE] = (float)(shadow != NULL ? (double)shadow->size : 1.0);
    c[C_SKY_ENABLED] = (float)(s->sky_enabled ? 1 : 0);
    c[C_BG_R] = (float)scene->background[0];
    c[C_BG_G] = (float)scene->background[1];
    c[C_BG_B] = (float)scene->background[2];
    double top[3], hor[3];
    gc_linear3(light->sky_top, top);
    gc_linear3(light->sky_horizon, hor);
    c[C_SKYTOP_R] = (float)top[0]; c[C_SKYTOP_G] = (float)top[1]; c[C_SKYTOP_B] = (float)top[2];
    c[C_SKYHOR_R] = (float)hor[0]; c[C_SKYHOR_G] = (float)hor[1]; c[C_SKYHOR_B] = (float)hor[2];

    ws_vec3 forward = ws_vec3_normalize(ws_vec3_sub(scene->camera.target, scene->camera.eye));
    ws_vec3 right = ws_vec3_normalize(ws_vec3_cross(forward, scene->camera.up));
    ws_vec3 up = ws_vec3_cross(right, forward);
    c[C_FWD_X] = (float)forward.x; c[C_FWD_Y] = (float)forward.y; c[C_FWD_Z] = (float)forward.z;
    c[C_RIGHT_X] = (float)right.x; c[C_RIGHT_Y] = (float)right.y; c[C_RIGHT_Z] = (float)right.z;
    c[C_UP_X] = (float)up.x; c[C_UP_Y] = (float)up.y; c[C_UP_Z] = (float)up.z;
    c[C_TAN_HALF] = (float)tan(scene->camera.fov_y_radians * 0.5);
    c[C_ASPECT] = (float)((double)w / (double)h);
}

static float gc_shininess(const ws_material *m)
{
    if (m->specular > 0.0) {
        double rough = ws_clamp_double(m->roughness, 0.02, 1.0);
        return (float)(2.0 / (rough * rough) - 2.0);
    }
    return 0.0f;
}

/* Render `scene` on the device. Returns WS_OK on success. On any error the
 * caller falls back to the CPU renderer. */
static ws_result gc_render(ws_opencl *cl, const ws_scene *scene, ws_framebuffer *fb,
                           float *depth, unsigned tile, ws_render_stats *stats)
{
    const int w = (int)fb->width;
    const int h = (int)fb->height;

    /* --- Optional shadow map (built with the CPU reference for exactness). */
    float *shadow_depth = NULL;
    unsigned ssize = 0u;
    ws_mat4 light_vp = ws_mat4_identity();
    float light_vp_f[16];
    shadow_map_view shadow;
    shadow.ready = 0;
    const shadow_map_view *shadow_ptr = NULL;
    if (scene->settings.shadows_enabled) {
        ssize = scene->settings.shadow_map_size ? scene->settings.shadow_map_size : 1024u;
        shadow_depth = (float *)malloc((size_t)ssize * (size_t)ssize * sizeof(float));
        if (shadow_depth != NULL) {
            if (ws_render_shadow_map(scene, shadow_depth, ssize, 1u, &light_vp) == WS_OK) {
                for (int i = 0; i < 16; ++i) {
                    light_vp_f[i] = (float)light_vp.m[i];
                }
                shadow.depth = shadow_depth;
                shadow.size = ssize;
                shadow.strength = ws_clamp_double(scene->settings.shadow_strength, 0.0, 1.0);
                shadow.bias = scene->settings.shadow_bias;
                shadow.softness = scene->settings.shadow_softness > 0.0
                                      ? scene->settings.shadow_softness : 0.0;
                shadow.ready = 1;
                shadow_ptr = &shadow;
            }
        }
    }

    /* --- Count triangles and objects so the arrays can be sized once. */
    uint32_t tri_cap = 0u;
    for (uint32_t oi = 0u; oi < scene->object_count; ++oi) {
        const ws_scene_object *obj = &scene->objects[oi];
        if (obj->mesh == NULL || obj->mesh->vertices == NULL || obj->mesh->indices == NULL) {
            continue;
        }
        tri_cap += obj->mesh->index_count / 3u;
    }

    ws_result result = WS_ERR_OUT_OF_MEMORY;
    float *verts = NULL;
    float *obj_mat = NULL;
    uint32_t *tri_obj = NULL;
    float *consts = NULL;
    /* Near-plane clipping can turn one input triangle into a fan of up to two
     * output triangles (a triangle clipped against one plane yields a quad), so
     * size the output arrays for the worst case. */
    uint32_t out_cap = tri_cap * 2u;
    if (out_cap > 0u) {
        /* One extra trailing float so the last vertex's blue channel read stays
         * in bounds (see kernels/ws_scene.cl). */
        verts = (float *)malloc(((size_t)out_cap * GC_TRI + 1u) * sizeof(float));
        tri_obj = (uint32_t *)malloc((size_t)out_cap * sizeof(uint32_t));
    }
    if (scene->object_count > 0u) {
        obj_mat = (float *)malloc((size_t)scene->object_count * 4u * sizeof(float));
    }
    consts = (float *)malloc((size_t)C_COUNT * sizeof(float));
    if ((tri_cap > 0u && (verts == NULL || tri_obj == NULL)) ||
        (scene->object_count > 0u && obj_mat == NULL) || consts == NULL) {
        goto done;
    }

    for (uint32_t oi = 0u; oi < scene->object_count; ++oi) {
        const ws_material *m = &scene->objects[oi].material;
        obj_mat[oi * 4u + 0u] = gc_shininess(m);
        obj_mat[oi * 4u + 1u] = (float)(1.0 - ws_clamp_double(m->metallic, 0.0, 1.0));
        obj_mat[oi * 4u + 2u] = (float)ws_clamp_double(m->metallic, 0.0, 1.0);
        obj_mat[oi * 4u + 3u] = (float)m->specular;
    }

    ws_mat4 view = ws_mat4_look_at(scene->camera.eye, scene->camera.target, scene->camera.up);
    double aspect = (double)fb->width / (double)fb->height;
    ws_mat4 proj = ws_mat4_perspective(scene->camera.fov_y_radians, aspect,
                                       scene->camera.near_z, scene->camera.far_z);
    ws_mat4 view_proj = ws_mat4_multiply(proj, view);

    uint32_t nt = 0u;
    for (uint32_t oi = 0u; oi < scene->object_count; ++oi) {
        const ws_scene_object *obj = &scene->objects[oi];
        const ws_mesh *mesh = obj->mesh;
        if (mesh == NULL || mesh->vertices == NULL || mesh->indices == NULL) {
            continue;
        }
        ws_mat4 mvp = ws_mat4_multiply(view_proj, obj->transform);
        ws_mat3 nmat = ws_mat4_normal_matrix(obj->transform);
        for (uint32_t t = 0u; t + 3u <= mesh->index_count; t += 3u) {
            gc_clip poly[3];
            int ok = 1;
            for (int k = 0; k < 3; ++k) {
                uint32_t idx = mesh->indices[t + (uint32_t)k];
                if (idx >= mesh->vertex_count) {
                    ok = 0;
                    break;
                }
                poly[k] = gc_make_clip(obj, mvp, nmat, &mesh->vertices[idx]);
            }
            if (!ok) {
                continue;
            }
            gc_clip clipped[4];
            int cn = gc_clip_near(poly, 3, clipped);
            if (cn < 3) {
                continue;
            }
            if ((size_t)nt + (size_t)(cn - 2) > (size_t)out_cap) {
                break; /* defensive: out_cap already covers the 2-triangle worst case */
            }
            float scratch[4][GC_STRIDE];
            for (int k = 0; k < cn; ++k) {
                gc_to_screen(&clipped[k], w, h, scratch[k]);
            }
            for (int k = 1; k + 1 < cn; ++k) {
                float *slot = verts + (size_t)nt * GC_TRI;
                memcpy(slot, scratch[0], GC_STRIDE * sizeof(float));
                memcpy(slot + GC_STRIDE, scratch[k], GC_STRIDE * sizeof(float));
                memcpy(slot + 2u * GC_STRIDE, scratch[k + 1], GC_STRIDE * sizeof(float));
                tri_obj[nt] = oi;
                nt += 1u;
            }
        }
    }
    /* Sentinel so the kernel's last-vertex blue read is in bounds. */
    if (verts != NULL) {
        verts[(size_t)nt * GC_TRI] = 0.0f;
    }

    if (nt == 0u) {
        /* No visible geometry: let the CPU reference draw the sky/background so
         * the degenerate case stays bit-identical (rare; not worth a GPU pass). */
        result = WS_ERR_UNSUPPORTED;
        goto done;
    }

    gc_build_consts(scene, shadow_ptr, w, h, consts);

    ws_gpu_scene_input in;
    memset(&in, 0, sizeof(in));
    in.verts = verts;
    in.tri_obj = tri_obj;
    in.obj_mat = obj_mat;
    in.triangle_count = nt;
    in.object_count = scene->object_count;
    in.consts = consts;
    in.const_count = C_COUNT;
    in.light_vp = shadow_ptr != NULL ? light_vp_f : NULL;
    in.shadow_depth = shadow_ptr != NULL ? shadow_depth : NULL;
    in.shadow_size = ssize;
    in.tile_size = tile;

    result = ws_opencl_render_scene(cl, &in, fb->width, fb->height, fb->pixels, depth);
    if (result == WS_OK && stats != NULL) {
        stats->triangles_submitted = nt;
        stats->triangles_drawn = nt;
    }

done:
    free(verts);
    free(tri_obj);
    free(obj_mat);
    free(consts);
    free(shadow_depth);
    return result;
}

static ws_opencl *g_cl = NULL;
static int g_cl_tried = 0;

static ws_opencl *gc_backend(void)
{
    if (!g_cl_tried) {
        g_cl_tried = 1;
        if (ws_opencl_create(&g_cl) != WS_OK) {
            g_cl = NULL;
        }
    }
    if (g_cl != NULL && !ws_opencl_available(g_cl)) {
        ws_opencl_destroy(g_cl);
        g_cl = NULL;
    }
    return g_cl;
}

ws_result ws_render_scene_gpu(const ws_scene *scene, ws_framebuffer *fb,
                              float *depth, unsigned tile, ws_render_stats *stats)
{
    if (scene == NULL || fb == NULL || fb->pixels == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    ws_opencl *cl = gc_backend();
    if (cl != NULL) {
        if (stats != NULL) {
            memset(stats, 0, sizeof(*stats));
        }
        ws_result r = gc_render(cl, scene, fb, depth, tile, stats);
        if (r == WS_OK) {
            if (stats != NULL) {
                stats->path = WS_RENDER_PATH_GPU;
            }
            return WS_OK;
        }
        /* Device failure: fall through to the CPU reference path. */
    }
    ws_result r = ws_render_scene(scene, fb, depth, tile, stats);
    if (r == WS_OK && stats != NULL) {
        stats->path = WS_RENDER_PATH_CPU;
    }
    return r;
}
