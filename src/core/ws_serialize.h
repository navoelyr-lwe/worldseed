/* Worldseed Engine — explicit little-endian byte writer/reader.
 *
 * Disk and IPC never store raw C structs. All persistent data is written field
 * by field through these helpers so that layout, endianness and sizes are
 * explicit and stable across compilers and platforms.
 */
#ifndef WS_CORE_SERIALIZE_H
#define WS_CORE_SERIALIZE_H

#include <stddef.h>
#include <stdint.h>

#include "ws_result.h"

typedef struct ws_writer {
    uint8_t *buffer;
    size_t capacity;
    size_t offset;
} ws_writer;

typedef struct ws_reader {
    const uint8_t *buffer;
    size_t length;
    size_t offset;
} ws_reader;

void ws_writer_init(ws_writer *w, void *buffer, size_t capacity);
size_t ws_writer_size(const ws_writer *w);
ws_result ws_writer_u8(ws_writer *w, uint8_t value);
ws_result ws_writer_u16(ws_writer *w, uint16_t value);
ws_result ws_writer_u32(ws_writer *w, uint32_t value);
ws_result ws_writer_u64(ws_writer *w, uint64_t value);
ws_result ws_writer_f64(ws_writer *w, double value);
ws_result ws_writer_bytes(ws_writer *w, const void *data, size_t length);

void ws_reader_init(ws_reader *r, const void *buffer, size_t length);
size_t ws_reader_remaining(const ws_reader *r);
ws_result ws_reader_u8(ws_reader *r, uint8_t *out);
ws_result ws_reader_u16(ws_reader *r, uint16_t *out);
ws_result ws_reader_u32(ws_reader *r, uint32_t *out);
ws_result ws_reader_u64(ws_reader *r, uint64_t *out);
ws_result ws_reader_f64(ws_reader *r, double *out);
ws_result ws_reader_bytes(ws_reader *r, void *out, size_t length);

#endif /* WS_CORE_SERIALIZE_H */
