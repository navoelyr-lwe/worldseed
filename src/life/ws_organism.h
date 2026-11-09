/* Worldseed Engine — organism internal state and homeostasis (P6).
 *
 * The organism owns the animal's slow internal state: energy reserve, hydration,
 * body temperature, fatigue and health, plus the need drives derived from them
 * (docs/07 "Organizma modeli" and "Davranis sistemi" -> ihtiyac ve homeostaz).
 * The locomotion (P5) and behaviour (P6 agent) layers read these drives and
 * write back their consequences: movement spends energy and water and builds
 * fatigue, eating and drinking refill reserves, resting recovers.
 *
 * Reserves are stored as fractions in [0,1] of a species maximum so the model
 * is mass-independent; the rates are per second and calibrated to give a
 * plausible foraging cycle at the reference gait speed (see the model card
 * docs/life/MODEL-KARTI-HOMEOSTAZ.md, evidence level D1/D2). This is not a full
 * bioenergetic model: it is a documented, deterministic approximation whose
 * conservation properties (reserves never leave [0,1], no free energy) are
 * asserted by the P6 tests.
 *
 * All quantities are SI; storage is caller-owned and nothing is allocated.
 */
#ifndef WS_LIFE_ORGANISM_H
#define WS_LIFE_ORGANISM_H

#include <stdint.h>

#include "../core/ws_result.h"

#define WS_ORGANISM_VERSION 1u

typedef enum ws_need {
    WS_NEED_ENERGY = 0, /* hunger */
    WS_NEED_WATER = 1,  /* thirst */
    WS_NEED_WARMTH = 2, /* thermal discomfort */
    WS_NEED_REST = 3,   /* fatigue */
    WS_NEED_COUNT = 4
} ws_need;

const char *ws_need_name(int need);

typedef struct ws_organism_config {
    double comfort_temp_c;     /* preferred body temperature, degrees C */
    double thermal_scale_c;    /* temperature error that saturates the drive */
    double basal_energy_rate;  /* energy reserve fraction spent per second at rest */
    double move_energy_rate;   /* extra energy fraction per second at 1 m/s */
    double basal_water_rate;   /* hydration fraction lost per second at rest */
    double move_water_rate;    /* extra hydration fraction per second at 1 m/s */
    double thermal_rate;       /* body-temp relaxation rate toward target, 1/s */
    double activity_heat_c;    /* steady-state temperature rise per (m/s) */
    double fatigue_rate;       /* fatigue index gained per second at 1 m/s */
    double recovery_rate;      /* fatigue index recovered per second at rest */
    double starve_health_rate; /* health lost per second with an empty reserve */
} ws_organism_config;

ws_organism_config ws_organism_config_default(void);

typedef struct ws_organism {
    ws_organism_config config;
    double energy;      /* 0..1 */
    double hydration;   /* 0..1 */
    double body_temp_c; /* degrees C */
    double fatigue;     /* 0..1 */
    double health;      /* 0..1 */
    int alive;          /* 0 once health reaches 0 */
    double hunger;      /* derived drive 0..1 */
    double thirst;      /* derived drive 0..1 */
    double warmth;      /* derived drive 0..1 */
} ws_organism;

/* Initialise with full reserves at the comfort temperature. */
void ws_organism_init(ws_organism *o, const ws_organism_config *config);

/* Derived need drive in [0,1]; 1 is the most urgent. */
double ws_organism_drive(const ws_organism *o, int need);

/* The most urgent need, or -1 when every drive is below `threshold`. Ties are
 * broken by lower need index so the choice is deterministic. */
int ws_organism_most_urgent(const ws_organism *o, double threshold);

/* Advance homeostasis by `dt` seconds. `speed` is the current path speed
 * (m/s); `ambient_temp_c` is the air temperature; `resting` is non-zero when the
 * animal is resting (adds fatigue recovery). Reserves are clamped to [0,1] and
 * health is lost only while a reserve is empty. Returns WS_OK, or
 * WS_ERR_INVALID_ARGUMENT for a non-positive dt. */
ws_result ws_organism_step(ws_organism *o, double dt, double speed,
                           double ambient_temp_c, int resting);

/* Refill energy reserve by `amount` (fraction, clamped); returns the amount
 * actually absorbed (never more than was needed), so a caller can consume only
 * the food that was used. */
double ws_organism_eat(ws_organism *o, double amount);

/* Refill hydration by `amount` (fraction, clamped); returns the amount
 * absorbed. */
double ws_organism_drink(ws_organism *o, double amount);

#endif /* WS_LIFE_ORGANISM_H */
