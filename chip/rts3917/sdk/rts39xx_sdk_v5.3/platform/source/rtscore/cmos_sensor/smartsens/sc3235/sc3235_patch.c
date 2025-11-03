/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2020 Grant Shen <grant_shen@realsil.com.cn>
 */

#include <stdio.h>
#include <rts_isp_patch.h>

/* #define DEBUG */
#ifdef DEBUG
#define debug(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

static int sc3235_patch_init(uint32_t isp_id)
{
	debug("sc3235 patch init\n");
	return RTS_ISP_OK;
}

static int sc3235_patch_cleanup(uint32_t isp_id)
{
	debug("sc3235 patch cleanup\n");
	return RTS_ISP_OK;
}

static int sc3235_patch_preview_start(uint32_t isp_id)
{
	debug("sc3235 patch preview_start\n");
	return RTS_ISP_OK;
}

static int sc3235_patch_preview_stop(uint32_t isp_id)
{
	debug("sc3235 patch preview_stop\n");
	return RTS_ISP_OK;
}

static int sc3235_patch_iq_change(uint32_t isp_id, int iq_sel, int night)
{
	debug("sc3235 patch iq change\n");
	debug("iq_sel: %d, night: %d\n", iq_sel, night);
	return RTS_ISP_OK;
}

static int sc3235_patch_dynamic(uint32_t isp_id,
				const struct isp_notify_dynamic *dynamic)
{
	debug("sc3235 patch dynamic\n");
	debug("gain: %u, color_temp: %u\n",
	      dynamic->ae.gain, dynamic->awb.color_temp);
	return RTS_ISP_OK;
}

static const struct rts_isp_sensor_patch_ops sc3235_patch_ops = {
	.api_version = PATCH_API_VERSION,
	.init = sc3235_patch_init,
	.cleanup = sc3235_patch_cleanup,
	.preview_start = sc3235_patch_preview_start,
	.preview_stop = sc3235_patch_preview_stop,
	.iq_change = sc3235_patch_iq_change,
	.dynamic = sc3235_patch_dynamic,
};

const void *sc3235_get_patch_ops(void)
{
	return &sc3235_patch_ops;
}

