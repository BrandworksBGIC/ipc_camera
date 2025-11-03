/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2023 wei_mo <wei_mo@apowertec.com>
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

struct sc3332_status {
	float exp_step;
	float last_exposure;
	uint16_t cur_fps;
	uint16_t min_vts;
	struct rts_isp_i2c_reg regs1[3];
};

static struct sc3332_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_sc3332_fps_info[] = {
	{30, 2500, 102000000},
};

//sensor setting MIPI@30FPS
static struct rts_isp_i2c_reg g_sc3332_i2c_init_regs[] = {
	{0x0103, 0x01},
	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x301f, 0x05},
	{0x30b8, 0x44},
	{0x3200, 0x00},
	{0x3201, 0x00},
	{0x3202, 0x00},
	{0x3203, 0x00},
	{0x3204, 0x09},
	{0x3205, 0x0f},
	{0x3206, 0x05},
	{0x3207, 0x1b},
	{0x3208, 0x09},
	{0x3209, 0x08},
	{0x320a, 0x05},
	{0x320b, 0x18},
	{0x320e, 0x05},
	{0x320f, 0x50},
	{0x3210, 0x00},
	{0x3211, 0x04},
	{0x3212, 0x00},
	{0x3213, 0x02},
	{0x3253, 0x10},
	{0x3301, 0x08},
	{0x3302, 0xff},
	{0x3305, 0x00},
	{0x3306, 0x90},
	{0x3308, 0x18},
	{0x330a, 0x01},
	{0x330b, 0xc0},
	{0x330d, 0x70},
	{0x330e, 0x30},
	{0x3314, 0x15},
	{0x3333, 0x10},
	{0x3334, 0x40},
	{0x335e, 0x06},
	{0x335f, 0x0a},
	{0x3364, 0x5e},
	{0x337d, 0x0e},
	{0x3390, 0x08},
	{0x3391, 0x09},
	{0x3392, 0x0f},
	{0x3393, 0x10},
	{0x3394, 0x80},
	{0x3395, 0xff},
	{0x33a2, 0x04},
	{0x33ad, 0x2c},
	{0x33b3, 0x48},
	{0x33f8, 0x00},
	{0x33f9, 0xc0},
	{0x33fa, 0x00},
	{0x33fb, 0xf0},
	{0x33fc, 0x0b},
	{0x33fd, 0x0f},
	{0x349f, 0x03},
	{0x34a6, 0x0b},
	{0x34a7, 0x0f},
	{0x34a8, 0x40},
	{0x34a9, 0x30},
	{0x34aa, 0x01},
	{0x34ab, 0xf0},
	{0x34ac, 0x02},
	{0x34ad, 0x10},
	{0x34f8, 0x1f},
	{0x34f9, 0x30},
	{0x3630, 0xf0},
	{0x3631, 0x8c},
	{0x3632, 0x78},
	{0x3633, 0x33},
	{0x363a, 0xcc},
	{0x363c, 0x0f},
	{0x363f, 0xc0},
	{0x3641, 0x00},
	{0x3670, 0x5e},
	{0x3674, 0xf0},
	{0x3675, 0xf0},
	{0x3676, 0xd0},
	{0x3677, 0x87},
	{0x3678, 0x8a},
	{0x3679, 0x8d},
	{0x367c, 0x08},
	{0x367d, 0x0f},
	{0x367e, 0x08},
	{0x367f, 0x0f},
	{0x3690, 0x74},
	{0x3691, 0x78},
	{0x3692, 0x78},
	{0x3696, 0x33},
	{0x3697, 0x33},
	{0x3698, 0x45},
	{0x369c, 0x0b},
	{0x369d, 0x0f},
	{0x36a0, 0x09},
	{0x36a1, 0x0f},
	{0x36b0, 0x88},
	{0x36b1, 0x91},
	{0x36b2, 0xa4},
	{0x36b3, 0xcf},
	{0x36b4, 0x09},
	{0x36b5, 0x0b},
	{0x36b6, 0x0f},
	{0x36ea, 0x11},
	{0x36eb, 0x0c},
	{0x36ec, 0x1c},
	{0x36ed, 0x26},
	{0x370f, 0x01},
	{0x3722, 0x05},
	{0x3724, 0x31},
	{0x3771, 0x09},
	{0x3772, 0x05},
	{0x3773, 0x05},
	{0x377a, 0x0b},
	{0x377b, 0x0f},
	{0x37fa, 0x11},
	{0x37fb, 0x31},
	{0x37fc, 0x11},
	{0x37fd, 0x08},
	{0x3904, 0x04},
	{0x3905, 0x8c},
	{0x391d, 0x01},
	{0x3922, 0x1f},
	{0x3925, 0x0f},
	{0x3926, 0x21},
	{0x3933, 0x80},
	{0x3934, 0x03},
	{0x3937, 0x6f},
	{0x39dc, 0x02},
	{0x3e00, 0x00},
	{0x3e01, 0x54},
	{0x3e02, 0x80},
	{0x440e, 0x02},
	{0x4509, 0x28},
	{0x450d, 0x32},
	{0x4819, 0x07},
	{0x481b, 0x04},
	{0x481d, 0x0e},
	{0x481f, 0x03},
	{0x4821, 0x09},
	{0x4823, 0x04},
	{0x4825, 0x03},
	{0x4827, 0x03},
	{0x4829, 0x06},
	{0x5780, 0x66},
	{0x578d, 0x40},
	{0x36e9, 0x54},
	{0x37f9, 0x47},
	{0x0100, 0x01},
};

