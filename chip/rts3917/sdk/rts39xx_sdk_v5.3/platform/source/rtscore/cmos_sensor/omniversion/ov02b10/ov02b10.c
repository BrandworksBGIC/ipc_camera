/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2024 george <george_liu@realsil.com.cn>
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

struct ov02b10_status {
	float exp_step;
	float last_exposure;
	uint16_t min_vts;
};

static struct ov02b10_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_ov02b10_fps_info[] = {
	{30, 1792, 1221, 65640960},
};

static struct rts_isp_i2c_reg g_ov02b10_i2c_init_regs1[] = {
	{0xfc, 0x01}, //soft reset //;delay 5ms
};

static struct rts_isp_i2c_reg g_ov02b10_i2c_init_regs2[] = {
	{0x05, 0x05},
	{0xfd, 0x00},
	{0x24, 0x02},
	{0x25, 0x06},
	{0x28, 0x00},//fae suggest
	{0x29, 0x01},//fae suggest
	//{0x29, 0x03},
	{0x2a, 0xb4},
	{0x2b, 0x00},
	{0x1e, 0x17},
	{0x33, 0x07},
	{0x35, 0x07},
	{0x4a, 0x0c},
	{0x3a, 0x05},
	{0x3b, 0x02},
	{0x3e, 0x00},
	{0x46, 0x01},
	{0x6d, 0x03},
	{0xfd, 0x01},
	{0x0e, 0x02},
	{0x0f, 0x1a},
	{0x18, 0x00},
	{0x22, 0xff},
	{0x23, 0x02},
	{0x17, 0x2c},
	{0x19, 0x20},
	{0x1b, 0x06},
	{0x1c, 0x04},
	{0x20, 0x03},
	{0x30, 0x01},
	{0x33, 0x01},
	{0x31, 0x0a},
	{0x32, 0x09},
	{0x38, 0x01},
	{0x39, 0x01},
	{0x3a, 0x01},
	{0x3b, 0x01},
	{0x4f, 0x04},
	{0x4e, 0x05},
	{0x50, 0x01},
	{0x35, 0x0c},
	{0x45, 0x2a},
	{0x46, 0x2a},
	{0x47, 0x2a},
	{0x48, 0x2a},
	{0x4a, 0x2c},
	{0x4b, 0x2c},
	{0x4c, 0x2c},
	{0x4d, 0x2c},
	{0x56, 0x3a},
	{0x57, 0x0a},
	{0x58, 0x24},
	{0x59, 0x20},
	{0x5a, 0x0a},
	{0x5b, 0xff},
	{0x37, 0x0a},
	{0x42, 0x0e},
	{0x68, 0x90},
	{0x69, 0xcd},
	{0x6a, 0x8f},
	{0x7c, 0x0a},
	{0x7d, 0x09},
	{0x7e, 0x09},
	{0x7f, 0x08},
	{0x83, 0x14},
	{0x84, 0x14},
	{0x86, 0x14},
	{0x87, 0x07},
	{0x88, 0x0f},
	{0x94, 0x02},
	{0x98, 0xd1},
	{0xfe, 0x02},
	{0xfd, 0x03},
	{0x97, 0x78},
	{0x98, 0x78},
	{0x99, 0x78},
	{0x9a, 0x78},
	{0xa1, 0x40},
	{0xb1, 0x30},
	{0xae, 0x0d},
	{0x88, 0x5b},
	{0x89, 0x7c},
	{0xb4, 0x05},
	{0x8c, 0x40},
	{0x8e, 0x40},
	{0x90, 0x40},
	{0x92, 0x40},
	{0x9b, 0x46},
	{0xac, 0x40},
	{0xfd, 0x00},
	{0x5a, 0x15},
	{0x74, 0x01},
	{0xfd, 0x00},
	{0x50, 0x40},
	{0x52, 0xb0},
	{0xfd, 0x01},
	{0x03, 0x70},
	{0x05, 0x11},
	{0x07, 0x20},
	{0x09, 0xb0},
	{0xfb, 0x01},
	{0xfd, 0x03},
	{0xc2, 0x01}, //;MIPI_EN
	{0xfd, 0x01},
};

