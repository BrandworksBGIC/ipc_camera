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

/** \file
 * \brief RTStream common API
 * \addtogroup core
 * \{
 */
#ifndef _INC_RTSAVDEF_H
#define _INC_RTSAVDEF_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <rtscolor.h>
#include <rtsdef.h>
#include <stdbool.h>

#define rts_av_fourcc(a, b, c, d)\
	((uint32_t)(a) | ((uint32_t)(b) << 8) | \
	((uint32_t)(c)<<16) | ((uint32_t)((d) & 0x7f)<<24))

#define RTS_AV_FMT_AUDIO		0x8000

/**
 * @brief video/audio format
 */
enum RTS_AV_FMT {
	RTS_AV_FMT_UNDEFINED = 0x0, /**< Undefined format */
	RTS_V_FMT_YUYV = RTS_PIX_FMT_YUYV, /**< YUYV */
	RTS_V_FMT_YUV420PLANAR = RTS_PIX_FMT_YUV420M, /**< YUV420 planar */
	RTS_V_FMT_YUV420SEMIPLANAR = RTS_PIX_FMT_NV12, /**< YUV420 semiplanar */
	RTS_V_FMT_YUV422SEMIPLANAR = RTS_PIX_FMT_NV16, /**< YUV422 semiplanar */
	RTS_V_FMT_MJPEG = RTS_PIX_FMT_MJPEG, /**< MJPEG */
	RTS_V_FMT_H264 = RTS_PIX_FMT_H264, /**< H.264 */
	RTS_V_FMT_H265 = RTS_PIX_FMT_H265, /**< H.265 */
	RTS_V_FMT_RGB = RTS_PIX_FMT_RGB888, /**< RGB888 */
	RTS_V_FMT_BGR = RTS_PIX_FMT_BGR888, /**< BGR888 */
	RTS_V_FMT_Y_ONLY = RTS_PIX_FMT_GRAY_8, /**< GRAY8 */
	RTS_V_FMT_YVYU = RTS_PIX_FMT_YVYU, /**< YVYU */

	RTS_A_FMT_AUDIO = RTS_AV_FMT_AUDIO | 0x1, /**< PCM */
	RTS_A_FMT_ULAW = RTS_A_FMT_AUDIO + 2, /**< ULAW */
	RTS_A_FMT_ALAW, /**< ALAW */
	RTS_A_FMT_G726, /**< G726 */
};

/**
 * @brief rotation description, RTS_AV_VIN_MODE low-latency mode
 * dose not support rotate operation
 */
enum RTS_AV_ROTATION {
	RTS_AV_ROTATION_0 = 0, /**< rotation 0 degree */
	RTS_AV_ROTATION_90R = 1, /**< rotation right 90 degree */
	RTS_AV_ROTATION_90L = 2, /**< rotation left 90 degree */
	RTS_AV_ROTATION_180 = 3, /**< rotation 180 degree, same as RTS_AV_MIRROR_HOR_VER */
};

/**
 * @brief mirror description, RTS_AV_VIN_MODE low-latency mode
 * dose not support mirror operation
 */
enum RTS_AV_MIRROR {
	RTS_AV_MIRROR_NO = 0, /**< no mirror */
	RTS_AV_MIRROR_VER, /**< mirror vertical */
	RTS_AV_MIRROR_HOR, /**< mirror horizontal */
	RTS_AV_MIRROR_HOR_VER, /**< mirror horizontal and vertical, same as RTS_AV_ROTATION_180 */
};

/**
 * @brief MJPEG stream mode description
 */
enum RTS_AV_JPG_STREAM_MODE {
	RTS_AV_JPG_NO_TRIGGER = 0, /**< no trigger mode */
	RTS_AV_JPG_TRIGGER, /**< trigger mode */
};

/**
 * @brief video-in mode description, relation between vin_mode and vin_id,
 * low-latency mode only support no mirror mode
 */
enum RTS_AV_VIN_MODE {
	RTS_AV_VIN_FRAME_MODE = 0, /**< Normal mode, all vins support */
	RTS_AV_VIN_RING_MODE, /**< H26x low-latency mode, vin-0 support */
	RTS_AV_VIN_DIRECT_MODE, /**< Mjpeg low-latency mode, vin-0&1 support */
};

enum RTS_AV_ABILITY_TYPE {
	RTS_AV_ABILITY_UNDEFINED = 0,
	RTS_AV_ABILITY_V4L2,
	RTS_AV_ABILITY_ISP,
	RTS_AV_ABILITY_AUDIO,
	RTS_AV_ABILITY_RESERVED,
};

