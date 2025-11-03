/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2023 Wei mo <wei_mo@apowertec.com>
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

struct sc2336p_status {
	float exp_step;
	float last_exposure;
	uint16_t cur_fps;
	uint16_t min_vts;
	struct rts_isp_i2c_reg regs1[1];
};

static struct rts_isp_i2c_info sc2336p_i2c_info = {
	.i2c_id = 0x30,
	.addr_len = 2,
	.data_len = 1,
};

static struct sc2336p_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_sc2336p_fps_info[] = {
	{30, 2200, 74250000},
};

static struct rts_isp_i2c_reg g_sc2336p_sensor_id_i2c_init_reg1[] = {
	{0x301a, 0xf8},
	{0x0100, 0x01},
};

static struct rts_isp_i2c_regs g_sc2336p_sensor_id_i2c_init_regs1 = {
	.num = ARRAY_SIZE(g_sc2336p_sensor_id_i2c_init_reg1),
	.regs = g_sc2336p_sensor_id_i2c_init_reg1,
	.udelay = 5000,
};

static struct rts_isp_i2c_reg g_sc2336p_sensor_id_i2c_init_reg2[] = {
	{0x0100, 0x00},
};

static struct rts_isp_i2c_regs g_sc2336p_sensor_id_i2c_init_regs2 = {
	.num = ARRAY_SIZE(g_sc2336p_sensor_id_i2c_init_reg2),
	.regs = g_sc2336p_sensor_id_i2c_init_reg2,
	.udelay = 0,
};

static struct rts_isp_i2c_reg g_sc2336p_30fps_i2c_init_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x301f, 0x02},
	{0x3106, 0x05},
	{0x3213, 0x03},
	{0x3248, 0x04},
	{0x3249, 0x0b},
	{0x3253, 0x08},
	{0x3301, 0x09},
	{0x3302, 0xff},
	{0x3303, 0x10},
	{0x3306, 0x80},
	{0x3307, 0x02},
	{0x3309, 0xc8},
	{0x330a, 0x01},
	{0x330b, 0x30},
	{0x330c, 0x16},
	{0x330d, 0xff},
	{0x3318, 0x02},
	{0x331f, 0xb9},
	{0x3321, 0x0a},
	{0x3327, 0x0e},
	{0x332b, 0x12},
	{0x3333, 0x10},
	{0x3334, 0x40},
	{0x335e, 0x06},
	{0x335f, 0x0a},
	{0x3364, 0x1f},
	{0x337c, 0x02},
	{0x337d, 0x0e},
	{0x3390, 0x09},
	{0x3391, 0x0f},
	{0x3392, 0x1f},
	{0x3393, 0x20},
	{0x3394, 0x20},
	{0x3395, 0xe0},
	{0x33a2, 0x04},
	{0x33b1, 0x80},
	{0x33b2, 0x68},
	{0x33b3, 0x42},
	{0x33f9, 0x90},
	{0x33fb, 0xd0},
	{0x33fc, 0x0f},
	{0x33fd, 0x1f},
	{0x349f, 0x03},
	{0x34a6, 0x0f},
	{0x34a7, 0x1f},
	{0x34a8, 0x42},
	{0x34a9, 0x18},
	{0x34aa, 0x01},
	{0x34ab, 0x43},
	{0x34ac, 0x01},
	{0x34ad, 0x80},
	{0x3630, 0xf4},
	{0x3632, 0x44},
	{0x3633, 0x22},
	{0x3639, 0xf4},
	{0x363c, 0x47},
	{0x3670, 0x09},
	{0x3674, 0xf4},
	{0x3675, 0xfb},
	{0x3676, 0xed},
	{0x367c, 0x09},
	{0x367d, 0x0f},
	{0x3690, 0x22},
	{0x3691, 0x22},
	{0x3692, 0x22},
	{0x3698, 0x89},
	{0x3699, 0x96},
	{0x369a, 0xd0},
	{0x369b, 0xd0},
	{0x369c, 0x09},
	{0x369d, 0x0f},
	{0x36a2, 0x09},
	{0x36a3, 0x0f},
	{0x36a4, 0x1f},
	{0x36d0, 0x01},
	{0x3722, 0xc1},
	{0x3724, 0x41},
	{0x3725, 0xc1},
	{0x3728, 0x20},
	{0x3900, 0x0d},
	{0x3905, 0x98},
	{0x3919, 0x04},
	{0x391b, 0x81},
	{0x391c, 0x10},
	{0x3933, 0x81},
	{0x3934, 0xd0},
	{0x3940, 0x75},
	{0x3941, 0x00},
	{0x3942, 0x01},
	{0x3943, 0xd1},
	{0x3952, 0x02},
	{0x3953, 0x0f},
	{0x3e01, 0x45},
	{0x3e02, 0xf0},
	{0x3e08, 0x1f},
	{0x3e1b, 0x14},
	{0x440e, 0x02},
	{0x4509, 0x38},
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
	{0x36e9, 0x20},
	{0x37f9, 0x27},
	{0x0100, 0x01},
};

