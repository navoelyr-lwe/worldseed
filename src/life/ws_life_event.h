/* Worldseed Engine — life events (P9).
 *
 * Discrete, observable things that happen in the living world: an animal eats,
 * drinks, is born, dies, is killed, or makes a kill. They are produced by the
 * simulation as it runs (ws_ecology) and are the only source the cinematic
 * director is allowed to observe. The director never invents one.
 *
 * Encoding: a life event is a WS_EVENT_LIFE (src/world/ws_event.h) whose
 * payload[0] is a ws_life_event_kind and whose remaining bytes are fixed
 * little-endian fields. This keeps the event type shared and pointer-free so it
 * journals and replays deterministically.
 *
 *   offset 0   kind (1 byte)
 *   offset 1   actor (uint32 LE)      species or actor index, kind-dependent
 *   offset 5   subject (uint32 LE)    prey species for a kill, else 0
 *   offset 9   x (float LE)           world position, metres
 *   offset 13  y (float LE)
 *   offset 17  value (float LE)       energy 0..1, or 0
 *   offset 21  tick (uint32 LE)       world tick the event happened on
 *
 * SI units and seconds.
 */
#ifndef WS_LIFE_EVENT_H
#define WS_LIFE_EVENT_H

#include <stdint.h>

typedef enum ws_life_event_kind {
    WS_LIFE_EV_ATE = 0,     /* a herbivore/omnivore grazed plant biomass */
    WS_LIFE_EV_BORN = 1,    /* a birth */
    WS_LIFE_EV_DIED = 2,    /* a death (age or starvation) */
    WS_LIFE_EV_KILLED = 3   /* a predator killed prey (subject = prey species) */
} ws_life_event_kind;

/* Only kinds the P7 ecosystem actually produces are listed. There is no water
 * layer in the patch, so no drinking event exists to observe; the director
 * never invents one (docs/09 "olay yaratmaz"). */

#define WS_LIFE_EVENT_PAYLOAD_SIZE 25u

const char *ws_life_event_kind_name(uint8_t kind);

/* Encode a life event into `payload` (at least WS_LIFE_EVENT_PAYLOAD_SIZE
 * bytes). Returns the number of bytes written. */
uint16_t ws_life_event_encode(uint8_t *payload, uint8_t kind, uint32_t actor,
                              uint32_t subject, float x, float y, float value,
                              uint32_t tick);

/* Decode a life event payload. Any out pointer may be NULL. Returns 1 on
 * success, 0 when the payload kind is not a valid life event kind. */
int ws_life_event_decode(const uint8_t *payload, uint16_t payload_size,
                         uint8_t *out_kind, uint32_t *out_actor,
                         uint32_t *out_subject, float *out_x, float *out_y,
                         float *out_value, uint32_t *out_tick);

/* True when `kind` is a valid life event kind. */
int ws_life_event_kind_valid(uint8_t kind);

#endif /* WS_LIFE_EVENT_H */
