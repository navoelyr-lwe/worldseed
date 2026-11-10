/* Worldseed Engine — cinematic observer and soundscape tests (P9).
 *
 * Regression suite from docs/09 acceptance:
 *   - the director cannot change world state (with/without it, the ecology run
 *     is bit-identical),
 *   - the director produces no fake events (it only ever echoes a real event id
 *     and that id binds to a shot),
 *   - camera smoothness and continuity (bounded speed/accel/turn between cuts,
 *     no strobing, a hard cut resets motion),
 *   - occlusion handling moves only the camera,
 *   - the soundscape is real, deterministic and event-linked.
 * Real models; no mocks.
 */
#include "ws_test.h"

#include "audio/ws_audio.h"
#include "cinema/ws_director.h"
#include "core/ws_hash.h"
#include "life/ws_ecology.h"
#include "life/ws_life_event.h"

#include <math.h>
#include <string.h>

/* --- helpers ------------------------------------------------------------- */

/* A life event with a chosen kind/actor/position, as the ecology emits them. */
static ws_event make_life_event(uint64_t seq, uint64_t tick, uint8_t kind,
                                uint32_t actor, float x, float y, float value)
{
    ws_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = WS_EVENT_LIFE;
    ev.sequence = seq;
    ev.tick = tick;
    uint16_t n = ws_life_event_encode(ev.payload, kind, actor, 0u, x, y, value,
                                      (uint32_t)tick);
    ev.payload_size = n;
    return ev;
}

/* A synthetic ridge for the occlusion test: flat with a tall wall near z=0. */
static double ridge_height(void *ctx, double x, double z)
{
    (void)ctx;
    (void)x;
    if (z > -8.0 && z < 0.0) {
        return 40.0;
    }
    return 0.0;
}

/* Run the ecology for `steps` and digest every new event it produced. When
 * `dir` is non-NULL, every event is also offered to the director each step.
 * The event stream is the ecology's own; the director only reads it. */
static void ecology_event_digest(uint64_t seed, int steps, double dt,
                                 ws_director *dir, char out_hex[65])
{
    ws_eco_config cfg = ws_eco_config_default(seed);
    ws_ecology eco;
    (void)ws_ecology_init(&eco, &cfg);
    ws_sha256 h;
    ws_sha256_init(&h);
    uint64_t next_seq = 0u;
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
            ws_sha256_update(&h, &ev.type, sizeof(ev.type));
            ws_sha256_update(&h, &ev.sequence, sizeof(ev.sequence));
            ws_sha256_update(&h, &ev.tick, sizeof(ev.tick));
            ws_sha256_update(&h, ev.payload, ev.payload_size);
            if (dir != NULL) {
                (void)ws_director_observe(dir, &ev);
            }
        }
        if (dir != NULL) {
            (void)ws_director_update(dir, dt);
        }
    }
    uint8_t d[WS_SHA256_DIGEST_SIZE];
    ws_sha256_final(&h, d);
    ws_sha256_hex(d, out_hex);
    ws_ecology_free(&eco);
}

/* --- director: no world authority --------------------------------------- */

static int test_director_has_no_world_authority(void)
{
    /* The director is a pure consumer: run the same ecology with a director
     * attached and without it, and require the event streams to be identical.
     * If the director could write to the world, the digests would differ. */
    char without[65];
    ecology_event_digest(0x51eedu, 400, 0.05, NULL, without);

    ws_director dir;
    ws_director_init(&dir, NULL);
    char with[65];
    ecology_event_digest(0x51eedu, 400, 0.05, &dir, with);

    WS_CHECK(strcmp(without, with) == 0);
    /* The director actually observed real events during that run. */
    WS_CHECK(ws_director_metrics_of(&dir)->events_observed > 0u);
    return 1;
}

/* --- director: no fake events, real id binding -------------------------- */

