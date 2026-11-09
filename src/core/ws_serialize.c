/* Worldseed Engine — explicit little-endian byte writer/reader. */
#include "ws_serialize.h"

#include <string.h>

void ws_writer_init(ws_writer *w, void *buffer, size_t capacity)
{
    if (w == NULL) {
        return;
    }
    w->buffer = (uint8_t *)buffer;
    w->capacity = buffer != NULL ? capacity : 0u;
    w->offset = 0u;
}

size_t ws_writer_size(const ws_writer *w)
{
    return w != NULL ? w->offset : 0u;
}

static ws_result ws_writer_reserve(ws_writer *w, size_t length)
{
    if (w == NULL || w->buffer == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (length > w->capacity - w->offset) {
        return WS_ERR_CAPACITY;
    }
    return WS_OK;
}

ws_result ws_writer_u8(ws_writer *w, uint8_t value)
{
    ws_result r = ws_writer_reserve(w, 1u);
    if (r != WS_OK) {
        return r;
    }
    w->buffer[w->offset++] = value;
    return WS_OK;
}

ws_result ws_writer_u16(ws_writer *w, uint16_t value)
{
    ws_result r = ws_writer_reserve(w, 2u);
    if (r != WS_OK) {
        return r;
    }
    w->buffer[w->offset++] = (uint8_t)(value & 0xFFu);
    w->buffer[w->offset++] = (uint8_t)((value >> 8) & 0xFFu);
    return WS_OK;
}

ws_result ws_writer_u32(ws_writer *w, uint32_t value)
{
    ws_result r = ws_writer_reserve(w, 4u);
    if (r != WS_OK) {
        return r;
    }
    for (unsigned i = 0; i < 4u; ++i) {
        w->buffer[w->offset++] = (uint8_t)((value >> (8u * i)) & 0xFFu);
    }
    return WS_OK;
}

ws_result ws_writer_u64(ws_writer *w, uint64_t value)
{
    ws_result r = ws_writer_reserve(w, 8u);
    if (r != WS_OK) {
        return r;
    }
    for (unsigned i = 0; i < 8u; ++i) {
        w->buffer[w->offset++] = (uint8_t)((value >> (8u * i)) & 0xFFu);
    }
    return WS_OK;
}

ws_result ws_writer_f64(ws_writer *w, double value)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return ws_writer_u64(w, bits);
}

ws_result ws_writer_bytes(ws_writer *w, const void *data, size_t length)
{
    if (length > 0u && data == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    ws_result r = ws_writer_reserve(w, length);
    if (r != WS_OK) {
        return r;
    }
    if (length > 0u) {
        memcpy(&w->buffer[w->offset], data, length);
        w->offset += length;
    }
    return WS_OK;
}

void ws_reader_init(ws_reader *r, const void *buffer, size_t length)
{
    if (r == NULL) {
        return;
    }
    r->buffer = (const uint8_t *)buffer;
    r->length = buffer != NULL ? length : 0u;
    r->offset = 0u;
}

size_t ws_reader_remaining(const ws_reader *r)
{
    if (r == NULL || r->offset > r->length) {
        return 0u;
    }
    return r->length - r->offset;
}

static ws_result ws_reader_take(ws_reader *r, size_t length, const uint8_t **out)
{
    if (r == NULL || r->buffer == NULL || out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (length > ws_reader_remaining(r)) {
        return WS_ERR_PARSE;
    }
    *out = r->buffer + r->offset;
    r->offset += length;
    return WS_OK;
}

ws_result ws_reader_u8(ws_reader *r, uint8_t *out)
{
    if (out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    const uint8_t *p = NULL;
    ws_result res = ws_reader_take(r, 1u, &p);
    if (res != WS_OK) {
        return res;
    }
    *out = p[0];
    return WS_OK;
}

ws_result ws_reader_u16(ws_reader *r, uint16_t *out)
{
    if (out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    const uint8_t *p = NULL;
    ws_result res = ws_reader_take(r, 2u, &p);
    if (res != WS_OK) {
        return res;
    }
    *out = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
    return WS_OK;
}

ws_result ws_reader_u32(ws_reader *r, uint32_t *out)
{
    if (out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    const uint8_t *p = NULL;
    ws_result res = ws_reader_take(r, 4u, &p);
    if (res != WS_OK) {
        return res;
    }
    uint32_t value = 0u;
    for (unsigned i = 0; i < 4u; ++i) {
        value |= (uint32_t)p[i] << (8u * i);
    }
    *out = value;
    return WS_OK;
}

ws_result ws_reader_u64(ws_reader *r, uint64_t *out)
{
    if (out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    const uint8_t *p = NULL;
    ws_result res = ws_reader_take(r, 8u, &p);
    if (res != WS_OK) {
        return res;
    }
    uint64_t value = 0u;
    for (unsigned i = 0; i < 8u; ++i) {
        value |= (uint64_t)p[i] << (8u * i);
    }
    *out = value;
    return WS_OK;
}

ws_result ws_reader_f64(ws_reader *r, double *out)
{
    if (out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    uint64_t bits = 0u;
    ws_result res = ws_reader_u64(r, &bits);
    if (res != WS_OK) {
        return res;
    }
    memcpy(out, &bits, sizeof(*out));
    return WS_OK;
}

ws_result ws_reader_bytes(ws_reader *r, void *out, size_t length)
{
    if (length > 0u && out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    const uint8_t *p = NULL;
    ws_result res = ws_reader_take(r, length, &p);
    if (res != WS_OK) {
        return res;
    }
    if (length > 0u) {
        memcpy(out, p, length);
    }
    return WS_OK;
}
