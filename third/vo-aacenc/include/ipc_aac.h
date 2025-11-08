/**
 * @file ipc_aac.h
 * @author your name (you@domain.com)
 * @brief AAC audio codec interface
 * @version 0.1
 * @date 2022-03-29
 *
 * @copyright Copyright (c) 2022
 * Only supports ADTS AAC, does not support ADIF AAC
 */

#ifndef __IPC_AAC_H__
#define __IPC_AAC_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <ipc_std.h>

typedef struct {
    vptr buf;   ///< Data buffer
    s32  len;   ///< Data length
} ipc_aac_data_t, *ipc_aac_data_p;

typedef struct {
    s32 channel;
    s32 sample_rate;
    s32 bit_width;
} ipc_aac_info_t, *ipc_aac_info_p;

#define IPC_AAC_ADTS_HEAD_SIZE 7

/***************************** encoder ************************/
/**
 * @brief Open PCM -> AAC encoder
 *
 * @param bit_width   Bit width
 * @param sample_rate Sample rate
 * @param channel     Channel count (currently the modified faac library only supports 1 channel, if expansion is needed, the faac dictionary needs to be changed)
 * @return AAC encoder handle
 */
EXAPI vptr ipc_aac_encode_open(s32 bit_width, s32 sample_rate, s32 channel);

/**
 * @brief Close PCM -> AAC encoder
 *
 * @param handle Handle initialized through ipc_aac_encode_open
 */
EXAPI void ipc_aac_encode_close(vptr handle);

/**
 * @brief Iterative encoding
 *
 * @param handle   Handle initialized through ipc_aac_encode_open
 * @param pcm_data PCM data buffer
 * @param pcm_len  PCM data length
 * @return NULL: No encoded data available temporarily  Non-NULL: Encoded data and its length
 * @note The returned ipc_aac_data_p points to the handle's buffer, no need to free
 * @note This function is in iterator form, generally should be called within a while loop
 * @note The reason for iterator form is that when PCM single data volume is large, it will be divided into multiple iterative encoding data for return
 */
EXAPI ipc_aac_data_p ipc_aac_encode_iter(vptr handle, vptr pcm_data, s32 pcm_len);

/***************************** decoder ************************/
/**
 * @brief Open AAC -> PCM decoder
 *
 * @return AAC decoder handle
 */
EXAPI vptr ipc_aac_decode_open(void);

/**
 * @brief Close AAC -> PCM decoder
 *
 * @param handle Handle initialized through ipc_aac_decode_open
 */
EXAPI void ipc_aac_decode_close(vptr handle);

/**
 * @brief Iterative decoding
 *
 * @param handle   Handle initialized through ipc_aac_decode_open
 * @param aac_data AAC data input buffer
 * @param aac_len  AAC data input length
 * @param info     AAC audio output information
 * @return NULL: No decoded data available temporarily  Non-NULL: Decoded data and its length
 * @note The returned ipc_aac_data_p points to the handle's buffer, no need to free
 * @note This function is in iterator form, generally should be called within a while loop
 * @note The reason for iterator form is that when AAC single data volume is large, it will be divided into multiple iterative decoding data for return
 */
EXAPI ipc_aac_data_p ipc_aac_decode_iter(vptr handle, vptr aac_data, s32 aac_len, ipc_aac_info_p info);

/**
 * @brief Calculate corresponding frame length based on AAC header data information
 *
 * @param aac_data AAC data
 * @return Corresponding frame length
 * @note Data length must at least have the ADTS header length
 * @note This interface does not validate AAC data, meaning users need to ensure the passed data is a correct AAC data header
 */
EXAPI u32 ipc_aac_frame_len(pu8 aac_data);

#ifdef __cplusplus
}
#endif

#endif //__IPC_AAC_H__