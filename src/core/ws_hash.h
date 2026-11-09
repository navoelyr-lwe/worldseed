/* Worldseed Engine — SHA-256 (FIPS 180-4).
 *
 * Self-contained ISO C17 implementation with no external dependency. Used for
 * evidence checksums, golden-image fingerprints and release manifests. This is
 * a hash, not a password KDF.
 */
#ifndef WS_CORE_HASH_H
#define WS_CORE_HASH_H

#include <stddef.h>
#include <stdint.h>

#define WS_SHA256_DIGEST_SIZE 32u

typedef struct ws_sha256 {
    uint32_t state[8];
    uint64_t bit_length;
    uint8_t buffer[64];
    size_t buffer_length;
} ws_sha256;

void ws_sha256_init(ws_sha256 *ctx);
void ws_sha256_update(ws_sha256 *ctx, const void *data, size_t length);
void ws_sha256_final(ws_sha256 *ctx, uint8_t out_digest[WS_SHA256_DIGEST_SIZE]);

/* One-shot helper over a contiguous buffer. */
void ws_sha256_buffer(const void *data, size_t length,
                      uint8_t out_digest[WS_SHA256_DIGEST_SIZE]);

/* Write a digest as lowercase hex plus a terminating NUL.
 * out_hex must have room for 2 * WS_SHA256_DIGEST_SIZE + 1 bytes. */
void ws_sha256_hex(const uint8_t digest[WS_SHA256_DIGEST_SIZE], char *out_hex);

#endif /* WS_CORE_HASH_H */