enum RTS_V4L2_FRMSIZE_TYPE {
	RTS_V4L2_FRMSIZE_TYPE_DISCRETE = 1,
	RTS_V4L2_FRMSIZE_TYPE_CONTINUOUS = 2,
	RTS_V4L2_FRMSIZE_TYPE_STEPWISE = 3
};

enum RTS_V4L2_FRMIVAL_TYPE {
	RTS_V4L2_FRMIVAL_TYPE_DISCRETE = 1,
	RTS_V4L2_FRMIVAL_TYPE_CONTINUOUS = 2,
	RTS_V4L2_FRMIVAL_TYPE_STEPWISE = 3
};

/**
 * @brief fraction for RTStream
 */
struct rts_fract {
	uint32_t numerator; /**< numerator */
	uint32_t denominator; /**< denominator */
};

struct rts_v4l2_frmival_stepwise {
	struct rts_fract min;
	struct rts_fract max;
	struct rts_fract step;
};

struct rts_v4l2_resolution_discrets {
	uint32_t width;
	uint32_t height;
};

struct rts_v4l2_resolution_stepwise {
	uint32_t min_width;
	uint32_t max_width;
	uint32_t step_width;
	uint32_t min_height;
	uint32_t max_height;
	uint32_t step_height;
};

struct rts_v4l2_frmival {
	uint32_t type;
	union {
		struct rts_fract discrete;
		struct rts_v4l2_frmival_stepwise stepwise;
	};
};

struct rts_v4l2_resolution {
	uint32_t type;
	union {
		struct rts_v4l2_resolution_discrets discrete;
		struct rts_v4l2_resolution_stepwise stepwise;
	};
	int number;
	struct rts_v4l2_frmival *pfrmivals;
};

struct rts_v4l2_format {
	uint32_t format;
	int number;
	struct rts_v4l2_resolution *presolutions;
};

struct rts_v4l2_ability {
	int number;
	struct rts_v4l2_format *pformats;
};

/**
 * @brief isp ability description
 */
struct rts_isp_ability {
	int fmt_number; /**< number of supported formats */
	enum RTS_AV_FMT *pformats; /**< information of all formats supported, the pointer length is fmt_number */
	struct rts_v4l2_resolution_stepwise resolution; /**< information of resolution range supported */
	int frmival_type; /**< the type currently used to represent frmival, stepwize or discrete */
	struct rts_v4l2_frmival_stepwise frmival; /**< only used for stepwize firmival_type, information of all frmivals supported */
	int frmival_num; /**< only used for discrete firmival_type, number of supported frmivals */
	struct rts_fract *pfrmivals; /**< only used for discrete firmival_type, information of all frmivals supported, the pointer length is frmival_num */
};
/*!@}*/

/**
 * @addtogroup audio
 * @{
 */

/**
 * @brief Audio parameter description
 */
struct rts_audio_param {
	uint32_t format; /**< bit width */
	uint32_t channels; /**< sound track */
	uint32_t rate; /**< sample rate */
};

/**
 * @brief Audio ability description
 */
struct rts_audio_ability_t {
	int number; /**< number of audio parameter description */
	struct rts_audio_param *ability; /**< audio parameter description */
};

/*!@}*/

/**
 * @addtogroup core
 * @{
 */

struct rts_av_ability_t {
	const uint32_t type;
	union {
		struct rts_v4l2_ability v4l2_ability;
		struct rts_isp_ability isp_ability;
		struct rts_audio_ability_t audio_ability;
	};
};

enum RTS_AV_FMT rts_get_av_fmt_from_v4l2_fmt(uint32_t pixelformat);
uint32_t rts_get_v4l2_fmt_from_av_fmt(enum RTS_AV_FMT fmt);

/**
 * @brief RTStream buffer flags
 */
enum RTSTREAM_PKT_FLAG {
	RTSTREAM_PKT_FLAG_KEY			= (1 << 0), /**< key frame */
	RTSTREAM_PKT_FLAG_NO_OUTPUT		= (1 << 1), /**< no output, retry */
	RTSTREAM_PKT_FLAG_MEM_FROM_DEVICE	= (1 << 2), /**< sync cache from memory */
	RTSTREAM_PKT_FLAG_MEM_TO_DEVICE		= (1 << 3), /**< flush cache to memory */
	RTSTREAM_PKT_FLAG_SP			= (1 << 4), /**< super p frame */
	RTSTREAM_PKT_FLAG_END			= (1 << 5), /**< end of stream */
};

