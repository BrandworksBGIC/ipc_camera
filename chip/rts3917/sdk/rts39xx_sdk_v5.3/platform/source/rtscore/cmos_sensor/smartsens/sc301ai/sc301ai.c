/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2020 Martial Wu <martial_wu@realsil.com.cn>
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

struct sc301ai_gain {
	uint16_t ana_gain;
	float total_gain;
};

struct sc301ai_status {
	float exp_step;
	float last_exposure;
	uint16_t cur_fps;
	uint16_t min_vts;
};

static struct sc301ai_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_sc301ai_fps_info[] = {
	{30, 2250, 108000000},
};

static struct rts_isp_i2c_reg g_sc301ai_30fps_i2c_init_regs[] = {
	{0x0103, 0x01,},
	{0x0100, 0x00,},
	{0x36e9, 0x80,},
	{0x37f9, 0x80,},
	{0x301c, 0x78,},
	{0x301f, 0x01,},
	{0x30b8, 0x44,},
	{0x3208, 0x08,},
	{0x3209, 0x00,},
	{0x320a, 0x06,},
	{0x320b, 0x00,},
	{0x320c, 0x04,},
	{0x320d, 0x65,},
	{0x320e, 0x06,},
	{0x320f, 0x40,},
	{0x3213, 0x05,},
	{0x3214, 0x11,},
	{0x3215, 0x11,},
	{0x3223, 0xc0,},
	{0x3253, 0x0c,},
	{0x3274, 0x09,},
	{0x3301, 0x08,},
	{0x3306, 0x58,},
	{0x3308, 0x08,},
	{0x330a, 0x00,},
	{0x330b, 0xe0,},
	{0x330e, 0x10,},
	{0x331e, 0x55,},
	{0x331f, 0x7d,},
	{0x3333, 0x10,},
	{0x3334, 0x40,},
	{0x335e, 0x06,},
	{0x335f, 0x08,},
	{0x3364, 0x5e,},
	{0x337c, 0x02,},
	{0x337d, 0x0a,},
	{0x3390, 0x01,},
	{0x3391, 0x03,},
	{0x3392, 0x07,},
	{0x3393, 0x08,},
	{0x3394, 0x08,},
	{0x3395, 0x08,},
	{0x3396, 0x08,},
	{0x3397, 0x09,},
	{0x3398, 0x1f,},
	{0x3399, 0x08,},
	{0x339a, 0x20,},
	{0x339b, 0x40,},
	{0x339c, 0x78,},
	{0x33a2, 0x04,},
	{0x33ad, 0x0c,},
	{0x33b1, 0x80,},
	{0x33b3, 0x30,},
	{0x33f9, 0x68,},
	{0x33fb, 0x88,},
	{0x33fc, 0x48,},
	{0x33fd, 0x5f,},
	{0x349f, 0x03,},
	{0x34a6, 0x48,},
	{0x34a7, 0x5f,},
	{0x34a8, 0x30,},
	{0x34a9, 0x30,},
	{0x34aa, 0x00,},
	{0x34ab, 0xf0,},
	{0x34ac, 0x01,},
	{0x34ad, 0x12,},
	{0x34f8, 0x5f,},
	{0x34f9, 0x10,},
	{0x3630, 0xf0,},
	{0x3631, 0x85,},
	{0x3632, 0x74,},
	{0x3633, 0x22,},
	{0x3637, 0x4d,},
	{0x3638, 0xcb,},
	{0x363a, 0x8b,},
	{0x3641, 0x00,},
	{0x3670, 0x4e,},
	{0x3674, 0xf0,},
	{0x3675, 0xc0,},
	{0x3676, 0xc0,},
	{0x3677, 0x85,},
	{0x3678, 0x8a,},
	{0x3679, 0x8d,},
	{0x367c, 0x48,},
	{0x367d, 0x49,},
	{0x367e, 0x49,},
	{0x367f, 0x5f,},
	{0x3690, 0x22,},
	{0x3691, 0x33,},
	{0x3692, 0x44,},
	{0x3699, 0x88,},
	{0x369a, 0x98,},
	{0x369b, 0xc4,},
	{0x369c, 0x48,},
	{0x369d, 0x5f,},
	{0x36a2, 0x49,},
	{0x36a3, 0x4f,},
	{0x370f, 0x01,},
	{0x3714, 0x80,},
	{0x3722, 0x09,},
	{0x3724, 0x41,},
	{0x3725, 0xc1,},
	{0x3728, 0x00,},
	{0x3771, 0x09,},
	{0x3772, 0x05,},
	{0x3773, 0x05,},
	{0x377a, 0x48,},
	{0x377b, 0x49,},
	{0x3905, 0x8d,},
	{0x391d, 0x08,},
	{0x3922, 0x1a,},
	{0x3926, 0x21,},
	{0x3933, 0x80,},
	{0x3934, 0x02,},
	{0x3937, 0x72,},
	{0x3939, 0x00,},
	{0x393a, 0x03,},
	{0x39dc, 0x02,},
	{0x3e00, 0x00,},
	{0x3e01, 0x63,},
	{0x3e02, 0xc0,},
	{0x3e03, 0x0b,},
	{0x3e1b, 0x2a,},
	{0x4407, 0x34,},
	{0x440e, 0x02,},
	{0x5001, 0x40,},
	{0x5007, 0x80,},
	{0x36e9, 0x24,},
	{0x37f9, 0x24,},
	{0x0100, 0x01,},
};

