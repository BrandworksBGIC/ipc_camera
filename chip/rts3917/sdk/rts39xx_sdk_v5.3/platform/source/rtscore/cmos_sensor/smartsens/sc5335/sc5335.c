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

struct sc5335_status {
	float exp_step;
	float last_exposure;
	uint16_t min_vts;
	struct rts_isp_i2c_reg regs1[2];
};

static struct sc5335_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_sc5335_fps_info[] = {
	{30, 2880, 172800000},
};

static struct rts_isp_i2c_reg g_sc5335_i2c_init_regs[] = {
//2592x1944 30fps mipi_clk=864M
//{0x320c,0x320d} = default = 0x5a0, frame witdh = {0x320c,0x320d} * 2 = 2880
//{0x320e,0x320f} = default = 0x7d0, frame height = {0x320e,0x320f} = 2000
//pclk = 2880 * 2000 * 30 = 172,800,000
	{0x0103, 0x01}, {0x0100, 0x00}, {0x36e9, 0x80}, {0x36f9, 0x80},
	{0x301f, 0x01}, {0x3038, 0x66}, {0x3301, 0x09}, {0x3304, 0x50},
	{0x3306, 0x30}, {0x3307, 0x05}, {0x3309, 0x90}, {0x330b, 0x70},
	{0x330e, 0x18}, {0x330f, 0x04}, {0x3310, 0x04}, {0x3314, 0x96},
	{0x3317, 0x05}, {0x3318, 0x03}, {0x331e, 0x41}, {0x331f, 0x81},
	{0x3320, 0x05}, {0x3347, 0x05}, {0x334c, 0x08}, {0x335d, 0x60},
	{0x335e, 0x01}, {0x335f, 0x04}, {0x3364, 0x17}, {0x3366, 0x92},
	{0x3367, 0x0c}, {0x3368, 0x05}, {0x3369, 0x00}, {0x336a, 0x00},
	{0x336b, 0x00}, {0x336d, 0x03}, {0x337c, 0x06}, {0x337d, 0x0e},
	{0x3390, 0x08}, {0x3391, 0x38}, {0x3392, 0x3f}, {0x3393, 0x0e},
	{0x3394, 0x18}, {0x3395, 0x30}, {0x33a2, 0x0a}, {0x33ac, 0x07},
	{0x33ae, 0x12}, {0x33af, 0x24}, {0x33e0, 0x60}, {0x33e1, 0x08},
	{0x33e2, 0x38}, {0x33e3, 0x18}, {0x33e4, 0x18}, {0x33e5, 0x10},
	{0x33e6, 0x06}, {0x33e7, 0x02}, {0x33e8, 0x38}, {0x33e9, 0x10},
	{0x33ea, 0x0c}, {0x33eb, 0x10}, {0x33ec, 0x04}, {0x33ed, 0x02},
	{0x33ee, 0xa0}, {0x33ef, 0x08}, {0x33f4, 0x18}, {0x33f5, 0x10},
	{0x33f6, 0x0c}, {0x33f7, 0x10}, {0x33f8, 0x06}, {0x33f9, 0x02},
	{0x33fa, 0x18}, {0x33fb, 0x10}, {0x33fc, 0x0c}, {0x33fd, 0x10},
	{0x33fe, 0x04}, {0x33ff, 0x02}, {0x360f, 0x05}, {0x3614, 0x09},
	{0x3620, 0x88}, {0x3622, 0x16}, {0x3625, 0x0a}, {0x3630, 0xe4},
	{0x3631, 0x88}, {0x3632, 0x78}, {0x3633, 0x34}, {0x3635, 0x2c},
	{0x3636, 0x2a}, {0x3637, 0x2a}, {0x3638, 0x08}, {0x363a, 0x80},
	{0x363b, 0x24}, {0x363c, 0x87}, {0x3670, 0x4a}, {0x3671, 0x16},
	{0x3672, 0xf6}, {0x3673, 0x16}, {0x3674, 0xc0}, {0x3675, 0xa7},
	{0x3676, 0xa8}, {0x367a, 0x08}, {0x367b, 0x38}, {0x367c, 0x08},
	{0x367d, 0x18}, {0x3690, 0x44}, {0x3691, 0x44}, {0x3692, 0x44},
	{0x3699, 0x80}, {0x369a, 0x80}, {0x369b, 0x80}, {0x369c, 0x08},
	{0x369d, 0x38}, {0x36a2, 0x08}, {0x36a3, 0x38}, {0x36fa, 0x38},
	{0x36fb, 0x07}, {0x3902, 0xc5}, {0x3905, 0xd8}, {0x391b, 0x80},
	{0x391c, 0x03}, {0x3933, 0x09}, {0x3934, 0x21}, {0x3940, 0x6c},
	{0x3941, 0x18}, {0x3942, 0x01}, {0x3943, 0x21}, {0x395e, 0xff},
	{0x3960, 0x61}, {0x3961, 0x94}, {0x3962, 0x9d}, {0x3963, 0x80},
	{0x3966, 0x3a}, {0x3983, 0x00}, {0x3988, 0x06}, {0x3989, 0x0e},
	{0x398a, 0x18}, {0x398b, 0x30}, {0x398c, 0x38}, {0x398d, 0x28},
	{0x398e, 0x12}, {0x398f, 0x08}, {0x3990, 0x40}, {0x3991, 0x28},
	{0x3992, 0x18}, {0x3993, 0x09}, {0x3994, 0x09}, {0x3995, 0x16},
	{0x3996, 0x30}, {0x3997, 0x80},
#if 1 //0x3109 == 0x06
	{0x3980, 0xe0},
	{0x3981, 0x38},
	{0x3982, 0x18},
	{0x3984, 0x09},
	{0x3985, 0x18},
	{0x3986, 0x60},
	{0x3987, 0xe0},
#else //0x3109 != 0x06
	{0x3980, 0x00},
	{0x3981, 0x00},
	{0x3982, 0x00},
	{0x3984, 0x00},
	{0x3985, 0x00},
	{0x3986, 0x00},
	{0x3987, 0x00},
#endif
#if 1 //change Start Point and BLC target
	// Start x, y, default=(0x0008, 0x0008)
	{0x3210, 0x00}, {0x3211, 0x08}, {0x3212, 0x00}, {0x3213, 0x07},
	{0x3907, 0x00}, {0x3908, 0x11}, // BLC trg, default=0x0041
#endif
	{0x3998, 0x08}, {0x3999, 0x18}, {0x399a, 0x30},
	{0x399b, 0x50}, {0x399c, 0x48}, {0x399d, 0x30}, {0x399e, 0x18},
	{0x399f, 0x08}, {0x39b0, 0x03}, {0x3e01, 0xf9}, {0x3e02, 0x60},
	{0x3e09, 0x20}, {0x3e1b, 0x15}, {0x3e26, 0x20}, {0x4509, 0x20},
	{0x4800, 0x64}, {0x5784, 0x10}, {0x5785, 0x08}, {0x5787, 0x06},
	{0x5788, 0x06}, {0x5789, 0x00}, {0x578a, 0x06}, {0x578b, 0x06},
	{0x578c, 0x00}, {0x5790, 0x10}, {0x5791, 0x10}, {0x5792, 0x00},
	{0x5793, 0x10}, {0x5794, 0x10}, {0x5795, 0x00}, {0x5799, 0x07},
	{0x5988, 0x86}, {0x598e, 0x05}, {0x598f, 0x6a}, {0x36e9, 0x34},
	{0x36f9, 0x34}, {0x0100, 0x01}, {0x3631, 0x88}, {0x3636, 0x2a},
};

