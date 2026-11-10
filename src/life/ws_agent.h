/* Worldseed Engine — organism behaviour and limited learning (P6).
 *
 * The agent turns perception and internal drives into action (docs/07 "Davranis
 * sistemi" and "Ogrenme ve hafiza"). It is a utility-based decision layer: every
 * tick it scores the organism's needs, checks for a threat, and selects one of a
 * small, explicit set of behaviours (rest, seek food, seek water, flee, wander).
 * The choice is deterministic and explainable; there is no hidden state a
 * watcher could influence.
 *
 * Memory and limited learning: the agent keeps a short event memory (recent
 * significant events with their world position) and a spatial memory of the
 * last place food and water were found. When learning is enabled it prefers a
 * remembered site that has paid off before instead of re-searching at random,
 * and its forage success rate over repeated episodes improves against a control
 * with memory disabled. This is a deliberately small, measurable adaptation; no
 * claim of intelligence or consciousness is made (docs/07).
 *
 * The agent is kinematic: it owns a path position and heading and moves at a
 * speed it chooses. Coupling the path to the P5 sagittal body is done by the
 * scene bridge; the behaviour experiments run headless and never touch the
 * camera or the window, so behaviour is identical whether or not anything is
 * rendered (asserted by the P6 tests).
 *
 * Storage is caller-owned; nothing is allocated.
 */
#ifndef WS_LIFE_AGENT_H
#define WS_LIFE_AGENT_H

#include <stdint.h>

#include "../core/ws_result.h"
#include "../core/ws_rng.h"
#include "ws_organism.h"
#include "ws_perception.h"

#define WS_AGENT_VERSION 1u
#define WS_AGENT_MEMORY_CAP 32u

typedef enum ws_behavior {
    WS_BEHAVIOR_REST = 0,
    WS_BEHAVIOR_SEEK_FOOD = 1,
    WS_BEHAVIOR_SEEK_WATER = 2,
    WS_BEHAVIOR_FLEE = 3,
    WS_BEHAVIOR_WANDER = 4
} ws_behavior;

const char *ws_behavior_name(int behavior);

/* Kinds recorded in the short event memory. */
typedef enum ws_agent_event {
    WS_AGENT_EVENT_ATE = 0,
    WS_AGENT_EVENT_DRANK = 1,
    WS_AGENT_EVENT_FLED = 2,
    WS_AGENT_EVENT_RESTED = 3,
    WS_AGENT_EVENT_THREAT_SEEN = 4
} ws_agent_event;

typedef struct ws_agent_memory_event {
    uint64_t tick;
    uint16_t kind; /* ws_agent_event */
    double x;
    double y;
    double value;
} ws_agent_memory_event;

typedef struct ws_agent_config {
    ws_perception_config perception;
    ws_organism_config organism;
    double max_speed;       /* m/s */
    double turn_rate;       /* rad/s */
    double arrive_radius;   /* m, distance at which a target counts as reached */
    double eat_range;       /* m, distance at which food can be eaten */
    double drink_range;     /* m, distance at which water can be drunk */
    double flee_distance;   /* m, threat distance that triggers flight */
    double flee_persist_s;  /* s, flight continues this long after losing sight */
    double need_threshold;  /* drive at which a need is acted on directly */
    double explore_threshold; /* lower drive at which a remembered site is used */
    double bite_size;       /* energy fraction absorbed per full-value bite */
    double sip_size;        /* hydration fraction absorbed per full-value sip */
    double wander_lookahead;/* m, wander target distance ahead */
    int learning_enabled;   /* 1 to use spatial memory to revisit good sites */
} ws_agent_config;

ws_agent_config ws_agent_config_default(void);

typedef struct ws_agent {
    ws_agent_config config;
    ws_organism org;
    ws_perception perc;
    ws_rng rng;             /* per-agent wander stream */

    double x;
    double y;
    double heading;         /* rad */
    double speed;           /* m/s, last commanded */

    int behavior;           /* ws_behavior */
    int has_target;
    double target_x;
    double target_y;

    /* Spatial memory of the last productive site. */
    int has_food_mem;
    double food_mem_x;
    double food_mem_y;
    int has_water_mem;
    double water_mem_x;
    double water_mem_y;

    /* Short event memory (ring). */
    ws_agent_memory_event mem[WS_AGENT_MEMORY_CAP];
    uint32_t mem_head;
    uint32_t mem_count;

    uint64_t tick;
    double time_seconds;
    double distance_travelled;
    uint32_t food_found;
    uint32_t water_found;
    double alarm_s;         /* s of flight remaining after losing sight */
    double flee_dir_x;      /* last known threat direction to flee along */
    double flee_dir_y;
} ws_agent;

/* Initialise at (x, y) facing `heading`, with a wander stream drawn from
 * `seed`. */
void ws_agent_init(ws_agent *a, const ws_agent_config *config, uint64_t seed,
                   double x, double y, double heading);

/* Advance one tick of `dt` seconds. `points` is the mutable world resource
 * list; the agent may consume (remove) a point it reaches, in which case
 * *count is decremented. Passing count == 0 or points == NULL means nothing is
 * sensed. Returns WS_OK, or WS_ERR_INVALID_ARGUMENT for a non-positive dt. */
ws_result ws_agent_step(ws_agent *a, double dt, ws_sense_point *points,
                        uint32_t *count, double ambient_temp_c);

uint32_t ws_agent_memory_count(const ws_agent *a);
/* index 0 is the oldest retained event. */
ws_result ws_agent_memory_at(const ws_agent *a, uint32_t index,
                             ws_agent_memory_event *out);

#endif /* WS_LIFE_AGENT_H */