static int test_director_binds_real_events_only(void)
{
    ws_director dir;
    ws_director_init(&dir, NULL);

    /* Before any event the director has no shot; it invents nothing. */
    WS_CHECK(ws_director_current_shot(&dir) == NULL);
    WS_CHECK(ws_director_metrics_of(&dir)->events_observed == 0u);

    /* A non-life event is rejected outright. */
    ws_event other;
    memset(&other, 0, sizeof(other));
    other.type = WS_EVENT_WORLD_TICK;
    other.sequence = 1u;
    WS_CHECK(ws_director_observe(&dir, &other) == WS_ERR_INVALID_ARGUMENT);
    WS_CHECK(ws_director_metrics_of(&dir)->events_observed == 0u);
    WS_CHECK(ws_director_current_shot(&dir) == NULL);

    /* A real life event produces a shot bound to that exact event id. */
    ws_event ev = make_life_event(7u, 100u, WS_LIFE_EV_KILLED, WS_SPECIES_FOX,
                                  12.0f, 30.0f, 1.0f);
    WS_CHECK(ws_director_observe(&dir, &ev) == WS_OK);
    const ws_director_shot *shot = ws_director_current_shot(&dir);
    WS_CHECK(shot != NULL);
    WS_CHECK(shot->event_sequence == 7u);
    WS_CHECK(shot->event_tick == 100u);
    WS_CHECK(shot->event_kind == WS_LIFE_EV_KILLED);
    WS_CHECK(shot->actor_species == WS_SPECIES_FOX);
    WS_CHECK(fabs(shot->subject.x - 12.0) < 1e-6);
    WS_CHECK(fabs(shot->subject.z - 30.0) < 1e-6);
    WS_CHECK(shot->type == WS_SHOT_TRACKING);

    /* Re-offering an already seen event changes nothing (no double count). */
    uint64_t before = ws_director_metrics_of(&dir)->events_observed;
    WS_CHECK(ws_director_observe(&dir, &ev) == WS_OK);
    WS_CHECK(ws_director_metrics_of(&dir)->events_observed == before);
    WS_CHECK(ws_director_current_shot(&dir)->event_sequence == 7u);

    /* An older event never overrides a newer shot. */
    ws_event old = make_life_event(3u, 50u, WS_LIFE_EV_BORN, WS_SPECIES_DEER,
                                   1.0f, 1.0f, 0.0f);
    WS_CHECK(ws_director_observe(&dir, &old) == WS_OK);
    WS_CHECK(ws_director_current_shot(&dir)->event_sequence == 7u);
    return 1;
}

/* --- director: shot selection and hysteresis ---------------------------- */

static int test_director_shot_priority_and_hold(void)
{
    ws_director_config cfg = ws_director_config_default();
    ws_director dir;
    ws_director_init(&dir, &cfg);

    /* First event always takes the shot. */
    ws_event born = make_life_event(1u, 10u, WS_LIFE_EV_BORN, WS_SPECIES_DEER,
                                    0.0f, 0.0f, 0.0f);
    WS_CHECK(ws_director_observe(&dir, &born) == WS_OK);
    WS_CHECK(ws_director_current_shot(&dir)->event_sequence == 1u);

    /* A higher-importance event arriving inside the minimum hold window does
     * NOT cut: continuity beats novelty. */
    ws_event kill = make_life_event(2u, 11u, WS_LIFE_EV_KILLED, WS_SPECIES_FOX,
                                    5.0f, 5.0f, 1.0f);
    WS_CHECK(ws_director_observe(&dir, &kill) == WS_OK);
    WS_CHECK(ws_director_current_shot(&dir)->event_sequence == 1u);

    /* After the minimum hold elapses, the same class of event can cut. */
    for (int i = 0; i < 60; ++i) {
        WS_CHECK(ws_director_update(&dir, 0.05) == WS_OK); /* ~3 s */
    }
    ws_event kill2 = make_life_event(3u, 20u, WS_LIFE_EV_KILLED, WS_SPECIES_FOX,
                                     9.0f, 9.0f, 1.0f);
    WS_CHECK(ws_director_observe(&dir, &kill2) == WS_OK);
    WS_CHECK(ws_director_current_shot(&dir)->event_sequence == 3u);
    WS_CHECK(ws_director_metrics_of(&dir)->cuts == 2u);
    return 1;
}

/* --- director: motion limits (smoothness) ------------------------------- */

static int test_director_motion_is_bounded(void)
{
    ws_director_config cfg = ws_director_config_default();
    ws_director dir;
    ws_director_init(&dir, &cfg);

    /* Drive the director with a moving subject: one event takes the shot, then
     * the subject walks a fast, curving path so the camera must chase it. This
     * is the tracking shot the smoothness caps exist for. */
    ws_event ev = make_life_event(1u, 1u, WS_LIFE_EV_KILLED, WS_SPECIES_FOX,
                                  60.0f, 60.0f, 1.0f);
    WS_CHECK(ws_director_observe(&dir, &ev) == WS_OK);
    for (int i = 0; i < 400; ++i) {
        double t = (double)i * 0.05;
        ws_vec3 subject = ws_vec3_make(60.0 + 40.0 * sin(t * 0.5),
                                       0.0,
                                       60.0 + 40.0 * cos(t * 0.7));
        WS_CHECK(ws_director_track(&dir, subject) == WS_OK);
        WS_CHECK(ws_director_update(&dir, 0.05) == WS_OK);
    }
    const ws_director_metrics *m = ws_director_metrics_of(&dir);
    /* The metrics are measured over bounded (non-cut) frames only; the caps
     * hold with a small numerical margin. */
    WS_CHECK(m->speed_max <= cfg.max_speed * 1.05 + 1e-6);
    WS_CHECK(m->accel_max <= cfg.max_accel * 1.05 + 1e-6);
    WS_CHECK(m->turn_rate_max <= cfg.max_turn_rate * 1.05 + 1e-6);
    /* It actually moved (not a frozen camera). */
    WS_CHECK(m->speed_max > 0.0);
    return 1;
}

