/* Worldseed Engine — representative species catalogue (P7).
 *
 * The first release carries four representative species fixed by docs/01: red
 * deer (Cervus elaphus), red fox (Vulpes vulpes), European rabbit (Oryctolagus
 * cuniculus) and wild boar (Sus scrofa). Each species is a real physical body
 * and a species-specific gait (built by the P5 quadruped module from a species
 * profile), plus the life-history and foraging parameters the ecology layer
 * needs (diet, speed, sensory range, metabolic rates, reproduction, lifespan).
 *
 * Nothing here is an animation clip or a scripted pose: the body is an
 * articulated rigid chain and every number is a literature-informed planning
 * value recorded in the species cards (docs/life/TUR-KARTI-*.md) with its
 * evidence level. Values are approximations for a plausible ecosystem, not
 * measurements of a specific wild population.
 *
 * SI units throughout. Storage is a fixed table; nothing is allocated.
 */
#ifndef WS_LIFE_SPECIES_H
#define WS_LIFE_SPECIES_H

#include "ws_quadruped.h"

#define WS_SPECIES_VERSION 1u

typedef enum ws_species_kind {
    WS_SPECIES_DEER = 0,
    WS_SPECIES_FOX = 1,
    WS_SPECIES_RABBIT = 2,
    WS_SPECIES_BOAR = 3,
    WS_SPECIES_COUNT = 4
} ws_species_kind;

/* Trophic role. Used by the ecology layer to decide what an animal eats. */
typedef enum ws_diet {
    WS_DIET_HERBIVORE = 0,
    WS_DIET_CARNIVORE = 1,
    WS_DIET_OMNIVORE = 2
} ws_diet;

typedef struct ws_species_info {
    const char *name;      /* common name with the binomial */
    const char *card;      /* species card path (docs/life/...) */
    ws_species_profile body; /* mass, proportions, gait description */
    int diet;              /* ws_diet */

    double max_speed;      /* m/s, sustained travel speed */
    double sense_range;    /* m, foraging/perception radius */
    double bite;           /* biomass units taken per forage event */

    double basal_rate;     /* energy reserve fraction lost per second at rest */
    double move_rate;      /* extra energy fraction per second at 1 m/s */
    double forage_gain;    /* energy fraction gained per unit biomass eaten */
    double plant_gain;     /* energy fraction gained per unit plant/insect food;
                            * 0 for a strict carnivore. The red fox is a
                            * generalist and keeps a low-quality alternative
                            * food, which is what lets it persist when prey are
                            * scarce instead of starving out with them. */

    double maturity_s;     /* biological age of maturity (life-history record) */
    double max_age_s;      /* biological lifespan (life-history record) */
    double repro_energy;   /* reserve fraction needed to reproduce */
    double repro_cost;     /* reserve fraction one offspring costs the parent */
    double repro_rate;     /* expected offspring per mature animal per second */
    double eco_maturity_s; /* age at first reproduction on the ecology clock */
    double eco_lifespan_s; /* maximum age on the ecology clock */
    double juvenile_frac;  /* offspring starting reserve as a fraction */
} ws_species_info;

/* Fixed, literature-informed table entry for a species. */
const ws_species_info *ws_species_get(ws_species_kind kind);

const char *ws_species_name(ws_species_kind kind);
const char *ws_species_card(ws_species_kind kind);

/* Locomotion reference speed (m/s) for the species' walk used by the scene. */
double ws_species_walk_speed(ws_species_kind kind);

/* Species a predator `kind` is willing and able to hunt, or -1. Predation is a
 * fixed food-web edge for the first release (docs/07 "besin agi"). */
int ws_species_prey(ws_species_kind kind);

#endif /* WS_LIFE_SPECIES_H */
