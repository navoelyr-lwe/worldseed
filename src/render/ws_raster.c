/* Worldseed Engine — minimal tiled CPU rasterizer (P2 feasibility prototype). */
#include "ws_raster.h"

#include <stddef.h>

static float ws_edge(float ax, float ay, float bx, float by, float px, float py)
{
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

static int ws_in_range(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static unsigned char ws_to_u8(float v)
{
    if (v <= 0.0f) {
        return 0u;
    }
    if (v >= 255.0f) {
        return 255u;
    }
    return (unsigned char)v;
}

void ws_raster_triangle(ws_framebuffer *fb, float *depth,
                        const float vx[3], const float vy[3], const float vz[3],
                        const unsigned char *color,
                        unsigned tile, ws_raster_tile_stats *stats)
{
    if (fb == NULL || fb->pixels == NULL) {
        return;
    }
    int w = (int)fb->width;
    int h = (int)fb->height;

    float area = ws_edge(vx[0], vy[0], vx[1], vy[1], vx[2], vy[2]);
    if (area == 0.0f) {
        return;
    }

    float min_xf = vx[0];
    float max_xf = vx[0];
    float min_yf = vy[0];
    float max_yf = vy[0];
    for (int i = 1; i < 3; ++i) {
        if (vx[i] < min_xf) { min_xf = vx[i]; }
        if (vx[i] > max_xf) { max_xf = vx[i]; }
        if (vy[i] < min_yf) { min_yf = vy[i]; }
        if (vy[i] > max_yf) { max_yf = vy[i]; }
    }

    int x0 = ws_in_range((int)min_xf, 0, w - 1);
    int x1 = ws_in_range((int)max_xf + 1, 0, w - 1);
    int y0 = ws_in_range((int)min_yf, 0, h - 1);
    int y1 = ws_in_range((int)max_yf + 1, 0, h - 1);

    unsigned tile_size = tile == 0u ? (unsigned)((w > h ? w : h) + 1) : tile;
    float inv_area = 1.0f / area;

    if (stats != NULL) {
        stats->triangles_drawn += 1u;
    }

    for (int ty = y0; ty <= y1; ty += (int)tile_size) {
        int ty1 = ty + (int)tile_size - 1;
        if (ty1 > y1) { ty1 = y1; }
        for (int tx = x0; tx <= x1; tx += (int)tile_size) {
            int tx1 = tx + (int)tile_size - 1;
            if (tx1 > x1) { tx1 = x1; }
            if (stats != NULL) {
                stats->tiles_visited += 1u;
            }
            for (int y = ty; y <= ty1; ++y) {
                for (int x = tx; x <= tx1; ++x) {
                    float px = (float)x + 0.5f;
                    float py = (float)y + 0.5f;
                    float w0 = ws_edge(vx[1], vy[1], vx[2], vy[2], px, py);
                    float w1 = ws_edge(vx[2], vy[2], vx[0], vy[0], px, py);
                    float w2 = ws_edge(vx[0], vy[0], vx[1], vy[1], px, py);
                    int inside = area > 0.0f ? (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f)
                                             : (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f);
                    if (!inside) {
                        continue;
                    }
                    float b0 = w0 * inv_area;
                    float b1 = w1 * inv_area;
                    float b2 = w2 * inv_area;
                    float z = b0 * vz[0] + b1 * vz[1] + b2 * vz[2];
                    size_t idx = (size_t)y * fb->width + (size_t)x;
                    if (depth != NULL) {
                        if (z >= depth[idx]) {
                            continue;
                        }
                        depth[idx] = z;
                    }
                    float r = b0 * (float)color[0] + b1 * (float)color[3] + b2 * (float)color[6];
                    float g = b0 * (float)color[1] + b1 * (float)color[4] + b2 * (float)color[7];
                    float b = b0 * (float)color[2] + b1 * (float)color[5] + b2 * (float)color[8];
                    uint8_t *p = &fb->pixels[idx * 4u];
                    p[0] = ws_to_u8(r);
                    p[1] = ws_to_u8(g);
                    p[2] = ws_to_u8(b);
                    p[3] = 255u;
                    if (stats != NULL) {
                        stats->pixels_shaded += 1u;
                    }
                }
            }
        }
    }
}
