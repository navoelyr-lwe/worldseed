/* Worldseed Engine — full-scene tiled raster + shading kernel (OpenCL C 1.2).
 *
 * This is the ONLY permitted non-ISO-C source in the product (ADR-001): OpenCL C
 * is a C99-based dialect and the single GPU kernel language exception. It uses
 * only the OpenCL C 1.2 core feature set: no OpenCL 2.x/3.x builtins, no
 * sub-groups, no images/samplers, no atomics.
 *
 * Scope: this kernel accelerates the *whole* product scene pipeline that the CPU
 * reference renderer (src/render/ws_render.c) implements — perspective-correct
 * interpolation of world position, world normal and albedo, per-pixel Lambert +
 * hemisphere ambient + Blinn-Phong specular shading, optional directional
 * shadow-map sampling (3x3 or disc PCF), exponential distance fog, deferred
 * gradient sky and the exposure/tonemap/sRGB encode. It runs one work-item per
 * pixel over a host-built screen-space tile bin (src/gpu/ws_tilebin.c), so it
 * scales to a full scene.
 *
 * Determinism / correctness contract (ADR-001): the CPU renderer stays the
 * bit-identical golden reference. This kernel runs the same arithmetic in 32-bit
 * float, so its output matches the CPU frame within a documented per-channel
 * tolerance (see tools/ws_gpu_check.c and docs/P10). One work-item per pixel
 * means every pixel is written by exactly one item, so there are no atomics and
 * no cross-item ordering: the result is deterministic for a given batch.
 *
 * The engine never makes this kernel authoritative for simulation state; it only
 * accelerates rasterisation of the already-decided frame (ADR-001).
 *
 * Keep this file free of double quotes and backslashes: it is embedded verbatim
 * into a C string literal at configure time (see src/CMakeLists.txt).
 */

/* Per-vertex stride in floats and per-triangle stride (three vertices). */
#define WS_VS 13
#define WS_TS 39

/* Scene-constant buffer indices (see ws_gpu_scene.c). */
#define C_LIGHT_R 0
#define C_LIGHT_G 1
#define C_LIGHT_B 2
#define C_SKY_R 3
#define C_SKY_G 4
#define C_SKY_B 5
#define C_GROUND_R 6
#define C_GROUND_G 7
#define C_GROUND_B 8
#define C_AMB_SCALE 9
#define C_LIGHT_INT 10
#define C_USE_FOG 11
#define C_FOG_R 12
#define C_FOG_G 13
#define C_FOG_B 14
#define C_NEAR 15
#define C_FAR 16
#define C_FOG_DENSITY 17
#define C_EYE_X 18
#define C_EYE_Y 19
#define C_EYE_Z 20
#define C_LIGHT_DIR_X 21
#define C_LIGHT_DIR_Y 22
#define C_LIGHT_DIR_Z 23
#define C_EXPOSURE 24
#define C_TONEMAP 25
#define C_SHADOW_ACTIVE 26
#define C_SHADOW_STRENGTH 27
#define C_SHADOW_BIAS 28
#define C_SHADOW_SOFTNESS 29
#define C_SHADOW_SIZE 30
#define C_SKY_ENABLED 31
#define C_BG_R 32
#define C_BG_G 33
#define C_BG_B 34
#define C_SKYTOP_R 35
#define C_SKYTOP_G 36
#define C_SKYTOP_B 37
#define C_SKYHOR_R 38
#define C_SKYHOR_G 39
#define C_SKYHOR_B 40
#define C_FWD_X 41
#define C_FWD_Y 42
#define C_FWD_Z 43
#define C_RIGHT_X 44
#define C_RIGHT_Y 45
#define C_RIGHT_Z 46
#define C_UP_X 47
#define C_UP_Y 48
#define C_UP_Z 49
#define C_TAN_HALF 50
#define C_ASPECT 51
#define C_COUNT 52

