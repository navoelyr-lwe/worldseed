/* Worldseed Engine — soundscape evidence tool.
 *
 * Runs the P9 soundscape over a real ecology run and writes the resulting PCM
 * as a WAV, so the sound the world produced is real, listenable evidence rather
 * than a claim. Also prints machine-readable counters (events, voices, sample
 * count) and the PCM SHA-256 so CI can store and re-check it.
 *
 * The audio is generated entirely by our own C17 code (src/audio/ws_audio.c):
 * no recorded samples, no third-party audio runtime.
 *
 * Usage: ws_audio <output.wav> [seed]
 */
#include "audio/ws_audio.h"
#include "core/ws_hash.h"
#include "life/ws_ecology.h"
#include "life/ws_life_event.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : "evidence/audio/soundscape.wav";
    uint64_t seed = argc > 2 ? strtoull(argv[2], NULL, 10) : 20260101ULL;

    ws_eco_config cfg = ws_eco_config_default(0x7ec0u ^ seed);
    ws_ecology eco;
    if (ws_ecology_init(&eco, &cfg) != WS_OK) {
        fprintf(stderr, "ecology init failed\n");
        return 1;
    }
    ws_audio audio;
    ws_audio_init(&audio, WS_AUDIO_SAMPLE_RATE);

    /* Run the world and mix the audio in step with it, so each voice decays
     * over real time instead of piling up. 60 s at 48 kHz stereo. */
    const double dt = 0.05;
    const int steps = (int)(60.0 / dt);
    const uint32_t frames_per_step = (uint32_t)(WS_AUDIO_SAMPLE_RATE * dt);
    const uint32_t total_frames = frames_per_step * (uint32_t)steps;
    int16_t *pcm = (int16_t *)malloc((size_t)total_frames * 2u * sizeof(int16_t));
    if (pcm == NULL) {
        ws_ecology_free(&eco);
        fprintf(stderr, "pcm allocation failed\n");
        return 1;
    }
    uint64_t next_seq = 0u;
    uint32_t hits[4] = {0u, 0u, 0u, 0u};
    for (int i = 0; i < steps; ++i) {
        (void)ws_ecology_step(&eco, dt);
        uint32_t n = ws_ecology_event_count(&eco);
        for (uint32_t k = 0; k < n; ++k) {
            ws_event ev;
            if (ws_ecology_event_at(&eco, k, &ev) != WS_OK) {
                continue;
            }
            if (ev.sequence < next_seq) {
                continue;
            }
            next_seq = ev.sequence + 1u;
            uint8_t kind;
            float x;
            if (!ws_life_event_decode(ev.payload, ev.payload_size, &kind, NULL,
                                      NULL, &x, NULL, NULL, NULL)) {
                continue;
            }
            if (kind < 4u) {
                hits[kind] += 1u;
            }
            double pan = (x / cfg.area_m) * 2.0 - 1.0;
            (void)ws_audio_trigger_event(&audio, kind, pan, (double)i * dt);
        }
        if (ws_audio_mix(&audio, &pcm[(size_t)i * frames_per_step * 2u],
                         frames_per_step) != WS_OK) {
            free(pcm);
            ws_ecology_free(&eco);
            fprintf(stderr, "mix failed\n");
            return 1;
        }
    }

    const uint32_t frames = total_frames;
    ws_result w = ws_audio_write_wav(out, pcm, frames, WS_AUDIO_SAMPLE_RATE);

    uint8_t digest[WS_SHA256_DIGEST_SIZE];
    char hex[WS_SHA256_DIGEST_SIZE * 2u + 1u];
    ws_sha256_buffer(pcm, (size_t)frames * 2u * sizeof(int16_t), digest);
    ws_sha256_hex(digest, hex);

    /* Signal sanity: the mix must not be silent. */
    int nonzero = 0;
    for (uint32_t i = 0; i < frames * 2u; ++i) {
        if (pcm[i] != 0) {
            nonzero = 1;
            break;
        }
    }

    free(pcm);
    ws_ecology_free(&eco);

    printf("audio wav=%s seed=%llu sample_rate=%u frames=%u events=%llu "
           "skipped=%llu dropped=%llu samples=%llu ate=%u born=%u died=%u "
           "killed=%u sha256=%s\n",
           out, (unsigned long long)seed, WS_AUDIO_SAMPLE_RATE, frames,
           (unsigned long long)audio.events_played,
           (unsigned long long)audio.events_skipped,
           (unsigned long long)audio.voices_dropped,
           (unsigned long long)ws_audio_samples_rendered(&audio),
           hits[0], hits[1], hits[2], hits[3], hex);

    if (w != WS_OK || !nonzero || audio.events_played == 0u) {
        fprintf(stderr, "WS_AUDIO_FAILED\n");
        return 1;
    }
    printf("WS_AUDIO_OK\n");
    return 0;
}
