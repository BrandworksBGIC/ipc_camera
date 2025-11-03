/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2024 Eric Yang <eric_yang@realsil.com.cn>
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
	uint32_t clk;
};

struct ov9282_status {
	float exp_step;
	float last_exposure;
	int min_vts;
};

static struct ov9282_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_ov9282_fps_info[] = {
	{60, 1456, 80000000},
};
static struct rts_isp_i2c_reg g_ov9282_i2c_init_regs[] = {
	{0x0103, 0x01}, {0x0106, 0x00}, {0x0302, 0x1e}, {0x030d, 0x50},
	{0x030e, 0x02}, {0x3001, 0x00}, {0x3004, 0x00}, {0x3005, 0x00},
	{0x3006, 0x04}, {0x3011, 0x0a}, {0x3013, 0x18}, {0x301c, 0xf0},
	{0x3022, 0x01}, {0x3030, 0x10}, {0x3039, 0x32}, {0x303a, 0x00},
	{0x3500, 0x00}, {0x3501, 0x2a}, {0x3502, 0x90}, {0x3503, 0x08},
	{0x3505, 0x8c}, {0x3507, 0x03}, {0x3508, 0x00}, {0x3509, 0x10},
	{0x3610, 0x80}, {0x3611, 0xa0}, {0x3620, 0x6e}, {0x3632, 0x56},
	{0x3633, 0x78}, {0x3662, 0x05}, {0x3666, 0x00}, {0x366f, 0x5a},
	{0x3680, 0x84}, {0x3707, 0x56}, {0x370d, 0x00}, {0x370e, 0xfa},
	{0x3712, 0x80}, {0x372d, 0x22}, {0x3731, 0x80}, {0x3732, 0x30},
	{0x3778, 0x00}, {0x377d, 0x22}, {0x3788, 0x02}, {0x3789, 0xa4},
	{0x378a, 0x00}, {0x378b, 0x4a}, {0x3799, 0x20}, {0x379c, 0x01},
	{0x3800, 0x00}, {0x3801, 0x00}, {0x3802, 0x00}, {0x3803, 0x00},
	{0x3804, 0x05}, {0x3805, 0x0f}, {0x3806, 0x03}, {0x3807, 0x2f},
	{0x3808, 0x05}, {0x3809, 0x00}, {0x380a, 0x03}, {0x380b, 0x20},
	{0x380c, 0x05}, {0x380d, 0xb0}, {0x380e, 0x03}, {0x380f, 0x8e},
	{0x3810, 0x00}, {0x3811, 0x08}, {0x3812, 0x00}, {0x3813, 0x08},
	{0x3814, 0x11}, {0x3815, 0x11}, {0x3820, 0x40}, {0x3821, 0x00},
	{0x382b, 0x3a}, {0x382c, 0x06}, {0x382d, 0xc2}, {0x389d, 0x00},
	{0x3881, 0x42}, {0x3882, 0x02}, {0x3883, 0x12}, {0x3885, 0x07},
	{0x38a8, 0x02}, {0x38a9, 0x80}, {0x38b1, 0x00}, {0x38b3, 0x07},
	{0x38c4, 0x00}, {0x38c5, 0xc0}, {0x38c6, 0x04}, {0x38c7, 0x80},
	{0x3920, 0xff}, {0x4003, 0x40}, {0x4008, 0x04}, {0x4009, 0x0b},
	{0x400c, 0x01}, {0x400d, 0x07}, {0x4010, 0xf0}, {0x4011, 0x3b},
	{0x4043, 0x40}, {0x4307, 0x30}, {0x4317, 0x00}, {0x4501, 0x00},
	{0x4507, 0x00}, {0x4509, 0x00}, {0x450a, 0x08}, {0x4601, 0x04},
	{0x470f, 0x00}, {0x4f07, 0x00}, {0x4800, 0x00}, {0x4837, 0x21},
	{0x5000, 0x9f}, {0x5001, 0x00}, {0x5e00, 0x00}, {0x5d00, 0x07},
	{0x5d01, 0x00}, {0x4f00, 0x0c}, {0x4f10, 0x00}, {0x4f11, 0x88},
	{0x4f12, 0x0f}, {0x4f13, 0xc4}, {0x3501, 0x37}, {0x3502, 0x50},
	{0x0100, 0x01},
};

