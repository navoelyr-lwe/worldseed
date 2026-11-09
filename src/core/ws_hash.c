/* Worldseed Engine — SHA-256 (FIPS 180-4). */
#include "ws_hash.h"

#include <string.h>

static const uint32_t WS_SHA256_K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

static uint32_t ws_rotr(uint32_t x, unsigned n)
{
    return (x >> n) | (x << (32u - n));
}

static void ws_sha256_transform(ws_sha256 *ctx, const uint8_t block[64])
{
    uint32_t w[64];
    for (unsigned i = 0; i < 16u; ++i) {
        w[i] = ((uint32_t)block[i * 4u] << 24) |
               ((uint32_t)block[i * 4u + 1u] << 16) |
               ((uint32_t)block[i * 4u + 2u] << 8) |
               ((uint32_t)block[i * 4u + 3u]);
    }
    for (unsigned i = 16u; i < 64u; ++i) {
        uint32_t s0 = ws_rotr(w[i - 15u], 7u) ^ ws_rotr(w[i - 15u], 18u) ^ (w[i - 15u] >> 3);
        uint32_t s1 = ws_rotr(w[i - 2u], 17u) ^ ws_rotr(w[i - 2u], 19u) ^ (w[i - 2u] >> 10);
        w[i] = w[i - 16u] + s0 + w[i - 7u] + s1;
    }

    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];
    uint32_t e = ctx->state[4];
    uint32_t f = ctx->state[5];
    uint32_t g = ctx->state[6];
    uint32_t h = ctx->state[7];

    for (unsigned i = 0; i < 64u; ++i) {
        uint32_t s1 = ws_rotr(e, 6u) ^ ws_rotr(e, 11u) ^ ws_rotr(e, 25u);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + ch + WS_SHA256_K[i] + w[i];
        uint32_t s0 = ws_rotr(a, 2u) ^ ws_rotr(a, 13u) ^ ws_rotr(a, 22u);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

void ws_sha256_init(ws_sha256 *ctx)
{
    ctx->state[0] = 0x6a09e667u;
    ctx->state[1] = 0xbb67ae85u;
    ctx->state[2] = 0x3c6ef372u;
    ctx->state[3] = 0xa54ff53au;
    ctx->state[4] = 0x510e527fu;
    ctx->state[5] = 0x9b05688cu;
    ctx->state[6] = 0x1f83d9abu;
    ctx->state[7] = 0x5be0cd19u;
    ctx->bit_length = 0u;
    ctx->buffer_length = 0u;
}

void ws_sha256_update(ws_sha256 *ctx, const void *data, size_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    ctx->bit_length += (uint64_t)length * 8u;
    while (length > 0u) {
        size_t space = 64u - ctx->buffer_length;
        size_t take = length < space ? length : space;
        memcpy(&ctx->buffer[ctx->buffer_length], bytes, take);
        ctx->buffer_length += take;
        bytes += take;
        length -= take;
        if (ctx->buffer_length == 64u) {
            ws_sha256_transform(ctx, ctx->buffer);
            ctx->buffer_length = 0u;
        }
    }
}

void ws_sha256_final(ws_sha256 *ctx, uint8_t out_digest[WS_SHA256_DIGEST_SIZE])
{
    uint64_t bit_length = ctx->bit_length;
    uint8_t pad = 0x80u;
    ws_sha256_update(ctx, &pad, 1u);

    uint8_t zero = 0x00u;
    while (ctx->buffer_length != 56u) {
        ws_sha256_update(ctx, &zero, 1u);
    }

    uint8_t length_bytes[8];
    for (unsigned i = 0; i < 8u; ++i) {
        length_bytes[i] = (uint8_t)((bit_length >> (56u - 8u * i)) & 0xFFu);
    }
    ws_sha256_update(ctx, length_bytes, 8u);

    for (unsigned i = 0; i < 8u; ++i) {
        out_digest[i * 4u] = (uint8_t)((ctx->state[i] >> 24) & 0xFFu);
        out_digest[i * 4u + 1u] = (uint8_t)((ctx->state[i] >> 16) & 0xFFu);
        out_digest[i * 4u + 2u] = (uint8_t)((ctx->state[i] >> 8) & 0xFFu);
        out_digest[i * 4u + 3u] = (uint8_t)(ctx->state[i] & 0xFFu);
    }
}

void ws_sha256_buffer(const void *data, size_t length,
                      uint8_t out_digest[WS_SHA256_DIGEST_SIZE])
{
    ws_sha256 ctx;
    ws_sha256_init(&ctx);
    ws_sha256_update(&ctx, data, length);
    ws_sha256_final(&ctx, out_digest);
}

void ws_sha256_hex(const uint8_t digest[WS_SHA256_DIGEST_SIZE], char *out_hex)
{
    static const char hex[] = "0123456789abcdef";
    for (unsigned i = 0; i < WS_SHA256_DIGEST_SIZE; ++i) {
        out_hex[i * 2u] = hex[(digest[i] >> 4) & 0x0Fu];
        out_hex[i * 2u + 1u] = hex[digest[i] & 0x0Fu];
    }
    out_hex[WS_SHA256_DIGEST_SIZE * 2u] = '\0';
}