static int sc3332_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 2304;
	info->modes.mode[0].size.h = 1296;
	info->modes.mode[0].fps = g_sc3332_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x30;
	info->i2c.addr_len = 2;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 5000);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 5000);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V2, 5000);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 5000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 5000);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_HIGH, 5000);
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

static const struct fps_info *sc3332_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_sc3332_fps_info); i++)
		if (fps == g_sc3332_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_sc3332_fps_info))
		return NULL;

	return &g_sc3332_fps_info[i];
}

static int sc3332_get_init_info(uint32_t isp_id,
				 const struct rts_isp_sensor_mode *mode,
			       struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct sc3332_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("sc3332 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = sc3332_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, clk_div: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->clk_div, fps_info->hts);

	set_init_i2c_regs(info->sensor_regs[0], g_sc3332_i2c_init_regs, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0 | MIPI_LANE1;
	info->interface.mipi.hs_term = 0x5;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 2312;
	info->size.h = 1304;
	info->start.x = 0;
	info->start.y = 1;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = status->min_vts = 1360;
	info->max_vts = 65535;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */
	status->cur_fps = mode->fps;

	return RTS_ISP_OK;
}

static int sc3332_start(uint32_t isp_id)
{
	struct sc3332_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static uint16_t get_sensor_gain_reg(float fgain)
{
	uint16_t reg_value = 0;

	if (fgain >= 16.0)
		reg_value = 0x0f;
	else if (fgain >= 8.0)
		reg_value = 0x0b;
	else if (fgain >= 4.0)
		reg_value = 0x09;
	else if (fgain >= 2.0)
		reg_value = 0x08;
	else
		reg_value = 0x00;

	return reg_value;
}

static float get_sensor_real_gain(uint16_t reg_value)
{
	float gain = 0.0;

	if (reg_value >= 0x0f)
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

static int sc3332_get_tuned_again(uint32_t isp_id,
				   float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int sc3332_get_tuned_dgain(uint32_t isp_id,
				   float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int sc3332_get_exposure_gain_info(uint32_t isp_id,
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
	struct sc3332_status *status;
	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;
	status = &g_status[isp_id];

	gain = exp_gain->analog_gain[0] * exp_gain->digital_gain[0];
	gain_reg = get_sensor_gain_reg(gain);

	total_line = exp_gain->vts;

	exp_reg_value_float =
		exp_gain->exposure[0] / status->exp_step + 0.5f;
	exp_reg_value =
		clip_d_word(exp_reg_value_float,
		2, (total_line - 6));
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

static int sc3332_check(uint32_t isp_id)
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

	if (id == 0xcc44)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops sc3332_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "sc3332",
	.get_info = sc3332_get_info,
	.get_init_info = sc3332_get_init_info,
	.start = sc3332_start,
	.get_tuned_again = sc3332_get_tuned_again,
	.get_tuned_dgain = sc3332_get_tuned_dgain,
	.get_exposure_gain_info = sc3332_get_exposure_gain_info,
	.check = sc3332_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(sc3332_ops)
