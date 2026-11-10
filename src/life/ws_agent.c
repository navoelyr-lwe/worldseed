/* Worldseed Engine — organism behaviour and limited learning (P6). */
#include "ws_agent.h"

#include "../core/ws_math.h"

#include <math.h>
#include <string.h>

ws_agent_config ws_agent_config_default(void)
{
    ws_agent_config c;
    c.perception = ws_perception_config_default();
    c.organism = ws_organism_config_default();
    c.max_speed = 1.5;
    c.turn_rate = 2.0;
    c.arrive_radius = 1.5;
    c.eat_range = 1.2;
    c.drink_range = 1.2;
    c.flee_distance = 25.0;
    c.flee_persist_s = 1.5;
    c.need_threshold = 0.35;
    c.explore_threshold = 0.15;
    c.bite_size = 0.20;
    c.sip_size = 0.25;
    c.wander_lookahead = 12.0;
    c.learning_enabled = 1;
    return c;
}

const char *ws_behavior_name(int behavior)
{
    switch (behavior) {
    case WS_BEHAVIOR_REST: return "rest";
    case WS_BEHAVIOR_SEEK_FOOD: return "seek_food";
    case WS_BEHAVIOR_SEEK_WATER: return "seek_water";
    case WS_BEHAVIOR_FLEE: return "flee";
    case WS_BEHAVIOR_WANDER: return "wander";
    default: return "unknown";
    }
}

static double wrap_pi(double a)
{
    const double pi = 3.14159265358979323846;
    const double two_pi = 2.0 * pi;
    a = fmod(a + pi, two_pi);
    if (a < 0.0) {
        a += two_pi;
    }
    return a - pi;
}

static void memory_push(ws_agent *a, uint16_t kind, double x, double y, double value)
{
    ws_agent_memory_event *e = &a->mem[a->mem_head];
    e->tick = a->tick;
    e->kind = kind;
    e->x = x;
    e->y = y;
    e->value = value;
    a->mem_head = (a->mem_head + 1u) % WS_AGENT_MEMORY_CAP;
    if (a->mem_count < WS_AGENT_MEMORY_CAP) {
        a->mem_count += 1u;
    }
}

uint32_t ws_agent_memory_count(const ws_agent *a)
{
    return (a != NULL) ? a->mem_count : 0u;
}

