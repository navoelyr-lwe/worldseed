/* Worldseed Engine — framebuffer and PPM output. */
#include "ws_framebuffer.h"

#include "../core/ws_math.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ws_result ws_framebuffer_create(ws_framebuffer *fb, uint32_t width, uint32_t height)
{
    if (fb == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    fb->width = 0U;
    fb->height = 0U;
    fb->pixels = NULL;
    if (width == 0U || height == 0U) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    size_t bytes = 0;
    if (!ws_mul_size_checked((size_t)width * 4u, (size_t)height, &bytes)) {
        return WS_ERR_OVERFLOW;
    }
    fb->pixels = (uint8_t *)calloc(bytes, 1u);
    if (fb->pixels == NULL) {
        return WS_ERR_OUT_OF_MEMORY;
    }
    fb->width = width;
    fb->height = height;
    return WS_OK;
}

void ws_framebuffer_free(ws_framebuffer *fb)
{
    if (fb == NULL) {
        return;
    }
    free(fb->pixels);
    fb->pixels = NULL;
    fb->width = 0U;
    fb->height = 0U;
}

void ws_framebuffer_clear(ws_framebuffer *fb, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (fb == NULL || fb->pixels == NULL) {
        return;
    }
    size_t count = (size_t)fb->width * (size_t)fb->height;
    for (size_t i = 0; i < count; ++i) {
        uint8_t *p = &fb->pixels[i * 4u];
        p[0] = r;
        p[1] = g;
        p[2] = b;
        p[3] = a;
    }
}

void ws_framebuffer_set(ws_framebuffer *fb, uint32_t x, uint32_t y,
                        uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (fb == NULL || fb->pixels == NULL) {
        return;
    }
    if (x >= fb->width || y >= fb->height) {
        return;
    }
    uint8_t *p = &fb->pixels[((size_t)y * fb->width + x) * 4u];
    p[0] = r;
    p[1] = g;
    p[2] = b;
    p[3] = a;
}

ws_result ws_framebuffer_write_ppm(const ws_framebuffer *fb, const char *path)
{
    if (fb == NULL || path == NULL || fb->pixels == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        return WS_ERR_IO;
    }
    int header = fprintf(f, "P6\n%u %u\n255\n", fb->width, fb->height);
    if (header < 0) {
        fclose(f);
        return WS_ERR_IO;
    }
    size_t count = (size_t)fb->width * (size_t)fb->height;
    for (size_t i = 0; i < count; ++i) {
        const uint8_t *p = &fb->pixels[i * 4u];
        if (fwrite(p, 1u, 3u, f) != 3u) {
            fclose(f);
            return WS_ERR_IO;
        }
    }
    if (fclose(f) != 0) {
        return WS_ERR_IO;
    }
    return WS_OK;
}
