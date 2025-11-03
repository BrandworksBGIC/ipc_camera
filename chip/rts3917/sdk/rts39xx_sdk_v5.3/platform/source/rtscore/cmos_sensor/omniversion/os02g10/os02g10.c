/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2021 Martial <howardhuang@realtek.com>
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
#define abs(x) ((x) >= 0 ? (x) : -(x))

struct fps_info {
	uint16_t fps;
	uint16_t hts;
	uint16_t vts;
	uint32_t clk;
};

struct os02g10_status {
	float exp_step;
	float last_exposure;
	uint16_t min_vts;
};

static struct os02g10_status g_status[SUPPORTED_ISP_NUM];
static const struct fps_info g_os02g10_fps_info[] = {
	{30, 2164, 1109, 72000000},
};

static struct rts_isp_i2c_reg g_os02g10_i2c_init_regs1[] = {
	{0xfd, 0x00 },
	{0x36, 0x01 },
	{0xfd, 0x00 },
	{0x36, 0x00 },
	//{0xfd, 0x00 },
	//{0x20, 0x00 },
};

static struct rts_isp_i2c_reg g_os02g10_i2c_init_regs2[] = {
	{0xfd, 0x00 },
	{0xfd, 0x00 },
	{0x30, 0x0a },
	{0x35, 0x04 },
	{0x38, 0x11 },
	{0x41, 0x06 },
	{0x44, 0x20 },
	{0xfd, 0x01 },
	{0x03, 0x04 },
	{0x04, 0x4c },
	{0x06, 0x00 },
	{0x24, 0x30 },
	{0x01, 0x01 },
	{0x19, 0x50 },
	{0x1a, 0x0c },
	{0x1b, 0x0d },
	{0x1c, 0x00 },
	{0x1d, 0x75 },
	{0x1e, 0x52 },
	{0x22, 0x14 },
	{0x25, 0x44 },
	{0x26, 0x0f },
	{0x3c, 0xca },
	{0x3d, 0x4a },
	{0x40, 0x0f },
	{0x43, 0x38 },
	{0x46, 0x00 },
	{0x47, 0x00 },
	{0x49, 0x32 },
	{0x50, 0x01 },
	{0x51, 0x28 },
	{0x52, 0x20 },
	{0x53, 0x03 },
	{0x57, 0x16 },
	{0x59, 0x01 },
	{0x5a, 0x01 },
	{0x5d, 0x04 },
	{0x6a, 0x04 },
	{0x6b, 0x03 },
	{0x6e, 0x28 },
	{0x71, 0xbe },
	{0x72, 0x06 },
	{0x73, 0x38 },
	{0x74, 0x06 },
	{0x79, 0x00 },
	{0x7a, 0xb2 },
	{0x7b, 0x10 },
	{0x8f, 0x80 },
	{0x91, 0x38 },
	{0x92, 0x02 },
	{0x9d, 0x03 },
	{0x9e, 0x55 },
	{0xb8, 0x70 },
	{0xb9, 0x70 },
	{0xba, 0x70 },
	{0xbb, 0x70 },
	{0xbc, 0x00 },
	{0xc0, 0x00 },
	{0xc1, 0x00 },
	{0xc2, 0x00 },
	{0xc3, 0x00 },
	{0xc4, 0x6e },
	{0xc5, 0x6e },
	{0xc6, 0x6b },
	{0xc7, 0x6b },
	{0xcc, 0x11 },
	{0xcd, 0xe0 },
	{0xd0, 0x1b },
	{0xd2, 0x76 },
	{0xd3, 0x68 },
	{0xd4, 0x68 },
	{0xd5, 0x73 },
	{0xd6, 0x73 },
	{0xe8, 0x55 },
	{0xf0, 0x40 },
	{0xf1, 0x40 },
	{0xf2, 0x40 },
	{0xf3, 0x40 },
	{0xf4, 0x00 },
	{0xfa, 0x1c },
	{0xfb, 0x33 },
	{0xfc, 0xff },
	{0xfe, 0x01 },
	{0xfd, 0x03 },
	{0x03, 0x67 },
	{0x00, 0x59 },
	{0x04, 0x11 },
	{0x05, 0x04 },
	{0x06, 0x0c },
	{0x07, 0x08 },
	{0x08, 0x08 },
	{0x09, 0x4f },
	{0x0b, 0x08 },
	{0x0d, 0x26 },
	{0x0f, 0x00 },
	{0xfd, 0x02 },
	{0x34, 0xfe },
	{0x5e, 0x22 },
	{0xa1, 0x07 },
	{0xa3, 0x38 },
	{0xa5, 0x02 },
	{0xa7, 0x80 },
	{0xfd, 0x01 },
	{0xa1, 0x05 },
	{0x94, 0x44 },
	{0x95, 0x44 },
	{0x96, 0x09 },
	{0x98, 0x44 },
	{0x9c, 0x0e },
	{0xb1, 0x01 },
	{0xfd, 0x01 },
	{0xb1, 0x03 },
};

