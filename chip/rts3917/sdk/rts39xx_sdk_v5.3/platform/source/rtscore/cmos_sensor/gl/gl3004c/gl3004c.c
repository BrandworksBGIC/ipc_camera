/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2021 Grant Shen <grant_shen@realsil.com.cn>
 * Copyright (C) 2021 Martial_Wu <martial_wu@realsil.com.cn>
 */

#include <stdio.h>
#include <rts_isp_sensor.h>

/* #define DEBUG */
#ifdef DEBUG
#define debug(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

#define SUPPORTED_ISP_NUM 1

static int gl3004c_get_info(uint32_t isp_id,
			 struct rts_isp_sensor_info *info)
{
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 1920;
	info->modes.mode[0].size.h = 1080;
	info->modes.mode[0].fps = 30;
	info->modes.num = 1;

	info->i2c.i2c_id = 0;
	info->i2c.data_len = 1;
	info->i2c.addr_len = 1;

	up->num = 0;
	down->num = 0;

	return RTS_ISP_OK;
}


static int gl3004c_get_init_info(uint32_t isp_id,
				 const struct rts_isp_sensor_mode *mode,
			      struct rts_isp_sensor_init_info *info)
{
	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("gl3004c get fps %.1f init info\n", mode->fps);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0 | MIPI_LANE1;
	info->interface.mipi.hs_term = 0x4;
	info->interface.type = YUV_SENSOR;
	info->interface.type_config.yuv.order.yuv422 = SNR_UYVY;
	info->interface.bit_depth = SNR_8BIT;

	info->size.w = 1920;
	info->size.h = 1080;
	info->start.x = 0;
	info->start.y = 0;

	info->hts = 2200;
	info->pclk = 100e6;
	info->min_vts = info->max_vts = 1515;

	return RTS_ISP_OK;
}


static int gl3004c_get_tuned_again(uint32_t isp_id,
				   float again[RTS_ISP_HDR_CHAN_MAX])
{
	return RTS_ISP_OK;
}

static int gl3004c_get_tuned_dgain(uint32_t isp_id,
				   float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	return RTS_ISP_OK;
}

static int gl3004c_get_exposure_gain_info(uint32_t isp_id,
				       const struct rts_isp_sensor_exp_gain *exp_gain,
				       struct rts_isp_sync_regs *regs)
{
	return RTS_ISP_OK;
}

static const struct rts_isp_sensor_ops gl3004c_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "gl3004c",
	.get_info = gl3004c_get_info,
	.get_init_info = gl3004c_get_init_info,
	.get_tuned_again = gl3004c_get_tuned_again,
	.get_tuned_dgain = gl3004c_get_tuned_dgain,
	.get_exposure_gain_info = gl3004c_get_exposure_gain_info,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(gl3004c_ops)
