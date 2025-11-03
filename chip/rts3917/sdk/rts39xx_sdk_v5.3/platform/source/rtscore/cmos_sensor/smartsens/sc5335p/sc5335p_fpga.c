/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2020 PingCheng Huang <pingcheng.huang@realtek.com>
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

struct sc5335p_status {
	float exp_step;
	float last_exposure;
	uint16_t min_vts;
	struct rts_isp_i2c_reg regs1[2];
};

static struct sc5335p_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_sc5335p_fps_info[] = {
	{7, 3000, 42000000},
};

static struct rts_isp_i2c_reg g_sc5335p_i2c_init_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x301f, 0xff},
	{0x3200, 0x00},
	{0x3201, 0x00},
	{0x3202, 0x00},
	{0x3203, 0x00},
	{0x3204, 0x0a},
	{0x3205, 0x2b},
	{0x3206, 0x07},
	{0x3207, 0xa3},
	{0x3208, 0x0a},
	{0x3209, 0x28},
	{0x320a, 0x07},
	{0x320b, 0xa0},
	{0x320e, 0x07},
	{0x320f, 0xd0},
	{0x3210, 0x00},
	{0x3211, 0x02},
	{0x3212, 0x00},
	{0x3213, 0x02},
	{0x3301, 0x08},
	{0x3303, 0x06},
	{0x3304, 0x6c},
	{0x3306, 0x30},
	{0x3308, 0x12},
	{0x3309, 0x8a},
	{0x330b, 0x88},
	{0x3310, 0x03},
	{0x331e, 0x61},
	{0x331f, 0x7f},
	{0x3333, 0x10},
	{0x3356, 0x39},
	{0x3364, 0x17},
	{0x3390, 0x0c},
	{0x3391, 0x1c},
	{0x3392, 0x3c},
	{0x3393, 0x0c},
	{0x3394, 0x10},
	{0x3395, 0x30},
	{0x33af, 0x52},
	{0x33b5, 0x10},
	{0x3622, 0x16},
	{0x3630, 0x87},
	{0x3631, 0x80},
	{0x3633, 0x73},
	{0x3634, 0x43},
	{0x3637, 0x12},
	{0x3638, 0x08},
	{0x363b, 0x00},
	{0x3670, 0x0a},
	{0x3674, 0xb0},
	{0x3675, 0xba},
	{0x3676, 0xbf},
	{0x367c, 0x0c},
	{0x367d, 0x3c},
	{0x3690, 0x43},
	{0x3691, 0x53},
	{0x3692, 0x53},
	{0x369c, 0x1c},
	{0x369d, 0x3c},
	{0x36ea, 0x39},
	{0x36eb, 0x1c},
	{0x36ec, 0x0c},
	{0x36ed, 0x24},
	{0x36fa, 0x32},
	{0x36fb, 0x00},
	{0x36fc, 0x11},
	{0x36fd, 0x24},
	{0x3e01, 0xf9},
	{0x3e02, 0x80},
	{0x3f09, 0x47},
	{0x4505, 0x09},
	{0x4509, 0x10},
	{0x4819, 0x03},
	{0x481b, 0x02},
	{0x481d, 0x06},
	{0x481f, 0x02},
	{0x4821, 0x07},
	{0x4823, 0x02},
	{0x4825, 0x02},
	{0x4827, 0x02},
	{0x4829, 0x03},
	{0x5787, 0x10},
	{0x5788, 0x06},
	{0x578a, 0x10},
	{0x578b, 0x06},
	{0x5790, 0x10},
	{0x5791, 0x10},
	{0x5792, 0x00},
	{0x5793, 0x10},
	{0x5794, 0x10},
	{0x5795, 0x00},
	{0x5799, 0x00},
	{0x57c7, 0x10},
	{0x57c8, 0x06},
	{0x57ca, 0x10},
	{0x57cb, 0x06},
	{0x57d0, 0x10},
	{0x57d1, 0x10},
	{0x57d2, 0x00},
	{0x57d3, 0x10},
	{0x57d4, 0x10},
	{0x57d5, 0x00},
	{0x57d9, 0x00},
	{0x59e0, 0x60},
	{0x59e1, 0x08},
	{0x59e2, 0x3f},
	{0x59e3, 0x18},
	{0x59e4, 0x18},
	{0x59e5, 0x3f},
	{0x59e6, 0x06},
	{0x59e7, 0x02},
	{0x59e8, 0x38},
	{0x59e9, 0x10},
	{0x59ea, 0x0c},
	{0x59eb, 0x10},
	{0x59ec, 0x04},
	{0x59ed, 0x02},
	{0x36e9, 0x20},
	{0x36f9, 0x40},
	{0x0100, 0x01},

};

