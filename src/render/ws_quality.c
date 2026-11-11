/* Worldseed Engine — product visual-quality checklist. */
#include "ws_quality.h"

#include "ws_image.h"

#include <math.h>
#include <string.h>

/* Colour variety bucket: 4 bits per channel, so 4096 buckets. */
#define QUALITY_BUCKETS 4096u

/* Luminance below this counts as "deep shade"; above the blown threshold (all
 * channels) counts as over-exposed. */
#define QUALITY_DARK_LUMA 20.0
#define QUALITY_BLOWN_LEVEL 250u

static double pixel_luma(const uint8_t *p)
{
    return 0.2126 * (double)p[0] + 0.7152 * (double)p[1] + 0.0722 * (double)p[2];
}

/* HSV saturation from 8-bit RGB, 0..1. */
static double pixel_saturation(const uint8_t *p)
{
    uint8_t mx = p[0] > p[1] ? p[0] : p[1];
    if (p[2] > mx) { mx = p[2]; }
    uint8_t mn = p[0] < p[1] ? p[0] : p[1];
    if (p[2] < mn) { mn = p[2]; }
    if (mx == 0u) {
        return 0.0;
    }
    return (double)(mx - mn) / (double)mx;
}

ws_result ws_quality_measure(const ws_scene *scene, const ws_framebuffer *fb,
                             uint64_t triangles_submitted, uint64_t triangles_drawn,
                             uint64_t pixels_shaded, ws_quality_metrics *out)
{
    if (scene == NULL || fb == NULL || fb->pixels == NULL || out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    size_t count = (size_t)fb->width * (size_t)fb->height;
    if (count == 0u) {
        return WS_ERR_INVALID_ARGUMENT;
    }

    memset(out, 0, sizeof(*out));
    out->triangles_submitted = triangles_submitted;
    out->triangles_drawn = triangles_drawn;
    out->pixels_shaded = pixels_shaded;
    out->object_count = scene->object_count;
    double geom = (double)pixels_shaded / (double)count;
    out->geometry_coverage = geom > 1.0 ? 1.0 : geom;

    (void)ws_image_coverage(fb, scene->background, 8, &out->coverage);

    static uint8_t buckets[QUALITY_BUCKETS];
    memset(buckets, 0, sizeof(buckets));
    uint64_t distinct = 0u;

    double sum = 0.0;
    double sum_sq = 0.0;
    double sat_sum = 0.0;
    double peak = 0.0;
    uint64_t dark = 0u;
    uint64_t blown = 0u;

    uint32_t w = fb->width;
    uint32_t h = fb->height;
    /* Vertical sky-gradient probe: mean luminance of the top eighth vs the next
     * eighth. A gradient sky varies across these rows; a flat clear colour
     * does not. */
    uint32_t band = h / 8u;
    if (band == 0u) { band = 1u; }
    double top_sum = 0.0;
    uint64_t top_n = 0u;
    double next_sum = 0.0;
    uint64_t next_n = 0u;

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            const uint8_t *p = &fb->pixels[((size_t)y * w + x) * 4u];
            double lum = pixel_luma(p);
            sum += lum;
            sum_sq += lum * lum;
            sat_sum += pixel_saturation(p);
            if (lum > peak) { peak = lum; }
            if (lum < QUALITY_DARK_LUMA) { dark += 1u; }
            if (p[0] >= QUALITY_BLOWN_LEVEL && p[1] >= QUALITY_BLOWN_LEVEL &&
                p[2] >= QUALITY_BLOWN_LEVEL) {
                blown += 1u;
            }
            uint32_t key = (((uint32_t)p[0] >> 4) << 8) | (((uint32_t)p[1] >> 4) << 4) |
                           ((uint32_t)p[2] >> 4);
            if (!buckets[key]) {
                buckets[key] = 1u;
                distinct += 1u;
            }
            if (y < band) {
                top_sum += lum;
                top_n += 1u;
            } else if (y < 2u * band) {
                next_sum += lum;
                next_n += 1u;
            }
        }
    }

    double n = (double)count;
    double mean = sum / n;
    double var = sum_sq / n - mean * mean;
    out->mean_luma = mean;
    out->max_luma = peak;
    out->luma_stddev = var > 0.0 ? sqrt(var) : 0.0;
    out->dark_fraction = (double)dark / n;
    out->blown_fraction = (double)blown / n;
    out->mean_saturation = sat_sum / n;
    out->colour_variety = (double)distinct;

    double top_mean = top_n ? top_sum / (double)top_n : 0.0;
    double next_mean = next_n ? next_sum / (double)next_n : 0.0;
    out->sky_gradient = fabs(top_mean - next_mean);
    return WS_OK;
}

