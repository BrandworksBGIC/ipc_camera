/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2023 Eric Yang <eric_yang@realsil.com.cn>
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

struct mis4001_status {
	float exp_step;
	float last_exposure;
	uint16_t cur_fps;
	uint16_t min_vts;
	struct rts_isp_i2c_reg regs1[2];
};

static struct mis4001_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_mis4001_fps_info[] = {
	{30, 3000, 135000000},
};

static struct rts_isp_i2c_reg g_mis4001_30fps_i2c_init_regs[] = {
	{0x300a, 0x01},
	{0x3006, 0x02},
	{0x4240, 0x8c},
	{0x4103, 0xff},
	{0x3f00, 0x01},
	{0x3f02, 0x07},
	{0x3f01, 0x00},
	{0x3f04, 0x2a},
	{0x3f03, 0x00},
	{0x3f06, 0x6d},
	{0x3f05, 0x04},
	{0x3f08, 0xff},
	{0x3f07, 0x1f},
	{0x3f0a, 0xa4},
	{0x3f09, 0x01},
	{0x3f0c, 0x38},
	{0x3f0b, 0x00},
	{0x3f0e, 0xff},
	{0x3f0d, 0x1f},
	{0x3f10, 0xff},
	{0x3f0f, 0x1f},
	{0x3f13, 0x07},
	{0x3f12, 0x00},
	{0x3f15, 0x9d},
	{0x3f14, 0x01},
	{0x3f17, 0x31},
	{0x3f16, 0x00},
	{0x3f19, 0x73},
	{0x3f18, 0x01},
	{0x3f1b, 0x00},
	{0x3f1a, 0x00},
	{0x3f1d, 0x71},
	{0x3f1c, 0x04},
	{0x3f1f, 0xff},
	{0x3f1e, 0x1f},
	{0x3f21, 0xff},
	{0x3f20, 0x1f},
	{0x3f23, 0x85},
	{0x3f22, 0x00},
	{0x3f25, 0x26},
	{0x3f24, 0x01},
	{0x3f28, 0x46},
	{0x3f27, 0x00},
	{0x3f2a, 0x07},
	{0x3f29, 0x00},
	{0x3f2c, 0x3f},
	{0x3f2b, 0x00},
	{0x3f2e, 0x6f},
	{0x3f2d, 0x01},
	{0x3f30, 0x38},
	{0x3f2f, 0x00},
	{0x3f32, 0x3f},
	{0x3f31, 0x00},
	{0x3f34, 0xd1},
	{0x3f33, 0x00},
	{0x3f36, 0xc5},
	{0x3f35, 0x00},
	{0x3f3a, 0x73},
	{0x3f39, 0x02},
	{0x3f4f, 0x73},
	{0x3f4e, 0x02},
	{0x3f51, 0x73},
	{0x3f50, 0x02},
	{0x3f53, 0x73},
	{0x3f52, 0x02},
	{0x3f55, 0x73},
	{0x3f54, 0x02},
	{0x3f3c, 0x9a},
	{0x3f3b, 0x00},
	{0x3f3e, 0xd0},
	{0x3f3d, 0x03},
	{0x3f40, 0x92},
	{0x3f3f, 0x01},
	{0x3f42, 0x58},
	{0x3f41, 0x00},
	{0x3f44, 0x77},
	{0x3f43, 0x04},
	{0x3129, 0x38},
	{0x3128, 0x00},
	{0x312b, 0x3d},
	{0x312a, 0x00},
	{0x312f, 0x91},
	{0x312e, 0x00},
	{0x3124, 0x0e},
	{0x4200, 0x09},
	{0x4201, 0x00},
	{0x4214, 0x60},
	{0x420c, 0x50},
	{0x4104, 0xf8},
	{0x420e, 0x69},
	{0x420f, 0x26},
	{0x4240, 0x8d},
	{0x4242, 0x03},
	{0x4224, 0x00},
	{0x4225, 0x0a},
	{0x4226, 0xa0},
	{0x4227, 0x05},
	{0x4228, 0x00},
	{0x4229, 0x0a},
	{0x422a, 0xa0},
	{0x422b, 0x05},
	{0x422c, 0x00},
	{0x422d, 0x0a},
	{0x422e, 0xa0},
	{0x422f, 0x05},
	{0x4230, 0x00},
	{0x4231, 0x0a},
	{0x4232, 0xa0},
	{0x4233, 0x05},
	{0x4509, 0x0f},
	{0x4505, 0x00},
	{0x4501, 0xff},
	{0x4502, 0x33},
	{0x4503, 0x11},
	{0x4501, 0xf0},
	{0x4502, 0x30},
	{0x4503, 0x10},
	{0x3f3a, 0x2a},
	{0x3f4f, 0x2b},
	{0x3f51, 0x2e},
	{0x3f53, 0x34},
	{0x3f55, 0x38},
	{0x4004, 0x00},
	{0x3A01, 0xc0},
	{0x401E, 0x3C},
	{0x401d, 0xa0},
	{0x3f49, 0x70},
	{0x3012, 0x03},
	{0x3500, 0x13},
	{0x3501, 0x03},
	{0x3E00, 0x00},
	{0x3E01, 0x10},
	{0x400D, 0x30},
	{0x3508, 0x04},
	{0x3513, 0x01},
	{0x3514, 0x09},
	{0x3515, 0x0b},
	{0x3702, 0x80},
	{0x3704, 0x80},
	{0x3706, 0x80},
	{0x3708, 0x80},
	{0x3f36, 0xcd},
	{0x400D, 0x30},
	{0x4004, 0x00},
	{0x4009, 0x09},
	{0x400a, 0x48},
	{0x3f0c, 0x30},
	{0x4006, 0x86},
	{0x4007, 0xc4},
	{0x3f38, 0x30},
	{0x3f37, 0x02},
	{0x4004, 0x20},
	{0x4005, 0x0c},
	{0x3f0c, 0x20},
	{0x3306, 0x01},
	{0x3307, 0x78},
	{0x3309, 0x01},
	{0x3308, 0x03},
	{0x3302, 0x00},
	{0x330a, 0x04},
	{0x330b, 0x09},
	{0x3307, 0x64},
	{0x3302, 0x02},
	{0x4220, 0x2b},
	{0x4221, 0x6b},
	{0x4222, 0xab},
	{0x4223, 0xeb},
	{0x3011, 0x2b},
	{0x310f, 0xb8},
	{0x310e, 0x0b},
	{0x310d, 0xdc},
	{0x310c, 0x05},
	{0x3115, 0x10},
	{0x3114, 0x00},
	{0x3117, 0x0f},
	{0x3116, 0x0a},
	{0x3111, 0xfc},
	{0x3110, 0x00},
	{0x3113, 0x9d},
	{0x3112, 0x06},
	{0x3a01, 0xa0},
	{0x3006, 0x00},
};

