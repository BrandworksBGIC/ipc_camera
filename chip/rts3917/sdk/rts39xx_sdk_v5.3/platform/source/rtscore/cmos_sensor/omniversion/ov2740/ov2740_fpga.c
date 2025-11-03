/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2017 Grant Shen <grant_shen@realsil.com.cn>
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

struct ov2740_status {
	int hdr;
	float exp_step;
	float last_exposure;
	uint16_t min_vts;
};

struct ov2740_gain {
	uint16_t digital_gain;
	uint16_t fine_gain;
	float total_gain;
};

static struct ov2740_status g_status[SUPPORTED_ISP_NUM];

static struct rts_isp_i2c_reg g_ov2740_i2c_init_regs_linear[] = {
	/* linear */
	{0x0103, 0x01},
	{0x0302, 0x1e},
	{0x030d, 0x1e},
	{0x030e, 0x02},
	{0x0312, 0x01},
	{0x3000, 0x00},
	{0x3018, 0x32},
	{0x3031, 0x0a},
	{0x3080, 0x08},
	{0x3083, 0xB4},
	{0x3103, 0x00},
	{0x3104, 0x01},
	{0x3106, 0x01},
	{0x3500, 0x00},
	{0x3501, 0x08},
	{0x3502, 0x00},
	{0x3503, 0x88},
	{0x3507, 0x00},
	{0x3508, 0x00},
	{0x3509, 0x80},
	{0x350c, 0x00},
	{0x350d, 0x80},
	{0x3510, 0x00},
	{0x3511, 0x00},
	{0x3512, 0x20},
	{0x3632, 0x00},
	{0x3633, 0x10},
	{0x3634, 0x10},
	{0x3635, 0x10},
	{0x3645, 0x13},
	{0x3646, 0x81},
	{0x3636, 0x10},
	{0x3651, 0x0a},
	{0x3656, 0x02},
	{0x3659, 0x04},
	{0x365a, 0xda},
	{0x365b, 0xa2},
	{0x365c, 0x04},
	{0x365d, 0x1d},
	{0x365e, 0x1a},
	{0x3662, 0xd7},
	{0x3667, 0x78},
	{0x3669, 0x0a},
	{0x366a, 0x92},
	{0x3700, 0x54},
	{0x3702, 0x10},
	{0x3706, 0x42},
	{0x3709, 0x30},
	{0x370b, 0xc2},
	{0x3714, 0x63},
	{0x3715, 0x01},
	{0x3716, 0x00},
	{0x371a, 0x3e},
	{0x3732, 0x0e},
	{0x3733, 0x10},
	{0x375f, 0x0e},
	{0x3768, 0x30},
	{0x3769, 0x44},
	{0x376a, 0x22},
	{0x377b, 0x20},
	{0x377c, 0x00},
	{0x377d, 0x0c},
	{0x3798, 0x00},
	{0x37a1, 0x55},
	{0x37a8, 0x6d},
	{0x37c2, 0x04},
	{0x37c5, 0x00},
	{0x37c8, 0x00},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x07},
	{0x3805, 0x8f},
	{0x3806, 0x04},
	{0x3807, 0x47},
	{0x3808, 0x07},
	{0x3809, 0x88},
	{0x380a, 0x04},
	{0x380b, 0x40},
	{0x380c, 0x04},
	{0x380d, 0x38},
	{0x380e, 0x04},
	{0x380f, 0x60},
	{0x3810, 0x00},
	{0x3811, 0x04},
	{0x3812, 0x00},
	{0x3813, 0x04},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3820, 0x80},
	{0x3821, 0x46},
	{0x3822, 0x84},
	{0x3829, 0x00},
	{0x382a, 0x01},
	{0x382b, 0x01},
	{0x3830, 0x04},
	{0x3836, 0x01},
	{0x3837, 0x08},
	{0x3839, 0x01},
	{0x383a, 0x00},
	{0x383b, 0x08},
	{0x383c, 0x00},
	{0x3f0b, 0x00},
	{0x4001, 0x20},
	{0x4009, 0x07},
	{0x4003, 0x10},
	{0x4010, 0xe0},
	{0x4016, 0x00},
	{0x4017, 0x10},
	{0x4044, 0x02},
	{0x4304, 0x08},
	{0x4307, 0x30},
	{0x4320, 0x80},
	{0x4322, 0x00},
	{0x4323, 0x00},
	{0x4324, 0x00},
	{0x4325, 0x00},
	{0x4326, 0x00},
	{0x4327, 0x00},
	{0x4328, 0x00},
	{0x4329, 0x00},
	{0x432c, 0x03},
	{0x432d, 0x81},
	{0x4501, 0x84},
	{0x4502, 0x40},
	{0x4503, 0x18},
	{0x4504, 0x04},
	{0x4508, 0x02},
	{0x4601, 0x10},
	{0x4800, 0x00},
	{0x4816, 0x52},
	{0x4837, 0x16},
	{0x5000, 0x7f},
	{0x5001, 0x00},
	{0x5005, 0x38},
	{0x501e, 0x0d},
	{0x5040, 0x00},
	{0x5901, 0x00},
	{0x0100, 0x01},
};

