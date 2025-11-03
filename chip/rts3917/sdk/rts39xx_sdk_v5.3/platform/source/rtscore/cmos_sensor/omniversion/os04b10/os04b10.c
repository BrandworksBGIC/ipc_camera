/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2017 Martial <martial_wu@realsil.com.cn>
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

struct os04b10_status {
	float exp_step;
	float last_exposure;
	uint16_t min_vts;
};

static struct os04b10_status g_status[SUPPORTED_ISP_NUM];
static const struct fps_info g_os04b10_fps_info[] = {
	{30, 3168, 1500, 142560000},
};

static struct rts_isp_i2c_reg g_os04b10_i2c_init_regs1[] = {
	{0xfd, 0x00,},
	{0x20, 0x00,},
};

static struct rts_isp_i2c_reg g_os04b10_i2c_init_regs2[] = {
	{0xfd, 0x00,},
	{0x34, 0x71,},
	{0x32, 0x01,},
	{0x33, 0x01,},
	{0x2e, 0x0c,},
	{0xfd, 0x01,},
	{0x03, 0x01,},
	{0x04, 0xc6,},
	{0x06, 0x0b,},
	{0x0a, 0x50,},
	{0x38, 0x20,},
	{0x39, 0x08,},
	{0x31, 0x01,},
	{0x24, 0xff,},
	{0x01, 0x01,},
	{0x11, 0x59,},
	{0x13, 0xf4,},
	{0x14, 0xff,},
	{0x19, 0xf2,},
	{0x16, 0x68,},
	{0x1a, 0x5e,},
	{0x1c, 0x1a,},
	{0x1d, 0xd6,},
	{0x1f, 0x17,},
	{0x20, 0x99,},
	{0x26, 0x76,},
	{0x27, 0x0c,},
	{0x29, 0x3b,},
	{0x2a, 0x00,},
	{0x2b, 0x8e,},
	{0x2c, 0x0b,},
	{0x2e, 0x02,},
	{0x44, 0x03,},
	{0x45, 0xbe,},
	{0x50, 0x06,},
	{0x51, 0x10,},
	{0x52, 0x0d,},
	{0x53, 0x08,},
	{0x55, 0x15,},
	{0x56, 0x00,},
	{0x57, 0x09,},
	{0x59, 0x00,},
	{0x5a, 0x04,},
	{0x5b, 0x00,},
	{0x5c, 0xe0,},
	{0x5d, 0x00,},
	{0x65, 0x00,},
	{0x67, 0x00,},
	{0x66, 0x2a,},
	{0x68, 0x2c,},
	{0x69, 0x0c,},
	{0x6a, 0x0a,},
	{0x6b, 0x03,},
	{0x6c, 0x18,},
	{0x71, 0x42,},
	{0x72, 0x04,},
	{0x73, 0x30,},
	{0x74, 0x03,},
	{0x77, 0x28,},
	{0x7b, 0x00,},
	{0x7f, 0x18,},
	{0x83, 0xf0,},
	{0x85, 0x10,},
	{0x86, 0xf0,},
	{0x8a, 0x33,},
	{0x8b, 0x33,},
	{0x28, 0x04,},
	{0x34, 0x00,},
	{0x35, 0x08,},
	{0x36, 0x0a,},
	{0x37, 0x08,},
	{0x4a, 0x00,},
	{0x4b, 0x04,},
	{0x4c, 0x05,},
	{0x4d, 0xa8,},
	{0x01, 0x01,},
	{0x8e, 0x0a,},
	{0x8f, 0x08,},
	{0x90, 0x05,},
	{0x91, 0xa8,},
	{0xa1, 0x04,},
	{0xc4, 0x80,},
	{0xc5, 0x80,},
	{0xc6, 0x80,},
	{0xc7, 0x80,},
	{0xfb, 0x00,},
	{0xf0, 0x40,},
	{0xf1, 0x40,},
	{0xf2, 0x40,},
	{0xf3, 0x40,},
	{0xb1, 0x01,},
	{0xb6, 0x80,},
	{0xfd, 0x00,},
	{0x36, 0x01,},
	{0x34, 0x72,},
	{0x34, 0x71,},
	{0x36, 0x00,},
	{0xfd, 0x01,},
	{0xfb, 0x03,},
	{0xfd, 0x03,},
	{0xc0, 0x01,},
	{0xfd, 0x02,},
	{0xa8, 0x01,},
	{0xa9, 0x00,},
	{0xaa, 0x08,},
	{0xab, 0x00,},
	{0xac, 0x08,},
	{0xad, 0x05,},
	{0xae, 0xa8,},
	{0xaf, 0x0a,},
	{0xb0, 0x08,},
	{0x62, 0x09,},
	{0x63, 0x00,},
	{0xfd, 0x01,},
	{0xb1, 0x03,},
#if 0
	{0xfd, 0x01,},//@@ 1 11 Mirror_On_Flip_Off
	{0x3f, 0x01,},// 0x03,
	{0x01, 0x01,},
	{0xfd, 0x02,},
	{0x62, 0x09,},
	{0x63, 0x00,},
	{0xfd, 0x01,},
	{0xfd, 0x01,},//@@ 1 14 Mirror_Off_Flip_On
	{0x3f, 0x02,},// 0x03
	{0x01, 0x01,},
	{0xfd, 0x02,},
	{0x62, 0xa8,},
	{0x63, 0x05,},
	{0xfd, 0x01,},
	{0xfd, 0x01,},//@@ 1 12 Mirror_On_Flip_On
	{0x3f, 0x03,},// 0x03,
	{0x01, 0x01,},
	{0xfd, 0x02,},
	{0x62, 0xa8,},
	{0x63, 0x05,},
	{0xfd, 0x01,},
	{0xfd, 0x01,},//@@ 1 13 Normal
	{0x3f, 0x00,},// 0x03,
	{0x01, 0x01,},
	{0xfd, 0x02,},
	{0x62, 0x09,},
	{0x63, 0x00,},
	{0xfd, 0x01,},
#endif
};

