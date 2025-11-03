/*
 * Copyright (c) 2022 Realtek Semiconductor Corp. All rights reserved.
 *
 * SPDX-License-Identifier: LicenseRef-Realtek-Proprietary
 *
 * This software component is confidential and proprietary to Realtek
 * Semiconductor Corp. Disclosure, reproduction, redistribution, in whole
 * or in part, of this work and its derivatives without express permission
 * is prohibited.
 */

 /**
  * @file rtsavapi.h
  * @brief RTStream API reference
  * @defgroup core RTStream Common API
  * @brief RTStream global API
  * @{
  */

#ifndef _INC_RTSAVAPI_H
#define _INC_RTSAVAPI_H

#include <rtsavdef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @example example_change_fps.c
 * @example example_change_resolution.c
 * @example example_isp_h264_jpeg.c
 * @example example_isp_h265_jpeg.c
 */

/**
 * @brief Init RTStream library
 * @attention This API must be called before any RTStream API, and it should be called only once.
 * @return 0 : success, minus : fail
 */
int rts_av_init(void);

/**
 * @brief Release RTStream library
 * @attention This API should called at last, corresponding to \ref rts_av_init.
 * @return 0 : success, minus : fail
 */
int rts_av_release(void);

/**
 * @brief New a \ref rts_av_buffer object
 * @param[in] length
 * if length > 0, alloc memory and assign the address to vm_addr;\n
 * if length = 0, will not alloc memory.
 * @attention every \ref rts_av_buffer should obtained by This API,
 *  and released by \ref rts_av_delete_buffer. If length is zero,
 *  \ref rts_av_init_buffer should be followed to initialize.
 * @return pointer of the buffer when success NULL: fail
 */
struct rts_av_buffer *rts_av_new_buffer(uint32_t length);

/**
 * @brief Delete a \ref rts_av_buffer object
 * @param[in] buffer pointer of the buffer, get from rts_av_new_buffer
 * @attention If \ref rts_av_new_buffer is called with the parameter length
 *  is zero, then \ref rts_av_init_buffer is called to initialize ,\n
 *  it needs to call \ref rts_av_uninit_buffer
 *  for uninitialize, then call \ref rts_av_delete_buffer for releasing.
 * @return 0 : success, minus : fail
 */
int rts_av_delete_buffer(struct rts_av_buffer *buffer);

/**
 * @brief Initialize custom buffer
 * @param[in] buffer pointer of the buffer, get from rts_av_new_buffer
 * @param[in] vm_addr virtual address to be assigned for param buffer
 * @param[in] length length to be assigned to param buffer
 * @param[in] phy_addr physical address to be assigned to param buffer
 * @attention The buffer which is not initialized, only needs to
 *  call this interface for initialization when creating,
 *  that is when \ref rts_av_new_buffer is called with length equals to zero.
 * @return 0 : success, minus : fail
 */
int rts_av_init_buffer(struct rts_av_buffer *buffer,
			void *vm_addr, uint32_t length, uint32_t phy_addr);

/**
 * @brief Uninitialize custom buffer
 * @param[in] buffer pointer of the buffer, get from \ref rts_av_new_buffer
 * @attention The interface should appear in pairs with \ref rts_av_init_buffer
 * @return 0 : success, minus : fail
 */
int rts_av_uninit_buffer(struct rts_av_buffer *buffer);

/**
 * @brief Increase reference count for buffer
 * @param[in] buffer pointer of the buffer, get from \ref rts_av_new_buffer
 * @return pointer of the buffer NULL: fail
 */
struct rts_av_buffer *rts_av_get_buffer(struct rts_av_buffer *buffer);

/**
 * @brief Reduce reference count for buffer
 * @param[in] buffer pointer of the buffer, get from \ref rts_av_new_buffer
 * @attention When the reference count is reduced to 0,
 *  and the recycle callback function of the buffer is set,
 *  then the callback function will be called.\n
 *  The callback function is set by \ref rts_av_set_buffer_callback.
 * @return pointer of the buffer NULL: fail
 */
int rts_av_put_buffer(struct rts_av_buffer *buffer);

/**
 * @brief Get the reference count of the buffer
 * @param[in] buffer pointer of the buffer, get from \ref rts_av_new_buffer
 * @return reference count of the buffer 0: fail
 */
unsigned int rts_av_get_buffer_refs(struct rts_av_buffer *buffer);

/**
 * @brief Set address offset for buffer
 * @param[in] buffer pointer of the buffer, get from \ref rts_av_new_buffer
 * @param[in] offset address offset
 * @return 0 : success, minus : fail
 */
int rts_av_set_buffer_offset(struct rts_av_buffer *buffer, uint32_t offset);

typedef void (*buffer_handle)(void *master, struct rts_av_buffer *buffer);

/**
 * @brief Set callback function when recycle buffer
 * @param[in] buffer pointer of the buffer, get from \ref rts_av_new_buffer
 * @param[in] master pointer used to pass to the callback function
 * @param[in] cb pointer of buffer_handle
 * @attention The recycle callback function will be called,
 *  when the buffer reference count is reduced to zero.
 * @return 0 : success, minus : fail
 */
