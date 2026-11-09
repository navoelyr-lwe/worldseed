/* Worldseed Engine — deterministic value noise and fractal sums. */
#include "ws_noise.h"

#include <math.h>

double ws_noise_hash2(uint64_t seed, int64_t x, int64_t y)
{
    uint64_t h = seed;
    h ^= (uint64_t)x * 0x9E3779B97F4A7C15ULL;
    h ^= (uint64_t)y * 0xC2B2AE3D27D4EB4FULL;
    h ^= h >> 30;
    h *= 0xBF58476D1CE4E5B9ULL;
    h ^= h >> 27;
    h *= 0x94D049BB133111EBULL;
    h ^= h >> 31;
    return (double)(h >> 11) / (double)(1ULL << 53);
}

/* Quintic smoothstep: continuous first and second derivatives, so terrain
 * normals derived by finite differences stay smooth across cell borders. */
static double fade(double t)
{
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

double ws_noise_value2(uint64_t seed, double x, double y)
{
    double xf = floor(x);
    double yf = floor(y);
    int64_t xi = (int64_t)xf;
    int64_t yi = (int64_t)yf;
    double tx = x - xf;
    double ty = y - yf;
    double sx = fade(tx);
    double sy = fade(ty);
    double v00 = ws_noise_hash2(seed, xi, yi);
    double v10 = ws_noise_hash2(seed, xi + 1, yi);
    double v01 = ws_noise_hash2(seed, xi, yi + 1);
    double v11 = ws_noise_hash2(seed, xi + 1, yi + 1);
    double top = v00 + (v10 - v00) * sx;
    double bot = v01 + (v11 - v01) * sx;
    return top + (bot - top) * sy;
}

double ws_noise_fbm2(uint64_t seed, double x, double y, int octaves,
                     double lacunarity, double gain)
{
    if (octaves < 1) {
        return ws_noise_value2(seed, x, y);
    }
    double sum = 0.0;
    double amplitude = 1.0;
    double normaliser = 0.0;
    double frequency = 1.0;
    for (int i = 0; i < octaves; ++i) {
        sum += amplitude * ws_noise_value2(seed + (uint64_t)i * 0x9E3779B97F4A7C15ULL,
                                           x * frequency, y * frequency);
        normaliser += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }
    return normaliser > 0.0 ? sum / normaliser : 0.0;
}

double ws_noise_ridged2(uint64_t seed, double x, double y, int octaves,
                        double lacunarity, double gain)
{
    if (octaves < 1) {
        octaves = 1;
    }
    double sum = 0.0;
    double amplitude = 1.0;
    double normaliser = 0.0;
    double frequency = 1.0;
    for (int i = 0; i < octaves; ++i) {
        double n = ws_noise_value2(seed + (uint64_t)i * 0xD1B54A32D192ED03ULL,
                                   x * frequency, y * frequency);
        double ridge = 1.0 - fabs(2.0 * n - 1.0);
        ridge *= ridge;
        sum += amplitude * ridge;
        normaliser += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }
    return normaliser > 0.0 ? sum / normaliser : 0.0;
}
