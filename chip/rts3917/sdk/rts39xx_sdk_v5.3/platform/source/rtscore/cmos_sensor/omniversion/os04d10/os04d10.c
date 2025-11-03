/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2025 Eric Yang <eric_yang@realsil.com.cn>
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
	uint32_t hts;
	uint32_t clk;
};

struct os04d10_status {
	int min_vts;
	float exp_step;
	float last_exposure;
};

static struct os04d10_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_os04d10_fps_info[] = {
	{30, 2898, 144000000},
};
static struct rts_isp_i2c_reg g_os04d10_i2c_init_regs[] = {
	{0xfd, 0x00}, {0x20, 0x00}, {0x20, 0x01}, {0x20, 0x01},
	{0x20, 0x01}, {0x20, 0x01}, {0x41, 0xa8}, {0x45, 0x24},
	{0x31, 0x20}, {0x38, 0x15}, {0xfd, 0x01}, {0x03, 0x01},
	{0x04, 0x04}, {0x24, 0x80}, {0x02, 0x01}, {0x42, 0x5a},
	{0x47, 0x0c}, {0x45, 0x02}, {0x48, 0x0c}, {0x4b, 0x88},
	{0xd4, 0x05}, {0xd5, 0xd2}, {0xd7, 0x05}, {0xd8, 0xd2},
	{0x50, 0x01}, {0x51, 0x11}, {0x52, 0x18}, {0x53, 0x01},
	{0x54, 0x01}, {0x55, 0x01}, {0x57, 0x08}, {0x5c, 0x40},
	{0x7c, 0x06}, {0x7d, 0x05}, {0x7e, 0x05}, {0x7f, 0x05},
	{0x90, 0x60}, {0x91, 0x0f}, {0x92, 0x35}, {0x93, 0x36},
	{0x94, 0x0f}, {0x95, 0x7e}, {0x98, 0x5d}, {0xa8, 0x50},
	{0xaa, 0x14}, {0xab, 0x05}, {0xac, 0x14}, {0xad, 0x05},
	{0xae, 0x4a}, {0xaf, 0x0e}, {0xb2, 0x07}, {0xb3, 0x0c},
	{0xc9, 0x28}, {0xca, 0x5e}, {0xcb, 0x5e}, {0xcc, 0x5e},
	{0xcd, 0x5e}, {0xce, 0x5c}, {0xcf, 0x5c}, {0xd0, 0x5c},
	{0xd1, 0x5c}, {0xd2, 0x7c}, {0xd3, 0x7c}, {0xdb, 0x0f},
	{0xfd, 0x01}, {0x46, 0x77}, {0xdd, 0x00}, {0xde, 0x3f},
	{0xfd, 0x03}, {0x2b, 0x0a}, {0x01, 0x22}, {0x02, 0x03},
	{0x00, 0x06}, {0x2a, 0x22}, {0x29, 0x0b}, {0x1e, 0x10},
	{0x1f, 0x02}, {0x1a, 0x24}, {0x1b, 0x62}, {0x1c, 0xce},
	{0x1d, 0xd3}, {0x04, 0x0f}, {0x36, 0x00}, {0x37, 0x05},
	{0x38, 0x09}, {0x39, 0x19}, {0x3a, 0x38}, {0x3b, 0x22},
	{0x3c, 0x22}, {0x3d, 0x22}, {0x3e, 0x03}, {0xfd, 0x02},
	{0x8e, 0x0a}, {0x8f, 0x00}, {0x90, 0x05}, {0x91, 0xa0},
	{0xce, 0x65}, {0xfd, 0x03}, {0x03, 0x30}, {0x05, 0x00},
	{0x12, 0x70}, {0x13, 0x70}, {0x16, 0x13}, {0x21, 0xca},
	{0x27, 0x95}, {0x2c, 0x55}, {0x2d, 0x08}, {0x2e, 0xca},
	{0x3f, 0xe7}, {0xfd, 0x00}, {0x8b, 0x01}, {0x8d, 0x00},
	{0xfd, 0x01}, {0x01, 0x02}, {0xfd, 0x05}, {0xc4, 0x62},
	{0xc5, 0x62}, {0xc6, 0x62}, {0xc7, 0x62}, {0xf0, 0x40},
	{0xf1, 0x40}, {0xf2, 0x40}, {0xf3, 0x40}, {0xf4, 0x00},
	{0xf9, 0x03}, {0xfa, 0x5d}, {0xfb, 0x6b}, {0xb1, 0x01},
	{0xfd, 0x00}, {0x20, 0x03}, {0xfd, 0x02}, {0x5e, 0x32},
	{0xfd, 0x02}, {0xa0, 0x00}, {0xa1, 0x03}, {0xa2, 0x05},
	{0xa3, 0xa0}, {0xa4, 0x00}, {0xa5, 0x04}, {0xa6, 0x0a},
	{0xa7, 0x00}, {0xfd, 0x01}, {0x32, 0x01}, {0x01, 0x01},
	{0xfd, 0x01}, {0x32, 0x02}, {0x01, 0x01}, {0xfd, 0x01},
	{0x32, 0x03}, {0x01, 0x01}, {0xfd, 0x01}, {0x32, 0x03},
	{0x01, 0x01},
};

