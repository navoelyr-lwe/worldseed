/* Worldseed Engine — procedural textures and sampling. */
#include "ws_texture.h"

#include <math.h>
#include <stdlib.h>

#include "../core/ws_rng.h"

static uint8_t to_u8(double v)
{
    if (v <= 0.0) {
        return 0u;
    }
    if (v >= 255.0) {
        return 255u;
    }
    return (uint8_t)(v + 0.5);
}

ws_result ws_texture_create(ws_texture *tex, uint32_t width, uint32_t height,
                            uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (tex == NULL || width == 0u || height == 0u) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    size_t count = (size_t)width * (size_t)height;
    uint8_t *pixels = (uint8_t *)malloc(count * 4u);
    if (pixels == NULL) {
        return WS_ERR_OUT_OF_MEMORY;
    }
    for (size_t i = 0; i < count; ++i) {
        pixels[i * 4u + 0u] = r;
        pixels[i * 4u + 1u] = g;
        pixels[i * 4u + 2u] = b;
        pixels[i * 4u + 3u] = a;
    }
    tex->width = width;
    tex->height = height;
    tex->rgba = pixels;
    return WS_OK;
}

void ws_texture_free(ws_texture *tex)
{
    if (tex == NULL) {
        return;
    }
    free(tex->rgba);
    tex->rgba = NULL;
    tex->width = 0u;
    tex->height = 0u;
}

void ws_texture_set(ws_texture *tex, uint32_t x, uint32_t y,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (tex == NULL || tex->rgba == NULL || x >= tex->width || y >= tex->height) {
        return;
    }
    uint8_t *p = &tex->rgba[((size_t)y * tex->width + x) * 4u];
    p[0] = r;
    p[1] = g;
    p[2] = b;
    p[3] = a;
}

/* sRGB transfer function (IEC 61966-2-1) on a single 0..255 component. */
static double srgb_to_linear(double c8)
{
    double c = c8 / 255.0;
    if (c <= 0.04045) {
        return c / 12.92;
    }
    return pow((c + 0.055) / 1.055, 2.4);
}

/* Positive remainder so lattice coordinates wrap correctly for negative input. */
static uint32_t wrap_index(long long value, uint32_t size)
{
    long long m = value % (long long)size;
    if (m < 0) {
        m += (long long)size;
    }
    return (uint32_t)m;
}

void ws_texture_sample(const ws_texture *tex, double u, double v, double out[3])
{
    if (out == NULL) {
        return;
    }
    if (tex == NULL || tex->rgba == NULL || tex->width == 0u || tex->height == 0u) {
        out[0] = out[1] = out[2] = 1.0;
        return;
    }

    double fx = u * (double)tex->width - 0.5;
    double fy = v * (double)tex->height - 0.5;
    long long x0d = (long long)floor(fx);
    long long y0d = (long long)floor(fy);
    double tx = fx - (double)x0d;
    double ty = fy - (double)y0d;

    uint32_t w = tex->width;
    uint32_t h = tex->height;
    uint32_t x0 = wrap_index(x0d, w);
    uint32_t y0 = wrap_index(y0d, h);
    uint32_t x1 = (x0 + 1u) % w;
    uint32_t y1 = (y0 + 1u) % h;

    const uint8_t *p00 = &tex->rgba[((size_t)y0 * w + x0) * 4u];
    const uint8_t *p10 = &tex->rgba[((size_t)y0 * w + x1) * 4u];
    const uint8_t *p01 = &tex->rgba[((size_t)y1 * w + x0) * 4u];
    const uint8_t *p11 = &tex->rgba[((size_t)y1 * w + x1) * 4u];

    for (int c = 0; c < 3; ++c) {
        double c00 = srgb_to_linear((double)p00[c]);
        double c10 = srgb_to_linear((double)p10[c]);
        double c01 = srgb_to_linear((double)p01[c]);
        double c11 = srgb_to_linear((double)p11[c]);
        double top = c00 + (c10 - c00) * tx;
        double bot = c01 + (c11 - c01) * tx;
        out[c] = top + (bot - top) * ty;
    }
}

ws_result ws_texture_make_checker(ws_texture *tex, uint32_t size, uint32_t squares,
                                  const uint8_t c0[3], const uint8_t c1[3])
{
    if (squares == 0u) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    ws_result res = ws_texture_create(tex, size, size, 0u, 0u, 0u, 255u);
    if (res != WS_OK) {
        return res;
    }
    uint32_t cell = size / squares;
    if (cell == 0u) {
        cell = 1u;
    }
    for (uint32_t y = 0u; y < size; ++y) {
        for (uint32_t x = 0u; x < size; ++x) {
            uint32_t cx = x / cell;
            uint32_t cy = y / cell;
            const uint8_t *c = ((cx + cy) & 1u) ? c1 : c0;
            ws_texture_set(tex, x, y, c[0], c[1], c[2], 255u);
        }
    }
    return WS_OK;
}

/* Value noise: hash lattice points, bilinear-interpolate. Reproducible. */
static double lattice_value(uint64_t seed, int x, int y)
{
    uint64_t h = seed;
    h ^= (uint64_t)(uint32_t)x * 0x9E3779B97F4A7C15ULL;
    h ^= (uint64_t)(uint32_t)y * 0xC2B2AE3D27D4EB4FULL;
    h ^= h >> 30;
    h *= 0xBF58476D1CE4E5B9ULL;
    h ^= h >> 27;
    h *= 0x94D049BB133111EBULL;
    h ^= h >> 31;
    return (double)(h >> 11) / (double)(1ULL << 53);
}

static double value_noise(uint64_t seed, double x, double y)
{
    int xi = (int)floor(x);
    int yi = (int)floor(y);
    double tx = x - (double)xi;
    double ty = y - (double)yi;
    double sx = tx * tx * (3.0 - 2.0 * tx);
    double sy = ty * ty * (3.0 - 2.0 * ty);
    double v00 = lattice_value(seed, xi, yi);
    double v10 = lattice_value(seed, xi + 1, yi);
    double v01 = lattice_value(seed, xi, yi + 1);
    double v11 = lattice_value(seed, xi + 1, yi + 1);
    double top = v00 + (v10 - v00) * sx;
    double bot = v01 + (v11 - v01) * sx;
    return top + (bot - top) * sy;
}

ws_result ws_texture_make_noise(ws_texture *tex, uint32_t size, uint64_t seed,
                                const uint8_t base[3], double amplitude)
{
    ws_result res = ws_texture_create(tex, size, size, 0u, 0u, 0u, 255u);
    if (res != WS_OK) {
        return res;
    }
    double scale = 8.0 / (double)size;
    for (uint32_t y = 0u; y < size; ++y) {
        for (uint32_t x = 0u; x < size; ++x) {
            double n = value_noise(seed, (double)x * scale, (double)y * scale);
            double d = (n - 0.5) * 2.0 * amplitude;
            uint8_t px[3];
            for (int c = 0; c < 3; ++c) {
                px[c] = to_u8((double)base[c] * (1.0 + d));
            }
            ws_texture_set(tex, x, y, px[0], px[1], px[2], 255u);
        }
    }
    return WS_OK;
}