/* The six product reference scenes from docs/00 §5, mapped to catalog scene
 * ids. Thresholds are calibrated from the deterministic reference frames and
 * are deliberately conservative: they fail on a missing, unlit or washed-out
 * frame, not on a legitimate design change. */
/* fields: id, geom_coverage, coverage, stddev, variety, sat, sky, objects, blown, sky? */
static const ws_quality_spec g_specs[] = {
    /* 1. Gündüz geniş orman/vadi/nehir planı */
    {"terrain_day",     0.90, 0.55, 10.0, 20.0, 0.15, 0.50, 3u, 0.02, 1},
    /* 2. Çayırda canlıların doğal davranışı */
    {"ecosystem",       0.70, 0.50,  8.0, 20.0, 0.15, 4.00, 4u, 0.02, 1},
    /* 3. Su kaynağında birden fazla türün etkileşimi */
    {"organism_forage", 0.85, 0.50,  7.0, 20.0, 0.15, 8.00, 4u, 0.02, 1},
    /* 4. Yakın plan beden/baş/hareket ayrıntısı */
    {"creature_walk",   0.80, 0.45,  6.0, 20.0, 0.15, 8.00, 4u, 0.02, 1},
    /* 5. Yağmur/sis gibi atmosferik koşul */
    {"terrain_weather", 0.90, 0.40, 12.0, 20.0, 0.15, 1.00, 3u, 0.02, 1},
    /* 6. Gece veya düşük ışık sahnesi */
    {"terrain_dusk",    0.90, 0.40, 15.0, 20.0, 0.15, 1.50, 3u, 0.02, 1},
};

size_t ws_quality_spec_count(void)
{
    return sizeof(g_specs) / sizeof(g_specs[0]);
}

const ws_quality_spec *ws_quality_spec_at(size_t index)
{
    if (index >= ws_quality_spec_count()) {
        return NULL;
    }
    return &g_specs[index];
}

ws_result ws_quality_check(const ws_quality_spec *spec, const ws_quality_metrics *m,
                           uint32_t *out_failures)
{
    if (spec == NULL || m == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    uint32_t fail = 0u;
    if (m->geometry_coverage < spec->min_geometry_coverage) {
        fail |= WS_QUALITY_FAIL_GEOMETRY;
    }
    if (m->coverage < spec->min_coverage) {
        fail |= WS_QUALITY_FAIL_COVERAGE;
    }
    if (m->luma_stddev < spec->min_luma_stddev) {
        fail |= WS_QUALITY_FAIL_CONTRAST;
    }
    if (m->colour_variety < spec->min_colour_variety) {
        fail |= WS_QUALITY_FAIL_VARIETY;
    }
    if (m->mean_saturation < spec->min_saturation) {
        fail |= WS_QUALITY_FAIL_SATURATION;
    }
    if (spec->require_sky && m->sky_gradient < spec->min_sky_gradient) {
        fail |= WS_QUALITY_FAIL_SKY;
    }
    if (m->object_count < spec->min_objects) {
        fail |= WS_QUALITY_FAIL_OBJECTS;
    }
    if (m->blown_fraction > spec->max_blown) {
        fail |= WS_QUALITY_FAIL_BLOWN;
    }
    if (out_failures != NULL) {
        *out_failures = fail;
    }
    return fail == 0u ? WS_OK : WS_ERR_INVALID_ARGUMENT;
}
