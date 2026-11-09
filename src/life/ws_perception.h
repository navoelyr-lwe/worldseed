/* Worldseed Engine — organism perception (P6).
 *
 * A small, deterministic, headless sensory model for the reference organism
 * (docs/07). It answers one question per tick: which resource or threat does
 * the animal currently sense, and where? Sensing is derived only from the
 * world state (positions of food, water and threats plus the animal's own body
 * pose) and from the animal's internal state (energy, hydration, temperature,
 * fatigue). It never reads the camera, the window or any UI, so a behaviour
 * decided from perception is identical whether or not anything is being
 * watched; that property is asserted by the P6 tests.
 *
 * The model is deliberately coarse: a range, a field of view and a "nearest
 * visible" selection per sense channel. It is a defensible level of detail for
 * a utility-based behaviour layer and is recorded as such in the model card;
 * retinal resolution, occlusion and hearing are later work.
 *
 * All quantities are SI: positions metres, angles radians, temperature degrees
 * Celsius. Storage is caller-owned; nothing is allocated.
 */
#ifndef WS_LIFE_PERCEPTION_H
#define WS_LIFE_PERCEPTION_H

#include <stdint.h>

#include "../core/ws_math.h"
#include "../core/ws_result.h"

#define WS_PERCEPTION_VERSION 1u

/* Sense channels and the kind of world point that feeds them. */
typedef enum ws_sense_kind {
    WS_SENSE_FOOD = 0,
    WS_SENSE_WATER = 1,
    WS_SENSE_THREAT = 2,
    WS_SENSE_KIND_COUNT = 3
} ws_sense_kind;

/* A sensed world point: a food patch, a water source or a threat. `value` is a
 * quality/amount in [0,1] (richness of a food patch, threat severity). */
typedef struct ws_sense_point {
    double x;      /* m */
    double y;      /* m */
    double value;  /* 0..1 */
    uint8_t kind;  /* ws_sense_kind */
} ws_sense_point;

typedef struct ws_perception_config {
    double range; /* m, maximum sensing distance */
    double fov;   /* rad, total horizontal field of view */
} ws_perception_config;

ws_perception_config ws_perception_config_default(void);

/* One channel's result for the current tick. */
typedef struct ws_perception_target {
    int seen;        /* 1 when a target is visible this tick */
    double dist;     /* m to target (0 when not seen) */
    double bearing;  /* rad, target angle relative to heading, [-pi, pi] */
    double wx, wy;   /* m, world position of the target (0 when not seen) */
    double value;    /* 0..1 */
} ws_perception_target;

/* Internal-state snapshot mirrored into perception, so decision code can read
 * one struct. These are copies; the organism owns the authoritative values. */
typedef struct ws_perception_internal {
    double energy;      /* 0..1 fraction of reserve */
    double hydration;   /* 0..1 fraction of reserve */
    double body_temp_c; /* degrees C */
    double fatigue;     /* 0..1 index */
} ws_perception_internal;

typedef struct ws_perception {
    ws_perception_target food;
    ws_perception_target water;
    ws_perception_target threat;
    ws_perception_internal internal;
} ws_perception;

/* Fill the perception for the current tick. `heading` is the animal's path
 * yaw (rad); points are world positions. Internal scalars are copied through.
 * Any pointer may be NULL (treated as "nothing sensed"). Deterministic: ties in
 * distance are broken by lower array index, and iteration order is fixed. */
void ws_perception_update(ws_perception *out, const ws_perception_config *cfg,
                          double px, double py, double heading,
                          const ws_sense_point *points, uint32_t count,
                          const ws_perception_internal *internal);

#endif /* WS_LIFE_PERCEPTION_H */
