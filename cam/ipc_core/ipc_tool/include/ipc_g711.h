#ifndef __IPC_G711_H__
#define __IPC_G711_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc_std.h>

/**
 * @brief G711U is decoded into pcm
 *
 * @param dest Buffer that receives pcm, must be more than twice the length of src_len
 * @param src G711u data to be decoded
 * @param src_len The length of the g711u data
 * @return The length of the pcm after decoding
 */
EXAPI s32 ipc_g711u_decode(vptr dest, vptr src, s32 src_len);

/**
 * @brief Encode pcm into G711U
 *
 * @param dest Buffer that receives g711u, must be more than 1/2 of src_len
 * @param src Pcm data to be encoded
 * @param src_len The length of the pcm data
 * @return The length of the g711u after encoding
 */
EXAPI s32 ipc_g711u_encode(vptr dest, vptr src, s32 src_len);

/**
 * @brief G711A is decoded into pcm
 *
 * @param dest Buffer that receives pcm, must be more than twice the length of src_len
 * @param src G711a data to be decoded
 * @param src_len The length of the g711a data
 * @return The length of the pcm after decoding
 */
EXAPI s32 ipc_g711a_decode(vptr dest, vptr src, s32 src_len);

/**
 * @brief Encode pcm into G711A
 *
 * @param dest Buffer that receives g711a, must be more than 1/2 of src_len
 * @param src Pcm data to be encoded
 * @param src_len The length of the pcm data
 * @return The length of the g711a after encoding
 */
EXAPI s32 ipc_g711a_encode(vptr dest, vptr src, s32 src_len);

#ifdef __cplusplus
}
#endif

#endif //__IPC_G711_H__
