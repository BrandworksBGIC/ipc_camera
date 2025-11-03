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

#ifndef _RTS_ISP_SENSOR_H_INC_
#define _RTS_ISP_SENSOR_H_INC_

#include <stdint.h>
#include <rts_camera_isp_snr.h>
#include <rts_isp_errno.h>
#include <rts_isp_geom.h>
#include <rts_isp_define.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define SENSOR_API_VERSION_MAGIC ((uint32_t)'s' << 8 | (uint32_t)'n')
#define SENSOR_API_MAJOR_VERSION 1
#define SENSOR_API_MINOR_VERSION 2
#define SENSOR_API_VERSION \
	(SENSOR_API_VERSION_MAGIC << 16 | SENSOR_API_MAJOR_VERSION << 8 | \
	 SENSOR_API_MINOR_VERSION)
#define SENSOR_VERSION_MASK ((1 << 16) - 1)

#define set_init_i2c_regs(info, _regs, _udelay) ({ \
	typeof(info) *_info = &info; \
	_info->regs = _regs; \
	_info->num = ARRAY_SIZE(_regs); \
	_info->udelay = _udelay; })

#define SENSOR_FPS_PRECISION 1000

static inline void set_power_item(struct rts_isp_snr_pwr_item *item,
				  enum rts_isp_snr_pwr_type type,
				  __u32 value, __u32 delay)
{
	item->type = type;
	item->value = value;
	item->delay = delay;
}

static inline void set_init_i2c(struct rts_isp_i2c_reg *reg,
				__u16 addr, __u16 data)
{
	reg->addr = addr;
	reg->data = data;
}

static inline void set_sync_i2c(struct rts_isp_sync_reg *reg,
				__u16 addr, __u16 data)
{
	reg->type = RTS_ISP_SYNC_TYPE_I2C;
	reg->i2c.addr = addr;
	reg->i2c.data = data;
	reg->i2c.mask = 0;
}

static inline void set_sync_isp(struct rts_isp_sync_reg *reg,
				__u32 addr, __u32 data)
{
	reg->type = RTS_ISP_SYNC_TYPE_REG;
	reg->reg.addr = addr;
	reg->reg.data = data;
	reg->reg.mask = 0;
}

static inline void set_sync_info(struct rts_isp_sync_reg *reg,
				 __u32 delay_frames, __u32 interrupt)
{
	reg->type = RTS_ISP_SYNC_TYPE_INFO;
	reg->info.delay_frames = delay_frames;
	reg->info.interrupt = interrupt;
}

static inline void set_sync_i2c_mask(struct rts_isp_sync_reg *reg,
				     __u16 addr, __u16 data, __u16 mask)
{
	reg->type = RTS_ISP_SYNC_TYPE_I2C;
	reg->i2c.addr = addr;
	reg->i2c.data = data;
	reg->i2c.mask = mask;
}

static inline void set_sync_isp_mask(struct rts_isp_sync_reg *reg,
				     __u32 addr, __u32 data, __u32 mask)
{
	reg->type = RTS_ISP_SYNC_TYPE_REG;
	reg->reg.addr = addr;
	reg->reg.data = data;
	reg->reg.mask = mask;
}

enum rts_isp_sensor_power {
	PWR_NONE = 0,
	PWR_1V2 = 1200000,
	PWR_1V5 = 1500000,
	PWR_1V8 = 1800000,
	PWR_2V7 = 2700000,
	PWR_2V8 = 2800000,
	PWR_2V9 = 2900000,
	PWR_3V0 = 3000000,
	PWR_3V3 = 3300000,
};

enum rts_isp_sensor_clock {
	CLK_NONE = 0,
	CLK_6M = 6000000,
	CLK_12M = 12000000,
	CLK_24M = 24000000,
	CLK_27M = 27000000,
	CLK_37M125 = 37125000,
	CLK_54M = 54000000,
	CLK_74M25 = 74250000,
};

enum rts_isp_sensor_gpio {
	GPIO_LOW = 0,
	GPIO_HIGH = 1,
};

struct rts_isp_focus_info {
	uint32_t min;
	uint32_t max;
	uint32_t step;
	uint8_t resv[4];
};

struct rts_isp_sensor_info {
	struct rts_isp_sensor_modes modes;
	struct rts_isp_i2c_info i2c;
	struct rts_isp_snr_pwr power_up;
	struct rts_isp_snr_pwr power_down;
	struct rts_isp_focus_info focus;
};