static struct rts_isp_i2c_reg g_ov2740_i2c_init_regs_hdr[] = {
	{0x0103, 0x01},
	{0x0302, 0x1e},
	{0x030d, 0x1e},
	{0x030e, 0x02},
	{0x0312, 0x01},
	{0x3000, 0x00},
	{0x3018, 0x32},
	{0x3031, 0x0a},
	{0x3080, 0x08},
	{0x3083, 0xb1},
	{0x3103, 0x00},
	{0x3104, 0x01},
	{0x3106, 0x01},
	{0x3500, 0x00},	//   exp L
	{0x3501, 0x20},
	{0x3502, 0x00},
	{0x3503, 0x88},
	{0x3507, 0x00},
	{0x3508, 0x02},
	{0x3509, 0x00},
	{0x350c, 0x02},
	{0x350d, 0x00},
	{0x3510, 0x00},	//   exp S
	{0x3511, 0x02},
	{0x3512, 0x00},
	{0x3632, 0x00},
	{0x3633, 0x02},
	{0x3634, 0x04},
	{0x3635, 0x08},
	{0x3645, 0x13},
	{0x3646, 0x81},
	{0x3636, 0x10},
	{0x3651, 0x0a},
	{0x3656, 0x02},
	{0x3659, 0x04},
	{0x365a, 0xda},
	{0x365b, 0xa2},
	{0x365c, 0x04},
	{0x365d, 0x1d},
	{0x365e, 0x1a},
	{0x3662, 0xd7},
	{0x3667, 0x78},
	{0x3669, 0x0a},
	{0x366a, 0x92},
	{0x3700, 0x54},
	{0x3702, 0x10},
	{0x3706, 0x42},
	{0x3709, 0x30},
	{0x370b, 0xc2},
	{0x3714, 0x63},
	{0x3715, 0x01},
	{0x3716, 0x00},
	{0x371a, 0x3e},
	{0x3732, 0x0e},
	{0x3733, 0x10},
	{0x375f, 0x0e},
	{0x3768, 0x30},
	{0x3769, 0x44},
	{0x376a, 0x22},
	{0x377b, 0x26},
	{0x377c, 0x00},
	{0x377d, 0x0c},
	{0x3798, 0x00},
	{0x37a1, 0x55},
	{0x37a8, 0x6d},
	{0x37c2, 0x04},
	{0x37c5, 0x00},
	{0x37c8, 0x00},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x04},
	{0x3804, 0x07},
	{0x3805, 0x8f},
	{0x3806, 0x04},
	{0x3807, 0x47},
	{0x3808, 0x07},
	{0x3809, 0x84},
	{0x380a, 0x04},
	{0x380b, 0x3c},
	{0x380c, 0x04},
	{0x380d, 0x38},		//1080*2
	{0x380e, 0x04},
	{0x380f, 0x58},		//1112
	{0x3810, 0x00},
	{0x3811, 0x04},
	{0x3812, 0x00},
	{0x3813, 0x04},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3820, 0x80},
	{0x3821, 0x46},
	{0x3822, 0x84},
	{0x3829, 0x10},
	{0x382a, 0x01},
	{0x382b, 0x01},
	{0x3830, 0x04},
	{0x3836, 0x01},
	{0x3837, 0x08},
	{0x3839, 0x01},
	{0x383a, 0x00},
	{0x383b, 0x08},
	{0x383c, 0x00},
	{0x3f0b, 0x00},
	{0x4001, 0x20},
	{0x4009, 0x07},
	{0x4003, 0x10},
	{0x4010, 0xe0},
	{0x4016, 0x00},
	{0x4017, 0x10},
	{0x4044, 0x02},
	{0x4304, 0x08},
	{0x4307, 0x30},
	{0x4320, 0x80},
	{0x4322, 0x00},
	{0x4323, 0x00},
	{0x4324, 0x00},
	{0x4325, 0x00},
	{0x4326, 0x00},
	{0x4327, 0x00},
	{0x4328, 0x00},
	{0x4329, 0x00},
	{0x432c, 0x03},
	{0x432d, 0x81},
	{0x4501, 0x84},
	{0x4502, 0x40},
	{0x4503, 0x18},
	{0x4504, 0x04},
	{0x4508, 0x02},
	{0x4601, 0x10},
	{0x4800, 0x4c},
	{0x4816, 0x52},
	{0x4837, 0x16},
	{0x5000, 0x7f},
	{0x5001, 0x02},
	{0x5005, 0x38},
	{0x501e, 0x0d},
	{0x5040, 0x00},
	{0x5901, 0x00},
	{0x0100, 0x01},
};