static int ov02b10_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;

	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 1600;
	info->modes.mode[0].size.h = 1200;
	info->modes.mode[0].fps = g_ov02b10_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x3c;
	info->i2c.addr_len = 1;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 0);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 5000);
	//set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V2, 7000);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 1000);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_HIGH, 4000);
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

static const struct fps_info *ov02b10_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_ov02b10_fps_info); i++)
		if (fps == g_ov02b10_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_ov02b10_fps_info))
		return NULL;

	return &g_ov02b10_fps_info[i];
}

static int ov02b10_get_init_info(uint32_t isp_id,
				 const struct rts_isp_sensor_mode *mode,
				struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct ov02b10_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("ov02b10 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = ov02b10_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, clk_div: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->clk_div, fps_info->hts);

	set_init_i2c_regs(info->sensor_regs[0], g_ov02b10_i2c_init_regs1, 5000);
	set_init_i2c_regs(info->sensor_regs[1], g_ov02b10_i2c_init_regs2, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0;
	info->interface.mipi.hs_term = 0x5;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 1600;
	info->size.h = 1200;
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

static int ov02b10_start(uint32_t isp_id)
{
	struct ov02b10_status *status;

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

static int ov02b10_get_tuned_again(uint32_t isp_id,
				   float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int ov02b10_get_tuned_dgain(uint32_t isp_id,
				   float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int ov02b10_get_exposure_gain_info(uint32_t isp_id,
			 const struct rts_isp_sensor_exp_gain *exp_gain,
					 struct rts_isp_sync_regs *regs)
{
	int i;
	uint16_t gain_reg;
	uint16_t dummy;
	uint16_t exp_reg_value;
	uint16_t total_line;
	struct ov02b10_status *status;
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

		/*set vts*/
		total_line = exp_gain->vts;
		dummy = exp_gain->vts - status->min_vts;
		set_sync_i2c(&reg[i++], 0x14, dummy >> 8);
		set_sync_i2c(&reg[i++], 0x15, dummy & 0xff);

		/* gain */
		set_sync_i2c(&reg[i++], 0x22, gain_reg);

		/* exposure */
		exp_reg_value = clip_d_word(exposure, 4, total_line - 7);
		set_sync_i2c(&reg[i++], 0x0e, (exp_reg_value >> 8) & 0xff);
		set_sync_i2c(&reg[i++], 0x0f, exp_reg_value & 0xff);
		status->last_exposure = exp_gain->exposure[0];

		/* end & launch group1 */
		set_sync_i2c(&reg[i++], 0xfe, 0x02);
	} else {
		/* page */
		set_sync_i2c(&reg[i++], 0xfd, 1);

		/* gain */
		set_sync_i2c(&reg[i++], 0x22, gain_reg);

		/*set vts*/
		dummy = exp_gain->vts - status->min_vts;
		set_sync_i2c(&reg[i++], 0x14, dummy >> 8);
		set_sync_i2c(&reg[i++], 0x15, dummy & 0xff);

		/* end & launch group1 */
		set_sync_i2c(&reg[i++], 0xfe, 0x02);
	}

	regs->num = i;

	return RTS_ISP_OK;
}

static int ov02b10_check(uint32_t isp_id)
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

	if (id == 0x002b)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops ov02b10_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "ov02b10",
	.get_info = ov02b10_get_info,
	.get_init_info = ov02b10_get_init_info,
	.start = ov02b10_start,
	.get_tuned_again = ov02b10_get_tuned_again,
	.get_tuned_dgain = ov02b10_get_tuned_dgain,
	.get_exposure_gain_info = ov02b10_get_exposure_gain_info,
	.check = ov02b10_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(ov02b10_ops)
