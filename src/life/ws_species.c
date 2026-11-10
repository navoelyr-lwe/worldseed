/* Worldseed Engine — representative species catalogue (P7). */
#include "ws_species.h"

#include <stddef.h>

/* The body profile for each species. Proportions are literature-informed
 * planning values; see the species cards for sources and evidence level. */
static ws_species_profile deer_body(void)
{
    ws_species_profile p;
    p.name = "red deer (Cervus elaphus)";
    p.total_mass = 110.0;
    p.body_length = 1.6;
    p.shoulder_h = 1.05;
    p.leg_upper = 0.60;
    p.leg_lower = 0.60;
    p.stance = 0.80;
    p.gait_freq = 1.4;
    p.duty_factor = 0.70;
    p.friction = 0.85;
    return p;
}

static ws_species_profile fox_body(void)
{
    ws_species_profile p;
    p.name = "red fox (Vulpes vulpes)";
    p.total_mass = 6.5;
    p.body_length = 0.70;
    p.shoulder_h = 0.38;
    p.leg_upper = 0.22;
    p.leg_lower = 0.22;
    p.stance = 0.34;
    p.gait_freq = 2.6;
    p.duty_factor = 0.60;
    p.friction = 0.85;
    return p;
}

static ws_species_profile rabbit_body(void)
{
    ws_species_profile p;
    p.name = "European rabbit (Oryctolagus cuniculus)";
    p.total_mass = 1.8;
    p.body_length = 0.40;
    p.shoulder_h = 0.18;
    p.leg_upper = 0.09;
    p.leg_lower = 0.09;
    p.stance = 0.16;
    p.gait_freq = 3.0;
    p.duty_factor = 0.55;
    p.friction = 0.90;
    return p;
}

static ws_species_profile boar_body(void)
{
    ws_species_profile p;
    p.name = "wild boar (Sus scrofa)";
    p.total_mass = 80.0;
    p.body_length = 1.3;
    p.shoulder_h = 0.80;
    p.leg_upper = 0.30;
    p.leg_lower = 0.35;
    p.stance = 0.55;
    p.gait_freq = 1.8;
    p.duty_factor = 0.65;
    p.friction = 0.80;
    return p;
}

/* One entry per species kind; filled lazily so the function bodies above stay
 * close to their cards. */