static int sc5335p_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 2592;
	info->modes.mode[0].size.h = 1944;
	info->modes.mode[0].fps = g_sc5335p_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x30;
	info->i2c.addr_len = 2;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_HCLK, CLK_27M, 5000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 0);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V5, 1000);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 2000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 5000);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_HIGH, 5000);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&down->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 0);
	set_power_item(&down->items[i++], SNR_IO_POWER, PWR_NONE, 0);
	set_power_item(&down->items[i++], SNR_CORE_POWER, PWR_NONE, 0);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, PWR_NONE, 0);
	set_power_item(&down->items[i++], SNR_HCLK, CLK_NONE, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static const struct fps_info *sc5335p_get_fps_info(uint16_t fps)
{
	int i;

		for (i = 0; i < ARRAY_SIZE(g_sc5335p_fps_info); i++)
			if (fps == g_sc5335p_fps_info[i].fps)
				break;
		if (i == ARRAY_SIZE(g_sc5335p_fps_info))
			return NULL;

		return &g_sc5335p_fps_info[i];

}

static int sc5335p_get_init_info(uint32_t isp_id,
				 const struct rts_isp_sensor_mode *mode,
			       struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct sc5335p_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("sc5335p get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = sc5335p_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, clk_div: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->clk_div, fps_info->hts);

	set_init_i2c_regs(info->sensor_regs[0],
	g_sc5335p_i2c_init_regs, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0 | MIPI_LANE1;
	info->interface.mipi.hs_term = 0x2;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 2600;
	info->size.h = 1946;
	info->start.x = 0;
	info->start.y = 1;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = status->min_vts = 2000;
	info->max_vts = 65535 - info->min_vts;

	set_init_i2c(&status->regs1[0], 0x320d, ((fps_info->hts)>>1) & 0xff);
	set_init_i2c(&status->regs1[1], 0x320c, ((fps_info->hts)>>1) >> 8);
	set_init_i2c_regs(info->sensor_regs[1], status->regs1, 0);

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */

	return RTS_ISP_OK;
}

static int sc5335p_start(uint32_t isp_id)
{
	struct sc5335p_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static uint16_t get_sensor_gain_reg(float fgain)
{
	uint16_t reg_value = 0;

	if (fgain >= 15.75) {
		reg_value = 0x1f3f;
	} else {
		if (fgain >= 8)
			reg_value = (uint16_t)(fgain * 4.0f) | 0x1f00;
		else if (fgain >= 4)
			reg_value = (uint16_t)(fgain * 8.0f) | 0x0f00;
		else if (fgain >= 2)
			reg_value = (uint16_t)(fgain * 16.0f) | 0x0700;
		else
			reg_value = (uint16_t)(fgain * 32.0f) | 0x0300;
	}

	return reg_value;
}

static float get_sensor_real_gain(uint16_t reg_value)
{
	float gain;

	gain = ((reg_value & 0xff) / 32.0f) *
		((((reg_value >> 8) & 0x1f) >> 2) + 1);

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

static int sc5335p_get_tuned_again(uint32_t isp_id,
				   float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int sc5335p_get_tuned_dgain(uint32_t isp_id,
				   float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int sc5335p_get_exposure_gain_info(uint32_t isp_id,
					const struct rts_isp_sensor_exp_gain *exp_gain,
					struct rts_isp_sync_regs *regs)
{
	int i;
	uint16_t total_line;
	uint16_t gain_reg;
	float exp_reg_value_float;
	uint32_t exp_reg_value;
	struct sc5335p_status *status;
	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	gain_reg = get_sensor_gain_reg(exp_gain->analog_gain[0] *
				       exp_gain->digital_gain[0]);
	exp_reg_value_float =
		2.0 * exp_gain->exposure[0] / status->exp_step + 0.5f;
	exp_reg_value =
		clip_d_word(exp_reg_value_float, 0,
			    2 * (exp_gain->vts));
	exp_reg_value = exp_reg_value << 4;

	reg = regs->reg;

	i = 0;

	if (abs(status->last_exposure - exp_gain->exposure[0]) > 0.0001f) {
		total_line = exp_gain->vts;

		//total length
		set_sync_i2c(&reg[i++], 0x320e, (total_line & 0xff00) >> 8);
		set_sync_i2c(&reg[i++], 0x320f, (total_line & 0xff));

		//set exposure time
		set_sync_i2c(&reg[i++], 0x3e00,
			     (exp_reg_value & 0xff0000) >> 16);
		set_sync_i2c(&reg[i++], 0x3e01, (exp_reg_value & 0xff00) >> 8);
		set_sync_i2c(&reg[i++], 0x3e02, exp_reg_value & 0xff);

		//finegain, corsegain at 0x3e09[6:0]
		set_sync_i2c(&reg[i++], 0x3e08, (gain_reg & 0xff00) >> 8);
		set_sync_i2c(&reg[i++], 0x3e09, (gain_reg & 0xff));

		status->last_exposure = exp_gain->exposure[0];

	} else {
		total_line = exp_gain->vts;
		//total length
		set_sync_i2c(&reg[i++], 0x320e, (total_line & 0xff00) >> 8);
		set_sync_i2c(&reg[i++], 0x320f, (total_line & 0xff));

		set_sync_i2c(&reg[i++], 0x3e08, (gain_reg & 0xff00) >> 8);
		set_sync_i2c(&reg[i++], 0x3e09, (gain_reg & 0xff));
	}

	regs->num = i;

	return RTS_ISP_OK;
}

static int sc5335p_check(uint32_t isp_id)
{
	int ret;
	int id;
	struct rts_isp_i2c_reg reg = {};

	reg.addr = 0x3107;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id = reg.data << 8;

	reg.addr = 0x3108;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id |= reg.data;

	if (id == 0xce1a)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops sc5335p_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "sc5335p",
	.get_info = sc5335p_get_info,
	.get_init_info = sc5335p_get_init_info,
	.start = sc5335p_start,
	.get_tuned_again = sc5335p_get_tuned_again,
	.get_tuned_dgain = sc5335p_get_tuned_dgain,
	.get_exposure_gain_info = sc5335p_get_exposure_gain_info,
	.check = sc5335p_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(sc5335p_ops)