static int mis4001_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 2560;
	info->modes.mode[0].size.h = 1440;
	info->modes.mode[0].fps = g_mis4001_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x30;
	info->i2c.addr_len = 2;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_HIGH, 0);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 0);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 1000);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V2, 1000);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 5000);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_27M, 5000);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_HCLK, 0, 0);
	set_power_item(&down->items[i++], SNR_IO_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_CORE_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, 0, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static const struct fps_info *mis4001_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_mis4001_fps_info); i++)
		if (fps == g_mis4001_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_mis4001_fps_info))
		return NULL;

	return &g_mis4001_fps_info[i];
}

static int mis4001_get_init_info(uint32_t isp_id,
				 const struct rts_isp_sensor_mode *mode,
			       struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct mis4001_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("mis4001 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = mis4001_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, clk_div: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->clk_div, fps_info->hts);


	set_init_i2c(&status->regs1[0], 0x310f, fps_info->hts & 0xff);
	set_init_i2c(&status->regs1[1], 0x310e, fps_info->hts >> 8);

	set_init_i2c_regs(info->sensor_regs[0],
		g_mis4001_30fps_i2c_init_regs, 0);

	set_init_i2c_regs(info->sensor_regs[1], status->regs1, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0 | MIPI_LANE1;
	info->interface.mipi.hs_term = 0x6;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 2560;
	info->size.h = 1440;
	info->start.x = 0;
	info->start.y = 0;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = status->min_vts = 1500;
	info->max_vts = 65535;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */
	status->cur_fps = mode->fps;

	return RTS_ISP_OK;
}

static int mis4001_start(uint32_t isp_id)
{
	struct mis4001_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static uint16_t get_sensor_gain_reg(float fgain)
{
	uint16_t reg_value = 0;

		if (fgain >= 15.75f)
		reg_value = 0x7f;
		else if (fgain >= 8.0f)
		reg_value = (uint16_t)(0x60 + (fgain - 8.0f)*4.0f);
		else if (fgain >= 4.0f)
		reg_value = (uint16_t)(0x40 + (fgain - 4.0f)*8.0f);
		else if (fgain >= 2.0f)
		reg_value = (uint16_t)(0x20 + (fgain - 2.0f)*16.0f);
		else
		reg_value = (uint16_t)((fgain - 1.0f)*32.0f);

	return reg_value;
}

static float get_sensor_real_gain(uint16_t reg_value)
{
	float gain = 1.0f;

		if (reg_value >= 0x7f)
		gain = 15.75f;
		else if (reg_value >= 0x60)
		gain = (float)8.0f*(0x20+(reg_value&0x1f))/32.0f;
		else if (reg_value >= 0x40)
		gain = (float)4.0f*(0x20+(reg_value&0x1f))/32.0f;
		else if (reg_value >= 0x20)
		gain = (float)2.0f*(0x20+(reg_value&0x1f))/32.0f;
		else
		gain = (float)1.0f*(0x20+(reg_value&0x1f))/32.0f;

	return gain;
}

static uint32_t clip_d_word(uint32_t current, uint32_t minimum,
			    uint32_t maximum)
{
	if (current > maximum)
		return maximum;
	if (current < minimum)
		return minimum;
	return current;
}

static int mis4001_get_tuned_again(uint32_t isp_id,
				   float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int mis4001_get_tuned_dgain(uint32_t isp_id,
				   float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int mis4001_get_exposure_gain_info(uint32_t isp_id,
				const struct rts_isp_sensor_exp_gain *exp_gain,
				struct rts_isp_sync_regs *regs)
{
	int i;
	int exp_set;
	uint16_t gain_reg;
	float exp_reg_value_float;
	uint32_t exp_reg_value;
	float gain;
	struct mis4001_status *status;
	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	gain = exp_gain->analog_gain[0] * exp_gain->digital_gain[0];
	gain_reg = get_sensor_gain_reg(gain);

	exp_reg_value_float =
		1.0 * exp_gain->exposure[0] / status->exp_step + 0.5f;
	exp_reg_value = clip_d_word(exp_reg_value_float, 2, 1500 - 2);

	reg = regs->reg;
	i = 0;
	exp_set = abs(status->last_exposure - exp_gain->exposure[0]) > 0.001f;
	if (exp_set) {
		set_sync_i2c(&reg[i++], 0x3100, (exp_reg_value >> 8));
		set_sync_i2c(&reg[i++], 0x3101, (exp_reg_value & 0xff));
		status->last_exposure = exp_gain->exposure[0];
	}
	set_sync_i2c(&reg[i++], 0x3102, gain_reg);

	regs->num = i;

	return RTS_ISP_OK;
}

static int mis4001_check(uint32_t isp_id)
{
	int ret;
	int id;
	struct rts_isp_i2c_reg reg = {};

	reg.addr = 0x3000;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id = reg.data << 8;

	reg.addr = 0x3001;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id |= reg.data;

	if (id == 0x1311)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops mis4001_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "mis4001",
	.get_info = mis4001_get_info,
	.get_init_info = mis4001_get_init_info,
	.start = mis4001_start,
	.get_tuned_again = mis4001_get_tuned_again,
	.get_tuned_dgain = mis4001_get_tuned_dgain,
	.get_exposure_gain_info = mis4001_get_exposure_gain_info,
	.check = mis4001_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(mis4001_ops)