int rts_av_set_buffer_callback(struct rts_av_buffer *buffer,
				void *master, buffer_handle cb);

/**
 * @brief Set buffer profile
 * @param[in] buffer pointer of the buffer, get from \ref rts_av_new_buffer
 * @param[in] profile pointer of \ref rts_av_profile
 * @attention For buffer sent through \ref rts_av_send, the profile
 *  should be setted by this API, and the profile should be
 *  consistent with the profile of the channel.
 * @return 0 : success, minus : fail
 */
int rts_av_set_buffer_profile(struct rts_av_buffer *buffer,
			struct rts_av_profile *profile);

/**
 * @brief Get buffer profile
 * @param[in] buffer pointer of the buffer, get from \ref rts_av_new_buffer
 * @param[out] profile pointer of \ref rts_av_profile
 * @return 0 : success, minus : fail
 */
int rts_av_get_buffer_profile(struct rts_av_buffer *buffer,
			struct rts_av_profile *profile);

int rts_av_create_chn(unsigned int id, void *arg);

/**
 * @brief Destroy channel
 * @param[in] chnno channel number obtained when create channel
 * @return 0 : success, minus : fail
 */
int rts_av_destroy_chn(unsigned int chnno);

/**
 * @brief Get id of the channel.
 * Each Channel has an id, each id indicates the type of Channel and
 * what function is provided.
 * @param[in] chnno channel number obtained when create channel
 * @return id of the channel
 */
int rts_av_get_id(unsigned int chnno);

/**
 * @brief Get type of the channel.
 * Types of Channels supported by RTStream are as follows:
 * Channel Type | Description
 * --------------| -------------
 * UNIT_TYPE_SOURCE(1) | Only output, no input. It can only be connected with other channel as input
 * UNIT_TYPE_SINK(2) | Only input, no output. It can only be connected with other channel as output
 * UNIT_TYPE_FILTER(3) | There is both input and output. It can be used as input or as output to connect with other channel
 * @param[in] chnno channel number obtained when create channel
 * @return type of the channel
 */
int rts_av_get_type(unsigned int chnno);

/**
 * @brief Get channel status(whether enable or disable).
 * @param[in] chnno channel number obtained when create channel
 * @return 0 : disable, 1 : enable
 */
int rts_av_get_chn_status(unsigned int chnno);

/**
 * @brief Enable channel.
 * @param[in] chnno channel number obtained when create channel
 * @return 0 : success, minus : fail
 */
int rts_av_enable_chn(unsigned int chnno);

/**
 * @brief Disable channel.
 * @param[in] chnno channel number obtained when create channel
 * @return 0 : success, minus : fail
 */
int rts_av_disable_chn(unsigned int chnno);

/**
 * @brief Establish a connection between two channels
 * @param[in] src channel number of source
 * @param[in] dst channel number of destination
 * @attention When two channels have been binded together,
 *  the output profile of source channel will serve as the
 *  input profile of destination channel. If destination
 *  channel does not support this profile, it will return
 *  fail.\n If destination channel supports connecting to
 *  multiple source channel, the output profile of multiple
 *  source channel must be consistent.
 * @return 0 : success, minus : fail
 */
int rts_av_bind(unsigned int src, unsigned int dst);

/**
 * @brief Unlink two channels
 * @param[in] src channel number of source
 * @param[in] dst channel number of destination
 * @return 0 : success, minus : fail
 */
int rts_av_unbind(unsigned int src, unsigned int dst);

/**
 * @brief Get number of input channels of the channel
 * @param[in] chnno channel number of source
 * @return number of input channels of the channel
 */
int rts_av_get_src_count(unsigned int chnno);

/**
 * @brief Get number of output channels of the channel
 * @param[in] chnno channel number of source
 * @return number of output channels of the channel
 */
int rts_av_get_dst_count(unsigned int chnno);

/**
 * @brief Get the input channel of Channel
 * @param[in] chnno channel number
 * @param[in] index specify to get which one
 * @return number of input channels to get
 */
int rts_av_get_bind_by_src(unsigned int chnno, int index);

/**
 * @brief Get the output channel of Channel
 * @param[in] chnno channel number
 * @param[in] index specify to get which one
 * @return number of output channels to get
 */
int rts_av_get_bind_by_dst(unsigned int chnno, int index);

/**
 * @brief let channel start to receive data
 * @param[in] chnno channel number
 * @attention After this API called, the number of output channels
 * connected with this channel will plus one.
 * @return 0 : success, minus : fail
 */
int rts_av_start_recv(unsigned int chnno);

/**
 * @brief let channel stop to receive data
 * @param[in] chnno channel number
 * @attention It needs to appear in pairs with \ref rts_av_start_recv
 * @return 0 : success, minus : fail
 */
int rts_av_stop_recv(unsigned int chnno);

/**
 * @brief Allow data to be sent to the channel
 * @param[in] chnno channel number
 * @attention After this function is executed,
 * the number of source of the channel will plus one.
 * @return 0 : success, minus : fail
 */
