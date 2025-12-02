#include "ipc_sha256.h"
#include <string.h>

/* SHA-256 constants and round constants */
static const u32 K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

/* SHA-256 initial hash values */
static const u32 H0[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

/* Bit manipulation macros */
#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define SHR(x, n)  ((x) >> (n))
#define Ch(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define Maj(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define Sigma0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define Sigma1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define sigma0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ SHR(x, 3))
#define sigma1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ SHR(x, 10))

/* Convert between host byte order and big-endian */
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define BYTE_SWAP32(x) \
    (((x) >> 24) | (((x) >> 8) & 0xff00) | (((x) << 8) & 0xff0000) | ((x) << 24))
#else
#define BYTE_SWAP32(x) (x)
#endif

/* Initialize SHA-256 context */
void ipc_sha256_init(ipc_sha256_ctx_t* ctx)
{
    if (!ctx) return;

    /* Copy initial hash values */
    memcpy(ctx->h, H0, sizeof(H0));

    /* Initialize other fields */
    ctx->total_length = 0;
    ctx->buffer_length = 0;
    memset(ctx->buffer, 0, sizeof(ctx->buffer));
}

/* Process one 512-bit block */
static void process_block(ipc_sha256_ctx_t* ctx, const u8* block)
{
    u32 w[64];
    u32 a, b, c, d, e, f, g, h;
    u32 t1, t2;
    int i;

    /* Prepare message schedule */
    for (i = 0; i < 16; i++) {
        memcpy(&w[i], block + i * 4, 4);
        w[i] = BYTE_SWAP32(w[i]);
    }

    for (i = 16; i < 64; i++) {
        w[i] = sigma1(w[i - 2]) + w[i - 7] + sigma0(w[i - 15]) + w[i - 16];
    }

    /* Initialize working variables */
    a = ctx->h[0];
    b = ctx->h[1];
    c = ctx->h[2];
    d = ctx->h[3];
    e = ctx->h[4];
    f = ctx->h[5];
    g = ctx->h[6];
    h = ctx->h[7];

    /* Main compression function */
    for (i = 0; i < 64; i++) {
        t1 = h + Sigma1(e) + Ch(e, f, g) + K[i] + w[i];
        t2 = Sigma0(a) + Maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    /* Update hash values */
    ctx->h[0] += a;
    ctx->h[1] += b;
    ctx->h[2] += c;
    ctx->h[3] += d;
    ctx->h[4] += e;
    ctx->h[5] += f;
    ctx->h[6] += g;
    ctx->h[7] += h;
}

/* Update SHA-256 context with new data */
void ipc_sha256_update(ipc_sha256_ctx_t* ctx, const u8* data, u64 len)
{
    u64 i = 0;

    if (!ctx || !data) return;

    /* Update total length */
    ctx->total_length += len;

    /* Process existing buffer data if any */
    if (ctx->buffer_length > 0) {
        u64 copy_len = (len < (64 - ctx->buffer_length)) ? len : (64 - ctx->buffer_length);
        memcpy(ctx->buffer + ctx->buffer_length, data, copy_len);
        ctx->buffer_length += copy_len;
        data += copy_len;
        len -= copy_len;
        i = copy_len;

        /* If buffer is full, process it */
        if (ctx->buffer_length == 64) {
            process_block(ctx, ctx->buffer);
            ctx->buffer_length = 0;
        }
    }

    /* Process complete blocks */
    while (len >= 64) {
        process_block(ctx, data);
        data += 64;
        len -= 64;
        i += 64;
    }

    /* Copy remaining data to buffer */
    if (len > 0) {
        memcpy(ctx->buffer, data, len);
        ctx->buffer_length = (u32)len;
    }
}

/* Finalize SHA-256 calculation and get hash result */
void ipc_sha256_final(ipc_sha256_ctx_t* ctx, u8* hash)
{
    u64 bit_length;
    u8 padding[128];
    u32 pad_len;
    int i;

    if (!ctx || !hash) return;

    /* Calculate total message length in bits */
    bit_length = ctx->total_length * 8;

    /* Prepare padding */
    memset(padding, 0, sizeof(padding));
    padding[0] = 0x80; /* Append '1' bit */

    /* Calculate padding length */
    if (ctx->buffer_length < 56) {
        pad_len = 56 - ctx->buffer_length;
    } else {
        pad_len = 120 - ctx->buffer_length;
    }

    /* Append padding */
    ipc_sha256_update(ctx, padding, pad_len);

    /* Append length (64-bit big-endian) */
    u8 length_bytes[8];
    for (i = 0; i < 8; i++) {
        length_bytes[i] = (u8)(bit_length >> (56 - i * 8));
    }
    ipc_sha256_update(ctx, length_bytes, 8);

    /* Convert hash to bytes (big-endian) */
    for (i = 0; i < 8; i++) {
        u32 h_val = ctx->h[i];
        hash[i*4] = (u8)(h_val >> 24);
        hash[i*4+1] = (u8)(h_val >> 16);
        hash[i*4+2] = (u8)(h_val >> 8);
        hash[i*4+3] = (u8)h_val;
    }
}

/* Calculate SHA-256 hash in one step */
void ipc_sha256(const u8* data, u64 len, u8* hash)
{
    ipc_sha256_ctx_t ctx;

    ipc_sha256_init(&ctx);
    ipc_sha256_update(&ctx, data, len);
    ipc_sha256_final(&ctx, hash);
}