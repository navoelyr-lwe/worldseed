/* Worldseed Engine — life event encoding (P9). */
#include "ws_life_event.h"

#include <string.h>

const char *ws_life_event_kind_name(uint8_t kind)
{
    switch (kind) {
    case WS_LIFE_EV_ATE:    return "ate";
    case WS_LIFE_EV_BORN:   return "born";
    case WS_LIFE_EV_DIED:   return "died";
    case WS_LIFE_EV_KILLED: return "killed";
    default:                return "unknown";
    }
}

int ws_life_event_kind_valid(uint8_t kind)
{
    return kind <= WS_LIFE_EV_KILLED;
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void put_f32(uint8_t *p, float f)
{
    uint32_t v;
    memcpy(&v, &f, 4);
    put_u32(p, v);
}

static float get_f32(const uint8_t *p)
{
    uint32_t v = get_u32(p);
    float f;
    memcpy(&f, &v, 4);
    return f;
}

uint16_t ws_life_event_encode(uint8_t *payload, uint8_t kind, uint32_t actor,
                              uint32_t subject, float x, float y, float value,
                              uint32_t tick)
{
    if (payload == NULL) {
        return 0u;
    }
    memset(payload, 0, WS_LIFE_EVENT_PAYLOAD_SIZE);
    payload[0] = kind;
    put_u32(&payload[1], actor);
    put_u32(&payload[5], subject);
    put_f32(&payload[9], x);
    put_f32(&payload[13], y);
    put_f32(&payload[17], value);
    put_u32(&payload[21], tick);
    return WS_LIFE_EVENT_PAYLOAD_SIZE;
}

int ws_life_event_decode(const uint8_t *payload, uint16_t payload_size,
                         uint8_t *out_kind, uint32_t *out_actor,
                         uint32_t *out_subject, float *out_x, float *out_y,
                         float *out_value, uint32_t *out_tick)
{
    if (payload == NULL || payload_size < WS_LIFE_EVENT_PAYLOAD_SIZE) {
        return 0;
    }
    if (!ws_life_event_kind_valid(payload[0])) {
        return 0;
    }
    if (out_kind != NULL) {
        *out_kind = payload[0];
    }
    if (out_actor != NULL) {
        *out_actor = get_u32(&payload[1]);
    }
    if (out_subject != NULL) {
        *out_subject = get_u32(&payload[5]);
    }
    if (out_x != NULL) {
        *out_x = get_f32(&payload[9]);
    }
    if (out_y != NULL) {
        *out_y = get_f32(&payload[13]);
    }
    if (out_value != NULL) {
        *out_value = get_f32(&payload[17]);
    }
    if (out_tick != NULL) {
        *out_tick = get_u32(&payload[21]);
    }
    return 1;
}