static int ov9282_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 1280;
	info->modes.mode[0].size.h = 800;
	info->modes.mode[0].fps = g_ov9282_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x10;
	info->i2c.addr_len = 2;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 100);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 100);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 100);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V2, 5000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 5000);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 5000);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_RST_GPIO, 0, 0);
	set_power_item(&down->items[i++], SNR_HCLK, 0, 0);
	set_power_item(&down->items[i++], SNR_CORE_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_IO_POWER, 0, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static const struct fps_info *ov9282_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_ov9282_fps_info); i++)
		if (fps == g_ov9282_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_ov9282_fps_info))
		return NULL;

	return &g_ov9282_fps_info[i];
}

static int ov9282_get_init_info(uint32_t isp_id,
				const struct rts_isp_sensor_mode *mode,
				struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct ov9282_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("ov9282 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = ov9282_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	set_init_i2c_regs(info->sensor_regs[0], g_ov9282_i2c_init_regs, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0 | MIPI_LANE1;
	info->interface.mipi.hs_term = 0x3;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 1280;
	info->size.h = 800;
	info->start.x = 0;
	info->start.y = 0;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = 910;
	info->max_vts = 65535;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */
	status->min_vts = info->min_vts;

	return RTS_ISP_OK;
}

static int ov9282_start(uint32_t isp_id)
{
	struct ov9282_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static uint32_t clip_d_word(uint32_t current,
				uint32_t minimum, uint32_t maximum)
{
	if (current > maximum)
		return maximum;
	if (current < minimum)
		return minimum;
	return current;
}

static uint8_t get_sensor_gain_reg(float fgain)
{
	uint16_t gain = fgain * 16;

	return gain > 255 ? 255 : gain;
}

static float get_sensor_real_gain(uint8_t reg_value)
{
	return (float)reg_value / 16.0f;
}

static int ov9282_get_tuned_again(uint32_t isp_id,
				  float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int ov9282_get_tuned_dgain(uint32_t isp_id,
				  float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int ov9282_get_exposure_gain_info(uint32_t isp_id,
			const struct rts_isp_sensor_exp_gain *exp_gain,
			struct rts_isp_sync_regs *regs)
{
	int i;
	float gain;
	uint32_t vts;
	uint8_t gain_reg;
	uint32_t exposure_rows;
	struct ov9282_status *status;
	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	vts = exp_gain->vts;

	gain = exp_gain->analog_gain[0] * exp_gain->digital_gain[0];
	gain_reg = get_sensor_gain_reg(gain);

	reg = regs->reg;
	exposure_rows = exp_gain->exposure[0] / status->exp_step + 0.5f;
	exposure_rows = clip_d_word(exposure_rows, 1, vts - 25);
	exposure_rows = exposure_rows << 4;

	i = 0;
	if (abs(status->last_exposure - exp_gain->exposure[0]) > 0.001f) {
		set_sync_i2c(&reg[i++], 0x3500, exposure_rows >> 16);
		set_sync_i2c(&reg[i++], 0x3501, exposure_rows >> 8);
		set_sync_i2c(&reg[i++], 0x3502, exposure_rows & 0xff);
		status->last_exposure = exp_gain->exposure[0];
	}
	set_sync_i2c(&reg[i++], 0x380e, vts >> 8);
	set_sync_i2c(&reg[i++], 0x380f, vts & 0xff);
	set_sync_i2c(&reg[i++], 0x3509, gain_reg & 0xff);

	if (gain < 4.0f)
		set_sync_i2c(&reg[i++], 0x38b1, 0x02);
	else if (gain >= 4.0f)
		set_sync_i2c(&reg[i++], 0x38b1, 0x00);

	regs->num = i;

	return RTS_ISP_OK;
}

static int ov9282_check(uint32_t isp_id)
{
	int ret;
	int id;
	struct rts_isp_i2c_reg reg = {};

	reg.addr = 0x300a;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id = reg.data << 8;

	reg.addr = 0x300b;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id |= reg.data;

	if (id == 0x9281)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops ov9282_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "ov9282",
	.get_info = ov9282_get_info,
	.get_init_info = ov9282_get_init_info,
	.start = ov9282_start,
	.get_tuned_again = ov9282_get_tuned_again,
	.get_tuned_dgain = ov9282_get_tuned_dgain,
	.get_exposure_gain_info = ov9282_get_exposure_gain_info,
	.check = ov9282_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(ov9282_ops)