static const ws_species_info *table(void)
{
    static ws_species_info info[WS_SPECIES_COUNT];
    static int ready = 0;
    if (!ready) {
        info[WS_SPECIES_DEER].name = "red deer (Cervus elaphus)";
        info[WS_SPECIES_DEER].card = "docs/life/TUR-KARTI-KIZIL-GEYIK.md";
        info[WS_SPECIES_DEER].body = deer_body();
        info[WS_SPECIES_DEER].diet = WS_DIET_HERBIVORE;
        info[WS_SPECIES_DEER].max_speed = 6.0;
        info[WS_SPECIES_DEER].sense_range = 45.0;
        info[WS_SPECIES_DEER].bite = 0.30;
        info[WS_SPECIES_DEER].basal_rate = 0.0018;
        info[WS_SPECIES_DEER].move_rate = 0.0018;
        info[WS_SPECIES_DEER].forage_gain = 0.60;
        info[WS_SPECIES_DEER].plant_gain = 0.60;
        info[WS_SPECIES_DEER].maturity_s = 2.0 * 365.25 * 86400.0;
        info[WS_SPECIES_DEER].max_age_s = 15.0 * 365.25 * 86400.0;
        info[WS_SPECIES_DEER].repro_energy = 0.70;
        info[WS_SPECIES_DEER].repro_cost = 0.25;
        info[WS_SPECIES_DEER].repro_rate = 0.0016;
        info[WS_SPECIES_DEER].eco_maturity_s = 900.0; /* 15 min */
        info[WS_SPECIES_DEER].eco_lifespan_s = 1800.0;
        info[WS_SPECIES_DEER].juvenile_frac = 0.50;

        info[WS_SPECIES_FOX].name = "red fox (Vulpes vulpes)";
        info[WS_SPECIES_FOX].card = "docs/life/TUR-KARTI-KIZIL-TILKI.md";
        info[WS_SPECIES_FOX].body = fox_body();
        info[WS_SPECIES_FOX].diet = WS_DIET_CARNIVORE;
        info[WS_SPECIES_FOX].max_speed = 13.0;
        info[WS_SPECIES_FOX].sense_range = 90.0;
        info[WS_SPECIES_FOX].bite = 0.35;
        info[WS_SPECIES_FOX].basal_rate = 0.0004;
        info[WS_SPECIES_FOX].move_rate = 0.0005;
        info[WS_SPECIES_FOX].forage_gain = 0.85;
        info[WS_SPECIES_FOX].plant_gain = 0.06;
        info[WS_SPECIES_FOX].maturity_s = 1.0 * 365.25 * 86400.0;
        info[WS_SPECIES_FOX].max_age_s = 5.0 * 365.25 * 86400.0;
        info[WS_SPECIES_FOX].repro_energy = 0.65;
        info[WS_SPECIES_FOX].repro_cost = 0.18;
        info[WS_SPECIES_FOX].repro_rate = 0.0018;
        info[WS_SPECIES_FOX].eco_maturity_s = 900.0; /* 15 min */
        info[WS_SPECIES_FOX].eco_lifespan_s = 1800.0;
        info[WS_SPECIES_FOX].juvenile_frac = 0.45;

        info[WS_SPECIES_RABBIT].name = "European rabbit (Oryctolagus cuniculus)";
        info[WS_SPECIES_RABBIT].card = "docs/life/TUR-KARTI-AVRUPA-TAVSANI.md";
        info[WS_SPECIES_RABBIT].body = rabbit_body();
        info[WS_SPECIES_RABBIT].diet = WS_DIET_HERBIVORE;
        info[WS_SPECIES_RABBIT].max_speed = 10.0;
        info[WS_SPECIES_RABBIT].sense_range = 40.0;
        info[WS_SPECIES_RABBIT].bite = 0.18;
        info[WS_SPECIES_RABBIT].basal_rate = 0.0025;
        info[WS_SPECIES_RABBIT].move_rate = 0.0025;
        info[WS_SPECIES_RABBIT].forage_gain = 0.70;
        info[WS_SPECIES_RABBIT].plant_gain = 0.70;
        info[WS_SPECIES_RABBIT].maturity_s = 0.5 * 365.25 * 86400.0;
        info[WS_SPECIES_RABBIT].max_age_s = 4.0 * 365.25 * 86400.0;
        info[WS_SPECIES_RABBIT].repro_energy = 0.55;
        info[WS_SPECIES_RABBIT].repro_cost = 0.20;
        info[WS_SPECIES_RABBIT].repro_rate = 0.0035;
        info[WS_SPECIES_RABBIT].eco_maturity_s = 450.0; /* 7.5 min */
        info[WS_SPECIES_RABBIT].eco_lifespan_s = 1350.0;
        info[WS_SPECIES_RABBIT].juvenile_frac = 0.50;

        info[WS_SPECIES_BOAR].name = "wild boar (Sus scrofa)";
        info[WS_SPECIES_BOAR].card = "docs/life/TUR-KARTI-YABAN-DOMUZU.md";
        info[WS_SPECIES_BOAR].body = boar_body();
        info[WS_SPECIES_BOAR].diet = WS_DIET_OMNIVORE;
        info[WS_SPECIES_BOAR].max_speed = 5.0;
        info[WS_SPECIES_BOAR].sense_range = 30.0;
        info[WS_SPECIES_BOAR].bite = 0.32;
        info[WS_SPECIES_BOAR].basal_rate = 0.0020;
        info[WS_SPECIES_BOAR].move_rate = 0.0020;
        info[WS_SPECIES_BOAR].forage_gain = 0.75;
        info[WS_SPECIES_BOAR].plant_gain = 0.75;
        info[WS_SPECIES_BOAR].maturity_s = 1.5 * 365.25 * 86400.0;
        info[WS_SPECIES_BOAR].max_age_s = 10.0 * 365.25 * 86400.0;
        info[WS_SPECIES_BOAR].repro_energy = 0.70;
        info[WS_SPECIES_BOAR].repro_cost = 0.28;
        info[WS_SPECIES_BOAR].repro_rate = 0.0022;
        info[WS_SPECIES_BOAR].eco_maturity_s = 900.0; /* 15 min */
        info[WS_SPECIES_BOAR].eco_lifespan_s = 1800.0;
        info[WS_SPECIES_BOAR].juvenile_frac = 0.50;

        ready = 1;
    }
    return info;
}

const ws_species_info *ws_species_get(ws_species_kind kind)
{
    if (kind < 0 || kind >= WS_SPECIES_COUNT) {
        return NULL;
    }
    return &table()[kind];
}

const char *ws_species_name(ws_species_kind kind)
{
    const ws_species_info *i = ws_species_get(kind);
    return (i != NULL) ? i->name : "unknown";
}

const char *ws_species_card(ws_species_kind kind)
{
    const ws_species_info *i = ws_species_get(kind);
    return (i != NULL) ? i->card : "";
}

double ws_species_walk_speed(ws_species_kind kind)
{
    switch (kind) {
    case WS_SPECIES_DEER: return 1.5;
    case WS_SPECIES_FOX:  return 1.8;
    case WS_SPECIES_RABBIT: return 2.0;
    case WS_SPECIES_BOAR: return 1.3;
    default:              return 1.5;
    }
}

int ws_species_prey(ws_species_kind kind)
{
    switch (kind) {
    case WS_SPECIES_FOX: return WS_SPECIES_RABBIT; /* fox hunts rabbit */
    default:             return -1;
    }
}