/**
 * @brief buffer for RTStream
 */
struct rts_av_buffer {
	void *vm_addr; /**< virtual address */
	uint32_t addr_offset[3]; /**< (y,u,v) or (r,g,b) address offset for video */
	uint32_t length; /**< buffer length */
	uint32_t bytesused; /**< buffer already used */
	uint32_t phy_addr; /**< physical address */
	uint32_t flags; /**< buffer flags,  refer to \ref RTSTREAM_PKT_FLAG */
	uint32_t index; /**< buffer index, used for audio mixer */
	uint32_t frame_idx;  /**< frame index */
	/**
	 * audio encode type or video frame type
	 * for audio, refer to the definition of enum RTS_AUDIO_TYPE_ID
	 * for video, refer to the definition of enum RTS_AV_FMT
	 */
	uint32_t type;
	uint32_t volume_l; /**< left channel volume for audio */
	uint32_t volume_r; /**< right channel volume for audio */
	uint64_t timestamp; /**< timestamp, in unit of microsecond */
	void *priv; /**< private member pointer */
};

/**
 * @brief buffer name
 */
enum RTS_AV_BUFFER_NAME {
	RTS_AV_ISP_FRAME_BUFFER = 0, /**< ISP frame buffer */
};

/**
 * @brief stream profile
 * @note when vin mode of channel is RTS_AV_VIN_RING_MODE \ref RTS_AV_VIN_MODE,
 * fps of profile maybe not effective, actual fps equals sensor's framerate
 */
struct rts_av_profile {
	enum RTS_AV_FMT fmt; /**< video/audio format */
	union {
		struct {
			uint32_t width; /**< video width */
			uint32_t height; /**< video height */
			uint32_t numerator; /**< numerator of fps */
			uint32_t denominator; /**< denominator of fps */
		} video; /**< video profile */
		struct {
			uint32_t samplerate; /**< sample rate */
			uint32_t bitfmt; /**< bit format */
			uint32_t channels; /**< channels */
			uint32_t reserved;
		} audio; /**< audio profile */
	}; /**< union of video/audio profile*/
};

/*!@}*/

#define RTS_AV_ID_VIN			rts_av_fourcc('v', 'i', 'n', 0)
#define RTS_AV_ID_H264			rts_av_fourcc('h', '2', '6', '4')
#define RTS_AV_ID_H265			rts_av_fourcc('h', '2', '6', '5')
#define RTS_AV_ID_MJPGENC		rts_av_fourcc('m', 'j', 'p', 'g')
#define RTS_AV_ID_MJPGDEC		rts_av_fourcc('j', 'p', 'g', 'd')
#define RTS_AV_ID_OSDENC		rts_av_fourcc('o', 's', 'd', '2')
#define RTS_AV_ID_VOUT			rts_av_fourcc('v', 'o', 'u', 't')
#define RTS_AV_ID_AUDIO_PLAYBACK	rts_av_fourcc('a', 'u', 'p', 'l')
#define RTS_AV_ID_AUDIO_CAPTURE		rts_av_fourcc('a', 'u', 'c', 'a')
#define RTS_AV_ID_AUDIO_ENCODE		rts_av_fourcc('a', 'e', 'n', 'c')
#define RTS_AV_ID_AUDIO_DECODE		rts_av_fourcc('a', 'd', 'e', 'c')
#define RTS_AV_ID_AUDIO_RESAMPLE	rts_av_fourcc('a', 'r', 's', 'm')
#define RTS_AV_ID_AUDIO_AMIXER		rts_av_fourcc('a', 'm', 'i', 'x')

/**
* @addtogroup common
* @{
*/

#define RTS_VIN_CHN_YUV_NUM		4
#define RTS_VIN_CHN_RGB_NUM		1

/**
 * @brief Video-in channel ID
 */
enum RTS_VIN_CHANNEL {
	RTS_VIN_CHANNEL_YUV0 = 0, /**< channel for YUV-0 */
	RTS_VIN_CHANNEL_YUV1 = 1, /**< channel for YUV-1 */
	RTS_VIN_CHANNEL_YUV2 = 2, /**< channel for YUV-2 */
	RTS_VIN_CHANNEL_YUV3 = 3, /**< channel for YUV-3 */
	RTS_VIN_CHANNEL_RGB0 = 10, /**< channel for RGB-0 */
};

/**
 * @brief Video-in channel attribute
 */
