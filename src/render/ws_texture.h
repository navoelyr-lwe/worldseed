/* Worldseed Engine — procedural textures and sampling.
 *
 * Textures are RGBA8 images produced in-project by our own generators (ADR-008):
 * no external image or texture pack is read. Storage is caller-owned so the
 * runtime has no hidden allocation contract. Sampling is bilinear with repeat
 * wrapping; sRGB inputs are decoded to linear before shading.
 */
#ifndef WS_RENDER_TEXTURE_H
#define WS_RENDER_TEXTURE_H

#include <stddef.h>
#include <stdint.h>

#include "../core/ws_result.h"

typedef struct ws_texture {
    uint32_t width;
    uint32_t height;
    uint8_t *rgba; /* width * height * 4 bytes, RGBA8, sRGB-encoded */
} ws_texture;

/* Allocate `width` x `height` RGBA8 storage and fill it with a flat colour.
 * The caller owns the storage and must call ws_texture_free. */
ws_result ws_texture_create(ws_texture *tex, uint32_t width, uint32_t height,
                            uint8_t r, uint8_t g, uint8_t b, uint8_t a);

/* Release storage owned by ws_texture_create. Safe on a zeroed texture. */
void ws_texture_free(ws_texture *tex);

/* Write a pixel (bounds-checked; out-of-range is ignored). */
void ws_texture_set(ws_texture *tex, uint32_t x, uint32_t y,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a);

/* Bilinear sample with repeat wrapping. `u`/`v` are in texture units (0..1 is
 * one tile); the result is linear RGB in 0..1 (sRGB decoded). Safe when `tex`
 * is empty, in which case `out` is left white. */
void ws_texture_sample(const ws_texture *tex, double u, double v, double out[3]);

/* Deterministic generators. All are original procedural content. */

/* Checkerboard of `squares` x `squares` alternating between two colours. */
ws_result ws_texture_make_checker(ws_texture *tex, uint32_t size, uint32_t squares,
                                  const uint8_t c0[3], const uint8_t c1[3]);

/* Value-noise grain over a base colour; `seed` makes it reproducible. */
ws_result ws_texture_make_noise(ws_texture *tex, uint32_t size, uint64_t seed,
                                const uint8_t base[3], double amplitude);

#endif /* WS_RENDER_TEXTURE_H */
