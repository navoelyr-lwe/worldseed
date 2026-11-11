/* Worldseed Engine — product motion-quality checklist harness.
 *
 * Drives the real reference quadruped through the gait controller and evaluates
 * the run against the objective checklist in docs/QUALITY-CHECKLIST.md, writing
 * a CSV of metrics and a per-scene verdict. This is the automated evidence for
 * the P12 "motion" quality dimension; a still frame or golden hash cannot cover
 * it (docs/12).
 *
 * Usage: ws_motion <output-dir> [seed]
 * Exits non-zero (and prints WS_MOTION_FAIL) when any scene fails its spec.
 */
#include "render/ws_motion.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *failure_names(uint32_t fail)
{
    static char buf[128];
    buf[0] = '\0';
    struct { uint32_t bit; const char *name; } names[] = {
        {WS_MOTION_FAIL_FORWARD, "forward"},
        {WS_MOTION_FAIL_SPEED, "speed"},
        {WS_MOTION_FAIL_SMOOTHNESS, "smoothness"},
        {WS_MOTION_FAIL_BALANCE, "balance"},
        {WS_MOTION_FAIL_CONTACT, "contact"},
        {WS_MOTION_FAIL_ENERGY, "energy"},
        {WS_MOTION_FAIL_DURATION, "duration"},
        {WS_MOTION_FAIL_DETERMINISM, "determinism"},
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (fail & names[i].bit) {
            if (buf[0] != '\0') {
                strncat(buf, "+", sizeof(buf) - strlen(buf) - 1u);
            }
            strncat(buf, names[i].name, sizeof(buf) - strlen(buf) - 1u);
        }
    }
    return buf;
}

/* Simulated interval measured per scene, in seconds. */
#define MOTION_SECONDS 20.0

int main(int argc, char **argv)
{
    const char *out_dir = argc > 1 ? argv[1] : "evidence/motion";
    uint64_t seed = argc > 2 ? strtoull(argv[2], NULL, 10) : 20260101ULL;

    char csv_path[512];
    snprintf(csv_path, sizeof(csv_path), "%s/motion.csv", out_dir);
    FILE *csv = fopen(csv_path, "wb");
    if (csv == NULL) {
        fprintf(stderr, "cannot write %s (does the directory exist?)\n", csv_path);
        return 1;
    }
    fprintf(csv, "scene,seed,frames,forward_m,forward_speed_mps,max_body_speed_mps,"
                 "max_com_accel_mps2,max_torso_tilt_rad,min_foot_clearance_m,"
                 "max_energy_j,failures,verdict\n");

    int failed = 0;
    size_t spec_count = ws_motion_spec_count();
    for (size_t i = 0; i < spec_count; ++i) {
        const ws_motion_spec *spec = ws_motion_spec_at(i);

        ws_motion_metrics m;
        ws_result res = ws_motion_measure(seed, MOTION_SECONDS, &m);
        if (res != WS_OK) {
            fprintf(stderr, "motion: run failed for %s: %s\n", spec->scene_id,
                    ws_result_name(res));
            failed = 1;
            continue;
        }

        /* Determinism: the same seed must reproduce identical metrics. */
        ws_motion_metrics m2;
        int reproducible = 0;
        if (ws_motion_measure(seed, MOTION_SECONDS, &m2) == WS_OK) {
            reproducible = (m.frames == m2.frames) &&
                           (m.forward_m == m2.forward_m) &&
                           (m.forward_speed_mps == m2.forward_speed_mps) &&
                           (m.max_body_speed_mps == m2.max_body_speed_mps) &&
                           (m.max_com_accel_mps2 == m2.max_com_accel_mps2) &&
                           (m.max_torso_tilt_rad == m2.max_torso_tilt_rad) &&
                           (m.min_foot_clearance_m == m2.min_foot_clearance_m) &&
                           (m.max_energy_j == m2.max_energy_j);
        }

        uint32_t failures = 0u;
        ws_result verdict = ws_motion_check(spec, &m, &failures);
        if (!reproducible) {
            failures |= WS_MOTION_FAIL_DETERMINISM;
        }
        int scene_ok = (verdict == WS_OK) && reproducible;
        if (!scene_ok) {
            failed = 1;
        }

        fprintf(csv, "%s,%llu,%llu,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%s,%s\n",
                spec->scene_id, (unsigned long long)seed, (unsigned long long)m.frames,
                m.forward_m, m.forward_speed_mps, m.max_body_speed_mps,
                m.max_com_accel_mps2, m.max_torso_tilt_rad, m.min_foot_clearance_m,
                m.max_energy_j, scene_ok ? "-" : failure_names(failures),
                scene_ok ? "PASS" : "FAIL");

        printf("motion scene=%s verdict=%s reproducible=%d frames=%llu forward_m=%.3f "
               "speed=%.3f max_speed=%.3f com_accel=%.2f tilt=%.4f "
               "foot_clear=%.4f max_energy=%.2f failures=%s\n",
               spec->scene_id, scene_ok ? "PASS" : "FAIL", reproducible,
               (unsigned long long)m.frames, m.forward_m, m.forward_speed_mps,
               m.max_body_speed_mps, m.max_com_accel_mps2, m.max_torso_tilt_rad,
               m.min_foot_clearance_m, m.max_energy_j,
               scene_ok ? "-" : failure_names(failures));
    }

    fclose(csv);
    if (failed) {
        printf("WS_MOTION_FAIL scenes=%zu\n", spec_count);
        return 1;
    }
    printf("WS_MOTION_OK scenes=%zu csv=%s\n", spec_count, csv_path);
    return 0;
}