struct rts_isp_i2c_regs {
	uint32_t num;
	struct rts_isp_i2c_reg *regs;
	uint32_t udelay;
};

enum rts_isp_interface {
	SNR_INTERFACE_NONE,
	SNR_INTERFACE_DVP,
	SNR_INTERFACE_MIPI,
};

struct rts_isp_dvp_info {
	uint8_t sample_rising;
	uint8_t hsync_active_high;
	uint8_t vsync_active_high;
	uint8_t resv[13];
};

enum rts_isp_mipi_lane {
	MIPI_LANE0 = 1 << 0,
	MIPI_LANE1 = 1 << 1,
	MIPI_LANE2 = 1 << 2,
	MIPI_LANE3 = 1 << 3,
};

enum rts_isp_mipi_hdr_mode {
	MIPI_HDR_NONE,
	MIPI_HDR_VC, /* virtual channel based hdr */
	MIPI_HDR_FID, /* sony dol fid based hdr */
};

struct rts_isp_mipi_info {
	enum rts_isp_mipi_hdr_mode hdr;
	uint8_t lanes;
	uint8_t hs_term; /* 0 to 0xf */
	uint32_t clk_skew; /* 0 to 0xf */
	uint32_t data_skew; /* 0 to 0xf */
	uint8_t resv[8];
};

enum rts_isp_sensor_type {
	RAW_SENSOR,
	YUV_SENSOR,
};

enum rts_isp_sensor_yuv_type {
	SNR_YUV422 = 0,
	SNR_YUV420_LEGACY = 1,
	SNR_YUV420 = 2,
	SNR_YUV420_CSPS = 3,
};

enum rts_isp_sensor_bit_depth {
	SNR_8BIT = 8,
	SNR_10BIT = 10,
	SNR_12BIT = 12,
};

enum rts_isp_sensor_yuv422_order {
	SNR_UYVY = 0,
	SNR_VYUY = 1,
	SNR_YVYU = 2,
	SNR_YUYV = 3,
};

enum rts_isp_sensor_bayer_order {
	SNR_GRBG = 0,
	SNR_RGGB = 1,
	SNR_BGGR = 2,
	SNR_GBRG = 3,
};

enum rts_isp_sensor_yuv420_legacy_order {
	SNR_UYY_VYY = 0,
	SNR_VYY_UYY = 1,
	SNR_YYU_YYV = 2,
	SNR_YYV_YYU = 3,
};

enum rts_isp_sensor_yuv420_order {
	SNR_YY_UYVY = 0,
	SNR_YY_VYUY = 1,
	SNR_UYVY_YY = 2,
	SNR_VYUY_YY = 3,
};

struct rts_isp_sensor_raw_config {
	enum rts_isp_sensor_bayer_order bayer;
	uint8_t resv[12];
};

struct rts_isp_sensor_yuv_config {
	enum rts_isp_sensor_yuv_type type;
	union {
		enum rts_isp_sensor_yuv422_order yuv422;
		enum rts_isp_sensor_yuv420_legacy_order yuv420_legacy;
		enum rts_isp_sensor_yuv420_order yuv420;
	} order;
	uint8_t resv[8];
};

struct rts_isp_sensor_type_config {
	union {
		struct rts_isp_sensor_raw_config raw;
		struct rts_isp_sensor_yuv_config yuv;
	};
};

struct rts_isp_interface_info {
	enum rts_isp_interface interface;
	union {
		struct rts_isp_dvp_info dvp;
		struct rts_isp_mipi_info mipi;
	};
	enum rts_isp_sensor_bit_depth bit_depth;
	enum rts_isp_sensor_type type;
	struct rts_isp_sensor_type_config type_config;
	uint8_t resv[16];
};

struct rts_isp_sensor_init_info {
	struct rts_isp_i2c_regs sensor_regs[8];

	struct rts_isp_interface_info interface;
	rts_isp_size_t size;
	rts_isp_point_t start;
	uint32_t pclk;
	uint32_t hts;
	uint32_t min_vts;
	uint32_t max_vts;
	float exposure_step; /* us */
	uint8_t resv[16];
};

struct rts_isp_sensor_exp_gain {
	float exposure[RTS_ISP_HDR_CHAN_MAX];
	float analog_gain[RTS_ISP_HDR_CHAN_MAX];
	float digital_gain[RTS_ISP_HDR_CHAN_MAX];
	uint32_t vts;
};

