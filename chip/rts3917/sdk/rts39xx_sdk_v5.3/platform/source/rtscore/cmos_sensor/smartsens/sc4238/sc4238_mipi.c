/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2020 Yinna Liu <yinna_liu@realsil.com.cn>
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

struct sc4238_status {
	float exp_step;
	float last_exposure;
	uint16_t cur_fps;
	uint16_t min_vts;

};

static struct sc4238_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_sc4238_fps_info[] = {
	{30, 3200, 153600000},
};

static struct rts_isp_i2c_reg g_sc4238_i2c_init_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x3018, 0x72},
	{0x3019, 0x00},
	{0x301f, 0x97},
	{0x3031, 0x0a},
	{0x3037, 0x20},
	{0x3038, 0x22},
	{0x3106, 0x81},
	{0x3200, 0x00},
	{0x3201, 0x3c},
	{0x3202, 0x00},
	{0x3203, 0x24},
	{0x3204, 0x0a},
	{0x3205, 0x4b},
	{0x3206, 0x05},
	{0x3207, 0xd3},
	{0x3208, 0x0a},
	{0x3209, 0x08},
	{0x320a, 0x05},
	{0x320b, 0xa8},
	{0x320c, 0x06},
	{0x320d, 0x40},
	{0x320e, 0x06},
	{0x320f, 0x40},
	{0x3210, 0x00},
	{0x3211, 0x04},
	{0x3212, 0x00},
	{0x3213, 0x04},
	{0x3221, 0x00},
	{0x3251, 0x88},
	{0x3253, 0x0a},
	{0x325f, 0x0c},
	{0x3273, 0x01},
	{0x3301, 0x30},
	{0x3304, 0x30},
	{0x3306, 0x70},
	{0x3308, 0x10},
	{0x3309, 0x50},
	{0x330b, 0xf0},
	{0x330e, 0x14},
	{0x3314, 0x94},
	{0x331e, 0x29},
	{0x331f, 0x49},
	{0x3320, 0x09},
	{0x334c, 0x10},
	{0x3352, 0x02},
	{0x3356, 0x1f},
	{0x335e, 0x02},
	{0x335f, 0x04},
	{0x3363, 0x00},
	{0x3364, 0x1e},
	{0x3366, 0x92},
	{0x336d, 0x03},
	{0x337a, 0x08},
	{0x337b, 0x10},
	{0x337c, 0x06},
	{0x337d, 0x0a},
	{0x337f, 0x2d},
	{0x3390, 0x08},
	{0x3391, 0x18},
	{0x3392, 0x38},
	{0x3393, 0x30},
	{0x3394, 0x30},
	{0x3395, 0x30},
	{0x3399, 0xff},
	{0x33a2, 0x08},
	{0x33a3, 0x0c},
	{0x33e0, 0xa0},
	{0x33e1, 0x08},
	{0x33e2, 0x00},
	{0x33e3, 0x10},
	{0x33e4, 0x10},
	{0x33e5, 0x00},
	{0x33e6, 0x10},
	{0x33e7, 0x10},
	{0x33e8, 0x00},
	{0x33e9, 0x10},
	{0x33ea, 0x16},
	{0x33eb, 0x00},
	{0x33ec, 0x10},
	{0x33ed, 0x18},
	{0x33ee, 0xa0},
	{0x33ef, 0x08},
	{0x33f4, 0x00},
	{0x33f5, 0x10},
	{0x33f6, 0x10},
	{0x33f7, 0x00},
	{0x33f8, 0x10},
	{0x33f9, 0x10},
	{0x33fa, 0x00},
	{0x33fb, 0x10},
	{0x33fc, 0x16},
	{0x33fd, 0x00},
	{0x33fe, 0x10},
	{0x33ff, 0x18},
	{0x360f, 0x05},
	{0x3622, 0xee},
	{0x3625, 0x0a},
	{0x3630, 0xa8},
	{0x3631, 0x80},
	{0x3633, 0x44},
	{0x3634, 0x34},
	{0x3635, 0x60},
	{0x3636, 0x20},
	{0x3637, 0x11},
	{0x3638, 0x2a},
	{0x363a, 0x1f},
	{0x363b, 0x03},
	{0x366e, 0x04},
	{0x3670, 0x4a},
	{0x3671, 0xee},
	{0x3672, 0x0e},
	{0x3673, 0x0e},
	{0x3674, 0x70},
	{0x3675, 0x40},
	{0x3676, 0x45},
	{0x367a, 0x08},
	{0x367b, 0x38},
	{0x367c, 0x08},
	{0x367d, 0x38},
	{0x3690, 0x43},
	{0x3691, 0x63},
	{0x3692, 0x63},
	{0x3699, 0x80},
	{0x369a, 0x9f},
	{0x369b, 0x9f},
	{0x369c, 0x08},
	{0x369d, 0x38},
	{0x36a2, 0x08},
	{0x36a3, 0x38},
	{0x36ea, 0x38},
	{0x36eb, 0x0c},
	{0x36ec, 0x1c},
	{0x36ed, 0x14},
	{0x36fa, 0x38},
	{0x36fb, 0x09},
	{0x36fc, 0x00},
	{0x36fd, 0x14},
	{0x3902, 0xc5},
	{0x3905, 0xd8},
	{0x3908, 0x11},
	{0x391b, 0x80},
	{0x391c, 0x0f},
	{0x3933, 0x28},
	{0x3934, 0x20},
	{0x3940, 0x6c},
	{0x3942, 0x08},
	{0x3943, 0x28},
	{0x3980, 0x00},
	{0x3981, 0x00},
	{0x3982, 0x00},
	{0x3983, 0x00},
	{0x3984, 0x00},
	{0x3985, 0x00},
	{0x3986, 0x00},
	{0x3987, 0x00},
	{0x3988, 0x00},
	{0x3989, 0x00},
	{0x398a, 0x00},
	{0x398b, 0x04},
	{0x398c, 0x00},
	{0x398d, 0x04},
	{0x398e, 0x00},
	{0x398f, 0x08},
	{0x3990, 0x00},
	{0x3991, 0x10},
	{0x3992, 0x03},
	{0x3993, 0xd8},
	{0x3994, 0x03},
	{0x3995, 0xe0},
	{0x3996, 0x03},
	{0x3997, 0xf0},
	{0x3998, 0x03},
	{0x3999, 0xf8},
	{0x399a, 0x00},
	{0x399b, 0x00},
	{0x399c, 0x00},
	{0x399d, 0x08},
	{0x399e, 0x00},
	{0x399f, 0x10},
	{0x39a0, 0x00},
	{0x39a1, 0x18},
	{0x39a2, 0x00},
	{0x39a3, 0x28},
	{0x39af, 0x58},
	{0x39b5, 0x30},
	{0x39b6, 0x00},
	{0x39b7, 0x34},
	{0x39b8, 0x00},
	{0x39b9, 0x00},
	{0x39ba, 0x34},
	{0x39bb, 0x00},
	{0x39bc, 0x00},
	{0x39bd, 0x00},
	{0x39be, 0x00},
	{0x39bf, 0x00},
	{0x39c0, 0x00},
	{0x39c1, 0x00},
	{0x39c5, 0x21},
	{0x39c8, 0x00},
	{0x39db, 0x20},
	{0x39dc, 0x00},
	{0x39de, 0x20},
	{0x39df, 0x00},
	{0x39e0, 0x00},
	{0x39e1, 0x00},
	{0x39e2, 0x00},
	{0x39e3, 0x00},
	{0x3e00, 0x00},
	{0x3e01, 0xc7},
	{0x3e02, 0x60},
	{0x3e03, 0x0b},
	{0x3e06, 0x00},
	{0x3e07, 0x80},
	{0x3e08, 0x03},
	{0x3e09, 0x40},
	{0x3e14, 0xb1},
	{0x3e25, 0x03},
	{0x3e26, 0x40},
	{0x4501, 0xb4},
	{0x4509, 0x20},
	{0x4800, 0x44},
	{0x4818, 0x00},
	{0x4819, 0x30},
	{0x481a, 0x00},
	{0x481b, 0x0b},
	{0x481c, 0x00},
	{0x481d, 0xc8},
	{0x4821, 0x02},
	{0x4822, 0x00},
	{0x4823, 0x03},
	{0x4828, 0x00},
	{0x4829, 0x02},
	{0x4837, 0x3b},
	{0x5784, 0x10},
	{0x5785, 0x08},
	{0x5787, 0x06},
	{0x5788, 0x06},
	{0x5789, 0x00},
	{0x578a, 0x06},
	{0x578b, 0x06},
	{0x578c, 0x00},
	{0x5790, 0x10},
	{0x5791, 0x10},
	{0x5792, 0x00},
	{0x5793, 0x10},
	{0x5794, 0x10},
	{0x5795, 0x00},
	{0x57c4, 0x10},
	{0x57c5, 0x08},
	{0x57c7, 0x06},
	{0x57c8, 0x06},
	{0x57c9, 0x00},
	{0x57ca, 0x06},
	{0x57cb, 0x06},
	{0x57cc, 0x00},
	{0x57d0, 0x10},
	{0x57d1, 0x10},
	{0x57d2, 0x00},
	{0x57d3, 0x10},
	{0x57d4, 0x10},
	{0x57d5, 0x00},
	{0x5988, 0x84},
	{0x598e, 0x05},
	{0x598f, 0x6c},
	{0x36e9, 0x43},
	{0x36f9, 0x43},
	{0x0100, 0x01},
};

