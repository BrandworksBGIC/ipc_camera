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

struct sc4335p_status {
	float exp_step;
	float last_exposure;
	uint16_t min_vts;
	struct rts_isp_i2c_reg regs1[2];
};

static struct sc4335p_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_sc4335p_fps_info[] = {
	{30, 2700, 121500000},
};

static struct rts_isp_i2c_reg g_sc4335p_i2c_init_regs[] = {
//2560x1440 30fps dvp_clk=121.5M
//{0x320c,0x320d} = default = 0xa8c, frame witdh = {0x320c,0x320d}  = 2700
//{0x320e,0x320f} = default = 0x5dc, frame height = {0x320e,0x320f} = 1500
//pclk = 2700 * 1500 * 30 = 121,500,000
//30fsp 2560*1440 ini setting
	{0x0103, 0x01}, {0x0100, 0x00}, {0x36e9, 0xd4}, {0x36f9, 0xd4},
	{0x3001, 0xff}, {0x3002, 0xf0}, {0x3018, 0x3f}, {0x301a, 0xf8},
	{0x301c, 0x94}, {0x301f, 0x22}, {0x3030, 0x01}, {0x303f, 0x81},
	{0x3213, 0x07},
	{0x3301, 0x10}, {0x3306, 0x60}, {0x3309, 0x88}, {0x330a, 0x01},
	{0x330b, 0x08},
	{0x330e, 0x38}, {0x330f, 0x04}, {0x3310, 0x20}, {0x3314, 0x94},
	{0x331f, 0x79}, {0x3342, 0x01}, {0x3347, 0x05}, {0x3364, 0x1d},
	{0x3367, 0x10}, {0x33b6, 0x07}, {0x33b7, 0x2f}, {0x33b8, 0x10},
	{0x33b9, 0x18}, {0x33ba, 0x70}, {0x360f, 0x05}, {0x3614, 0x80},
	{0x3620, 0xa8}, {0x3622, 0xf6}, {0x3625, 0x0a}, {0x3630, 0xc0},
	{0x3631, 0x88}, {0x3632, 0x18}, {0x3633, 0x33}, {0x3636, 0x25},
	{0x3637, 0x70}, {0x3638, 0x22}, {0x363a, 0x90}, {0x363b, 0x09},
	{0x3641, 0x03},
	{0x3650, 0x06}, {0x366e, 0x04}, {0x3670, 0x0a}, {0x3671, 0xf6},
	{0x3672, 0xf6}, {0x3673, 0x16}, {0x3674, 0xc0}, {0x3675, 0xc8},
	{0x3676, 0xaf}, {0x367a, 0x08}, {0x367b, 0x38}, {0x367c, 0x38},
	{0x367d, 0x3f}, {0x3690, 0x33}, {0x3691, 0x34}, {0x3692, 0x44},
	{0x369c, 0x38}, {0x369d, 0x3f}, {0x36ea, 0x65}, {0x36ed, 0x03},
	{0x36fa, 0x65}, {0x36fd, 0x04}, {0x3902, 0xc5}, {0x3904, 0x10},
	{0x3908, 0x41}, {0x3933, 0x0a}, {0x3934, 0x0d}, {0x3940, 0x65},
	{0x3941, 0x18}, {0x3942, 0x02}, {0x3943, 0x12}, {0x395e, 0xa0},
	{0x3960, 0x9d}, {0x3961, 0x9d}, {0x3962, 0x89}, {0x3963, 0x80},
	{0x3966, 0x4e}, {0x3980, 0x60}, {0x3981, 0x30}, {0x3982, 0x15},
	{0x3983, 0x10}, {0x3984, 0x0d}, {0x3985, 0x20}, {0x3986, 0x30},
	{0x3987, 0x60}, {0x3988, 0x04}, {0x3989, 0x0c}, {0x398a, 0x14},
	{0x398b, 0x24}, {0x398c, 0x50}, {0x398d, 0x32}, {0x398e, 0x1e},
	{0x398f, 0x0a}, {0x3990, 0xc0}, {0x3991, 0x50}, {0x3992, 0x22},
	{0x3993, 0x0c}, {0x3994, 0x10}, {0x3995, 0x38},
	{0x3996, 0x80},
	{0x3997, 0xff}, {0x3998, 0x08}, {0x3999, 0x16}, {0x399a, 0x28},
	{0x399b, 0x40}, {0x399c, 0x50}, {0x399d, 0x28}, {0x399e, 0x18},
	{0x399f, 0x0c}, {0x3d08, 0x01},
	{0x3e01, 0xbb}, {0x3e02, 0x00}, {0x3e09, 0x20},
	{0x3e25, 0x03}, {0x3e26, 0x20}, {0x3000, 0x0f}, {0x4603, 0x01},
	{0x5781, 0x04}, {0x5782, 0x04},
	{0x5783, 0x02}, {0x5784, 0x02}, {0x5785, 0x40}, {0x5786, 0x20},
	{0x5787, 0x18}, {0x5788, 0x10}, {0x5789, 0x10}, {0x57a4, 0xa0},
	{0x36e9, 0x52}, {0x36f9, 0x53}, {0x0100, 0x01},
};