struct rts_isp_exp_gain_report {
	float exposure[RTS_ISP_HDR_CHAN_MAX];
	float analog_gain[RTS_ISP_HDR_CHAN_MAX];
	float digital_gain[RTS_ISP_HDR_CHAN_MAX];
};

struct rts_isp_mirror_flip {
	uint32_t mirror;
	uint32_t flip;
};

struct rts_isp_ratio_range {
	float min_ratio_total;
	float max_ratio_total;
	float min_ratio[RTS_ISP_HDR_CHAN_MAX - 1];
	float max_ratio[RTS_ISP_HDR_CHAN_MAX - 1];
};

struct rts_isp_sensor_ops {
	uint32_t api_version;
	char name[32];
	int (*get_info)(uint32_t isp_id, struct rts_isp_sensor_info *info);
	int (*get_init_info)(uint32_t isp_id,
			     const struct rts_isp_sensor_mode *mode,
			     struct rts_isp_sensor_init_info *info);
	int (*get_exposure_range)(uint32_t isp_id, uint32_t vts,
				  float ratio[RTS_ISP_HDR_CHAN_MAX - 1],
				  float min_exposure[RTS_ISP_HDR_CHAN_MAX],
				  float max_exposure[RTS_ISP_HDR_CHAN_MAX]);
	int (*get_tuned_again)(uint32_t isp_id,
			       float again[RTS_ISP_HDR_CHAN_MAX]);
	int (*get_tuned_dgain)(uint32_t isp_id,
			       float dgain[RTS_ISP_HDR_CHAN_MAX]);
	int (*get_exposure_gain_info)(uint32_t isp_id,
			const struct rts_isp_sensor_exp_gain *exp_gain,
			struct rts_isp_sync_regs *regs);

	/* for iq patch, optional */
	const void *(*get_patch_ops)(void);

	/* optional */
	int (*init)(uint32_t isp_id);
	int (*cleanup)(uint32_t isp_id);
	int (*start)(uint32_t isp_id);
	int (*stop)(uint32_t isp_id);
	int (*set_focus)(uint32_t isp_id, uint32_t position);
	int (*get_temperature)(uint32_t isp_id);
	int (*check)(uint32_t isp_id);
	int (*get_mirror_flip)(uint32_t isp_id,
			       const struct rts_isp_mirror_flip *mf_info,
			       struct rts_isp_sync_regs *regs);
	int (*suspend)(uint32_t isp_id);
	int (*resume)(uint32_t isp_id);
	long resv[6];
};

#define RTS_ISP_DEFINE_SENSOR_PLUGIN(sensor_ops)                               \
	const struct rts_isp_sensor_ops *rts_isp_get_sensor_ops(void)          \
	{                                                                      \
		return &sensor_ops;                                            \
	}

int rts_isp_i2c_read(uint32_t isp_id, uint8_t i2c_id, void *buf, int len);
int rts_isp_i2c_write(uint32_t isp_id, uint8_t i2c_id, void *buf, int len);
int rts_isp_read_sensor_reg(uint32_t isp_id, struct rts_isp_i2c_reg *reg);
int rts_isp_write_sensor_reg(uint32_t isp_id, struct rts_isp_i2c_reg *reg);
int rts_isp_read_i2c_regs(const struct rts_isp_i2c_info *info,
			  const struct rts_isp_i2c_regs *regs);
int rts_isp_write_i2c_regs(const struct rts_isp_i2c_info *info,
			   const struct rts_isp_i2c_regs *regs);
int rts_isp_read_i2c_reg(const struct rts_isp_i2c_info *info,
			 struct rts_isp_i2c_reg *reg);
int rts_isp_write_i2c_reg(const struct rts_isp_i2c_info *info,
			  struct rts_isp_i2c_reg *reg);
void rts_isp_set_isp_gain_delay(uint32_t isp_id, uint32_t delay);
int rts_isp_report_sensor_exp_gain(uint32_t isp_id,
				   struct rts_isp_exp_gain_report *report);
void rts_isp_drop_frames(uint32_t isp_id, int frames);
int rts_isp_set_exp_gain_ratio_range(uint32_t isp_id,
				     struct rts_isp_ratio_range *range);

#ifdef __cplusplus
}
#endif

#endif /* _RTS_ISP_SENSOR_H_INC_ */