ws_result ws_agent_memory_at(const ws_agent *a, uint32_t index,
                             ws_agent_memory_event *out)
{
    if (a == NULL || out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (index >= a->mem_count) {
        return WS_ERR_NOT_FOUND;
    }
    uint32_t start = (a->mem_count < WS_AGENT_MEMORY_CAP) ? 0u : a->mem_head;
    *out = a->mem[(start + index) % WS_AGENT_MEMORY_CAP];
    return WS_OK;
}

void ws_agent_init(ws_agent *a, const ws_agent_config *config, uint64_t seed,
                   double x, double y, double heading)
{
    if (a == NULL) {
        return;
    }
    memset(a, 0, sizeof(*a));
    a->config = (config != NULL) ? *config : ws_agent_config_default();
    ws_organism_init(&a->org, &a->config.organism);
    ws_rng_init(&a->rng, seed, 0x1006u);
    a->x = x;
    a->y = y;
    a->heading = heading;
    a->behavior = WS_BEHAVIOR_WANDER;
    a->perc.internal.energy = a->org.energy;
    a->perc.internal.hydration = a->org.hydration;
    a->perc.internal.body_temp_c = a->org.body_temp_c;
    a->perc.internal.fatigue = a->org.fatigue;
}

/* Consume `amount` from the sensed point whose world position matches (x, y),
 * reducing its value; returns the amount removed. A point at zero value is left
 * in place (the world layer regrows it) but is skipped by perception. */
static double deplete_point(ws_sense_point *points, uint32_t count,
                            uint8_t kind, double x, double y, double amount)
{
    for (uint32_t i = 0; i < count; ++i) {
        ws_sense_point *p = &points[i];
        if (p->kind != kind) {
            continue;
        }
        if (fabs(p->x - x) > 1e-6 || fabs(p->y - y) > 1e-6) {
            continue;
        }
        double taken = (amount < p->value) ? amount : p->value;
        p->value -= taken;
        if (p->value < 0.0) {
            p->value = 0.0;
        }
        return taken;
    }
    return 0.0;
}

ws_result ws_agent_step(ws_agent *a, double dt, ws_sense_point *points,
                        uint32_t *count, double ambient_temp_c)
{
    if (a == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (!(dt > 0.0)) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    uint32_t n = (count != NULL) ? *count : 0u;
    const ws_agent_config *cfg = &a->config;

    /* 1. Perceive the world from the current body pose and internal state. */
    a->perc.internal.energy = a->org.energy;
    a->perc.internal.hydration = a->org.hydration;
    a->perc.internal.body_temp_c = a->org.body_temp_c;
    a->perc.internal.fatigue = a->org.fatigue;
    ws_perception_update(&a->perc, &cfg->perception, a->x, a->y, a->heading,
                         points, n, &a->perc.internal);

    /* 2. Choose a behaviour from drives and perception. */
    int resting = 0;
    int behavior;
    int has_target = 0;
    double tx = 0.0;
    double ty = 0.0;
    double speed = 0.0;

    if (a->perc.threat.seen && a->perc.threat.dist < cfg->flee_distance) {
        /* Latch the flight direction away from the threat and keep fleeing for a
         * short while after the threat leaves the field of view, the way a real
         * animal does not instantly calm down. */
        a->alarm_s = cfg->flee_persist_s;
        double dx = a->x - a->perc.threat.wx;
        double dy = a->y - a->perc.threat.wy;
        double len = sqrt(dx * dx + dy * dy);
        if (len < 1e-9) {
            dx = 1.0;
            dy = 0.0;
            len = 1.0;
        }
        a->flee_dir_x = dx / len;
        a->flee_dir_y = dy / len;
        memory_push(a, WS_AGENT_EVENT_FLED, a->x, a->y, a->perc.threat.value);
    }

    if (a->alarm_s > 0.0) {
        a->alarm_s -= dt;
        behavior = WS_BEHAVIOR_FLEE;
        tx = a->x + a->flee_dir_x * cfg->wander_lookahead;
        ty = a->y + a->flee_dir_y * cfg->wander_lookahead;
        has_target = 1;
        speed = cfg->max_speed;
    } else {
        int urgent = ws_organism_most_urgent(&a->org, cfg->need_threshold);
        if (urgent == WS_NEED_REST) {
            behavior = WS_BEHAVIOR_REST;
            resting = 1;
        } else if (urgent == WS_NEED_ENERGY) {
            behavior = WS_BEHAVIOR_SEEK_FOOD;
        } else if (urgent == WS_NEED_WATER) {
            behavior = WS_BEHAVIOR_SEEK_WATER;
        } else if (cfg->learning_enabled && a->org.hunger > cfg->explore_threshold &&
                   a->has_food_mem) {
            behavior = WS_BEHAVIOR_SEEK_FOOD;
        } else if (cfg->learning_enabled && a->org.thirst > cfg->explore_threshold &&
                   a->has_water_mem) {
            behavior = WS_BEHAVIOR_SEEK_WATER;
        } else {
            behavior = WS_BEHAVIOR_WANDER;
        }

        if (behavior == WS_BEHAVIOR_SEEK_FOOD) {
            if (a->perc.food.seen) {
                tx = a->perc.food.wx;
                ty = a->perc.food.wy;
                has_target = 1;
            } else if (cfg->learning_enabled && a->has_food_mem) {
                tx = a->food_mem_x;
                ty = a->food_mem_y;
                has_target = 1;
            } else {
                behavior = WS_BEHAVIOR_WANDER;
            }
        } else if (behavior == WS_BEHAVIOR_SEEK_WATER) {
            if (a->perc.water.seen) {
                tx = a->perc.water.wx;
                ty = a->perc.water.wy;
                has_target = 1;
            } else if (cfg->learning_enabled && a->has_water_mem) {
                tx = a->water_mem_x;
                ty = a->water_mem_y;
                has_target = 1;
            } else {
                behavior = WS_BEHAVIOR_WANDER;
            }
        }

        if (behavior == WS_BEHAVIOR_WANDER) {
            /* Walk ahead with a small random heading offset; keep the target on
             * the ground plane so the animal keeps searching. */
            double jitter = (ws_rng_next_double(&a->rng) - 0.5) * 0.6;
            double dir = a->heading + jitter;
            tx = a->x + cos(dir) * cfg->wander_lookahead;
            ty = a->y + sin(dir) * cfg->wander_lookahead;
            has_target = 1;
            speed = cfg->max_speed * 0.4;
        } else if (behavior == WS_BEHAVIOR_SEEK_FOOD ||
                   behavior == WS_BEHAVIOR_SEEK_WATER) {
            speed = cfg->max_speed;
        }
    }

    /* 3. Steer toward the target (bounded turn rate) and advance. */
    if (has_target) {
        double dx = tx - a->x;
        double dy = ty - a->y;
        double dist = sqrt(dx * dx + dy * dy);
        double desired = atan2(dy, dx);
        double err = wrap_pi(desired - a->heading);
        double max_turn = cfg->turn_rate * dt;
        if (err > max_turn) {
            err = max_turn;
        } else if (err < -max_turn) {
            err = -max_turn;
        }
        a->heading = wrap_pi(a->heading + err);
        /* Slow down on final approach so the animal can stop on the resource. */
        if (dist < cfg->arrive_radius) {
            speed *= dist / cfg->arrive_radius;
        }
    }

    a->speed = speed;
    a->x += cos(a->heading) * speed * dt;
    a->y += sin(a->heading) * speed * dt;
    a->distance_travelled += speed * dt;

    /* 4. Interact: eat or drink when in range of a sensed resource. */
    if (a->perc.food.seen && a->perc.food.dist <= cfg->eat_range && points != NULL) {
        double taken = deplete_point(points, n, WS_SENSE_FOOD, a->perc.food.wx,
                                     a->perc.food.wy, cfg->bite_size);
        if (taken > 0.0) {
            double absorbed = ws_organism_eat(&a->org, taken);
            a->has_food_mem = 1;
            a->food_mem_x = a->perc.food.wx;
            a->food_mem_y = a->perc.food.wy;
            memory_push(a, WS_AGENT_EVENT_ATE, a->perc.food.wx, a->perc.food.wy, absorbed);
            a->food_found += 1u;
        }
    }
    if (a->perc.water.seen && a->perc.water.dist <= cfg->drink_range && points != NULL) {
        double taken = deplete_point(points, n, WS_SENSE_WATER, a->perc.water.wx,
                                     a->perc.water.wy, cfg->sip_size);
        if (taken > 0.0) {
            double absorbed = ws_organism_drink(&a->org, taken);
            a->has_water_mem = 1;
            a->water_mem_x = a->perc.water.wx;
            a->water_mem_y = a->perc.water.wy;
            memory_push(a, WS_AGENT_EVENT_DRANK, a->perc.water.wx, a->perc.water.wy, absorbed);
            a->water_found += 1u;
        }
    }

    /* 5. Homeostasis: the chosen behaviour's cost feeds back into the state. */
    (void)ws_organism_step(&a->org, dt, speed, ambient_temp_c, resting);

    a->behavior = behavior;
    a->has_target = has_target;
    a->target_x = tx;
    a->target_y = ty;
    a->tick += 1u;
    a->time_seconds += dt;
    return WS_OK;
}
