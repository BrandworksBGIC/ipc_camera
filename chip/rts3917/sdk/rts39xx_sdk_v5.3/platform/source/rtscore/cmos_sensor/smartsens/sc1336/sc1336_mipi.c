/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2024 benno_ma <benno_ma@realsil.com.cn>
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

struct sc1336_gain {
	uint16_t ana_gain;
	uint16_t fine_gain;
	float total_gain;
};

struct sc1336_status {
	float exp_step;
	float last_exposure;
	uint16_t cur_fps;
	uint16_t min_vts;
	struct rts_isp_i2c_reg regs1[1];
};

static struct sc1336_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_sc1336_fps_info[] = {
	{60, 1540, 72000000},
};

static struct rts_isp_i2c_reg g_sc1336_60fps_i2c_init_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x301f, 0x0a},
	{0x303f, 0x82},
	{0x30b0, 0x21},
	{0x3106, 0x81},
	{0x320c, 0x06},
	{0x320d, 0x04},
	{0x320e, 0x03},
	{0x320f, 0x0c},
	{0x3213, 0x05},
	{0x3248, 0x04},
	{0x3249, 0x0b},
	{0x3301, 0x04},
	{0x3302, 0x10},
	{0x3303, 0x10},
	{0x3304, 0x40},
	{0x3306, 0x38},
	{0x3307, 0x02},
	{0x3308, 0x08},
	{0x3309, 0x60},
	{0x330a, 0x00},
	{0x330b, 0xa0},
	{0x330c, 0x16},
	{0x330d, 0x10},
	{0x330e, 0x10},
	{0x3318, 0x02},
	{0x331e, 0x39},
	{0x331f, 0x59},
	{0x3327, 0x0a},
	{0x3333, 0x10},
	{0x3334, 0x40},
	{0x335e, 0x06},
	{0x335f, 0x0a},
	{0x3364, 0x1f},
	{0x337a, 0x02},
	{0x337b, 0x06},
	{0x337c, 0x02},
	{0x337d, 0x0e},
	{0x3390, 0x01},
	{0x3391, 0x07},
	{0x3392, 0x0f},
	{0x3393, 0x04},
	{0x3394, 0x04},
	{0x3395, 0x04},
	{0x3396, 0x48},
	{0x3397, 0x49},
	{0x3398, 0x4f},
	{0x3399, 0x04},
	{0x339a, 0x05},
	{0x339b, 0x20},
	{0x339c, 0x38},
	{0x33a2, 0x04},
	{0x33a3, 0x04},
	{0x33ad, 0x0c},
	{0x33b1, 0x80},
	{0x33b2, 0x54},
	{0x33b3, 0x48},
	{0x33f9, 0x48},
	{0x33fb, 0x68},
	{0x33fc, 0x49},
	{0x33fd, 0x4f},
	{0x349f, 0x03},
	{0x34a6, 0x49},
	{0x34a7, 0x4f},
	{0x34a8, 0x30},
	{0x34a9, 0x18},
	{0x34aa, 0x00},
	{0x34ab, 0xa8},
	{0x34ac, 0x00},
	{0x34ad, 0xc8},
	{0x3630, 0xc0},
	{0x3631, 0x84},
	{0x3632, 0x74},
	{0x3633, 0x52},
	{0x3637, 0x2a},
	{0x363a, 0x89},
	{0x363b, 0x03},
	{0x363c, 0x08},
	{0x3641, 0x3a},
	{0x3670, 0x0f},
	{0x3674, 0xb0},
	{0x3675, 0xc0},
	{0x3676, 0xc0},
	{0x367c, 0x40},
	{0x367d, 0x48},
	{0x3690, 0x43},
	{0x3691, 0x43},
	{0x3692, 0x63},
	{0x3693, 0x84},
	{0x3694, 0x88},
	{0x3695, 0x8a},
	{0x3698, 0x89},
	{0x3699, 0x92},
	{0x369a, 0xa5},
	{0x369b, 0xca},
	{0x369c, 0x48},
	{0x369d, 0x5f},
	{0x369e, 0x48},
	{0x369f, 0x4b},
	{0x36a2, 0x49},
	{0x36a3, 0x4b},
	{0x36a4, 0x4f},
	{0x36a6, 0x49},
	{0x36a7, 0x4b},
	{0x36ab, 0x74},
	{0x36ac, 0x74},
	{0x36ad, 0x78},
	{0x36d0, 0x01},
	{0x36ea, 0x06},
	{0x36eb, 0x0c},
	{0x36ec, 0x1c},
	{0x36ed, 0x28},
	{0x370f, 0x01},
	{0x3722, 0x01},
	{0x3724, 0x41},
	{0x3725, 0xc4},
	{0x37b0, 0x01},
	{0x37b1, 0x01},
	{0x37b2, 0x01},
	{0x37b3, 0x4f},
	{0x37b4, 0x5f},
	{0x37fa, 0x09},
	{0x37fb, 0x32},
	{0x37fc, 0x01},
	{0x37fd, 0x17},
	{0x3900, 0x0d},
	{0x3902, 0xdf},
	{0x3905, 0xb8},
	{0x3908, 0x41},
	{0x391b, 0x81},
	{0x391c, 0x10},
	{0x391f, 0x30},
	{0x3933, 0x81},
	{0x3934, 0xd4},
	{0x3940, 0x6b},
	{0x3941, 0x00},
	{0x3942, 0x01},
	{0x3943, 0xd7},
	{0x3952, 0x02},
	{0x3953, 0x0f},
	{0x3e01, 0x61},
	{0x3e02, 0x00},
	{0x3e08, 0x1f},
	{0x3e1b, 0x14},
	{0x4509, 0x1c},
	{0x4819, 0x05},
	{0x481b, 0x03},
	{0x481d, 0x0a},
	{0x481f, 0x02},
	{0x4821, 0x08},
	{0x4823, 0x03},
	{0x4825, 0x02},
	{0x4827, 0x03},
	{0x4829, 0x04},
	{0x4831, 0x02},
	{0x5799, 0x06},
	{0x5ae0, 0xfe},
	{0x5ae1, 0x40},
	{0x5ae2, 0x30},
	{0x5ae3, 0x28},
	{0x5ae4, 0x20},
	{0x5ae5, 0x30},
	{0x5ae6, 0x28},
	{0x5ae7, 0x20},
	{0x5ae8, 0x3c},
	{0x5ae9, 0x30},
	{0x5aea, 0x28},
	{0x5aeb, 0x3c},
	{0x5aec, 0x30},
	{0x5aed, 0x28},
	{0x5aee, 0xfe},
	{0x5aef, 0x40},
	{0x5af4, 0x30},
	{0x5af5, 0x28},
	{0x5af6, 0x20},
	{0x5af7, 0x30},
	{0x5af8, 0x28},
	{0x5af9, 0x20},
	{0x5afa, 0x3c},
	{0x5afb, 0x30},
	{0x5afc, 0x28},
	{0x5afd, 0x3c},
	{0x5afe, 0x30},
	{0x5aff, 0x28},
	{0x36e9, 0x24},
	{0x37f9, 0x24},
	{0x0100, 0x01},
};