struct rts_vin_attr {
	enum RTS_AV_VIN_MODE vin_mode; /**< video-in mode */
	int vin_id; /**< Video-in ID, it will only be used in get attr */
	int vin_buf_num; /**< video-in outbuf number, only used for RTS_AV_VIN_FRAME_MODE */
};
/*!@}*/
enum RTS_H264_PROFILE {
	H264_PROFILE_UNKNOWN,
	H264_PROFILE_BASE,
	H264_PROFILE_MAIN,
	H264_PROFILE_HIGH
};

enum RTS_H264_LEVEL {
	H264_LEVEL_UNKNOWN = 0,
	H264_LEVEL_1,
	H264_LEVEL_1_b,
	H264_LEVEL_1_1,
	H264_LEVEL_1_2,
	H264_LEVEL_1_3,
	H264_LEVEL_2,
	H264_LEVEL_2_1,
	H264_LEVEL_2_2,
	H264_LEVEL_3,
	H264_LEVEL_3_1,
	H264_LEVEL_3_2,
	H264_LEVEL_4,
	H264_LEVEL_4_1,
	H264_LEVEL_4_2,
	H264_LEVEL_5,
	H264_LEVEL_5_1,
	H264_LEVEL_RESERVED
};

enum RTS_H265_TIER {
	MAIN_TIER = 0,
	HIGH_TIER = 1
};

enum RTS_H265_LEVEL {
	H265_LEVEL_UNKNOWN = 0,
	H265_LEVEL_1,
	H265_LEVEL_2,
	H265_LEVEL_2_1,
	H265_LEVEL_3,
	H265_LEVEL_3_1,
	H265_LEVEL_4,
	H265_LEVEL_4_1,
	H265_LEVEL_5,
	H265_LEVEL_RESERVED
};

/**
* @addtogroup common
* @{
*/

/**
 * @brief H.264 channel attribute
 */
struct rts_h264_attr {
	enum RTS_H264_LEVEL level; /**< H.264 level */
	enum RTS_AV_ROTATION rotation; /**< rotation, 0~3 */
	enum RTS_AV_MIRROR mirror; /**< mirror, 0~3 */
};

/**
 * @brief H.265 channel attribute
 */
struct rts_h265_attr {
	enum RTS_H265_LEVEL level; /**< H.265 level */
	/**
	 * defined by H.265 spec, 0-main tier, 1-high tier
	 */
	enum RTS_H265_TIER tier;
	enum RTS_AV_ROTATION rotation; /**< rotation, 0~3 */
	enum RTS_AV_MIRROR mirror; /**< mirror, 0~3 */
};

/**
 * @brief MJPEG channel attribute
 */
struct rts_jpgenc_attr {
	/**
	 * stream_mode, the value refers to RTS_AV_JPG_STREAM_MODE
	 * when jpg is binded with ring/deirect mode vin, jpg will be set
	 * as stream mode. In this mode, jpg can be set as trig mode (trig
	 * to output) or no-trig mode (continuous output)
	 */
	enum RTS_AV_JPG_STREAM_MODE stream_mode;
	enum RTS_AV_MIRROR mirror; /**< rotation, 0~3 */
	enum RTS_AV_ROTATION rotation; /**< mirror, 0~3 */
};

/**
 * @brief MJPEG decoder channel attribute
 */
struct rts_jpgdec_attr {
	enum RTS_AV_FMT out_fmt; /**< output video format */
	uint8_t out_num; /**< output buffer number */
};

/**
 * @brief Video-out channel attribute
 */
struct rts_vout_attr {
	struct {
		uint32_t x; /**< X coordinates to display */
		uint32_t y; /**< Y coordinates to display */
	} display; /**< display */

	struct {
		uint32_t start_x; /**< X coordinates to crop */
		uint32_t start_y; /**< Y coordinates to crop */
		uint32_t width; /**< width to crop */
		uint32_t height; /**< height to crop */
	} crop; /**< crop */
};

/**
 * @brief audio attribute
 * @note period_frames must be initialized to zero when it isn’t used
 */
struct rts_audio_attr {
	char dev_node[64]; /**< device node */
	uint32_t format; /**< bit width */
	uint32_t channels; /**< sound track */
	uint32_t rate; /**< sample rate */

	unsigned long period_frames; /**< sample frames */
};
/*!@}*/
enum RTS_PIC_TYPE {
	PIC_I_TYPE = 0,
	PIC_P_TYPE = 1,
	PIC_B_TYPE = 2,
	PIC_IDR_TYPE = 3,
	PIC_CRA_TYPE
};