static float ws_clamp01(float v)
{
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

/* linear -> sRGB-encoded 0..255 (the CPU path uses a 4096-entry LUT with linear
 * interpolation; the float pow here agrees within a fraction of one 8-bit step,
 * which is inside the documented per-channel tolerance). */
static float ws_srgb255(float lin)
{
    if (lin <= 0.0f) {
        return 0.0f;
    }
    float s;
    if (lin <= 0.0031308f) {
        s = lin * 12.92f;
    } else {
        s = 1.055f * pow(lin, 1.0f / 2.4f) - 0.055f;
    }
    if (s < 0.0f) {
        s = 0.0f;
    }
    return s * 255.0f;
}

static uchar ws_encode(float linear, float exposure, int tonemap)
{
    float v = linear * exposure;
    if (tonemap == 1) {
        v = v / (1.0f + v);
    } else {
        v = ws_clamp01(v);
    }
    if (v <= 0.0f) {
        return (uchar)0;
    }
    float e = ws_srgb255(v);
    if (e <= 0.0f) {
        return (uchar)0;
    }
    if (e >= 255.0f) {
        return (uchar)255;
    }
    return (uchar)(e + 0.5f);
}

/* Fraction of the direct light reaching the world point: 1 in light, lower in
 * shadow. Mirrors shadow_factor() in ws_render.c. */
static float ws_shadow_factor(__global const float *sd,
                              __global const float *lvp,
                              float size, float strength, float bias, float softness,
                              float wx, float wy, float wz, int active)
{
    if (!active) {
        return 1.0f;
    }
    float cx = lvp[0] * wx + lvp[4] * wy + lvp[8] * wz + lvp[12];
    float cy = lvp[1] * wx + lvp[5] * wy + lvp[9] * wz + lvp[13];
    float cz = lvp[2] * wx + lvp[6] * wy + lvp[10] * wz + lvp[14];
    float cw = lvp[3] * wx + lvp[7] * wy + lvp[11] * wz + lvp[15];
    if (cw == 0.0f) {
        return 1.0f;
    }
    float ndcx = cx / cw;
    float ndcy = cy / cw;
    float ndcz = cz / cw;
    if (ndcx < -1.0f || ndcx > 1.0f || ndcy < -1.0f || ndcy > 1.0f ||
        ndcz < -1.0f || ndcz > 1.0f) {
        return 1.0f;
    }
    float u = ndcx * 0.5f + 0.5f;
    float v = 1.0f - (ndcy * 0.5f + 0.5f);
    float ref = ndcz * 0.5f + 0.5f;
    float texel = 1.0f / size;
    int rad = softness > 0.0f ? (int)(softness + 0.5f) : 1;
    if (rad < 1) {
        rad = 1;
    }
    if (rad > 8) {
        rad = 8;
    }
    float lit = 0.0f;
    int samples = 0;
    int isize = (int)size;
    for (int dy = -rad; dy <= rad; ++dy) {
        for (int dx = -rad; dx <= rad; ++dx) {
            if (dx * dx + dy * dy > rad * rad + rad) {
                continue;
            }
            int sx = (int)((u + (float)dx * texel) * size);
            int sy = (int)((v + (float)dy * texel) * size);
            if (sx < 0 || sy < 0 || sx >= isize || sy >= isize) {
                continue;
            }
            float stored = sd[sy * isize + sx];
            lit += (ref - bias > stored) ? 0.0f : 1.0f;
            samples += 1;
        }
    }
    if (samples == 0) {
        return 1.0f;
    }
    lit /= (float)samples;
    return 1.0f - strength * (1.0f - lit);
}

/* Shade one pixel from interpolated surface attributes; writes linear RGB. */
static void ws_shade_pixel(__global const float *c,
                           __global const float *mat, /* 4 floats for this object */
                           float wxp, float wyp, float wzp,
                           float nx, float ny, float nz,
                           float ar, float ag, float ab,
                           __global const float *sd, __global const float *lvp,
                           float *out_r, float *out_g, float *out_b)
{
    float nlen2 = nx * nx + ny * ny + nz * nz;
    float n0, n1, n2;
    if (nlen2 > 0.0f) {
        float inv = 1.0f / sqrt(nlen2);
        n0 = nx * inv; n1 = ny * inv; n2 = nz * inv;
    } else {
        n0 = 0.0f; n1 = 1.0f; n2 = 0.0f;
    }
    float tex = c[C_EYE_X] - wxp;
    float tey = c[C_EYE_Y] - wyp;
    float tez = c[C_EYE_Z] - wzp;
    float vlen2 = tex * tex + tey * tey + tez * tez;
    float v0, v1, v2;
    if (vlen2 > 0.0f) {
        float inv = 1.0f / sqrt(vlen2);
        v0 = tex * inv; v1 = tey * inv; v2 = tez * inv;
    } else {
        v0 = 0.0f; v1 = 0.0f; v2 = 0.0f;
    }

    float diffuse = -(n0 * c[C_LIGHT_DIR_X] + n1 * c[C_LIGHT_DIR_Y] + n2 * c[C_LIGHT_DIR_Z]);
    if (diffuse < 0.0f) {
        diffuse = 0.0f;
    }
    diffuse *= c[C_LIGHT_INT];

    float up = 0.5f * (n1 + 1.0f);

    float shininess = mat[0];
    float mat_diffuse = mat[1];
    float metallic = mat[2];
    float specular = mat[3];

    float spec = 0.0f;
    if (specular > 0.0f) {
        float hx = v0 - c[C_LIGHT_DIR_X];
        float hy = v1 - c[C_LIGHT_DIR_Y];
        float hz = v2 - c[C_LIGHT_DIR_Z];
        float hlen2 = hx * hx + hy * hy + hz * hz;
        if (hlen2 > 0.0f) {
            float inv = 1.0f / sqrt(hlen2);
            hx *= inv; hy *= inv; hz *= inv;
        }
        float ndoth = n0 * hx + n1 * hy + n2 * hz;
        if (ndoth > 0.0f) {
            spec = pow(ndoth, shininess) * specular * c[C_LIGHT_INT];
        }
    }

    if (c[C_SHADOW_ACTIVE] != 0.0f) {
        float s = ws_shadow_factor(sd, lvp, c[C_SHADOW_SIZE], c[C_SHADOW_STRENGTH],
                                   c[C_SHADOW_BIAS], c[C_SHADOW_SOFTNESS],
                                   wxp, wyp, wzp, 1);
        diffuse *= s;
        spec *= s;
    }

    float base[3];
    base[0] = ar; base[1] = ag; base[2] = ab;
    float lc[3];
    lc[0] = c[C_LIGHT_R]; lc[1] = c[C_LIGHT_G]; lc[2] = c[C_LIGHT_B];
    float sk[3];
    sk[0] = c[C_SKY_R]; sk[1] = c[C_SKY_G]; sk[2] = c[C_SKY_B];
    float gr[3];
    gr[0] = c[C_GROUND_R]; gr[1] = c[C_GROUND_G]; gr[2] = c[C_GROUND_B];
    float outv[3];
    for (int i = 0; i < 3; ++i) {
        float amb = (gr[i] + (sk[i] - gr[i]) * up) * c[C_AMB_SCALE];
        float diff = base[i] * mat_diffuse * (amb + diffuse * lc[i]);
        float spec_tint = 1.0f + (base[i] - 1.0f) * metallic;
        outv[i] = diff + spec * spec_tint * lc[i];
    }
    *out_r = outv[0];
    *out_g = outv[1];
    *out_b = outv[2];
}

__kernel void ws_scene(
    __global const float *verts,        /* WS_TS floats per triangle */
    __global const uint *tri_obj,       /* 1 per triangle: object index */
    __global const float *mat_data,     /* 4 floats per object */
    __global const uint *tri_index,     /* tile bin: triangle indices */
    __global const uint *tile_offset,   /* tile_count + 1 offsets */
    const uint tiles_x,
    const uint tile_size,
    const uint width,
    const uint height,
    __global const float *consts,       /* C_COUNT scene constants */
    __global const float *light_vp,     /* 16 floats */
    __global const float *shadow_depth, /* shadow_size * shadow_size */
    __global uchar *pixels,
    __global float *depth)
{
    const uint gid = get_global_id(0);
    const uint count = width * height;
    if (gid >= count) {
        return;
    }

    const uint x = gid % width;
    const uint y = gid / width;
    const uint tx = x / tile_size;
    const uint ty = y / tile_size;
    const uint tile_id = ty * tiles_x + tx;

    const float px = (float)x + 0.5f;
    const float py = (float)y + 0.5f;

    float best_z = 1.0e30f;
    float best_wx = 0.0f, best_wy = 0.0f, best_wz = 0.0f;
    float best_nx = 0.0f, best_ny = 1.0f, best_nz = 0.0f;
    float best_r = 0.0f, best_g = 0.0f, best_b = 0.0f;
    int best_obj = 0;
    int hit = 0;

    const uint begin = tile_offset[tile_id];
    const uint end = tile_offset[tile_id + 1u];
    for (uint li = begin; li < end; ++li) {
        const uint t = tri_index[li];
        const uint b = t * WS_TS;
        const float ax = verts[b + 0u];
        const float ay = verts[b + 1u];
        const float bx = verts[b + WS_VS + 0u];
        const float by = verts[b + WS_VS + 1u];
        const float cx = verts[b + 2u * WS_VS + 0u];
        const float cy = verts[b + 2u * WS_VS + 1u];

        const float area = (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
        if (area == 0.0f) {
            continue;
        }

        const float w0 = (px - bx) * (cy - by) - (py - by) * (cx - bx);
        const float w1 = (px - cx) * (ay - cy) - (py - cy) * (ax - cx);
        const float w2 = (px - ax) * (by - ay) - (py - ay) * (bx - ax);

        int inside;
        if (area > 0.0f) {
            inside = (w0 >= 0.0f) && (w1 >= 0.0f) && (w2 >= 0.0f);
        } else {
            inside = (w0 <= 0.0f) && (w1 <= 0.0f) && (w2 <= 0.0f);
        }
        if (!inside) {
            continue;
        }

        const float inv_area = 1.0f / area;
        const float l0 = w0 * inv_area;
        const float l1 = w1 * inv_area;
        const float l2 = w2 * inv_area;

        const float iw = l0 * verts[b + 3u] + l1 * verts[b + WS_VS + 3u] +
                         l2 * verts[b + 2u * WS_VS + 3u];
        if (iw <= 0.0f) {
            continue;
        }
        const float p0 = l0 * verts[b + 3u] / iw;
        const float p1 = l1 * verts[b + WS_VS + 3u] / iw;
        const float p2 = l2 * verts[b + 2u * WS_VS + 3u] / iw;

        const float z = p0 * verts[b + 2u] + p1 * verts[b + WS_VS + 2u] +
                        p2 * verts[b + 2u * WS_VS + 2u];
        if (z >= best_z) {
            continue;
        }
        best_z = z;
        hit = 1;

        best_wx = p0 * verts[b + 4u] + p1 * verts[b + WS_VS + 4u] + p2 * verts[b + 2u * WS_VS + 4u];
        best_wy = p0 * verts[b + 5u] + p1 * verts[b + WS_VS + 5u] + p2 * verts[b + 2u * WS_VS + 5u];
        best_wz = p0 * verts[b + 6u] + p1 * verts[b + WS_VS + 6u] + p2 * verts[b + 2u * WS_VS + 6u];
        best_nx = p0 * verts[b + 7u] + p1 * verts[b + WS_VS + 7u] + p2 * verts[b + 2u * WS_VS + 7u];
        best_ny = p0 * verts[b + 8u] + p1 * verts[b + WS_VS + 8u] + p2 * verts[b + 2u * WS_VS + 8u];
        best_nz = p0 * verts[b + 9u] + p1 * verts[b + WS_VS + 9u] + p2 * verts[b + 2u * WS_VS + 9u];
        best_r = p0 * verts[b + 10u] + p1 * verts[b + WS_VS + 10u] + p2 * verts[b + 2u * WS_VS + 10u];
        best_g = p0 * verts[b + 11u] + p1 * verts[b + WS_VS + 11u] + p2 * verts[b + 2u * WS_VS + 11u];
        /* Vertex 2's blue channel is the last float of the triangle; the host
         * pads the vertex array with one trailing float so this read is in
         * bounds for the final triangle. */
        best_b = p0 * verts[b + 12u] + p1 * verts[b + WS_VS + 12u] + p2 * verts[b + 2u * WS_VS + 12u];
        best_obj = (int)tri_obj[t];
    }

    uchar out_r;
    uchar out_g;
    uchar out_b;

    if (hit) {
        __global const float *mat = mat_data + (uint)best_obj * 4u;
        float rr, gg, bb;
        ws_shade_pixel(consts, mat, best_wx, best_wy, best_wz,
                       best_nx, best_ny, best_nz, best_r, best_g, best_b,
                       shadow_depth, light_vp, &rr, &gg, &bb);
        if (consts[C_USE_FOG] != 0.0f) {
            float ndc = best_z * 2.0f - 1.0f;
            float dist = (2.0f * consts[C_NEAR] * consts[C_FAR]) /
                         (consts[C_FAR] + consts[C_NEAR] -
                          ndc * (consts[C_FAR] - consts[C_NEAR]));
            float f = 1.0f - exp(-consts[C_FOG_DENSITY] * dist);
            if (f < 0.0f) { f = 0.0f; }
            if (f > 1.0f) { f = 1.0f; }
            rr = rr + (consts[C_FOG_R] - rr) * f;
            gg = gg + (consts[C_FOG_G] - gg) * f;
            bb = bb + (consts[C_FOG_B] - bb) * f;
        }
        out_r = ws_encode(rr, consts[C_EXPOSURE], (int)consts[C_TONEMAP]);
        out_g = ws_encode(gg, consts[C_EXPOSURE], (int)consts[C_TONEMAP]);
        out_b = ws_encode(bb, consts[C_EXPOSURE], (int)consts[C_TONEMAP]);
    } else if (consts[C_SKY_ENABLED] != 0.0f) {
        float ndc_y = 1.0f - 2.0f * ((float)y + 0.5f) / (float)height;
        float ndc_x = 2.0f * ((float)x + 0.5f) / (float)width - 1.0f;
        float dirx = consts[C_FWD_X] + consts[C_RIGHT_X] * ndc_x * consts[C_ASPECT] * consts[C_TAN_HALF] + consts[C_UP_X] * ndc_y * consts[C_TAN_HALF];
        float diry = consts[C_FWD_Y] + consts[C_RIGHT_Y] * ndc_x * consts[C_ASPECT] * consts[C_TAN_HALF] + consts[C_UP_Y] * ndc_y * consts[C_TAN_HALF];
        float dirz = consts[C_FWD_Z] + consts[C_RIGHT_Z] * ndc_x * consts[C_ASPECT] * consts[C_TAN_HALF] + consts[C_UP_Z] * ndc_y * consts[C_TAN_HALF];
        float dlen2 = dirx * dirx + diry * diry + dirz * dirz;
        if (dlen2 > 0.0f) {
            float inv = 1.0f / sqrt(dlen2);
            dirx *= inv; diry *= inv; dirz *= inv;
        }
        float tt = ws_clamp01((diry + 0.15f) / 0.9f);
        float sk[3];
        sk[0] = consts[C_SKYHOR_R]; sk[1] = consts[C_SKYHOR_G]; sk[2] = consts[C_SKYHOR_B];
        float tp[3];
        tp[0] = consts[C_SKYTOP_R]; tp[1] = consts[C_SKYTOP_G]; tp[2] = consts[C_SKYTOP_B];
        out_r = ws_encode(sk[0] + (tp[0] - sk[0]) * tt, consts[C_EXPOSURE], (int)consts[C_TONEMAP]);
        out_g = ws_encode(sk[1] + (tp[1] - sk[1]) * tt, consts[C_EXPOSURE], (int)consts[C_TONEMAP]);
        out_b = ws_encode(sk[2] + (tp[2] - sk[2]) * tt, consts[C_EXPOSURE], (int)consts[C_TONEMAP]);
    } else {
        out_r = (uchar)consts[C_BG_R];
        out_g = (uchar)consts[C_BG_G];
        out_b = (uchar)consts[C_BG_B];
    }

    const uint p = gid * 4u;
    pixels[p + 0u] = out_r;
    pixels[p + 1u] = out_g;
    pixels[p + 2u] = out_b;
    pixels[p + 3u] = 255u;
    depth[gid] = hit ? best_z : 1.0e30f;
}