static int sc4238_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 2560;
	info->modes.mode[0].size.h = 1440;
	info->modes.mode[0].fps = g_sc4238_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x30;
	info->i2c.addr_len = 2;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 5000);
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

static const struct fps_info *sc4238_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_sc4238_fps_info); i++)
		if (fps == g_sc4238_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_sc4238_fps_info))
		return NULL;

	return &g_sc4238_fps_info[i];
}

static int sc4238_get_init_info(uint32_t isp_id,
				const struct rts_isp_sensor_mode *mode,
			       struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct sc4238_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("sc4238 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = sc4238_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, clk_div: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->clk_div, fps_info->hts);

	set_init_i2c_regs(info->sensor_regs[0], g_sc4238_i2c_init_regs, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = (MIPI_LANE0 | MIPI_LANE1 |
				      MIPI_LANE2 | MIPI_LANE3);
	info->interface.mipi.hs_term = 0x2;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 2560;
	info->size.h = 1441;
	info->start.x = 0;
	info->start.y = 1;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = status->min_vts = 1600;
	info->max_vts = 65535;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */
	status->cur_fps = mode->fps;

	return RTS_ISP_OK;
}

static int sc4238_start(uint32_t isp_id)
{
	struct sc4238_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static uint16_t get_sensor_gain_reg(float fgain)
{
	uint16_t reg_value = 0;

	if (fgain >= 15.87) {
		reg_value = 0x1f7f;
	} else {
		if (fgain >= 8)
			reg_value = (uint16_t)(fgain * 8.0f) | 0x1f00;
		else if (fgain >= 4)
			reg_value = (uint16_t)(fgain * 16.0f) | 0x0f00;
		else if (fgain >= 2)
			reg_value = (uint16_t)(fgain * 32.0f) | 0x0700;
		else
			reg_value = (uint16_t)(fgain * 64.0f) | 0x0300;
	}
	return reg_value;
}

static float get_sensor_real_gain(uint16_t reg_value)
{
	float gain;

	gain = ((reg_value & 0xff) / 64.0f) *
		((((reg_value >> 8) & 0x1f) >> 2) + 1);

	return gain;
}

static int sc4238_get_tuned_again(uint32_t isp_id,
				  float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int sc4238_get_tuned_dgain(uint32_t isp_id,
				  float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int sc4238_get_exposure_gain_info(uint32_t isp_id,
					const struct rts_isp_sensor_exp_gain *exp_gain,
					struct rts_isp_sync_regs *regs)
{
	int i;
	int exp_set;
	uint16_t total_line;
	uint16_t gain_reg;
	uint32_t exp_reg_value;
	float gain;
	struct sc4238_status *status;
	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	gain = exp_gain->analog_gain[0] * exp_gain->digital_gain[0];
	gain_reg = get_sensor_gain_reg(gain);
	total_line = exp_gain->vts;
	total_line = (total_line + 1) / 2 * 2;
	reg = regs->reg;

	i = 0;
	exp_set = abs(status->last_exposure - exp_gain->exposure[0]) > 0.001f;
	if (exp_set) {
		set_sync_i2c(&reg[i++], 0x320e, total_line >> 8);
		set_sync_i2c(&reg[i++], 0x320f, total_line & 0xff);
		exp_reg_value = 2.0 * exp_gain->exposure[0] / status->exp_step
					+ 0.5f;
		if (exp_reg_value <= 0x0003)
			exp_reg_value = 0x0003;
		exp_reg_value *= 16;
		set_sync_i2c(&reg[i++], 0x3e00, exp_reg_value >> 16);
		set_sync_i2c(&reg[i++], 0x3e01, (exp_reg_value & 0xff00) >> 8);
		set_sync_i2c(&reg[i++], 0x3e02, exp_reg_value & 0xff);
		status->last_exposure = exp_gain->exposure[0];
	}
	set_sync_i2c(&reg[i++], 0x3e08, (gain_reg >> 8));
	set_sync_i2c(&reg[i++], 0x3e09, (gain_reg & 0xff));

	regs->num = i;

	return RTS_ISP_OK;
}

static int sc4238_check(uint32_t isp_id)
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

	if (id == 0x4235)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops sc4238_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "sc4238",
	.get_info = sc4238_get_info,
	.get_init_info = sc4238_get_init_info,
	.start = sc4238_start,
	.get_tuned_again = sc4238_get_tuned_again,
	.get_tuned_dgain = sc4238_get_tuned_dgain,
	.get_exposure_gain_info = sc4238_get_exposure_gain_info,
	.check = sc4238_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(sc4238_ops)