static int os02g10_get_info(uint32_t isp_id,
		struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 1920;
	info->modes.mode[0].size.h = 1080;
	info->modes.mode[0].fps = g_os02g10_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x3c;
	info->i2c.addr_len = 1;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 0);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 0);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V5, 5000);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 0);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_HIGH, 4000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 0);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&down->items[i++], SNR_PWDN_GPIO, GPIO_HIGH, 0);
	set_power_item(&down->items[i++], SNR_HCLK, 0, 0);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_IO_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_CORE_POWER, 0, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static const struct fps_info *os02g10_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_os02g10_fps_info); i++)
		if (fps == g_os02g10_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_os02g10_fps_info))
		return NULL;

	return &g_os02g10_fps_info[i];
}

static int os02g10_get_init_info(uint32_t isp_id,
				 const struct rts_isp_sensor_mode *mode,
				struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct os02g10_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("os02g10 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = os02g10_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, clk_div: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->clk_div, fps_info->hts);

	set_init_i2c_regs(info->sensor_regs[0], g_os02g10_i2c_init_regs1, 5000);
	set_init_i2c_regs(info->sensor_regs[1], g_os02g10_i2c_init_regs2, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0 | MIPI_LANE1;
	info->interface.mipi.hs_term = 0x03;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 1920;
	info->size.h = 1080;
	info->start.x = 0;
	info->start.y = 0;

	info->hts = fps_info->hts;
	info->min_vts = fps_info->vts;
	info->pclk = fps_info->clk;
	info->max_vts = 65536;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */
	status->min_vts =  fps_info->vts;

	return RTS_ISP_OK;
}

static int os02g10_start(uint32_t isp_id)
{
	struct os02g10_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static uint16_t get_sensor_gain_reg(float fgain)
{
	uint16_t reg_value = 0;

	reg_value = (uint16_t)(fgain * 16);
	if (reg_value < 0xf8)
		return  reg_value;
	else
		return 0xf8;
}

static float get_sensor_real_gain(uint8_t reg_value)
{
	return ((float)reg_value / 16.0);
}


static int os02g10_get_tuned_again(uint32_t isp_id,
				   float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int os02g10_get_tuned_dgain(uint32_t isp_id,
				   float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int os02g10_get_exposure_gain_info(uint32_t isp_id,
					 const struct rts_isp_sensor_exp_gain *exp_gain,
					 struct rts_isp_sync_regs *regs)
{
	int i;
	uint16_t gain_reg;
	uint16_t dummy;
	struct os02g10_status *status;
	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	gain_reg = get_sensor_gain_reg(exp_gain->analog_gain[0] *
				       exp_gain->digital_gain[0]);
	reg = regs->reg;

	i = 0;
	if (abs(status->last_exposure - exp_gain->exposure[0]) > 0.001f) {
		uint32_t exposure;

		exposure = exp_gain->exposure[0] / status->exp_step + 0.5f;
		/* page */
		set_sync_i2c(&reg[i++], 0xfd, 1);

		dummy = abs((int)exp_gain->vts - 1109);
		set_sync_i2c(&reg[i++], 0x05, dummy >> 8);
		set_sync_i2c(&reg[i++], 0x06, dummy & 0xff);

		/* gain */
		set_sync_i2c(&reg[i++], 0x24, gain_reg);

		set_sync_i2c(&reg[i++], 3, (exposure >> 8) & 0xff);
		set_sync_i2c(&reg[i++], 4, exposure & 0xff);
		status->last_exposure = exp_gain->exposure[0];

		/* end & launch group1 */
		set_sync_i2c(&reg[i++], 1, 1);
	} else {
		/* page */
		set_sync_i2c(&reg[i++], 0xfd, 1);

		/* gain */
		set_sync_i2c(&reg[i++], 0x24, gain_reg);
		dummy = abs((int)exp_gain->vts - 1109);
		set_sync_i2c(&reg[i++], 0x05, dummy >> 8);
		set_sync_i2c(&reg[i++], 0x06, dummy & 0xff);

		/* end & launch group1 */
		set_sync_i2c(&reg[i++], 1, 1);
	}

	regs->num = i;

	return RTS_ISP_OK;
}

static int os02g10_check(uint32_t isp_id)
{
	int ret;
	int id;
	struct rts_isp_i2c_reg reg;

	/* page */
	reg.addr = 0xfd;
	reg.data = 0x00;
	ret = rts_isp_write_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;

	reg.addr = 0x02;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;

	id = reg.data << 8;

	reg.addr = 0x03;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;

	id |= reg.data;

	if (id == 0x5602)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops os02g10_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "os02g10",
	.get_info = os02g10_get_info,
	.get_init_info = os02g10_get_init_info,
	.start = os02g10_start,
	.get_tuned_again = os02g10_get_tuned_again,
	.get_tuned_dgain = os02g10_get_tuned_dgain,
	.get_exposure_gain_info = os02g10_get_exposure_gain_info,
	.check = os02g10_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(os02g10_ops)
