/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2018 Grant Shen <sherry_cheng@realsil.com.cn>
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

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

#define FIXP_FRAME_WIDTH 0x0800
#define FIXP_FRAME_HEIGHT 0x0804
#define FIXP_DUMMY_PIX 0x0808
#define FIXP_DUMMY_LINE 0x080c
#define FIXP_CTRL 0x0820
#define BLOCK_START_X 0x0828
#define BLOCK_START_Y 0x082c
#define BLOCK_WIDTH 0x0830
#define BLOCK_HEIGHT 0x0834
#define FIXP_VC_DELAY 0x0840

#define FIXP_START 0x100
#define FIXP_STOP 0x200
#define FIXP_VC_EN 0x80

struct fixp_status {
	int hdr;
	uint16_t w;
	uint16_t h;
	uint16_t hts;
	uint16_t min_vts;
};

int isp_driver_is_fpga(void);

extern void *isp_io_base;

static struct fixp_status g_status[SUPPORTED_ISP_NUM];

static inline void isp_write_reg(uint32_t value, uint32_t offset)
{
	*(volatile uint32_t *)(isp_io_base + offset) = value;
}

static int fixp_get_info(uint32_t isp_id,
			 struct rts_isp_sensor_info *info)
{
	int i;
	int size;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;
	static const struct {
		uint16_t width;
		uint16_t height;
		uint16_t asic_fps;
		uint16_t fpga_fps;
	} fmts[] = { { 1920, 1080, 30, 20 },
		     { 2304, 1296, 30, 15 },
		     { 2560, 1440, 30, 10 },
		     { 3072, 2048, 20, 5 } };

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	size = ARRAY_SIZE(fmts);
	for (i = 0; i < size; i++) {
		info->modes.mode[i].hdr = RTS_ISP_HDR_NONE;
		info->modes.mode[i].size.w = fmts[i].width;
		info->modes.mode[i].size.h = fmts[i].height;
		if (isp_driver_is_fpga())
			info->modes.mode[i].fps = fmts[i].fpga_fps;
		else
			info->modes.mode[i].fps = fmts[i].asic_fps;

		info->modes.mode[i + size].hdr = RTS_ISP_HDR_LINE_2TO1;
		info->modes.mode[i + size].size.w = fmts[i].width;
		info->modes.mode[i + size].size.h = fmts[i].height;
		if (isp_driver_is_fpga())
			info->modes.mode[i + size].fps = fmts[i].fpga_fps;
		else
			info->modes.mode[i + size].fps = fmts[i].asic_fps;
	}
	info->modes.num = 2 * size;

	info->i2c.i2c_id = 0;
	info->i2c.data_len = 1;
	info->i2c.addr_len = 1;

	up->num = 0;
	down->num = 0;

	return RTS_ISP_OK;
}


static int fixp_get_init_info(uint32_t isp_id,
			      const struct rts_isp_sensor_mode *mode,
			      struct rts_isp_sensor_init_info *info)
{
	uint32_t hts;
	uint32_t min_vts;
	uint32_t isp_clk;
	struct fixp_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("fixp get fps %.1f init info\n", mode->fps);

	if (isp_driver_is_fpga())
		isp_clk = 48e6;
	else
		isp_clk = 160e6;
	switch (mode->size.w) {
	case 1920:
		hts = 2200;
		break;
	case 2304:
		hts = 2400;
		break;
	case 2560:
		hts = 2800;
		break;
	case 3072:
		hts = 3200;
		break;
	default:
		return -RTS_ISP_EINVAL;
	}

	min_vts = isp_clk / mode->fps / hts;
	if (min_vts <= mode->size.h)
		return -RTS_ISP_ERANGE;

	status = &g_status[isp_id];

	info->interface.interface = SNR_INTERFACE_NONE;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = mode->size.w;
	info->size.h = mode->size.h;
	info->start.x = 0;
	info->start.y = 0;

	info->hts = hts;
	info->pclk = isp_clk;
	info->min_vts = min_vts;
	info->max_vts = 65535;

	status->hdr = mode->hdr;
	status->w = mode->size.w;
	status->h = mode->size.h;
	status->hts = hts;
	status->min_vts = min_vts;

	return RTS_ISP_OK;
}

static int fixp_start(uint32_t isp_id)
{
	struct fixp_status *status;
	uint32_t block_width, block_height, block_start_x, block_start_y;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	isp_write_reg(status->w, FIXP_FRAME_WIDTH);
	isp_write_reg(status->h, FIXP_FRAME_HEIGHT);
	isp_write_reg(status->hts - status->w, FIXP_DUMMY_PIX);
	isp_write_reg(status->min_vts - status->h, FIXP_DUMMY_LINE);

	block_width = (status->w - 0x14 * 7) / 8;
	block_height = (status->h - 0x14 * 5) / 6;
	block_start_x = 0 + block_width;
	block_start_y = 0 + block_height;
	isp_write_reg(block_width, BLOCK_WIDTH);
	isp_write_reg(block_height, BLOCK_HEIGHT);
	isp_write_reg(block_start_x, BLOCK_START_X);
	isp_write_reg(block_start_y, BLOCK_START_Y);

	isp_write_reg(32, FIXP_VC_DELAY);

	if (status->hdr == RTS_ISP_HDR_NONE)
		isp_write_reg(FIXP_START, FIXP_CTRL);
	else
		isp_write_reg(FIXP_START | FIXP_VC_EN, FIXP_CTRL);

	return RTS_ISP_OK;
}

static int fixp_stop(uint32_t isp_id)
{
	isp_write_reg(FIXP_STOP, FIXP_CTRL);

	return RTS_ISP_OK;
}

static int fixp_get_tuned_again(uint32_t isp_id,
				float again[RTS_ISP_HDR_CHAN_MAX])
{
	return RTS_ISP_OK;
}

static int fixp_get_tuned_dgain(uint32_t isp_id,
				float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	return RTS_ISP_OK;
}

static int fixp_get_exposure_gain_info(uint32_t isp_id,
				       const struct rts_isp_sensor_exp_gain *exp_gain,
				       struct rts_isp_sync_regs *regs)
{
	uint32_t dummy;
	struct fixp_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	dummy = exp_gain->vts - status->h;

	isp_write_reg(dummy, FIXP_DUMMY_LINE);
	return RTS_ISP_OK;
}

static const struct rts_isp_sensor_ops fixp_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "fixp",
	.get_info = fixp_get_info,
	.get_init_info = fixp_get_init_info,
	.start = fixp_start,
	.stop = fixp_stop,
	.get_tuned_again = fixp_get_tuned_again,
	.get_tuned_dgain = fixp_get_tuned_dgain,
	.get_exposure_gain_info = fixp_get_exposure_gain_info,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(fixp_ops)