static int os04d10_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 2560;
	info->modes.mode[0].size.h = 1440;
	info->modes.mode[0].fps = g_os04d10_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x3c;
	info->i2c.addr_len = 1;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 1000);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 1000);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 1000);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V2, 5000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 5000);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 20000);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_HCLK, 0, 1000);
	set_power_item(&down->items[i++], SNR_RST_GPIO, 0, 0);
	set_power_item(&down->items[i++], SNR_CORE_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_IO_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_PWDN_GPIO, 0, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static const struct fps_info *os04d10_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_os04d10_fps_info); i++)
		if (fps == g_os04d10_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_os04d10_fps_info))
		return NULL;

	return &g_os04d10_fps_info[i];
}

static int os04d10_get_init_info(uint32_t isp_id,
				 const struct rts_isp_sensor_mode *mode,
				struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct os04d10_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("os04d10 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = os04d10_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->hts);

	set_init_i2c_regs(info->sensor_regs[0], g_os04d10_i2c_init_regs, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0 | MIPI_LANE1;
	info->interface.mipi.hs_term = 0x07;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 2560;
	info->size.h = 1440;
	info->start.x = 0;
	info->start.y = 0;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = 1656;
	info->max_vts = 65536;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */
	status->min_vts = info->min_vts;

	return RTS_ISP_OK;
}

static int os04d10_start(uint32_t isp_id)
{
	struct os04d10_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static uint16_t get_sensor_gain_reg(float fgain)
{
	uint16_t reg_value = 0;

	reg_value = (uint16_t)(fgain * 16.0f);
	if (reg_value < 0xf8)
		return  reg_value;
	else
		return 0xf8;
}

static float get_sensor_real_gain(uint8_t reg_value)
{
	return ((float)reg_value / 16.0f);
}

uint32_t clip_d_word(uint32_t current, uint32_t minimum, uint32_t maximum)
{
	if (current > maximum)
		return maximum;
	if (current < minimum)
		return minimum;
	return current;
}

static int os04d10_get_tuned_again(uint32_t isp_id,
				   float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int os04d10_get_tuned_dgain(uint32_t isp_id,
				   float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int os04d10_get_exposure_gain_info(uint32_t isp_id,
				 const struct rts_isp_sensor_exp_gain *exp_gain,
				 struct rts_isp_sync_regs *regs)
{
	int i;
	uint16_t gain_reg;
	uint16_t dummy;
	uint16_t exp_reg_value;
	uint32_t exposure;
	struct os04d10_status *status;
	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	gain_reg = get_sensor_gain_reg(exp_gain->analog_gain[0] *
				       exp_gain->digital_gain[0]);
	reg = regs->reg;

	i = 0;
	if (abs(status->last_exposure - exp_gain->exposure[0]) > 0.001f) {

		exposure = exp_gain->exposure[0] / status->exp_step + 0.5f;

		/*set vts*/
		dummy = abs((int)exp_gain->vts - status->min_vts);
		/* page */
		set_sync_i2c(&reg[i++], 0xfd, 1);
		set_sync_i2c(&reg[i++], 0x05, dummy >> 8);
		set_sync_i2c(&reg[i++], 0x06, dummy & 0xff);
		/* gain */
		set_sync_i2c(&reg[i++], 0x24, gain_reg);
		/* exposure */
		exp_reg_value = clip_d_word(exposure, 1, exp_gain->vts - 16);
		set_sync_i2c(&reg[i++], 0x03, (exp_reg_value >> 8) & 0xff);
		set_sync_i2c(&reg[i++], 0x04, exp_reg_value & 0xff);
		status->last_exposure = exp_gain->exposure[0];

		/* end & launch group1 */
		set_sync_i2c(&reg[i++], 1, 1);
	} else {
		/* page */
		set_sync_i2c(&reg[i++], 0xfd, 1);
		/* gain */
		set_sync_i2c(&reg[i++], 0x24, gain_reg);
		/*set vts*/
		dummy = abs((int)exp_gain->vts - status->min_vts);
		set_sync_i2c(&reg[i++], 0x05, dummy >> 8);
		set_sync_i2c(&reg[i++], 0x06, dummy & 0xff);

		/* end & launch group1 */
		set_sync_i2c(&reg[i++], 1, 1);
	}
	regs->num = i;

	return RTS_ISP_OK;
}

static int os04d10_check(uint32_t isp_id)
{
	int ret;
	int id;
	struct rts_isp_i2c_reg reg;

	reg.addr = 0x04;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id = reg.data << 8;

	reg.addr = 0x05;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id |= reg.data;

	if (id == 0x4410)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops os04d10_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "os04d10",
	.get_info = os04d10_get_info,
	.get_init_info = os04d10_get_init_info,
	.start = os04d10_start,
	.get_tuned_again = os04d10_get_tuned_again,
	.get_tuned_dgain = os04d10_get_tuned_dgain,
	.get_exposure_gain_info = os04d10_get_exposure_gain_info,
	.check = os04d10_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(os04d10_ops)
