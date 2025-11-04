#include <stdio.h>
#include <string.h>

#include "ipc_hex_bin.h"
#include "internel/ipc_wpa.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

// SHA1 constants definition
#define SHA1_BLOCK_SIZE 64
#define SHA1_DIGEST_SIZE 20

// SHA1 context structure
typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t buffer[64];
} sha1_context_t;
// SHA1 transform function
static void sha1_transform(uint32_t state[5], const uint8_t buffer[64])
{
    uint32_t a, b, c, d, e, t, W[80];
    int i;
    for (i = 0; i < 16; i++) {
        W[i] = ((uint32_t)buffer[i * 4] << 24) | ((uint32_t)buffer[i * 4 + 1] << 16) | ((uint32_t)buffer[i * 4 + 2] << 8)
               | ((uint32_t)buffer[i * 4 + 3]);
    }
    for (i = 16; i < 80; i++) {
        W[i] = (W[i - 3] ^ W[i - 8] ^ W[i - 14] ^ W[i - 16]);
        W[i] = (W[i] << 1) | (W[i] >> 31);
    }
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    for (i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        t = ((a << 5) | (a >> 27)) + f + e + k + W[i];
        e = d;
        d = c;
        c = (b << 30) | (b >> 2);
        b = a;
        a = t;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

// SHA1 initialization
static void sha1_init(sha1_context_t* ctx)
{
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
    ctx->count[0] = ctx->count[1] = 0;
}

// SHA1 update
static void sha1_update(sha1_context_t* ctx, const uint8_t* data, size_t len)
{
    size_t i, j;
    j = (ctx->count[0] >> 3) & 63;
    if ((ctx->count[0] += len << 3) < (len << 3))
        ctx->count[1]++;
    ctx->count[1] += (len >> 29);
    if ((j + len) > 63) {
        memcpy(&ctx->buffer[j], data, (i = 64 - j));
        sha1_transform(ctx->state, ctx->buffer);
        for (; i + 63 < len; i += 64)
            sha1_transform(ctx->state, &data[i]);
        j = 0;
    } else
        i = 0;
    memcpy(&ctx->buffer[j], &data[i], len - i);
}

// SHA1 finalization
static void sha1_final(sha1_context_t* ctx, uint8_t digest[20])
{
    uint8_t finalcount[8];
    for (int i = 0; i < 8; i++)
        finalcount[i] = (uint8_t)((ctx->count[(i >= 4 ? 0 : 1)] >> ((3 - (i & 3)) * 8)) & 255);
    sha1_update(ctx, (uint8_t*)"\x80", 1);
    while ((ctx->count[0] & 504) != 448)
        sha1_update(ctx, (uint8_t*)"", 1);
    sha1_update(ctx, finalcount, 8);
    for (int i = 0; i < 20; i++)
        digest[i] = (uint8_t)((ctx->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255);
}

// HMAC-SHA1 implementation
static void hmac_sha1(const uint8_t* key, size_t key_len, const uint8_t* data, size_t data_len, uint8_t* mac)
{
    sha1_context_t ctx;
    uint8_t k_pad[SHA1_BLOCK_SIZE];
    uint8_t o_key_pad[SHA1_BLOCK_SIZE];
    uint8_t i_key_pad[SHA1_BLOCK_SIZE];
    uint8_t inner_hash[SHA1_DIGEST_SIZE];
    int i;

    // If key length is greater than block size, hash it first
    if (key_len > SHA1_BLOCK_SIZE) {
        sha1_init(&ctx);
        sha1_update(&ctx, key, key_len);
        sha1_final(&ctx, k_pad);
        key_len = SHA1_DIGEST_SIZE;
    } else {
        memcpy(k_pad, key, key_len);
    }

    // Pad key to block size
    if (key_len < SHA1_BLOCK_SIZE) {
        memset(&k_pad[key_len], 0, SHA1_BLOCK_SIZE - key_len);
    }

    // Calculate outer_key_pad and inner_key_pad
    for (i = 0; i < SHA1_BLOCK_SIZE; i++) {
        o_key_pad[i] = k_pad[i] ^ 0x5c;
        i_key_pad[i] = k_pad[i] ^ 0x36;
    }

    // Calculate inner hash: H(K XOR ipad, text)
    sha1_init(&ctx);
    sha1_update(&ctx, i_key_pad, SHA1_BLOCK_SIZE);
    sha1_update(&ctx, data, data_len);
    sha1_final(&ctx, inner_hash);

    // Calculate outer hash: H(K XOR opad, inner_hash)
    sha1_init(&ctx);
    sha1_update(&ctx, o_key_pad, SHA1_BLOCK_SIZE);
    sha1_update(&ctx, inner_hash, SHA1_DIGEST_SIZE);
    sha1_final(&ctx, mac);
}

// PBKDF2-HMAC-SHA1 implementation
#define MAX_SALT_SIZE 252 // Ensure salt_len + 4 does not exceed 256

static int pbkdf2_hmac_sha1(const uint8_t* password, size_t password_len, const uint8_t* salt, size_t salt_len, uint32_t iteration_count,
                            uint32_t key_length, uint8_t* output)
{
    uint8_t u1[SHA1_DIGEST_SIZE];
    uint8_t u2[SHA1_DIGEST_SIZE];
    uint8_t block1[SHA1_DIGEST_SIZE];
    uint8_t salt_with_block[MAX_SALT_SIZE + 4]; // Maximum buffer size
    uint32_t i, j, k;
    uint8_t block_counter[4];

    if (key_length == 0) {
        return 0;
    }

    // Check if salt length exceeds maximum limit
    if (salt_len > MAX_SALT_SIZE) {
        return -1; // Salt length too long
    }

    // Calculate number of blocks needed
    uint32_t blocks_needed = (key_length + SHA1_DIGEST_SIZE - 1) / SHA1_DIGEST_SIZE;

    for (i = 1; i <= blocks_needed; i++) {
        // Prepare salt || INT(i)
        memcpy(salt_with_block, salt, salt_len);
        // Use big-endian to store block counter
        block_counter[0] = (i >> 24) & 0xFF;
        block_counter[1] = (i >> 16) & 0xFF;
        block_counter[2] = (i >> 8) & 0xFF;
        block_counter[3] = i & 0xFF;
        memcpy(salt_with_block + salt_len, block_counter, 4);

        // Calculate U1 = HMAC(password, salt || INT(i))
        hmac_sha1(password, password_len, salt_with_block, salt_len + 4, u1);
        memcpy(block1, u1, SHA1_DIGEST_SIZE);

        // Calculate U2 to Uc
        for (j = 1; j < iteration_count; j++) {
            hmac_sha1(password, password_len, u1, SHA1_DIGEST_SIZE, u2);
            memcpy(u1, u2, SHA1_DIGEST_SIZE);

            // XOR with block1
            for (k = 0; k < SHA1_DIGEST_SIZE; k++) {
                block1[k] ^= u1[k];
            }
        }

        // Copy result to output
        uint32_t bytes_to_copy = (i == blocks_needed) ? (key_length - (i - 1) * SHA1_DIGEST_SIZE) : SHA1_DIGEST_SIZE;
        memcpy(output + (i - 1) * SHA1_DIGEST_SIZE, block1, bytes_to_copy);
    }

    return 0;
}

s32 ipc_wpa_get_password_psk(pv8 ssid, pv8 password, pv8 psk_buffer, s32 psk_buffer_size)
{
    unsigned char buffer[32] = { 0 };

    int ret = pbkdf2_hmac_sha1((const uint8_t*)password, strlen(password), (const uint8_t*)ssid, strlen(ssid), 4096, sizeof(buffer), buffer);
    if (ret != 0) {
        return -1;
    }

    return ipc_bin_to_hex(buffer, sizeof(buffer), psk_buffer, psk_buffer_size);
}