int rts_av_start_send(unsigned int chnno);

/**
 * @brief Stop to send data to the channel
 * @param[in] chnno channel number
 * @attention It needs to appear in pairs with \ref rts_av_start_send
 * @return 0 : success, minus : fail
 */
int rts_av_stop_send(unsigned int chnno);

/**
 * @brief Send data to channel
 * @param[in] chnno channel number
 * @param[in] buffer pointer of \ref rts_av_buffer
 * @attention You need to invoke \ref rts_av_start_send firstly
 * @return 0 : success, minus : fail
 */
int rts_av_send(unsigned int chnno, struct rts_av_buffer *buffer);

/**
 * @brief Receive data of the channel
 * @param[in] chnno channel number
 * @param[out] ppbuf pointer of the address of \ref rts_av_buffer
 * @attention After taking out the data in the buffer,
 * you need to invoke \ref rts_av_put_buffer to return the \ref rts_av_buffer
 * pointed by the output parameter ppbuf.\n You need to invoke \ref rts_av_start_recv before this API.
 * @return 0 : success, minus : fail
 */
int rts_av_recv(unsigned int chnno, struct rts_av_buffer **ppbuf);

/**
 * @brief Poll and receive data of the channel by block method
 * @param[in] chnno channel number
 * @param[out] ppbuf pointer of the address of \ref rts_av_buffer
 * @param[in] timeout_ms timeout of block in unit of msec:\n
 * timeout_ms > 0, poll in block until timeout;\n
 * timeout_ms = 0, return at once;\n
 * timeout_ms < 0, poll in block until recv data.
 * @attention After taking out the data in the buffer,
 * you need to invoke \ref rts_av_put_buffer to return the \ref rts_av_buffer
 * pointed by the output parameter ppbuf.\n You need to
 * invoke \ref rts_av_start_recv before this API.
 * @return 0 : success, minus : fail
 */
int rts_av_recv_block(unsigned int chnno,
			struct rts_av_buffer **ppbuf, int timeout_ms);
/**
 * @brief Check if data is ready
 * @param[in] chnno channel number
 * @attention Return immediately. If there is no data, please recheck it after waiting
 * @return 0 : ready, minus : not ready
 */
int rts_av_poll(unsigned int chnno);

/**
 * @brief Set callback function for channel
 * @param[in] chnno channel number
 * @param[in] cb pointer of \ref rts_av_callback
 * @param[in] before
 * if before = 0, call callback function after the channel running;\n
 * if before = 1, call callback function before the chnannel running.
 * @return 0 : success, minus : fail
 */
int rts_av_set_callback(unsigned int chnno, struct rts_av_callback *cb,
			int before);

/**
 * @brief Get channel profile
 * @param[in] chnno channel number
 * @param[out] profile pointer of \ref rts_av_profile
 * @return 0 : success, minus : fail
 */
int rts_av_get_profile(unsigned int chnno, struct rts_av_profile *profile);

/**
 * @brief Set channel profile
 * @param[in] chnno channel number
 * @param[in] profile pointer of \ref rts_av_profile
 * @return 0 : success, minus : fail
 */
int rts_av_set_profile(unsigned int chnno, struct rts_av_profile *profile);

/**
 * @brief Change channel fps
 * @param[in] chnno channel number
 * @param[in] fps pointer of \ref rts_fract
 * @note when vin mode of channel is RTS_AV_VIN_RING_MODE \ref RTS_AV_VIN_MODE,
 * setting fps maybe not effective, actual fps equals sensor's framerate
 * @return 0 : success, minus : fail
 */
int rts_av_change_fps(unsigned int chnno, struct rts_fract *fps);
int rts_av_config_outbuf_info(unsigned int chnno,
		struct rts_av_buffer **buffer, int num_buffers);
int rts_av_get_outbuf_info(unsigned int chnno, int idx,
		unsigned long *virt, unsigned long *phy, int *len);

/**
 * @brief Check if channel is idle
 * @param[in] chnno channel number
 * @return 1 : idle, 0: not idle
 */
int rts_av_is_idle(unsigned int chnno);

int rts_av_list_chns(void);

/**
 * @brief Set upper limit of waiting buffer for the Channel input queue.
 * @param[in] chnno channel number
 * @param[in] limit upper limit of waiting buffer
 * @attention This must called after channel binded, if channels unbind
 * and bind again,it should be setted again.
 * @return 0 : success, minus : fail
 */
int rts_av_set_waiting_limit(unsigned int chnno, long limit);

/**
 * @brief Set skip info for the Channel input buffer.
 * @param[in] chnno channel number
 * @param[in] numerator indicates how many buffers are skipped in a statistical period,\n
 * default: 0, if numerator = 0, it means not to skip
 * @param[in] denominator indicates the size of a statistical period
 * @attention This must called after channel binded, if channels unbind
 * and bind again,it should be setted again.
 * @return 0 : success, minus : fail
 */
int rts_av_set_input_skip_info(unsigned int chnno,
			uint32_t numerator, uint32_t denominator);
/*!@}*/

#ifdef __cplusplus
}
#endif
#endif