static int os04b10_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;

	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 2560;
	info->modes.mode[0].size.h = 1440;
	info->modes.mode[0].fps = g_os04b10_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x3c;
	info->i2c.addr_len = 1;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 0);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 0);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V2, 7000);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 1000);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_HIGH, 0);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 9000);
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

static const struct fps_info *os04b10_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_os04b10_fps_info); i++)
		if (fps == g_os04b10_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_os04b10_fps_info))
		return NULL;

	return &g_os04b10_fps_info[i];
}

static int os04b10_get_init_info(uint32_t isp_id,
				 const struct rts_isp_sensor_mode *mode,
				struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct os04b10_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("os04b10 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = os04b10_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, clk_div: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->clk_div, fps_info->hts);

	set_init_i2c_regs(info->sensor_regs[0], g_os04b10_i2c_init_regs1, 5000);
	set_init_i2c_regs(info->sensor_regs[1], g_os04b10_i2c_init_regs2, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0 | MIPI_LANE1;
	info->interface.mipi.hs_term = 0x2;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 2568;
	info->size.h = 1448;
	info->start.x = 1;
	info->start.y = 1;

	info->hts = fps_info->hts;
	info->min_vts = fps_info->vts;
	info->pclk = fps_info->clk;
	info->max_vts = 65536;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */
	status->min_vts =  fps_info->vts;

	return RTS_ISP_OK;
}

static int os04b10_start(uint32_t isp_id)
{
	struct os04b10_status *status;

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

static int os04b10_get_tuned_again(uint32_t isp_id,
				   float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int os04b10_get_tuned_dgain(uint32_t isp_id,
				   float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int os04b10_get_exposure_gain_info(uint32_t isp_id,
					 const struct rts_isp_sensor_exp_gain *exp_gain,
					 struct rts_isp_sync_regs *regs)
{
	int i;
	uint16_t gain_reg;
	uint16_t dummy;
	struct os04b10_status *status;
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

		dummy = abs((int)exp_gain->vts - 1489);
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

		dummy = abs((int)exp_gain->vts - 1489);
		set_sync_i2c(&reg[i++], 0x05, dummy >> 8);
		set_sync_i2c(&reg[i++], 0x06, dummy & 0xff);

		/* end & launch group1 */
		set_sync_i2c(&reg[i++], 1, 1);
	}

	regs->num = i;

	return RTS_ISP_OK;
}

static int os04b10_check(uint32_t isp_id)
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

	if (id == 0x4308)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops os04b10_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "os04b10",
	.get_info = os04b10_get_info,
	.get_init_info = os04b10_get_init_info,
	.start = os04b10_start,
	.get_tuned_again = os04b10_get_tuned_again,
	.get_tuned_dgain = os04b10_get_tuned_dgain,
	.get_exposure_gain_info = os04b10_get_exposure_gain_info,
	.check = os04b10_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(os04b10_ops)