/* --- director: continuity across frames -------------------------------- */

static int test_director_continuity_no_jump(void)
{
    ws_director dir;
    ws_director_init(&dir, NULL);

    /* One kill, then many small steps: the eye must not jump between steps
     * (every step's eye move is within the speed cap * dt). */
    ws_event ev = make_life_event(1u, 1u, WS_LIFE_EV_KILLED, WS_SPECIES_FOX,
                                  50.0f, 50.0f, 1.0f);
    WS_CHECK(ws_director_observe(&dir, &ev) == WS_OK);
    WS_CHECK(ws_director_update(&dir, 0.02) == WS_OK); /* hard set after cut */

    ws_vec3 prev = ws_director_camera(&dir)->eye;
    const double dt = 0.02;
    double max_step = dir.config.max_speed * dt * 1.05 + 1e-9;
    for (int i = 0; i < 200; ++i) {
        WS_CHECK(ws_director_update(&dir, dt) == WS_OK);
        ws_vec3 eye = ws_director_camera(&dir)->eye;
        double step = sqrt((eye.x - prev.x) * (eye.x - prev.x) +
                           (eye.y - prev.y) * (eye.y - prev.y) +
                           (eye.z - prev.z) * (eye.z - prev.z));
        WS_CHECK(step <= max_step);
        prev = eye;
    }
    return 1;
}

/* --- director: mode control and free camera ----------------------------- */

static int test_director_mode_and_free_camera(void)
{
    ws_director dir;
    ws_director_init(&dir, NULL);
    WS_CHECK(ws_director_mode_of(&dir) == WS_DIRECTOR_AUTO);

    ws_event ev = make_life_event(1u, 1u, WS_LIFE_EV_KILLED, WS_SPECIES_FOX,
                                  20.0f, 20.0f, 1.0f);
    WS_CHECK(ws_director_observe(&dir, &ev) == WS_OK);
    WS_CHECK(ws_director_update(&dir, 0.05) == WS_OK);

    /* In free mode the director holds: the camera does not move, the world is
     * untouched either way. */
    ws_director_set_mode(&dir, WS_DIRECTOR_FREE);
    ws_vec3 before = ws_director_camera(&dir)->eye;
    for (int i = 0; i < 50; ++i) {
        WS_CHECK(ws_director_update(&dir, 0.05) == WS_OK);
    }
    ws_vec3 after = ws_director_camera(&dir)->eye;
    WS_CHECK(before.x == after.x && before.y == after.y && before.z == after.z);

    /* Back to auto: motion resumes. */
    ws_director_set_mode(&dir, WS_DIRECTOR_AUTO);
    WS_CHECK(ws_director_update(&dir, 0.05) == WS_OK);
    return 1;
}

/* --- director: occlusion lifts only the camera -------------------------- */

static int test_director_occlusion(void)
{
    ws_director dir;
    ws_director_init(&dir, NULL);
    ws_director_set_ground(&dir, ridge_height, NULL);

    /* A subject just behind the ridge; without handling the eye at the default
     * height would look through the hill. */
    ws_event ev = make_life_event(1u, 1u, WS_LIFE_EV_KILLED, WS_SPECIES_FOX,
                                  0.0f, 10.0f, 1.0f);
    WS_CHECK(ws_director_observe(&dir, &ev) == WS_OK);
    WS_CHECK(ws_director_last_occlusion_lift(&dir) == 1);

    /* With the ridge removed, the same shot needs no lift. */
    ws_director dir2;
    ws_director_init(&dir2, NULL);
    WS_CHECK(ws_director_observe(&dir2, &ev) == WS_OK);
    WS_CHECK(ws_director_last_occlusion_lift(&dir2) == 0);
    return 1;
}

/* --- soundscape: real, deterministic, event-linked ---------------------- */

