#ifndef __IPC_RESAMPLE_H__
#define __IPC_RESAMPLE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc_std.h>

/**
 * @brief Resampling
 * @note It is about to be abandoned and should not be used as much as possible
 */
EXAPI s32 ipc_resample_pcms16(vptr src_data, s32 src_len, s32 channels, s32 src_sample_rate, s32 dst_sample_rate,
                             vptr dst_buff);

typedef struct {
    s32 src_sample_rate;
    s32 dst_sample_rate;
    s32 channels;
    s32 skip_len;
    s16 energy[2]; // The energy of the last sample of each channel in the last frame, currently only supports recording
                   // up to two channels
    u64 cur_offset;
} ipc_resample_t, *ipc_resample_p;

/**
 * @brief Initialize audio resampling for pcm s16(le/be)
 *
 * @param resample The handle object to be initialized
 * @param channels The number of audio channels, currently supports up to 2 channels
 * @param src_sample_rate Original pcm sampling rate
 * @param dst_sample_rate Target pcm sampling rate
 * @return Standard return value in ipc_std.h
 */
EXAPI s32 ipc_resample_pcms16_init(ipc_resample_p resample, s32 channels, s32 src_sample_rate, s32 dst_sample_rate);

/**
 * @brief Perform iterative resampling on pcm s16(le/be) audio
 *
 * @param resample The handle object initialized by ipc_resample_pcms16_init
 * @param src_data Original pcm data
 * @param src_len Original pcm length
 * @param dst_buff Target pcm output buff
 * @return <0: Standard return value in ipc_std.h >=0: Length of resampled pcm
 * @note The size of the dst_buff needs to be calculated by the outside world. The difference between the two sampling
 * rates is rounded up. More redundancy is preferred for the size
 */
EXAPI s32 ipc_resample_pcms16_iter(ipc_resample_p resample, vptr src_data, s32 src_len, vptr dst_buff);

#ifdef __cplusplus
}
#endif

#endif //__IPC_RESAMPLE_H__
