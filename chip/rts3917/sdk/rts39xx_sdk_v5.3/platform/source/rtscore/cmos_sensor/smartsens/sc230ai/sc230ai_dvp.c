/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2021 wei_mo <wei_mo@apowertec.com>
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

struct sc230ai_status {
	float exp_step;
	float last_exposure;
	uint16_t cur_fps;
	uint16_t min_vts;
	struct rts_isp_i2c_reg regs1[3];
};

static struct sc230ai_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_sc230ai_fps_info[] = {
	{30, 2400, 81000000},
};

//sensor setting DVP@30FPS
static struct rts_isp_i2c_reg g_sc230ai_i2c_init_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x3001, 0xff},
	{0x3002, 0xf0},
	{0x300a, 0x24},
	{0x3018, 0x0f},
	{0x301a, 0xf8},
	{0x301c, 0x94},
	{0x301f, 0x10},
	{0x303f, 0x81},
	{0x3208, 0x07},
	{0x3209, 0x88},
	{0x320a, 0x04},
	{0x320b, 0x40},
	{0x320c, 0x09},
	{0x320d, 0x60},
	{0x3211, 0x00},
	{0x3213, 0x08},
	{0x3227, 0x00},
	{0x3250, 0x40},
	{0x3301, 0x07},
	{0x3304, 0x50},
	{0x3306, 0x70},
	{0x3308, 0x18},
	{0x3309, 0x68},
	{0x330a, 0x01},
	{0x330b, 0x20},
	{0x331e, 0x41},
	{0x331f, 0x59},
	{0x3333, 0x10},
	{0x3334, 0x40},
	{0x335d, 0x60},
	{0x335e, 0x06},
	{0x335f, 0x08},
	{0x3364, 0x5e},
	{0x337c, 0x02},
	{0x337d, 0x0a},
	{0x3390, 0x01},
	{0x3391, 0x0b},
	{0x3392, 0x0f},
	{0x3393, 0x09},
	{0x3394, 0x0d},
	{0x3395, 0x60},
	{0x3396, 0x48},
	{0x3397, 0x49},
	{0x3398, 0x4b},
	{0x3399, 0x07},
	{0x339a, 0x0a},
	{0x339b, 0x0d},
	{0x339c, 0x60},
	{0x33a2, 0x04},
	{0x33af, 0x40},
	{0x33b1, 0x80},
	{0x33b3, 0x40},
	{0x33b9, 0x0a},
	{0x33f9, 0xa0},
	{0x33fb, 0xbf},
	{0x33fc, 0x5f},
	{0x33fd, 0x7f},
	{0x349f, 0x03},
	{0x34a6, 0x4b},
	{0x34a7, 0x5f},
	{0x34a8, 0x30},
	{0x34a9, 0x20},
	{0x34aa, 0x01},
	{0x34ab, 0x28},
	{0x34ac, 0x01},
	{0x34ad, 0x58},
	{0x34f8, 0x7f},
	{0x34f9, 0x10},
	{0x3630, 0xc0},
	{0x3633, 0x44},
	{0x363b, 0x20},
	{0x3641, 0x01},
	{0x3670, 0x09},
	{0x3674, 0xb0},
	{0x3675, 0x80},
	{0x3676, 0x88},
	{0x367c, 0x40},
	{0x367d, 0x49},
	{0x3690, 0x44},
	{0x3691, 0x33},
	{0x3692, 0x43},
	{0x369c, 0x49},
	{0x369d, 0x4f},
	{0x36ae, 0x4b},
	{0x36af, 0x4f},
	{0x36b0, 0x87},
	{0x36b1, 0x94},
	{0x36b2, 0xbc},
	{0x36d0, 0x01},
	{0x36ea, 0x09},
	{0x36eb, 0x0c},
	{0x36ec, 0x1c},
	{0x36ed, 0x24},
	{0x3722, 0x97},
	{0x3728, 0x90},
	{0x37fa, 0x09},
	{0x37fb, 0x32},
	{0x37fc, 0x10},
	{0x37fd, 0x34},
	{0x3901, 0x02},
	{0x3902, 0xc5},
	{0x3904, 0x04},
	{0x3907, 0x00},
	{0x3908, 0x41},
	{0x3909, 0x00},
	{0x390a, 0x00},
	{0x3928, 0xc3},
	{0x3933, 0x84},
	{0x3934, 0x10},
	{0x3940, 0x78},
	{0x3942, 0x04},
	{0x3943, 0x11},
	{0x3e00, 0x00},
	{0x3e01, 0x8c},
	{0x3e02, 0x20},
	{0x440e, 0x02},
	{0x4603, 0x09},
	{0x4819, 0x06},
	{0x481b, 0x03},
	{0x481d, 0x0b},
	{0x481f, 0x03},
	{0x4821, 0x08},
	{0x4823, 0x03},
	{0x4825, 0x03},
	{0x4827, 0x03},
	{0x4829, 0x05},
	{0x5010, 0x01},
	{0x5787, 0x08},
	{0x5788, 0x03},
	{0x5789, 0x00},
	{0x578a, 0x10},
	{0x578b, 0x08},
	{0x578c, 0x00},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x00},
	{0x5793, 0x10},
	{0x5794, 0x08},
	{0x5795, 0x00},
	{0x5799, 0x06},
	{0x57ad, 0x00},
	{0x5ae0, 0xfe},
	{0x5ae1, 0x40},
	{0x5ae2, 0x3f},
	{0x5ae3, 0x38},
	{0x5ae4, 0x28},
	{0x5ae5, 0x3f},
	{0x5ae6, 0x38},
	{0x5ae7, 0x28},
	{0x5ae8, 0x3f},
	{0x5ae9, 0x3c},
	{0x5aea, 0x2c},
	{0x5aeb, 0x3f},
	{0x5aec, 0x3c},
	{0x5aed, 0x2c},
	{0x5af4, 0x3f},
	{0x3000, 0xff},
	{0x5af5, 0x38},
	{0x5af6, 0x28},
	{0x5af7, 0x3f},
	{0x5af8, 0x38},
	{0x5af9, 0x28},
	{0x5afa, 0x3f},
	{0x5afb, 0x3c},
	{0x5afc, 0x2c},
	{0x5afd, 0x3f},
	{0x5afe, 0x3c},
	{0x5aff, 0x2c},
	{0x36e9, 0x53},
	{0x37f9, 0x53},
	{0x0100, 0x01},
};

