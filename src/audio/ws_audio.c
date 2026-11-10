/* Worldseed Engine — procedural soundscape (P9). */
#include "ws_audio.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../core/ws_math.h"
#include "../life/ws_life_event.h"

/* Fixed per-kind voice: a distinct pitch, peak level and decay so each real
 * event has its own recognisable sound. Kinds with no mapping play nothing. */
static int voice_params(uint8_t kind, double *freq, double *amp, double *decay)
{
    switch (kind) {
    case WS_LIFE_EV_KILLED: *freq = 180.0; *amp = 0.80; *decay = 3.0; return 1;
    case WS_LIFE_EV_DIED:   *freq = 120.0; *amp = 0.70; *decay = 2.0; return 1;
    case WS_LIFE_EV_BORN:   *freq = 660.0; *amp = 0.50; *decay = 5.0; return 1;
    case WS_LIFE_EV_ATE:    *freq = 320.0; *amp = 0.30; *decay = 8.0; return 1;
    default:                return 0;
    }
}

/* Minimum world-time gap between two voices of the same kind. A herd grazing
 * produces far more forage events than a listener wants to hear, so the same
 * kind retriggers at most once per window; a sparse kind is unaffected. */
static double kind_cooldown(uint8_t kind)
{
    switch (kind) {
    case WS_LIFE_EV_KILLED: return 0.8;
    case WS_LIFE_EV_DIED:   return 1.0;
    case WS_LIFE_EV_BORN:   return 0.5;
    case WS_LIFE_EV_ATE:    return 0.6;
    default:                return 0.0;
    }
}

void ws_audio_init(ws_audio *a, uint32_t sample_rate)
{
    if (a == NULL) {
        return;
    }
    memset(a, 0, sizeof(*a));
    a->sample_rate = sample_rate ? sample_rate : WS_AUDIO_SAMPLE_RATE;
    /* Negative sentinel: the first event of every kind always sounds, whatever
     * the starting world time. */
    for (uint32_t i = 0; i < WS_AUDIO_KIND_COUNT; ++i) {
        a->last_trigger_s[i] = -1.0e12;
    }
}

ws_result ws_audio_trigger_event(ws_audio *a, uint8_t kind, double pan,
                                 double time_s)
{
    if (a == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    double freq, amp, decay;
    if (!voice_params(kind, &freq, &amp, &decay)) {
        return WS_OK; /* no sound for this kind; nothing is invented */
    }
    if (kind < WS_AUDIO_KIND_COUNT) {
        double cd = kind_cooldown(kind);
        if (cd > 0.0 && time_s - a->last_trigger_s[kind] < cd) {
            a->events_skipped += 1u; /* too soon for this kind; not a drone */
            return WS_OK;
        }
        a->last_trigger_s[kind] = time_s;
    }
    pan = ws_clamp_double(pan, -1.0, 1.0);
    for (uint32_t i = 0; i < WS_AUDIO_MAX_VOICES; ++i) {
        ws_audio_voice *v = &a->voices[i];
        if (!v->active) {
            v->active = 1;
            v->phase = 0.0;
            v->freq = freq;
            v->amp = 0.0;
            v->amp_target = amp;
            v->decay = decay;
            v->pan = pan;
            a->events_played += 1u;
            return WS_OK;
        }
    }
    /* All voices busy: prefer to keep the world moving over stuttering audio. */
    a->voices_dropped += 1u;
    return WS_OK;
}

uint32_t ws_audio_active_voices(const ws_audio *a)
{
    if (a == NULL) {
        return 0u;
    }
    uint32_t n = 0u;
    for (uint32_t i = 0; i < WS_AUDIO_MAX_VOICES; ++i) {
        if (a->voices[i].active) {
            ++n;
        }
    }
    return n;
}

ws_result ws_audio_mix(ws_audio *a, int16_t *out, uint32_t frames)
{
    if (a == NULL || out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (a->sample_rate == 0u) {
        return WS_ERR_INVALID_STATE;
    }
    double inv_sr = 1.0 / (double)a->sample_rate;
    /* Attack reaches the peak in about 5 ms. */
    double attack_rate = 200.0;
    for (uint32_t f = 0; f < frames; ++f) {
        double left = 0.0;
        double right = 0.0;
        for (uint32_t i = 0; i < WS_AUDIO_MAX_VOICES; ++i) {
            ws_audio_voice *v = &a->voices[i];
            if (!v->active) {
                continue;
            }
            if (v->amp < v->amp_target) {
                v->amp += v->amp_target * attack_rate * inv_sr;
                if (v->amp > v->amp_target) {
                    v->amp = v->amp_target;
                }
            } else {
                v->amp *= 1.0 - v->decay * inv_sr;
                if (v->amp < 1e-4) {
                    v->active = 0;
                    v->amp = 0.0;
                    continue;
                }
            }
            double s = v->amp * sin(v->phase);
            double gl = sqrt((1.0 - v->pan) * 0.5);
            double gr = sqrt((1.0 + v->pan) * 0.5);
            left += s * gl;
            right += s * gr;
            v->phase += 2.0 * WS_PI * v->freq * inv_sr;
            if (v->phase > 2.0 * WS_PI) {
                v->phase -= 2.0 * WS_PI;
            }
        }
        /* Master gain keeps a herd's overlapping voices below full scale, and
         * a soft saturator (x/(1+|x|)) rounds off what is left instead of
         * hard-clipping it, which would buzz. */
        const double master = 0.32;
        left *= master;
        right *= master;
        left = left / (1.0 + fabs(left));
        right = right / (1.0 + fabs(right));
        out[f * 2u] = (int16_t)lround(left * 32767.0);
        out[f * 2u + 1u] = (int16_t)lround(right * 32767.0);
        a->samples_rendered += 1u;
    }
    return WS_OK;
}

uint64_t ws_audio_samples_rendered(const ws_audio *a)
{
    return a != NULL ? a->samples_rendered : 0u;
}

static void put_u32le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static void put_u16le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

ws_result ws_audio_write_wav(const char *path, const int16_t *pcm,
                             uint32_t frames, uint32_t sample_rate)
{
    if (path == NULL || pcm == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        return WS_ERR_IO;
    }
    uint32_t data_bytes = frames * 2u * 2u; /* stereo, 16-bit */
    uint32_t riff = 36u + data_bytes;
    uint8_t hdr[44];
    memcpy(hdr, "RIFF", 4);
    put_u32le(&hdr[4], riff);
    memcpy(&hdr[8], "WAVE", 4);
    memcpy(&hdr[12], "fmt ", 4);
    put_u32le(&hdr[16], 16u);          /* PCM chunk size */
    put_u16le(&hdr[20], 1u);           /* PCM */
    put_u16le(&hdr[22], 2u);           /* stereo */
    put_u32le(&hdr[24], sample_rate);
    put_u32le(&hdr[28], sample_rate * 2u * 2u); /* byte rate */
    put_u16le(&hdr[32], 4u);           /* block align */
    put_u16le(&hdr[34], 16u);          /* bits per sample */
    memcpy(&hdr[36], "data", 4);
    put_u32le(&hdr[40], data_bytes);
    size_t wrote = fwrite(hdr, 1, sizeof(hdr), f);
    if (wrote == sizeof(hdr) && frames > 0u) {
        wrote = fwrite(pcm, sizeof(int16_t), (size_t)frames * 2u, f);
    }
    int ok = ferror(f) == 0;
    if (fclose(f) != 0) {
        ok = 0;
    }
    return ok ? WS_OK : WS_ERR_IO;
}
