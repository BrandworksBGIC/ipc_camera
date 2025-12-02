#ifndef __ipc_SHA256_H__
#define __ipc_SHA256_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc_std.h>


/* SHA-256 context structure */
typedef struct {
    u32 h[8];           /* Hash state (8 * 32-bit) */
    u8 buffer[64];      /* Input buffer (512-bit blocks) */
    u64 total_length;   /* Total message length in bits */
    u32 buffer_length;  /* Current buffer length in bytes */
} ipc_sha256_ctx_t;

/* SHA-256 API functions */
/**
 * @brief Initialize SHA-256 context
 * @param ctx Pointer to SHA-256 context
 */
void ipc_sha256_init(ipc_sha256_ctx_t* ctx);

/**
 * @brief Update SHA-256 context with new data
 * @param ctx Pointer to SHA-256 context
 * @param data Pointer to input data
 * @param len Length of input data in bytes
 */
void ipc_sha256_update(ipc_sha256_ctx_t* ctx, const u8* data, u64 len);

/**
 * @brief Finalize SHA-256 calculation and get hash result
 * @param ctx Pointer to SHA-256 context
 * @param hash Pointer to output buffer (32 bytes)
 */
void ipc_sha256_final(ipc_sha256_ctx_t* ctx, u8* hash);

/**
 * @brief Calculate SHA-256 hash in one step
 * @param data Pointer to input data
 * @param len Length of input data in bytes
 * @param hash Pointer to output buffer (32 bytes)
 */
void ipc_sha256(const u8* data, u64 len, u8* hash);

#ifdef __cplusplus
}
#endif

#endif /* __ipc_SHA256_H__ */