static int sc230ai_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 1920;
	info->modes.mode[0].size.h = 1080;
	info->modes.mode[0].fps = g_sc230ai_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x30;
	info->i2c.addr_len = 2;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 5000);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 1000);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V5, 1000);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 5000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 0);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_HIGH, 10000);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 5000);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_RST_GPIO, 0, 0);
	set_power_item(&down->items[i++], SNR_HCLK, 0, 0);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_CORE_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_IO_POWER, 0, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static const struct fps_info *sc230ai_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_sc230ai_fps_info); i++)
		if (fps == g_sc230ai_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_sc230ai_fps_info))
		return NULL;

	return &g_sc230ai_fps_info[i];
}

static int sc230ai_get_init_info(uint32_t isp_id,
				 const struct rts_isp_sensor_mode *mode,
			       struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct sc230ai_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("sc230ai get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = sc230ai_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, clk_div: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->clk_div, fps_info->hts);

	set_init_i2c(&status->regs1[0], 0x320d, fps_info->hts & 0xff);
	set_init_i2c(&status->regs1[1], 0x320c, fps_info->hts >> 8);
	set_init_i2c(&status->regs1[2], 0x3221, 0x0);

	set_init_i2c_regs(info->sensor_regs[0], g_sc230ai_i2c_init_regs, 0);
	set_init_i2c_regs(info->sensor_regs[1], status->regs1, 0);

	info->interface.interface = SNR_INTERFACE_DVP;
	info->interface.dvp.sample_rising = 1;
	info->interface.dvp.hsync_active_high = 1;
	info->interface.dvp.vsync_active_high = 0;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 1928;
	info->size.h = 1088;
	info->start.x = 0;
	info->start.y = 1;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = status->min_vts = 1125;
	info->max_vts = 65535;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */
	status->cur_fps = mode->fps;

	return RTS_ISP_OK;
}

static int sc230ai_start(uint32_t isp_id)
{
	struct sc230ai_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static uint16_t get_sensor_gain_reg(float fgain)
{
	uint16_t reg_value = 0;

	if (fgain >= 108.512)
		reg_value = 0x5f;
	else if (fgain >= 54.256)
		reg_value = 0x4f;
	else if (fgain >= 27.128)
		reg_value = 0x4b;
	else if (fgain >= 13.564)
		reg_value = 0x49;
	else if (fgain >= 6.782)
		reg_value = 0x48;
	else if (fgain >= 3.391)
		reg_value = 0x40;
	else if (fgain >= 2.000)
		reg_value = 0x01;
	else
		reg_value = 0x00;

	return reg_value;

}

static float get_sensor_real_gain(uint16_t reg_value)
{
	float gain = 0.0;

	if (reg_value >= 0x5f)
		gain = 108.512;
	else if (reg_value >= 0x4f)
		gain = 54.256;
	else if (reg_value >= 0x4b)
		gain = 27.128;
	else if (reg_value >= 0x49)
		gain = 13.564;
	else if (reg_value >= 0x48)
		gain = 6.782;
	else if (reg_value >= 0x40)
		gain = 3.391;
	else if (reg_value >= 0x01)
		gain = 2.000;
	else
		gain = 1.000;

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

static int sc230ai_get_tuned_again(uint32_t isp_id,
				   float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int sc230ai_get_tuned_dgain(uint32_t isp_id,
				   float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int sc230ai_get_exposure_gain_info(uint32_t isp_id,
		const struct rts_isp_sensor_exp_gain *exp_gain,
		struct rts_isp_sync_regs *regs)
{

	int i;
	int exp_set;
	uint16_t total_line;
	uint16_t gain_reg;
	float exp_reg_value_float;
	uint32_t exp_reg_value;
	float gain;
	struct sc230ai_status *status;
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
	set_sync_i2c(&reg[i++], 0x320e, (total_line >> 8));
	set_sync_i2c(&reg[i++], 0x320f, (total_line & 0xff));
	exp_set = abs(status->last_exposure - exp_gain->exposure[0]) > 0.001f;
	if (exp_set) {
		exp_reg_value_float =
			2.0 * exp_gain->exposure[0] / status->exp_step + 0.5f;
		exp_reg_value =
			clip_d_word(exp_reg_value_float,
			1, (2 * total_line - 8));
		exp_reg_value = exp_reg_value << 4;
		set_sync_i2c(&reg[i++], 0x3e00, exp_reg_value >> 16);
		set_sync_i2c(&reg[i++], 0x3e01, (exp_reg_value & 0xff00) >> 8);
		set_sync_i2c(&reg[i++], 0x3e02, exp_reg_value & 0xff);
		status->last_exposure = exp_gain->exposure[0];
	}

	set_sync_i2c(&reg[i++], 0x3e09, (gain_reg & 0xff));

	regs->num = i;

	return RTS_ISP_OK;

}

static int sc230ai_check(uint32_t isp_id)
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

	if (id == 0xcb34)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops sc230ai_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "sc230ai",
	.get_info = sc230ai_get_info,
	.get_init_info = sc230ai_get_init_info,
	.start = sc230ai_start,
	.get_tuned_again = sc230ai_get_tuned_again,
	.get_tuned_dgain = sc230ai_get_tuned_dgain,
	.get_exposure_gain_info = sc230ai_get_exposure_gain_info,
	.check = sc230ai_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(sc230ai_ops)