static int ov2740_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	i = 0;
	info->modes.mode[i].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[i].size.w = 1920;
	info->modes.mode[i].size.h = 1080;
	info->modes.mode[i].fps = 20;
	i++;
	info->modes.mode[i].hdr = RTS_ISP_HDR_LINE_2TO1;
	info->modes.mode[i].size.w = 1920;
	info->modes.mode[i].size.h = 1080;
	info->modes.mode[i].fps = 10;
	i++;
	info->modes.num = i;

	info->i2c.i2c_id = 0x36;
	info->i2c.addr_len = 2;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_HIGH, 0);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 0);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 0);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V2, 0);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 0);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_6M, 0);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_HCLK, 0, 0);
	set_power_item(&down->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&down->items[i++], SNR_CORE_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 0);
	set_power_item(&down->items[i++], SNR_IO_POWER, 0, 0);
	down->num = i;

	return RTS_ISP_OK;
}


static int ov2740_get_init_info(uint32_t isp_id,
				const struct rts_isp_sensor_mode *mode,
				struct rts_isp_sensor_init_info *info)
{
	struct ov2740_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("ov2740 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	status->hdr = mode->hdr;

	if (mode->hdr == RTS_ISP_HDR_LINE_2TO1) {
		set_init_i2c_regs(info->sensor_regs[0],
				  g_ov2740_i2c_init_regs_hdr, 0);

		info->interface.interface = SNR_INTERFACE_MIPI;
		info->interface.mipi.hdr = MIPI_HDR_VC;
		info->interface.mipi.lanes = MIPI_LANE0 | MIPI_LANE1;
		info->interface.mipi.hs_term = 0x6;
		info->interface.type = RAW_SENSOR;
		info->interface.bit_depth = SNR_10BIT;

		info->size.w = 1920;
		info->size.h = 1081;
		info->start.x = 0;
		info->start.y = 1;

		info->hts = 2160 * 2;
		info->pclk = 48000000;
		info->min_vts = status->min_vts = 1111;
		info->max_vts = info->min_vts * 2;

		status->exp_step = 1e6 * info->hts / 2 / info->pclk; /* us */
	} else {
		set_init_i2c_regs(info->sensor_regs[0],
				  g_ov2740_i2c_init_regs_linear, 0);

		info->interface.interface = SNR_INTERFACE_MIPI;
		info->interface.mipi.hdr = MIPI_HDR_NONE;
		info->interface.mipi.lanes = MIPI_LANE0 | MIPI_LANE1;
		info->interface.mipi.hs_term = 0x6;
		info->interface.type = RAW_SENSOR;
		info->interface.bit_depth = SNR_10BIT;

		info->size.w = 1920;
		info->size.h = 1081;
		info->start.x = 0;
		info->start.y = 1;

		info->hts = 2160;
		info->pclk = 48000000;
		info->min_vts = status->min_vts = 1111;
		info->max_vts = 65535 - info->min_vts;

		status->exp_step = 1e6 * info->hts / info->pclk; /* us */
	}

	return RTS_ISP_OK;
}

static int ov2740_start(uint32_t isp_id)
{
	struct ov2740_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static int ov2740_get_exposure_range(uint32_t isp_id, uint32_t vts,
				     float ratio[RTS_ISP_HDR_CHAN_MAX - 1],
				     float min_exposure[RTS_ISP_HDR_CHAN_MAX],
				     float max_exposure[RTS_ISP_HDR_CHAN_MAX])
{
	struct ov2740_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	if (status->hdr == RTS_ISP_HDR_NONE) {
		min_exposure[0] = status->exp_step;
		max_exposure[0] = (vts - 4) * status->exp_step;
	} else {
		uint32_t tmp1;

		tmp1 = (uint32_t)((vts - 8) / (ratio[0] + 1));
		max_exposure[1] = tmp1 * status->exp_step;
		min_exposure[1] = status->exp_step;
		max_exposure[0] = max_exposure[1] * ratio[0];
		min_exposure[0] = min_exposure[1] * ratio[0];
	}

	return RTS_ISP_OK;
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

static uint16_t get_sensor_gain_reg(float fgain)
{
	uint16_t reg_value = 0;

	reg_value = clip_d_word(fgain * 128, 128, 128 * 15.5);
	return reg_value;
}

static float get_sensor_real_gain(uint16_t reg_value)
{
	return (float)reg_value / 128;
}

static int ov2740_get_tuned_again(uint32_t isp_id, float *again)
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);
	gain_reg = get_sensor_gain_reg(again[1]);
	again[1] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int ov2740_get_tuned_dgain(uint32_t isp_id,
				  float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;
	dgain[1] = 1.0f;

	return RTS_ISP_OK;
}

static int ov2740_get_exposure_gain_info(uint32_t isp_id,
		const struct rts_isp_sensor_exp_gain *exp_gain,
		struct rts_isp_sync_regs *regs)
{
	int i;
	struct ov2740_status *status;
	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	if (status->hdr == RTS_ISP_HDR_NONE) {
		uint32_t exposure_rows;
		uint16_t gain;

		exposure_rows = exp_gain->exposure[0] / status->exp_step;
		gain = get_sensor_gain_reg(exp_gain->analog_gain[0] *
					exp_gain->digital_gain[0]);
		reg = regs->reg;
		i = 0;
		// set vts
		set_sync_i2c(&reg[i++], 0x380e, exp_gain->vts >> 8);
		set_sync_i2c(&reg[i++], 0x380f, exp_gain->vts & 0xff);
		// set exposure
		set_sync_i2c(&reg[i++], 0x3502, (exposure_rows << 4) & 0xff);
		set_sync_i2c(&reg[i++], 0x3501, (exposure_rows << 4) >> 8);
		set_sync_i2c(&reg[i++], 0x3500, (exposure_rows << 4) >> 16);
		// set gain
		set_sync_i2c(&reg[i++], 0x3508, gain >> 8);
		set_sync_i2c(&reg[i++], 0x3509, gain & 0xff);
		regs->num = i;
	} else {
		uint32_t exp_cnt[2];
		uint16_t gain_reg[2];

		for (i = 0; i < 2; i++) {
			exp_cnt[i] = exp_gain->exposure[i] / status->exp_step;
			gain_reg[i] =
				get_sensor_gain_reg(exp_gain->analog_gain[i] *
					exp_gain->digital_gain[i]);
		}
		reg = regs->reg;

		i = 0;
		/* set long exposure */
		set_sync_i2c(&reg[i++], 0x3500, (exp_cnt[0] << 4) >> 16);
		set_sync_i2c(&reg[i++], 0x3501, (exp_cnt[0] << 4) >> 8);
		set_sync_i2c(&reg[i++], 0x3502, (exp_cnt[0] << 4) & 0xff);
		/* set short exposure */
		set_sync_i2c(&reg[i++], 0x3510, (exp_cnt[1] << 4) >> 16);
		set_sync_i2c(&reg[i++], 0x3511, (exp_cnt[1] << 4) >> 8);
		set_sync_i2c(&reg[i++], 0x3512, (exp_cnt[1] << 4) & 0xff);
		/* set long gain */
		set_sync_i2c(&reg[i++], 0x3508, (gain_reg[0] >> 8));
		set_sync_i2c(&reg[i++], 0x3509, (gain_reg[0] & 0xff));
		/* set short gain */
		set_sync_i2c(&reg[i++], 0x350C, (gain_reg[1] >> 8));
		set_sync_i2c(&reg[i++], 0x350D, (gain_reg[1] & 0xff));

		set_sync_i2c(&reg[i++], 0x3208, 0x10);
		set_sync_i2c(&reg[i++], 0x320B, 0x00);
		set_sync_i2c(&reg[i++], 0x3208, 0xe0);
		regs->num = i;
	}
	return RTS_ISP_OK;
}

static int ov2740_check(uint32_t isp_id)
{
	int ret;
	int id;
	struct rts_isp_i2c_reg reg = {};

	reg.addr = 0x300a;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id = reg.data << 16;

	reg.addr = 0x300b;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id = reg.data << 8;

	reg.addr = 0x300c;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id |= reg.data;

	if (id == 0x002740)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops ov2740_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "ov2740",
	.get_info = ov2740_get_info,
	.get_init_info = ov2740_get_init_info,
	.start = ov2740_start,
	.get_exposure_range = ov2740_get_exposure_range,
	.get_tuned_again = ov2740_get_tuned_again,
	.get_tuned_dgain = ov2740_get_tuned_dgain,
	.get_exposure_gain_info = ov2740_get_exposure_gain_info,
	.check = ov2740_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(ov2740_ops)
