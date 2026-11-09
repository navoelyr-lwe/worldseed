/* Worldseed Engine — deterministic value noise and fractal sums.
 *
 * Original, self-contained C17 noise with no external dependency. The lattice
 * hash is integer-only and the interpolation uses doubles, so the same seed and
 * coordinate give the same value on every platform and build. This is the
 * shared source for terrain, water, vegetation and environment fields, which is
 * what lets adjacent world regions line up at their shared border.
 *
 * Noise is not a cryptographic primitive.
 */
#ifndef WS_CORE_NOISE_H
#define WS_CORE_NOISE_H

#include <stdint.h>

/* Hash an integer lattice point to a value in [0, 1). */
double ws_noise_hash2(uint64_t seed, int64_t x, int64_t y);

/* Smooth 2D value noise in [0, 1). */
double ws_noise_value2(uint64_t seed, double x, double y);

/* Fractal Brownian motion: `octaves` layers of value noise, each `lacunarity`
 * times finer and `gain` times weaker. Normalised to [0, 1). */
double ws_noise_fbm2(uint64_t seed, double x, double y, int octaves,
                     double lacunarity, double gain);

/* Ridged multifractal in [0, 1): sharp crests from 1 - |2*n - 1|. */
double ws_noise_ridged2(uint64_t seed, double x, double y, int octaves,
                        double lacunarity, double gain);

#endif /* WS_CORE_NOISE_H */