struct rts_audio_encode_attr {
	int codec_id;
	uint32_t bitrate;
};

/**
* @addtogroup core
* @{
*/

/**
 * @brief callback type
 */
enum RTS_AV_CB_TYPE {
	RTS_AV_CB_TYPE_ASYNC = 0, /**< asynchronization */
	RTS_AV_CB_TYPE_SYNC, /**< synchronism */
};

typedef void (*rts_av_cb_func)(void *priv, struct rts_av_profile *profile,
			       struct rts_av_buffer *buffer);

/**
 * @brief callback for RTStream
 */
struct rts_av_callback {
	rts_av_cb_func func; /**< callback function */
	void *priv; /**< private member used in callback function */
	unsigned int start; /**< after how many frames to call callback function */
	int times; /**< call callback function at how many times */
	unsigned int interval; /**< callback interval, how often to call callback function */
	int type; /**< callback type refer to \ref RTS_AV_CB_TYPE */
};

enum RTS_AV_FLAG {
	RTS_AV_FLAG_DROPPABLE = (0x1 << 0),
};

/**
 * @brief Color coefficient e type
 */
enum RTS_COLOR_COEF_E {
	RTS_COLOR_COEF_BT601, /**< BT601 */
	RTS_COLOR_COEF_BT709, /**< BT709 */
	RTS_COLOR_COEF_USER_DEFINED /**< Defined by user */
};

/**
 * @brief Color range
 */
enum RTS_COLOR_RANGE {
	RTS_COLOR_RANGE_STUDIO_SWING, /**< studio swing */
	RTS_COLOR_RANGE_FULL_SWING /**< full swing */
};

/**
 * @brief Color coefficient
 */
struct rts_color_coef_t {
	uint32_t coef_a; /**< coefficient a */
	uint32_t coef_b; /**< coefficient b */
	uint32_t coef_c; /**< coefficient c */
	uint32_t coef_e; /**< coefficient e */
	uint32_t coef_f; /**< coefficient f */
};

/**
 * @brief bitrate mode for H.264/H.265
 */
enum RTS_BITRATE_MODE {
	RTS_BITRATE_MODE_CBR		= (1 << 1), /**< CBR mode */
	RTS_BITRATE_MODE_VBR		= (1 << 2), /**< VBR mode */
	RTS_BITRATE_MODE_C_VBR		= (1 << 3), /**< CVBR mode */
	RTS_BITRATE_MODE_S_VBR		= (1 << 4) /**< SVBR mode */
};

/**
 * @brief gop mode for H.264/H.265
 */
enum RTS_GOP_MODE {
	RTS_GOP_MODE_NORMAL		= (1 << 1), /**< normal mode */
	RTS_GOP_MODE_SP			= (1 << 2) /**< sp mode */
};
/*!@}*/

/**
 * @addtogroup sys
 * @{
 */

/* sys video */
#define RTS_STREAM_MAX_NUMBER		5
#define RTS_STREAM_MAX_MODULE_NUMBER	8

/**
 * @brief Status of pre-allocated memory
 */
enum RTS_SYS_VMEM_STATUS {
	RTS_SYS_VMEM_STATUS_ON = 1, /**< pre-allocation on */
	RTS_SYS_VMEM_STATUS_OFF, /**< pre-allocation off */
};

/**
 * @brief Configuration of pre-allocated memory
 */
struct rts_sys_vmem_cfg {
	struct {
		/**
		 * enable stream cfg,only if value is 1,
		 * configuration of this stream is effective
		 */
		bool enable;
		uint32_t fmt; /**< video format */
		uint32_t width; /**< video width */
		uint32_t height; /**< video height */
		struct {
			uint32_t type; /**< module type */
			uint8_t cnt; /**< module number */
			/**
			 * module mode
			 * For vin module, 0 is FRAME_MODE,
			 * 1 is RING_MODE, 2 is DIRECT_MODE
			 */
			uint8_t mode;
			struct {
				bool setted; /**< output enable */
				/**
				 * share outbuf, only H264/H265 and MJPEG
				 * can share; outbuf support to be shared
				 * only in one process.
				 */
				uint8_t shared;
				uint8_t num; /**< output number */
				uint32_t size; /**< output size */
			} outbuf; /**< output information */
		} module[RTS_STREAM_MAX_MODULE_NUMBER]; /**< module information */
	} stream[RTS_STREAM_MAX_NUMBER]; /**< stream inforamtion */
};

/*!@}*/
#ifdef __cplusplus
}
#endif
#endif
