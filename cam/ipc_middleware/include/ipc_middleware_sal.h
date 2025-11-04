/**
 * @file ipc_middleware_sal.h
 * @author ouyang
 * @brief SAL (Software Abstraction Layer), i.e., software abstraction layer.
 *        This layer provides functions for internal modules to call, unlike callbacks which are typically bound to a specific module for its
 * exclusive use. The SAL provides functions for all internal modules to call as part of a normal layered architecture, akin to setting global
 * callback functions, but with enforced implementation compared to global callbacks.
 * @version 1.0
 * @date 2021-08-25
 *
 * @copyright Copyright (c) 2021
 *
 */

#ifndef __IPC_MIDDLEWARE_SAL_H__
#define __IPC_MIDDLEWARE_SAL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc_core.h>
#include <ipc_platform_api.h>

/************************ business *****************************/

/**
 * @brief Interface for obtaining media information when interfacing with a cloud platform (consolidates all required stream media configurations into
 * this function, including both internal and external cloud platforms, so that both can call this function).
 *
 * @param audio_attr Final audio attribute settings //TODO Ideally should also expand to set video attributes
 * @return vptr Singleton property needed by the external cloud platform, specific structure depends on the platform (not needed internally, external
 * cloud platforms may choose to implement it or not).
 */
EXAPI vptr ipc_middleware_sal_media_info(struct ipc_plat_audio_init_attr* audio_attr);

typedef enum {
    IPC_MIDDLEWARE_SAL_CLARITY_HD, ///< High Definition
    IPC_MIDDLEWARE_SAL_CLARITY_SD, ///< Standard Definition
    IPC_MIDDLEWARE_SAL_CLARITY_NUM,
} ipc_middleware_sal_clarity_e;

typedef struct {
    vptr data;    ///< Stream media data
    s32 len;      ///< Length of stream media data
    s64 tms;      ///< Timestamp of the stream
    u8 is_audio;  ///< 1: Audio 0: Video
    u8 key_frame; ///< Whether it's a key frame (including certain audio formats)
} ipc_middleware_sal_get_media_frame_t, *ipc_middleware_sal_get_media_frame_p;

/**
 * @brief Generic stream retrieval queue interface (adheres to ipc_core ipc_iter standard).
 *
 * @param h_iter ipc_iter standard handle
 * @param locate_tms <=0: Desired pre-recording time >0: Locate data at a specific timestamp (if not feasible, try to achieve seamless continuation
 * with the previous recording) (Note: only needs to be effective during initialization)
 * @param clarity Type of channel for stream retrieval (real-time updates)
 * @param frame Retrieved frame data
 * @return ipc_iter standard return value, IPC_ITER_CONTINUE: Continue IPC_ITER_BREAK: Exit
 * @note Normally, the function should wait until a stream is retrieved before returning.
 * @note h_iter->ret = Actual return value
 */
EXAPI s32 ipc_middleware_sal_get_media_iter(ipc_iter_p h_iter, s64 locate_tms, ipc_middleware_sal_clarity_e clarity, ipc_middleware_sal_get_media_frame_p frame);

/************************ main *****************************/

/**
 * @brief Main function for the final business program combining IPC with the cloud (for standardization, implemented externally, called during
 * firmware generation).
 *
 * @param ipc_version IPC firmware version
 * @return Program return value
 */
EXAPI s32 ipc_middleware_main_process(pv8 ipc_version);

#ifdef __cplusplus
}
#endif

#endif //__IPC_MIDDLEWARE_SAL_H__