static int sc1336_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 1280;
	info->modes.mode[0].size.h = 720;
	info->modes.mode[0].fps = g_sc1336_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x30;
	info->i2c.addr_len = 2;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 0);
	//set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 1000);
	//set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V4, 1000);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 3000);
	//set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 3000);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_HIGH, 10000);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 5000);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_PWDN_GPIO, 0, 0);
	set_power_item(&down->items[i++], SNR_HCLK, 0, 0);
	set_power_item(&down->items[i++], SNR_IO_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_CORE_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, 0, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static const struct fps_info *sc1336_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_sc1336_fps_info); i++)
		if (fps == g_sc1336_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_sc1336_fps_info))
		return NULL;

	return &g_sc1336_fps_info[i];
}

static int sc1336_get_init_info(uint32_t isp_id,
				const struct rts_isp_sensor_mode *mode,
			       struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct sc1336_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("sc1336 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = sc1336_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, clk_div: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->clk_div, fps_info->hts);

	set_init_i2c_regs(info->sensor_regs[0],
		g_sc1336_60fps_i2c_init_regs, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0 | MIPI_LANE1;
	info->interface.mipi.hs_term = 0x4;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 1280;
	info->size.h = 720;
	info->start.x = 0;
	info->start.y = 0;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = status->min_vts = 780;
	info->max_vts = 65535;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */
	status->cur_fps = mode->fps;

	return RTS_ISP_OK;
}

static int sc1336_start(uint32_t isp_id)
{
	struct sc1336_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}


static uint16_t get_sensor_gain_reg(float fgain)
{
	uint16_t reg_value = 0;

	if (fgain >= 32)
		reg_value = 0x3f0f;
	else if (fgain >= 16)
		reg_value = 0x3f0b;
		else if (fgain >= 8)
			reg_value = 0x3f09;
		else if (fgain >= 4)
			reg_value = 0x3f08;
		else if (fgain >= 2)
			reg_value = 0x3f00;
		else
			reg_value = 0x1f00;

	return reg_value;
}

static float get_sensor_real_gain(uint16_t reg_value)
{
	float gain = 0.0;

	if (reg_value >= 0x3f0f)
		gain = 32.0;
	else if (reg_value >= 0x3f0b)
		gain = 16.0;
		else if (reg_value >= 0x3f09)
			gain = 8.0;
		else if (reg_value >= 0x3f08)
			gain = 4.0;
		else if (reg_value >= 0x3f00)
			gain = 2.0;
		else
			gain = 1.0;

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

static int sc1336_get_tuned_again(uint32_t isp_id,
				  float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;

}

static int sc1336_get_tuned_dgain(uint32_t isp_id,
				  float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int sc1336_get_exposure_gain_info(uint32_t isp_id,
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

	struct sc1336_status *status;
	struct rts_isp_sync_reg *reg;


	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
	return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	gain = exp_gain->analog_gain[0] * exp_gain->digital_gain[0];
	gain_reg = get_sensor_gain_reg(gain);

	total_line = exp_gain->vts;

	exp_reg_value_float =
		2.0 * exp_gain->exposure[0] / status->exp_step + 0.5f;
	exp_reg_value =
		clip_d_word(exp_reg_value_float, 2, (2 * total_line - 8));
	exp_reg_value = exp_reg_value << 4;

	total_line = (total_line + 1) / 2 * 2;
	reg = regs->reg;

	i = 0;
	set_sync_i2c(&reg[i++], 0x320e, (total_line >> 8));
	set_sync_i2c(&reg[i++], 0x320f, (total_line & 0xff));
	exp_set = abs(status->last_exposure - exp_gain->exposure[0]) > 0.001f;
	if (exp_set) {
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

static int sc1336_get_mirror_flip(uint32_t isp_id,
				  const struct rts_isp_mirror_flip *mf_info,
				  struct rts_isp_sync_regs *regs)
{
	int i = 0;
	uint32_t val = 0;
	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !mf_info || !regs)
		return -RTS_ISP_EINVAL;

	rts_isp_drop_frames(isp_id, 1);
	if (mf_info->mirror)
		val |= 0x6;
	if (mf_info->flip)
		val |= 0x60;
	reg = regs->reg;
	set_sync_i2c_mask(&reg[i++], 0x3221, val, 0x66);
	regs->num = i;

	return RTS_ISP_OK;
}

static int sc1336_check(uint32_t isp_id)
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

	if (id == 0xca3f)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops sc1336_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "sc1336",
	.get_info = sc1336_get_info,
	.get_init_info = sc1336_get_init_info,
	.start = sc1336_start,
	.get_tuned_again = sc1336_get_tuned_again,
	.get_tuned_dgain = sc1336_get_tuned_dgain,
	.get_exposure_gain_info = sc1336_get_exposure_gain_info,
	.get_mirror_flip = sc1336_get_mirror_flip,
	.check = sc1336_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(sc1336_ops)