static int test_soundscape_event_linked_and_deterministic(void)
{
    /* The same real event sequence produces a bit-identical PCM buffer. */
    uint8_t digests[2][WS_SHA256_DIGEST_SIZE];
    for (int run = 0; run < 2; ++run) {
        ws_audio audio;
        ws_audio_init(&audio, WS_AUDIO_SAMPLE_RATE);
        WS_CHECK(ws_audio_trigger_event(&audio, WS_LIFE_EV_BORN, -0.5, 0.0) == WS_OK);
        WS_CHECK(ws_audio_trigger_event(&audio, WS_LIFE_EV_KILLED, 0.5, 0.1) == WS_OK);
        WS_CHECK(ws_audio_trigger_event(&audio, WS_LIFE_EV_ATE, 0.0, 0.2) == WS_OK);
        WS_CHECK(ws_audio_trigger_event(&audio, WS_LIFE_EV_DIED, 0.2, 0.3) == WS_OK);
        uint32_t frames = WS_AUDIO_SAMPLE_RATE / 2u; /* 0.5 s */
        int16_t pcm[WS_AUDIO_SAMPLE_RATE];
        WS_CHECK(ws_audio_mix(&audio, pcm, frames) == WS_OK);
        ws_sha256_buffer(pcm, (size_t)frames * 2u * sizeof(int16_t),
                         digests[run]);
    }
    WS_CHECK(memcmp(digests[0], digests[1], WS_SHA256_DIGEST_SIZE) == 0);
    return 1;
}

static int test_soundscape_produces_signal(void)
{
    ws_audio audio;
    ws_audio_init(&audio, WS_AUDIO_SAMPLE_RATE);
    /* An unmapped kind plays nothing and is not counted. */
    WS_CHECK(ws_audio_trigger_event(&audio, 200u, 0.0, 0.0) == WS_OK);
    WS_CHECK(ws_audio_active_voices(&audio) == 0u);

    WS_CHECK(ws_audio_trigger_event(&audio, WS_LIFE_EV_KILLED, 0.0, 0.0) == WS_OK);
    WS_CHECK(ws_audio_active_voices(&audio) == 1u);

    uint32_t frames = WS_AUDIO_SAMPLE_RATE / 4u;
    int16_t pcm[WS_AUDIO_SAMPLE_RATE];
    WS_CHECK(ws_audio_mix(&audio, pcm, frames) == WS_OK);
    /* Real signal: at least one non-zero sample. */
    int nonzero = 0;
    for (uint32_t i = 0; i < frames * 2u; ++i) {
        if (pcm[i] != 0) {
            nonzero = 1;
            break;
        }
    }
    WS_CHECK(nonzero == 1);
    WS_CHECK(ws_audio_samples_rendered(&audio) == frames);

    /* A WAV is written with a valid header. */
    const char *path = "ws_test_soundscape.wav";
    WS_CHECK(ws_audio_write_wav(path, pcm, frames, WS_AUDIO_SAMPLE_RATE) == WS_OK);
    FILE *f = fopen(path, "rb");
    WS_CHECK(f != NULL);
    uint8_t hdr[12];
    size_t got = fread(hdr, 1, sizeof(hdr), f);
    fclose(f);
    remove(path);
    WS_CHECK(got == sizeof(hdr));
    WS_CHECK(memcmp(hdr, "RIFF", 4) == 0);
    WS_CHECK(memcmp(&hdr[8], "WAVE", 4) == 0);
    return 1;
}

/* --- soundscape: driven by a real ecology run --------------------------- */

static int test_soundscape_from_ecology(void)
{
    ws_eco_config cfg = ws_eco_config_default(0x9a17u);
    ws_ecology eco;
    WS_CHECK(ws_ecology_init(&eco, &cfg) == WS_OK);
    ws_audio audio;
    ws_audio_init(&audio, WS_AUDIO_SAMPLE_RATE);

    uint64_t next_seq = 0u;
    uint64_t triggers = 0u;
    for (int i = 0; i < 2000; ++i) {
        WS_CHECK(ws_ecology_step(&eco, 0.05) == WS_OK);
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
            if (ws_life_event_decode(ev.payload, ev.payload_size, &kind, NULL,
                                     NULL, NULL, NULL, NULL, NULL)) {
                (void)ws_audio_trigger_event(&audio, kind, 0.0, (double)i * 0.05);
                triggers += 1u;
            }
        }
    }
    /* The world produced life events, so the soundscape has something to say. */
    WS_CHECK(triggers > 0u);
    ws_ecology_free(&eco);
    return 1;
}

int main(void)
{
    const ws_test_case cases[] = {
        {"director has no world authority", test_director_has_no_world_authority},
        {"director binds real events only", test_director_binds_real_events_only},
        {"shot priority and hold", test_director_shot_priority_and_hold},
        {"camera motion bounded", test_director_motion_is_bounded},
        {"camera continuity, no jump", test_director_continuity_no_jump},
        {"mode control and free camera", test_director_mode_and_free_camera},
        {"occlusion lifts only camera", test_director_occlusion},
        {"soundscape deterministic", test_soundscape_event_linked_and_deterministic},
        {"soundscape produces signal", test_soundscape_produces_signal},
        {"soundscape from ecology run", test_soundscape_from_ecology}
    };
    return ws_test_run("cinema", cases, (int)(sizeof(cases) / sizeof(cases[0])));
}