static int sc4335p_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 2560;
	info->modes.mode[0].size.h = 1440;
	info->modes.mode[0].fps = g_sc4335p_fps_info[0].fps;
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

static const struct fps_info *sc4335p_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_sc4335p_fps_info); i++)
		if (fps == g_sc4335p_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_sc4335p_fps_info))
		return NULL;

	return &g_sc4335p_fps_info[i];
}

static int sc4335p_get_init_info(uint32_t isp_id,
				 const struct rts_isp_sensor_mode *mode,
			       struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct sc4335p_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("sc4335p get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = sc4335p_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, clk_div: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->clk_div, fps_info->hts);

	set_init_i2c_regs(info->sensor_regs[0], g_sc4335p_i2c_init_regs, 10000);

	info->interface.interface = SNR_INTERFACE_DVP;
	info->interface.dvp.sample_rising = 1;
	info->interface.dvp.hsync_active_high = 1;
	info->interface.dvp.vsync_active_high = 0;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 2560;
	info->size.h = 1440;
	info->start.x = 0;
	info->start.y = 0;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = status->min_vts = 1500;
	info->max_vts = 65535 - info->min_vts;

	set_init_i2c(&status->regs1[0], 0x320d, (fps_info->hts) & 0xff);
	set_init_i2c(&status->regs1[1], 0x320c, (fps_info->hts) >> 8);
	set_init_i2c_regs(info->sensor_regs[1], status->regs1, 0);

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */

	return RTS_ISP_OK;
}

static int sc4335p_start(uint32_t isp_id)
{
	struct sc4335p_status *status;

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

static int sc4335p_get_tuned_again(uint32_t isp_id,
				   float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int sc4335p_get_tuned_dgain(uint32_t isp_id,
				   float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int sc4335p_get_exposure_gain_info(uint32_t isp_id,
					const struct rts_isp_sensor_exp_gain *exp_gain,
					struct rts_isp_sync_regs *regs)
{
	int i;
	uint16_t total_line;
	uint16_t gain_reg;
	float exp_reg_value_float;
	uint32_t exp_reg_value;
	struct sc4335p_status *status;
	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	gain_reg = get_sensor_gain_reg(exp_gain->analog_gain[0] *
				       exp_gain->digital_gain[0]);

	total_line = exp_gain->vts;

	exp_reg_value_float =
		2.0 * exp_gain->exposure[0] / status->exp_step + 0.5f;
	exp_reg_value =
		clip_d_word(exp_reg_value_float, 0,
			    (2 * total_line-8));
	exp_reg_value = exp_reg_value << 4;

	total_line = (total_line + 1) / 2 * 2;

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

	set_sync_i2c(&reg[i++], 0x3812, 0x00); //group hold
	if (exp_gain->analog_gain[0] < 2) {
		set_sync_i2c(&reg[i++], 0x3631, 0x88);
		set_sync_i2c(&reg[i++], 0x3632, 0x18);
		set_sync_i2c(&reg[i++], 0x3636, 0x25);
	} else if (exp_gain->analog_gain[0] < 4) {
		set_sync_i2c(&reg[i++], 0x3631, 0x8e);
		set_sync_i2c(&reg[i++], 0x3632, 0x18);
		set_sync_i2c(&reg[i++], 0x3636, 0x25);
	}  else if (exp_gain->analog_gain[0] < 15.75) {
		set_sync_i2c(&reg[i++], 0x3631, 0x80);
		set_sync_i2c(&reg[i++], 0x3632, 0x18);
		set_sync_i2c(&reg[i++], 0x3636, 0x65);
	} else {
		set_sync_i2c(&reg[i++], 0x3631, 0x80);
		set_sync_i2c(&reg[i++], 0x3632, 0xd8);
		set_sync_i2c(&reg[i++], 0x3636, 0x65);
	}
	set_sync_i2c(&reg[i++], 0x3812, 0x30); //group enable

	regs->num = i;
	return RTS_ISP_OK;
}

static int sc4335p_check(uint32_t isp_id)
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

	if (id == 0xcd01)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops sc4335p_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "sc4335p",
	.get_info = sc4335p_get_info,
	.get_init_info = sc4335p_get_init_info,
	.start = sc4335p_start,
	.get_tuned_again = sc4335p_get_tuned_again,
	.get_tuned_dgain = sc4335p_get_tuned_dgain,
	.get_exposure_gain_info = sc4335p_get_exposure_gain_info,
	.check = sc4335p_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(sc4335p_ops)