static int sc5335_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 2592;
	info->modes.mode[0].size.h = 1944;
	info->modes.mode[0].fps = g_sc5335_fps_info[0].fps;
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

static const struct fps_info *sc5335_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_sc5335_fps_info); i++)
		if (fps == g_sc5335_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_sc5335_fps_info))
		return NULL;

	return &g_sc5335_fps_info[i];
}

static int sc5335_get_init_info(uint32_t isp_id,
				const struct rts_isp_sensor_mode *mode,
			       struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct sc5335_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("sc5335 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = sc5335_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, clk_div: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->clk_div, fps_info->hts);

	set_init_i2c_regs(info->sensor_regs[0], g_sc5335_i2c_init_regs, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0 | MIPI_LANE1;
	info->interface.mipi.hs_term = 0x2;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 2592;
	info->size.h = 1944;
	info->start.x = 0;
	info->start.y = 0;

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

static int sc5335_start(uint32_t isp_id)
{
	struct sc5335_status *status;

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

static int sc5335_get_tuned_again(uint32_t isp_id,
				  float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int sc5335_get_tuned_dgain(uint32_t isp_id,
				  float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int sc5335_get_exposure_gain_info(uint32_t isp_id,
					const struct rts_isp_sensor_exp_gain *exp_gain,
					struct rts_isp_sync_regs *regs)
{
	int i;
	uint16_t total_line;
	uint16_t gain_reg;
	float exp_reg_value_float;
	uint32_t exp_reg_value;
	struct sc5335_status *status;
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
		status->last_exposure = exp_gain->exposure[0];

		set_sync_info(&reg[i++], 1, RTS_ISP_INT_DATA_START);

		//finegain, corsegain at 0x3e09[6:0]
		set_sync_i2c(&reg[i++], 0x3e08, (gain_reg & 0xff00) >> 8);
		set_sync_i2c(&reg[i++], 0x3e09, (gain_reg & 0xff));

	} else {
		total_line = exp_gain->vts;
		//total length
		set_sync_i2c(&reg[i++], 0x320e, (total_line & 0xff00) >> 8);
		set_sync_i2c(&reg[i++], 0x320f, (total_line & 0xff));

		set_sync_i2c(&reg[i++], 0x3e08, (gain_reg & 0xff00) >> 8);
		set_sync_i2c(&reg[i++], 0x3e09, (gain_reg & 0xff));
	}

	set_sync_i2c(&reg[i++], 0x3812, 0x0); //group hold
	if (exp_gain->analog_gain[0] < 2) {
		set_sync_i2c(&reg[i++], 0x3631, 0x88);
		set_sync_i2c(&reg[i++], 0x3636, 0x2a);
	} else if (exp_gain->analog_gain[0] < 17.75) {
		set_sync_i2c(&reg[i++], 0x3631, 0x80);
		set_sync_i2c(&reg[i++], 0x3636, 0x6a);
	} else {
		set_sync_i2c(&reg[i++], 0x3631, 0x88);
		set_sync_i2c(&reg[i++], 0x3636, 0x6a);
	}
	set_sync_i2c(&reg[i++], 0x3812, 0x30); //group enable

	regs->num = i;

	return RTS_ISP_OK;
}

static int sc5335_check(uint32_t isp_id)
{
	int ret;
	int id;
	struct rts_isp_i2c_reg reg;

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

	if (id == 0xce03)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops sc5335_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "sc5335",
	.get_info = sc5335_get_info,
	.get_init_info = sc5335_get_init_info,
	.start = sc5335_start,
	.get_tuned_again = sc5335_get_tuned_again,
	.get_tuned_dgain = sc5335_get_tuned_dgain,
	.get_exposure_gain_info = sc5335_get_exposure_gain_info,
	.check = sc5335_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(sc5335_ops)
