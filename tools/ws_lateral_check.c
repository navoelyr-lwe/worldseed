/* Worldseed Engine — frontal-plane lateral balance and turning evidence (P13).
 *
 * Runs the reduced-order lateral model (ws_lateral) for each representative
 * species, straight and in a commanded turn, and evaluates objective gates on
 * the resulting motion: bounded sway and roll, steps actually taken, a turn
 * rate that tracks its command within tolerance, and determinism. This is the
 * automated evidence for the P13 lateral-balance/turning criterion; the sagittal
 * solver's own gates stay in the life tests and the motion checklist.
 *
 * Usage: ws_lateral_check <output-dir> [seed]
 * Exits non-zero (and prints WS_LATERAL_FAIL) when any species fails a gate.
 */
#include "life/ws_lateral.h"
#include "life/ws_species.h"

#include <stdio.h>
#include <stdlib.h>

#define DT 0.002
#define STRAIGHT_SECONDS 60.0
#define TURN_SECONDS 30.0

/* Gates. Sway and roll bounds are per-species; the turn tolerance is absolute
 * because the commanded rate is well inside every species' bound. */
#define MAX_SWAY_FRACTION 0.6   /* of the species track, half-width */
#define MAX_ROLL_RAD 0.35
#define MIN_STEPS_PER_SECOND 1.0
#define MAX_TURN_TRACK_ERR 0.05 /* rad/s */

static int species_pass(const ws_species_info *si, double v_walk, double *out_max_y,
                        double *out_max_phi, double *out_err)
{
    ws_lateral_params p;
    ws_lateral_params_for(&si->body, &p);

    ws_lateral_metrics straight;
    if (ws_lateral_measure(&p, DT, v_walk, 0.0, STRAIGHT_SECONDS,
                           &straight) != WS_OK) {
        return 0;
    }

    /* A turn commanded well inside every species' bound: small enough to be
     * within reach of all four, large enough to demand real tracking. */
    double cmd = 0.3;
    ws_lateral_metrics turn;
    if (ws_lateral_measure(&p, DT, v_walk, cmd, TURN_SECONDS, &turn) != WS_OK) {
        return 0;
    }
    /* Determinism: the same call must reproduce identical metrics. */
    ws_lateral_metrics again;
    if (ws_lateral_measure(&p, DT, v_walk, cmd, TURN_SECONDS, &again) !=
        WS_OK) {
        return 0;
    }
    int reproducible = (turn.final_yaw == again.final_yaw) &&
                       (turn.turn_track_err == again.turn_track_err) &&
                       (turn.support_changes == again.support_changes);

    double sway_limit = MAX_SWAY_FRACTION * (p.track > 0.0 ? p.track : 1.0);
    double min_steps = MIN_STEPS_PER_SECOND * STRAIGHT_SECONDS;

    *out_max_y = straight.max_abs_y;
    *out_max_phi = straight.max_abs_phi;
    *out_err = turn.turn_track_err;

    int ok = reproducible &&
             straight.max_abs_y < sway_limit &&
             straight.max_abs_phi < MAX_ROLL_RAD &&
             (double)straight.support_changes >= min_steps &&
             turn.turn_track_err < MAX_TURN_TRACK_ERR &&
             turn.max_abs_phi < MAX_ROLL_RAD;
    return ok;
}

int main(int argc, char **argv)
{
    const char *out_dir = argc > 1 ? argv[1] : "evidence/lateral";
    (void)argv;

    char csv_path[512];
    snprintf(csv_path, sizeof(csv_path), "%s/lateral.csv", out_dir);
    FILE *csv = fopen(csv_path, "wb");
    if (csv == NULL) {
        fprintf(stderr, "cannot write %s (does the directory exist?)\n", csv_path);
        return 1;
    }
    fprintf(csv, "species,max_sway_m,max_roll_rad,turn_track_err_rad_s,verdict\n");

    int failed = 0;
    int scenes = 0;
    for (int k = 0; k < WS_SPECIES_COUNT; ++k) {
        const ws_species_info *si = ws_species_get((ws_species_kind)k);
        if (si == NULL) {
            continue;
        }
        double v_walk = ws_species_walk_speed((ws_species_kind)k);
        double max_y = 0.0, max_phi = 0.0, err = 0.0;
        int ok = species_pass(si, v_walk, &max_y, &max_phi, &err);
        if (!ok) {
            failed = 1;
        }
        scenes++;
        fprintf(csv, "%s,%.4f,%.4f,%.4f,%s\n", si->name, max_y, max_phi, err,
                ok ? "PASS" : "FAIL");
        printf("lateral species=%s verdict=%s max_sway=%.4f max_roll=%.4f "
               "turn_err=%.4f\n",
               si->name, ok ? "PASS" : "FAIL", max_y, max_phi, err);
    }
    fclose(csv);

    if (failed) {
        printf("WS_LATERAL_FAIL scenes=%d\n", scenes);
        return 1;
    }
    printf("WS_LATERAL_OK scenes=%d csv=%s\n", scenes, csv_path);
    return 0;
}
