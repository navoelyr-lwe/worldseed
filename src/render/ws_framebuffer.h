/* Worldseed Engine — framebuffer and PPM output.
 *
 * A linear 8-bit RGBA colour buffer. This is the CPU reference output used for
 * headless golden-scene capture. The buffer owns its pixels; callers must call
 * ws_framebuffer_free.
 */
#ifndef WS_RENDER_FRAMEBUFFER_H
#define WS_RENDER_FRAMEBUFFER_H

#include <stdint.h>

#include "../core/ws_result.h"

typedef struct ws_framebuffer {
    uint32_t width;
    uint32_t height;
    uint8_t *pixels; /* width * height * 4 bytes, RGBA8 */
} ws_framebuffer;

ws_result ws_framebuffer_create(ws_framebuffer *fb, uint32_t width, uint32_t height);
void ws_framebuffer_free(ws_framebuffer *fb);

/* Fill the whole buffer with an RGBA colour. */
void ws_framebuffer_clear(ws_framebuffer *fb, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

/* Write a pixel if in bounds; out-of-range coordinates are ignored. */
void ws_framebuffer_set(ws_framebuffer *fb, uint32_t x, uint32_t y,
                        uint8_t r, uint8_t g, uint8_t b, uint8_t a);

/* Write the buffer as a binary PPM (P6) file. Alpha is dropped.
 * PPM keeps the prototype dependency-free and human-inspectable. */
ws_result ws_framebuffer_write_ppm(const ws_framebuffer *fb, const char *path);

#endif /* WS_RENDER_FRAMEBUFFER_H */
