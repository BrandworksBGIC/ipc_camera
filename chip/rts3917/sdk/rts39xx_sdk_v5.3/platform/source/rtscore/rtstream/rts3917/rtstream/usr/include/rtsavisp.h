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

#ifndef _RTSAVISP_H_INC_
#define _RTSAVISP_H_INC_

#include <stdint.h>
#include <rts_isp_errno.h>
#include <rts_isp_define.h>

#ifdef __cplusplus
extern "C"
{
#endif
/**
 * @file rtsavisp.h
 * @brief Strucure and API for ISP of Realtek IPCam
 * @{
 */

/**
 * @brief Init ISP.
 * @attention This API must be called before any ISP API.
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_init(void);
/**
 * @brief Cleanup ISP.
 * @attention After calling \ref rts_av_isp_init, this API must be called to release.
 * @attention This API must be called at the same process that called \ref rts_av_isp_init.
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_cleanup(void);
/**
 * @brief Start ISP preview loop.
 * @attention This API must be called at the same process that called \ref rts_av_isp_init.
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_start(void);
/**
 * @brief Stop ISP preview loop.
 * @attention This API must be called at the same process that called \ref rts_av_isp_init.
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_stop(void);

/**
 * @brief ISP status
 */
enum rts_isp_status {
	RTS_ISP_UNINITIALIZED, /**< ISP uninitialized */
	RTS_ISP_STOPPED, /**< ISP preview loop stopped */
	RTS_ISP_RUNNING, /**< ISP preview loop is running but not previewing */
	RTS_ISP_STREAMING, /**< ISP preview loop is streaming */
};

/**
 * @brief Get ISP loop status.
 * @return minus : fail, others : ISP status, see detail in \ref rts_isp_status
 */
enum rts_isp_status rts_av_isp_get_status(void);

enum rts_isp_log_behavor {
	RTS_ISP_LOG2LOG, /* **< ISP log to syslog */
	RTS_ISP_LOG2CONS, /* **< ISP log to console */
	RTS_ISP_LOG2FILE, /* **< ISP log to /var/log/isp.log */
	RTS_ISP_LOG2KMSG, /* **< ISP log to kernel message */
};

/**
 * @brief Open isp log and set log behavor.
 */
void rts_av_isp_open_log(int32_t behavor);

/**
 * @brief Type of ISP algorithm.
 */
enum rts_isp_algo_type {
	RTS_ISP_ALGO_AE, /**< AE algorithm */
	RTS_ISP_ALGO_AWB, /**< AWB algorithm */
	RTS_ISP_ALGO_AF, /**< AF algorithm */
	RTS_ISP_ALGO_OTHER, /**< Other ISP algorithm */
	_RTS_ISP_ALGO_TYPE_NUM,
};

/**
 * @brief ISP algorithm ID
 */
enum rts_isp_algo_id {
	RTS_ISP_ALGO_AE_ID0, /**< AE algorithm 0 */
	RTS_ISP_ALGO_AE_ID1, /**< AE algorithm 1 */
	RTS_ISP_ALGO_AE_ID2, /**< AE algorithm 2 */
	RTS_ISP_ALGO_AE_ID3, /**< AE algorithm 3 */

	RTS_ISP_ALGO_AWB_ID0, /**< AWB algorithm 0 */
	RTS_ISP_ALGO_AWB_ID1, /**< AWB algorithm 1 */
	RTS_ISP_ALGO_AWB_ID2, /**< AWB algorithm 2 */
	RTS_ISP_ALGO_AWB_ID3, /**< AWB algorithm 3 */

	RTS_ISP_ALGO_AF_ID0, /**< AF algorithm 0 */
	RTS_ISP_ALGO_AF_ID1, /**< AF algorithm 1 */
	RTS_ISP_ALGO_AF_ID2, /**< AF algorithm 2 */
	RTS_ISP_ALGO_AF_ID3, /**< AF algorithm 3 */

	RTS_ISP_ALGO_OTHER_ID0, /**< other ISP algorithm 0 */
	RTS_ISP_ALGO_OTHER_ID1, /**< other ISP algorithm 1 */
	RTS_ISP_ALGO_OTHER_ID2, /**< other ISP algorithm 2 */
	RTS_ISP_ALGO_OTHER_ID3, /**< other ISP algorithm 3 */

	_RTS_ISP_ALGO_ID_NUM,
};

/**
 * @brief Structure of ISP algorithm.
 */
struct rts_isp_algo {
	enum rts_isp_algo_id id; /**< ID of algorithm */
	const char *path; /**< path of algorithm */
};

/**
 * @brief Register ISP algorithm.
 * @attention This API must be called at the same process that called \ref rts_av_isp_init.
 * @param[in] algo pointer of \ref rts_isp_algo.
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_register_algo(const struct rts_isp_algo *algo);
/**
 * @brief Unregister ISP algorithm.
 * @attention This API must be called at the same process that called \ref rts_av_isp_init.
 * @param[in] id ID of unregistered algorithm.
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_unregister_algo(enum rts_isp_algo_id id);
/**
 * @brief Get ISP algorithm path by ID.
 * @attention This API must be called at the same process that called \ref rts_av_isp_init.
 * @param[in] algo pointer of \ref rts_isp_algo, ID of algo must be specified.
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_get_algo(struct rts_isp_algo *algo);
/**
 * @brief Bind ISP algorithm with specified ISP.
 * @attention This API can be called at any process.
 * @param[in] isp_id ID of ISP.
 * @param[in] id ID of ISP algorithm.
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_bind_algo(uint32_t isp_id, enum rts_isp_algo_id id);
/**
 * @brief Unbind ISP algorithm from specified ISP.
 * @attention This API can be called at any process.
 * @param[in] isp_id ID of ISP.
 * @param[in] id ID of ISP algorithm.
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_unbind_algo(uint32_t isp_id, enum rts_isp_algo_id id);

/**
 * @brief ISP sensor
 */
struct rts_isp_sensor {
	const char *path; /**< path of sensor */
};

/**
 * @brief Register ISP sensor.
 * @attention This API must be called at the same process that called \ref rts_av_isp_init.
 * @param[in] snr pointer of \ref rts_isp_sensor.
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_register_sensor(const struct rts_isp_sensor *snr);
/**
 * @brief Unregister specofied sensor.
 * @attention This API must be called at the same process that called \ref rts_av_isp_init.
 * @param[in] snr_id ID of sensor
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_unregister_sensor(int snr_id);
/**
 * @brief Obtain ISP sensor path by sensor ID.
 * @attention This API must be called at the same process that called \ref rts_av_isp_init.
 * @param[in] snr_id sensor ID.
 * @param[in] snr pointer of \ref rts_isp_sensor.
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_get_sensor(int snr_id, struct rts_isp_sensor *snr);
/**
 * @brief Check whether sensor is connected with specified ISP or not.
 * @attention This API must be called at the same process that called \ref rts_av_isp_init.
 * @param[in] isp_id ID of ISP.
 * @param[in] snr_id ID of sensor.
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_check_sensor(uint32_t isp_id, int snr_id);
/**
 * @brief Bind ISP sensor with specified ISP.
 * @attention This API must be called at the same process that called \ref rts_av_isp_init.
 * @param[in] isp_id ID of ISP.
 * @param[in] snr_id ID of sensor.
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_bind_sensor(uint32_t isp_id, int snr_id);
/**
 * @brief Unbind ISP sensor from specified ISP.
 * @attention This API must be called at the same process that called \ref rts_av_isp_init.
 * @param[in] isp_id ID of ISP.
 * @param[in] snr_id ID of sensor.
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_unbind_sensor(uint32_t isp_id, int snr_id);

/**
 * @brief Register IQ to specified ISP.
 * @attention This API must be called at the same process that called \ref rts_av_isp_init.
 * @param[in] isp_id ISP ID.
 * @param[in] path file path of IQ binary
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_register_iq(uint32_t isp_id, const char *path);

/**
 * @brief type of out of ISP mipi
 */
enum rts_isp_mipiout_type {
	RTS_ISP_MIPIOUT_RAW8,/**< RAW8 */
	RTS_ISP_MIPIOUT_RAW10,/**< RAW10 */
	RTS_ISP_MIPIOUT_RAW12,/**< RAW12 */
	RTS_ISP_MIPIOUT_YUYV, /**< YUYV */
};

/**
 * @brief position of mipi out
 */
enum rts_isp_mipiout_position {
	RTS_ISP_MIPIOUT_BEFORE_CROP, /**< for raw, out before crop */
	RTS_ISP_MIPIOUT_AFTER_CROP, /**< for raw, out after crop */
	RTS_ISP_MIPIOUT_BEFORE_ZOOM, /**< for yuyv, out before zoom */
};

/**
 * @brief mipi out of ISP
 */
struct rts_isp_mipiout {
	int enable; /**< enable */
	int lanes; /**< number of mipi lane */
	enum rts_isp_mipiout_type type; /**< type of mipi out */
	enum rts_isp_mipiout_position position; /**< position of mipi out */
};

/**
 * @brief Obtain mipi out of specified ISP
 * @attention This API can be called at any process.
 * @attention This API can not be used on rts3917.
 * @param[in] isp_id ID of ISP.
 * @param[in] mipiout pointer of \ref rts_isp_mipiout.
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_get_mipiout(uint32_t isp_id, struct rts_isp_mipiout *mipiout);
/**
 * @brief Set mipi out for specified ISP
 * @attention This API can be called at any process.
 * @attention This API can not be used on rts3917.
 * @param[in] isp_id ID of ISP.
 * @param[in] mipiout pointer of \ref rts_isp_mipiout.
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_set_mipiout(uint32_t isp_id, struct rts_isp_mipiout *mipiout);

/**
 * @brief version of ISP plugins
 */
struct rts_isp_plugins_version {
	uint16_t ae_api_version; /**< version for ae algo api */
	uint16_t awb_api_version; /**< version for awb algo api */
	uint16_t af_api_version; /**< version for af algo api */
	uint16_t sensor_api_version; /**< version for sensor api */
	uint32_t iq_pack_version; /**< version for iq packed format */
	uint32_t iq_proto_version; /**< version for iq protobuf */
	uint8_t iq_customer_version; /**< version for customer iq */
};

/**
 * @brief Get plugin's version of specified ISP
 * @attention This API can be called at any process.
 * @param[in] isp_id ID of ISP.
 * @param[out] version pointer of \ref rts_isp_plugins_version.
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_get_plugins_version(uint32_t isp_id,
				   struct rts_isp_plugins_version *version);

/**
 * @brief Enum modes of sensor connect on specified ISP
 * @attention This API can be called at any process.
 * @param[in] isp_id ID of ISP.
 * @param[out] modes pointer of \ref rts_isp_sensor_modes.
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_enum_sensor_modes(uint32_t isp_id,
				 struct rts_isp_sensor_modes *modes);

/**
 * @brief Get mode of sensor connect on specified ISP
 * @attention This API can be called at any process.
 * @param[in] isp_id ID of ISP.
 * @param[out] mode pointer of \ref rts_isp_sensor_mode.
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_get_sensor_mode(uint32_t isp_id,
			       struct rts_isp_sensor_mode *mode);

/**
 * @brief Set mode of sensor connect on specified ISP
 * @attention This API can be called at any process.
 * @param[in] isp_id ID of ISP.
 * @param[in] mode pointer of \ref rts_isp_sensor_mode. size of mode can be
 * smaller than the size of enumed modes
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_set_sensor_mode(uint32_t isp_id,
			       struct rts_isp_sensor_mode *mode);

/**
 * @brief Set isp streaming watchdog timeout, 0 means stop watchdog
 * @attention This API can be called at any process.
 * @param[in] isp_id ID of ISP.
 * @param[in] timeout of watchdog, can be 0 or greater than 500 ms, precision 100 ms
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_set_stream_watchdog(uint32_t isp_id, uint32_t timeout);

/**
 * @brief ae roi level, bigger means higher level
 */
enum rts_isp_ae_roi_level {
	RTS_AE_ROI_DISABLE,
	RTS_AE_ROI_LEVEL1,
	RTS_AE_ROI_LEVEL2,
	RTS_AE_ROI_LEVEL3,
	RTS_AE_ROI_LEVEL4,
};

/**
 * @brief ae roi for one area
 */
struct rts_isp_ae_roi {
	enum rts_isp_ae_roi_level level; /**< level for this area */
	rts_isp_rect_t rect; /**< rect region for this area */
};

/**
 * @brief ae roi for multiple areas
 */
struct rts_isp_ae_roi_areas {
	struct rts_isp_ae_roi roi[RTS_ISP_ROI_AREA_MAX]; /**< areas list */
};

/**
 * @brief Get ae roi setting of specified ISP
 * @attention This API can be called at any process.
 * @param[in] isp_id ID of ISP.
 * @param[out] areas pointer of \ref rts_isp_ae_roi_areas.
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_get_ae_roi(uint32_t isp_id, struct rts_isp_ae_roi_areas *areas);

/**
 * @brief Set ae roi setting of specified ISP
 * @attention This API can be called at any process.
 * @param[in] isp_id ID of ISP.
 * @param[out] areas pointer of \ref rts_isp_ae_roi_areas.
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_set_ae_roi(uint32_t isp_id, struct rts_isp_ae_roi_areas *areas);

/**
 * @brief isp scene description
 */
enum rts_isp_scene {
	RTS_ISP_SCENE_NORMAL,
	RTS_ISP_SCENE_OUTDOOR = 2,
	RTS_ISP_SCENE_BACKLIGHTING,
	RTS_ISP_SCENE_HUMAN,
	RTS_ISP_SCENE_FOGGY,
	RTS_ISP_SCENE_SNOWING,
	RTS_ISP_SCENE_SKY,
	RTS_ISP_SCENE_GRASS,
	RTS_ISP_SCENE_DAWN,
	RTS_ISP_SCENE_DUSK,
	RTS_ISP_SCENE_NIGHT,
	RTS_ISP_SCENE_CUSTOM0 = 256,
	RTS_ISP_SCENE_CUSTOM1,
	RTS_ISP_SCENE_CUSTOM2,
	RTS_ISP_SCENE_CUSTOM3,
	RTS_ISP_SCENE_CUSTOM4,
	RTS_ISP_SCENE_CUSTOM5,
	RTS_ISP_SCENE_CUSTOM6,
	RTS_ISP_SCENE_CUSTOM7,
};

/**
 * @brief Get user scene setting of specified ISP
 * @attention This API can be called at any process.
 * @param[in] isp_id ID of ISP.
 * @param[out] scene pointer of \ref rts_isp_scene.
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_get_scene(uint32_t isp_id, int32_t *scene);

/**
 * @brief Set user scene setting of specified ISP
 * @attention This API can be called at any process.
 * @param[in] isp_id ID of ISP.
 * @param[out] scene value of \ref rts_isp_scene.
 * @return 0 : success, minus : fail.
 */
int rts_av_isp_set_scene(uint32_t isp_id, int32_t scene);

/*!@}*/
#ifdef __cplusplus
}
#endif

#endif /* _RTSAVISP_H_INC_ */
