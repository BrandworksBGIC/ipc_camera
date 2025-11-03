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
 * @file rts_raw.h
 * @brief RTStream raw API reference
 * @defgroup raw RTStream Raw API
 * @brief RTStream get raw image API reference
 * @{
 */

#ifndef _INCLUDE_RTS_RAW_H
#define _INCLUDE_RTS_RAW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define RTS_RAW_HEADER_LENGTH		128

struct rts_raw_t;
typedef void (*rts_raw_cb)(struct rts_raw_t *praw);

/**
 * @brief raw handler
 */
struct rts_raw_t {
	/**
	 * header of raw, RTS_RAW_HEADER_LENGTH equals to 128.\n
	 * byte[0]~byte[3]: format, which is 0x32595559.\n
	 * byte[4]~byte[7]: width of raw image.\n
	 * byte[8]~byte[11]: height of raw image.\n
	 * byte[12]~byte[15]: length of raw data.\n
	 * byte[16]~byte[19]: image file header version, 0 means v0, 1 means v1, and so on.\n
	 * byte[20]~byte[23]: the offset of data beginning in the image file.\n
	 * byte[24]~byte[27]: image data pattern.\n
	 * byte[28]~byte[31]: image data type.\n
	 * byte[32]~byte[35]: image file format.\n
	 * byte[36]~byte[39]: sensor format. 16: 16 bits; 32: 32 bits; 128: 128 bits.\n
	 * byte[40]~byte[43]: effective bits.
	 */
	uint8_t header[RTS_RAW_HEADER_LENGTH];
	uint32_t m_length; /**< map data length */
	uint32_t length; /**< raw data length */
	uint8_t *pdata; /**< raw data */
	uint32_t width; /**< width of raw image */
	uint32_t height; /**< height of raw image */
	uint32_t fmt; /**< format, which is 0x32595559 */
	 /**
	  * sensor format, see more in \ref rts_raw_fmt
	  */
	unsigned int snr_fmt;
	unsigned int eff_bits; /**< effective bits */
	unsigned int data_pattern; /**< data pattern, see more in \ref rts_data_pattern */
	unsigned int data_type; /**< data type, see more in \ref rts_data_type */
	unsigned int file_fmt; /**< image file format, see more in \ref rts_file_fmt */
	unsigned int raw_fmt; /**< raw source, see more in \ref rts_raw_src */
	int streamid; /**< stream id */
	int fd; /**< fd of raw video device */
	int initialized; /**< flag of initialize raw handler */
	int skip_count; /**< get raw image after skipping how many frames */
	void (*make_raw_data)(struct rts_raw_t *praw, uint8_t *pdata);
	rts_raw_cb func;
	void *master;
};

/**
 * @brief source of raw data
 */
enum rts_raw_src {
	RTS_RAW_SRC_DISABLE = 0, /**< disable raw */
	RTS_RAW_SRC_S_CROP_A = 0x1, /**< raw data after crop for short exp */
	RTS_RAW_SRC_L_CROP_A = 0x2, /**< raw data after crop for long exp */
	RTS_RAW_SRC_S_BLC_B = 0x3, /**< raw data before BLC for short exp */
	RTS_RAW_SRC_L_BLC_B = 0x4, /**< raw data before BLC for long exp */
	RTS_RAW_SRC_HDR_A = 0x5, /**< raw data after HDR fusion */
	/**
	 * raw data after global tone mapping
	 */
	RTS_RAW_SRC_GTM_A = 0x6,
	/**
	 * raw data after local tone mapping
	 */
	RTS_RAW_SRC_LTM_A = 0x7,
	/**
	 * raw data before CCM
	 */
	RTS_RAW_SRC_CCM_B = 0x8,
	/**
	 * raw data before UV tune
	 */
	RTS_RAW_SRC_UVT_B = 0x9,
	/**
	 * raw data before fusion for long exp
	 */
	RTS_RAW_SRC_L_FUSION_B = 0xA,
	/**
	 * raw data before fusion for short exp
	 */
	RTS_RAW_SRC_S_FUSION_B = 0xB,
	/**
	 * raw data before fusion
	 */
	RTS_RAW_SRC_FUSION_B = 0xC,
	/**
	 * raw data after ae gain
	 */
	RTS_RAW_SRC_AEGAIN_A = 0xD,
	/**
	 * raw data after WDR
	 */
	RTS_RAW_SRC_WDR_A = 0xE,
	/**
	 * raw data before yuv444to422
	 */
	RTS_RAW_SRC_YUV444TO422_B = 0xF,
	/**
	 * raw data before mcrop
	 */
	RTS_RAW_SRC_MCROP_B = 0x10,
	/**
	 * raw data after mask
	 */
	RTS_RAW_SRC_MASK_A = 0x11,
	/**
	 * raw data before awb gain
	 */
	RTS_RAW_SRC_AWBGAIN_B = 0x12,
	RTS_RAW_SRC_RESERVED
};
/**
 * @brief format of sensor raw data
 */
enum rts_raw_fmt {
	RTS_RAW_FMT_RAW16 = 16, /**< raw16 */
	RTS_RAW_FMT_RAW32 = 32, /**< raw32 */
	RTS_RAW_FMT_RAW128 = 128, /**< raw128 */
	RTS_RAW_FMT_RESERVED
};
/**
 * @brief format of image file
 */
enum rts_file_fmt {
	RTS_FILE_UNKNOWN = 0x0,
	RTS_FILE_BIN = 0x1,
	RTS_FILE_BIN_TEXT = 0x2,
	RTS_FILE_HEX_TEXT = 0x3,
};
/**
 * @brief data pattern
 */
enum rts_data_pattern {
	RTS_DATA_PATTERN_UNKNOWN = 0x0,

	RTS_DATA_PATTERN_RAW_GRBG = 0x1,
	RTS_DATA_PATTERN_RAW_RGGB = 0x2,
	RTS_DATA_PATTERN_RAW_BGGR = 0x3,
	RTS_DATA_PATTERN_RAW_GBRG = 0x4,

	RTS_DATA_PATTERN_HDR = 0x21,

	RTS_DATA_PATTERN_RGB = 0x41,

	RTS_DATA_PATTERN_YUV444 = 0x81,
	RTS_DATA_PATTERN_YUV422 = 0x82,
	RTS_DATA_PATTERN_YUV420 = 0x83,

	RTS_DATA_PATTERN_H264 = 0xB1,
	RTS_DATA_PATTERN_HEVC = 0xB2,
	RTS_DATA_PATTERN_MJPG = 0xB3,

	RTS_DATA_PATTERN_JPEG = 0xC1,
	RTS_DATA_PATTERN_BITMAP = 0xC2,
	RTS_DATA_PATTERN_PNG = 0xC3,
};
/**
 * @brief data type
 */
enum rts_data_type {
	RTS_DATA_TYPE_UNKNOWN = 0x0,

	RTS_DATA_TYPE_RAW_INTERLEAVED = 0x1,
	RTS_DATA_TYPE_RAW_PLANAR = 0x2,

	RTS_DATA_TYPE_HDR_INTERLEAVED = 0x21,
	RTS_DATA_TYPE_HDR_PLANAR = 0x22,

	RTS_DATA_TYPE_RGB = 0x41,
	RTS_DATA_TYPE_RGBA = 0x42,

	RTS_DATA_TYPE_YUV_INTERLEAVED = 0x81,
	RTS_DATA_TYPE_YUV_PLANAR_WITH_UV_ORDER = 0x91,
	RTS_DATA_TYPE_YUV_PLANAR_WITH_VU_ORDER = 0x92,
	RTS_DATA_TYPE_YUV_SEMI_PLANAR_WITH_UV_ORDER = 0xA1,
	RTS_DATA_TYPE_YUV_SEMI_PLANAR_WITH_VU_ORDER = 0xA2,
};

/**
 * @example example_get_raw_pict.c
 */

/**
 * @brief Initialize raw handler
 * @param[in] raw_fmt source of raw data, see detail in \ref rts_raw_src
 * @param[out] ppraw address of raw handler pointer,
 * raw handler see \ref rts_raw_t for more
 * @attention Raw format can be updated by \ref rts_update_raw_fmt
 * after initializing.
 * Raw handler must be released by \ref rts_release_raw at last.
 * @return 0 : success, minus : fail
 */
int rts_init_raw(unsigned int raw_fmt, struct rts_raw_t **ppraw);
/**
 * @brief Release raw handler
 * @param[in] praw raw handler obtained by \ref rts_init_raw
 */
void rts_release_raw(struct rts_raw_t *praw);
/**
 * @brief Get raw data
 * @param[out] praw raw handler obtained by \ref rts_init_raw.
 * @note Raw data will store in pdata of \ref rts_raw_t.
 * @return 0 : success, minus : fail
 */
int rts_get_raw_frame(struct rts_raw_t *praw);
int rts_get_raw_header(struct rts_raw_t *praw);
/**
 * @brief Update raw format
 * @param[in] raw_fmt format of raw data, see detail in \ref rts_raw_src
 * @param[in] praw raw handler obtained by \ref rts_init_raw.
 * @return 0 : success, minus : fail
 */
int rts_update_raw_fmt(unsigned int raw_fmt, struct rts_raw_t *praw);

/**
 * @brief Initialize raw streaming handler
 * @param[out] ppraw address of raw streaming handler pointer,
 * raw handler see \ref rts_raw_t for more
 * @param[in] cb Callback function
 * @param[in] master
 * @attention Raw format can be updated by \ref rts_update_raw_fmt
 * after initializing.
 * Raw streaming handler must be released by \ref rts_release_raw_streaming at last.
 * @return 0 : success, minus : fail
 */
int rts_init_raw_streaming(struct rts_raw_t **ppraw,
				rts_raw_cb cb, void *master);
/**
 * @brief Release raw streaming handler
 * @param[in] praw raw streaming handler obtained by \ref rts_init_raw_streaming
 */
void rts_release_raw_streaming(struct rts_raw_t *praw);
/**
 * @brief Start raw streaming
 * @param[in] praw raw streaming handler obtained by \ref rts_init_raw_streaming
 * @return 0 : success, minus : fail
 */
int rts_start_raw_streaming(struct rts_raw_t *praw);
/**
 * @brief Stop raw streaming
 * @param[in] praw raw streaming handler obtained by \ref rts_init_raw_streaming
 * @return 0 : success, minus : fail
 */
int rts_stop_raw_streaming(struct rts_raw_t *praw);
/**
 * @brief Config raw streaming
 * @param[in] raw_fmt source of raw data, see detail in \ref rts_raw_src
 * @param[in] praw raw streaming handler obtained by \ref rts_init_raw_streaming
 * @return 0 : success, minus : fail
 */
int rts_config_raw_streaming(unsigned int raw_fmt, struct rts_raw_t *praw);
/**
 * @brief Get raw data
 * @param[out] praw raw streaming handler obtained by \ref rts_init_raw_streaming.
 * @note Raw data will store in pdata of \ref rts_raw_t.
 * @return 0 : success, minus : fail
 */
int rts_get_raw_streaming(struct rts_raw_t *praw);
/*!@}*/
#ifdef __cplusplus
}
#endif
#endif