static int sc2336p_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 1920;
	info->modes.mode[0].size.h = 1080;
	info->modes.mode[0].fps = g_sc2336p_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c = sc2336p_i2c_info;

	i = 0;
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 5000);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 5000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 5000);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_HIGH, 5000);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_27M, 5000);
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

static const struct fps_info *sc2336p_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_sc2336p_fps_info); i++)
		if (fps == g_sc2336p_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_sc2336p_fps_info))
		return NULL;

	return &g_sc2336p_fps_info[i];
}

static int sc2336p_get_init_info(uint32_t isp_id,
				const struct rts_isp_sensor_mode *mode,
			       struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct sc2336p_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("sc2336p get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = sc2336p_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, clk_div: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->clk_div, fps_info->hts);

	set_init_i2c_regs(info->sensor_regs[0],
		g_sc2336p_30fps_i2c_init_regs, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0 | MIPI_LANE1;
	info->interface.mipi.hs_term = 0x3;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 1920;
	info->size.h = 1080;
	info->start.x = 0;
	info->start.y = 0;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = status->min_vts = 1125;
	info->max_vts = 65535;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */
	status->cur_fps = mode->fps;

	return RTS_ISP_OK;
}

static int sc2336p_start(uint32_t isp_id)
{
	struct sc2336p_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;
	debug("start**\n");
	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static uint16_t get_sensor_gain_reg(float fgain)
{
	uint16_t reg_value = 0;

	if (fgain >= 32)
		reg_value = 0x1f;
	else if (fgain >= 16)
		reg_value = 0x0f;
		else if (fgain >= 8)
			reg_value = 0x0b;
		else if (fgain >= 4)
			reg_value = 0x09;
		else if (fgain >= 2)
			reg_value = 0x08;
		else
			reg_value = 0x00;

	return reg_value;
}

static float get_sensor_real_gain(uint16_t reg_value)
{
	float gain = 0.0;

	if (reg_value >= 0x1f)
		gain = 32.0;
	else if (reg_value >= 0x0f)
		gain = 16.0;
		else if (reg_value >= 0x0b)
			gain = 8.0;
		else if (reg_value >= 0x09)
			gain = 4.0;
		else if (reg_value >= 0x08)
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

static int sc2336p_get_tuned_again(uint32_t isp_id,
				  float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int sc2336p_get_tuned_dgain(uint32_t isp_id,
				  float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int sc2336p_get_exposure_gain_info(uint32_t isp_id,
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

	struct sc2336p_status *status;
	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	gain = exp_gain->analog_gain[0] * exp_gain->digital_gain[0];
	gain_reg = get_sensor_gain_reg(gain);

	total_line = exp_gain->vts;

	exp_reg_value_float =
		1.0 * exp_gain->exposure[0] / status->exp_step + 0.5f;
	exp_reg_value =
		clip_d_word(exp_reg_value_float, 1, (1 * total_line - 6));
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
	set_sync_i2c(&reg[i++], 0x3e09, (gain_reg & 0xff));
	regs->num = i;

	return RTS_ISP_OK;
}

static int sc2336p_check(uint32_t isp_id)
{
	int ret;
	int id;
	struct rts_isp_i2c_reg reg = {};

	rts_isp_write_i2c_regs(&sc2336p_i2c_info,
			&g_sc2336p_sensor_id_i2c_init_regs1);

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

	rts_isp_write_i2c_regs(&sc2336p_i2c_info,
			&g_sc2336p_sensor_id_i2c_init_regs2);

	if (id == 0x9b3a)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops sc2336p_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "sc2336p",
	.get_info = sc2336p_get_info,
	.get_init_info = sc2336p_get_init_info,
	.start = sc2336p_start,
	.get_tuned_again = sc2336p_get_tuned_again,
	.get_tuned_dgain = sc2336p_get_tuned_dgain,
	.get_exposure_gain_info = sc2336p_get_exposure_gain_info,
	.check = sc2336p_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(sc2336p_ops)