static struct sc301ai_gain gain_mapping[] = {
	{0x00, 1.000},
	{0x40, 1.569},
	{0x48, 3.138},
	{0x49, 6.276},
	{0x4b, 12.552},
	{0x4f, 25.104},
	{0x5f, 50.208},
};

static int sc301ai_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 2048;
	info->modes.mode[0].size.h = 1536;
	info->modes.mode[0].fps = g_sc301ai_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x30;
	info->i2c.addr_len = 2;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 1000);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V2, 1000);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 3000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 3000);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_HIGH, 5000);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 5000);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_RST_GPIO, 0, 0);
	set_power_item(&down->items[i++], SNR_HCLK, 0, 0);
	set_power_item(&down->items[i++], SNR_IO_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_CORE_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, 0, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static const struct fps_info *sc301ai_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_sc301ai_fps_info); i++)
		if (fps == g_sc301ai_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_sc301ai_fps_info))
		return NULL;

	return &g_sc301ai_fps_info[i];
}

static int sc301ai_get_init_info(uint32_t isp_id,
				 const struct rts_isp_sensor_mode *mode,
			       struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct sc301ai_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("sc301ai get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = sc301ai_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, clk_div: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->clk_div, fps_info->hts);

	set_init_i2c_regs(info->sensor_regs[0],
		g_sc301ai_30fps_i2c_init_regs, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = (MIPI_LANE0 | MIPI_LANE1);
	info->interface.mipi.hs_term = 0x3;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 2048;
	info->size.h = 1536;
	info->start.x = 0;
	info->start.y = 0;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = status->min_vts = 1600;
	info->max_vts = 65535;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */
	status->cur_fps = mode->fps;

	return RTS_ISP_OK;
}

static int sc301ai_start(uint32_t isp_id)
{
	struct sc301ai_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static uint16_t get_sensor_gain_reg(float fgain)
{
	uint16_t reg_value = 0;
	int i;

	if (fgain >= 50.208) {
		reg_value = 0x5f;
	} else {
		for (i = 0; i < ((ARRAY_SIZE(gain_mapping)) - 1); i++) {
			if ((gain_mapping[i].total_gain <= fgain) &&
			    (fgain < gain_mapping[i + 1].total_gain)) {
				reg_value = gain_mapping[i].ana_gain;
				break;
			}
		}
	}
	return reg_value;
}

static float get_sensor_real_gain(uint16_t reg_value)
{
	float gain = 0.0;
	int i;

	for (i = 0; i < ARRAY_SIZE(gain_mapping); i++) {
		if (reg_value == gain_mapping[i].ana_gain) {
			gain = gain_mapping[i].total_gain;
			break;
		}
	}

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

static int sc301ai_get_tuned_again(uint32_t isp_id,
				   float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int sc301ai_get_tuned_dgain(uint32_t isp_id,
				   float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int sc301ai_get_exposure_gain_info(uint32_t isp_id,
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
	struct sc301ai_status *status;
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
		clip_d_word(exp_reg_value_float, 1, total_line - 4);
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

static int sc301ai_check(uint32_t isp_id)
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

	if (id == 0xcc40)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops sc301ai_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "sc301ai",
	.get_info = sc301ai_get_info,
	.get_init_info = sc301ai_get_init_info,
	.start = sc301ai_start,
	.get_tuned_again = sc301ai_get_tuned_again,
	.get_tuned_dgain = sc301ai_get_tuned_dgain,
	.get_exposure_gain_info = sc301ai_get_exposure_gain_info,
	.check = sc301ai_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(sc301ai_ops)
