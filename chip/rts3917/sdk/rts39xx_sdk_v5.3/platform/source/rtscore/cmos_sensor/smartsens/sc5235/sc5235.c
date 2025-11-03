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

struct sc5235_status {
	float exp_step;
	float last_exposure;
	uint16_t min_vts;
	struct rts_isp_i2c_reg regs1[2];
};

static struct sc5235_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_sc5235_fps_info[] = {
	{20, 3000, 118800000},
};

static struct rts_isp_i2c_reg g_sc5235_i2c_init_regs[] = {
	{0x0103, 0x01}, {0x0100, 0x00}, {0x3039, 0x80}, {0x3029, 0x80},
	{0x302a, 0x75}, {0x302b, 0x10}, {0x302c, 0x00}, {0x302d, 0x00},
	{0x3038, 0x44}, {0x303a, 0x75}, {0x303b, 0x0a}, {0x303c, 0x0e},
	{0x303d, 0x03}, {0x3200, 0x00}, {0x3201, 0x08}, {0x3202, 0x00},
	{0x3203, 0x04}, {0x3204, 0x0a}, {0x3205, 0x2f}, {0x3206, 0x07},
	{0x3207, 0xa5}, {0x3208, 0x0a}, {0x3209, 0x20}, {0x320a, 0x07},
	{0x320b, 0x9a}, {0x320c, 0x05}, {0x320d, 0xdc}, {0x320e, 0x07},
	{0x320f, 0xbc}, {0x3210, 0x00}, {0x3211, 0x04}, {0x3212, 0x00},
	{0x3213, 0x04}, {0x3235, 0x0f}, {0x3236, 0x76}, {0x3301, 0x18},
	{0x3303, 0x28}, {0x3304, 0x10}, {0x3306, 0x50}, {0x3308, 0x10},
	{0x3309, 0x70}, {0x330a, 0x00}, {0x330b, 0xb8}, {0x330e, 0x20},
	{0x3314, 0x14}, {0x3315, 0x02}, {0x331b, 0x83}, {0x331e, 0x19},
	{0x331f, 0x61}, {0x3320, 0x01}, {0x3321, 0x04}, {0x3326, 0x00},
	{0x3333, 0x20}, {0x3334, 0x40}, {0x3364, 0x05}, {0x3366, 0x78},
	{0x3367, 0x08}, {0x3368, 0x03}, {0x3369, 0x00}, {0x336a, 0x00},
	{0x336b, 0x00}, {0x336c, 0x01}, {0x336d, 0x40}, {0x337f, 0x03},
	{0x338f, 0x40}, {0x33b6, 0x07}, {0x33b7, 0x17}, {0x33b8, 0x20},
	{0x33b9, 0x20}, {0x33ba, 0x44}, {0x3620, 0x28}, {0x3621, 0xac},
	{0x3622, 0xf6}, {0x3623, 0x10}, {0x3624, 0x47}, {0x3625, 0x0b},
	{0x3630, 0x30}, {0x3631, 0x88}, {0x3632, 0x18}, {0x3633, 0x23},
	{0x3634, 0x86}, {0x3635, 0x4d}, {0x3636, 0x21}, {0x3637, 0x20},
	{0x3638, 0x18}, {0x3639, 0x09}, {0x363a, 0x83}, {0x363b, 0x02},
	{0x363c, 0x07}, {0x363d, 0x03}, {0x3670, 0x00}, {0x3677, 0x86},
	{0x3678, 0x86}, {0x3679, 0xa8}, {0x367e, 0x08}, {0x367f, 0x18},
	{0x3802, 0x01}, {0x3905, 0x98}, {0x3907, 0x01}, {0x3908, 0x11},
	{0x390a, 0x00}, {0x391b, 0x90}, {0x391c, 0x9f}, {0x391d, 0x00},
	{0x391e, 0x01}, {0x391f, 0xc0}, {0x3e00, 0x00}, {0x3e01, 0xf7},
	{0x3e02, 0x00}, {0x3e03, 0x0b}, {0x3e06, 0x00}, {0x3e07, 0x80},
	{0x3e08, 0x03}, {0x3e09, 0x20}, {0x3e1e, 0x30}, {0x3e26, 0x20},
	{0x3f00, 0x0d}, {0x3f04, 0x02}, {0x3f05, 0xca}, {0x3f08, 0x04},
	{0x4500, 0x5d}, {0x4509, 0x10}, {0x4800, 0x64}, {0x4809, 0x01},
	{0x4837, 0x21}, {0x5000, 0x06}, {0x5002, 0x06}, {0x5780, 0x7f},
	{0x5781, 0x06}, {0x5782, 0x04}, {0x5783, 0x00}, {0x5784, 0x00},
	{0x5785, 0x16}, {0x5786, 0x12}, {0x5787, 0x08}, {0x5788, 0x02},
	{0x578b, 0x07}, {0x57a0, 0x00}, {0x57a1, 0x72}, {0x57a2, 0x01},
	{0x57a3, 0xf2}, {0x5988, 0x02}, {0x598e, 0x05}, {0x598f, 0xaa},
	{0x6000, 0x20}, {0x6002, 0x00}, {0x3039, 0x25}, {0x3029, 0x37},
	{0x0100, 0x01},
};

static int sc5235_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 2592;
	info->modes.mode[0].size.h = 1944;
	info->modes.mode[0].fps = g_sc5235_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x30;
	info->i2c.addr_len = 2;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 0);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V5, 1000);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 2000);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 0);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 0);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_HIGH, 2000);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&down->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 0);
	set_power_item(&down->items[i++], SNR_HCLK, CLK_NONE, 0);
	set_power_item(&down->items[i++], SNR_IO_POWER, PWR_NONE, 0);
	set_power_item(&down->items[i++], SNR_CORE_POWER, PWR_NONE, 0);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, PWR_NONE, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static const struct fps_info *sc5235_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_sc5235_fps_info); i++)
		if (fps == g_sc5235_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_sc5235_fps_info))
		return NULL;

	return &g_sc5235_fps_info[i];
}

static int sc5235_get_init_info(uint32_t isp_id,
				const struct rts_isp_sensor_mode *mode,
			       struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct sc5235_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("sc5235 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = sc5235_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, clk_div: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->clk_div, fps_info->hts);

	set_init_i2c(&status->regs1[0], 0x320d, (fps_info->hts >> 1) & 0xff);
	set_init_i2c(&status->regs1[1], 0x320c, (fps_info->hts >> 1) >> 8);

	set_init_i2c_regs(info->sensor_regs[0], g_sc5235_i2c_init_regs, 0);
	set_init_i2c_regs(info->sensor_regs[1], status->regs1, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0 | MIPI_LANE1;
	info->interface.mipi.hs_term = 0x6;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 2592;
	info->size.h = 1946;
	info->start.x = 0;
	info->start.y = 1;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = status->min_vts = 1980;
	info->max_vts = 65535;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */

	return RTS_ISP_OK;
}

static int sc5235_start(uint32_t isp_id)
{
	struct sc5235_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static uint16_t get_sensor_gain_reg(float fgain)
{
	uint16_t reg_value = 0;

	if (fgain >= 15.5) {
		reg_value = 0x71f;
	} else {
		if (fgain >= 8)
			reg_value = (uint16_t)(fgain / 8 / 0.0625f) | 0x700;
		else if (fgain >= 4)
			reg_value = (uint16_t)(fgain / 4 / 0.0625f) | 0x300;
		else if (fgain >= 2)
			reg_value = (uint16_t)(fgain / 2 / 0.0625f) | 0x100;
		else
			reg_value = (uint16_t)(fgain / 1 / 0.0625f);
	}
	return reg_value;
}

static float get_sensor_real_gain(uint16_t reg_value)
{
	float gain;

	gain = ((reg_value & 0xff) * 0.0625f) * ((reg_value >> 8) + 1);

	return gain;
}

static int sc5235_get_tuned_again(uint32_t isp_id,
				  float again[RTS_ISP_HDR_CHAN_MAX])
{
	uint16_t gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int sc5235_get_tuned_dgain(uint32_t isp_id,
				  float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int sc5235_get_exposure_gain_info(uint32_t isp_id,
					const struct rts_isp_sensor_exp_gain *exp_gain,
					struct rts_isp_sync_regs *regs)
{
	int i;
	uint16_t total_line;
	uint16_t gain_reg;
	float gain;
	struct sc5235_status *status;
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
	if (abs(status->last_exposure - exp_gain->exposure[0]) > 0.001f) {
		uint32_t exp_reg_value;

		exp_reg_value = exp_gain->exposure[0] /
			(status->exp_step / 2) + 0.5f;
		exp_reg_value *= 16;
		set_sync_i2c(&reg[i++], 0x3e00, exp_reg_value >> 16);
		set_sync_i2c(&reg[i++], 0x3e01, (exp_reg_value & 0xff00) >> 8);
		set_sync_i2c(&reg[i++], 0x3e02, exp_reg_value & 0xff);
		status->last_exposure = exp_gain->exposure[0];
	}
	set_sync_i2c(&reg[i++], 0x320e, (total_line >> 8));
	set_sync_i2c(&reg[i++], 0x320f, (total_line & 0xff));
	set_sync_i2c(&reg[i++], 0x3e08, (gain_reg >> 8));
	set_sync_i2c(&reg[i++], 0x3e09, (gain_reg & 0xff));
	set_sync_info(&reg[i++], 1, RTS_ISP_INT_DATA_START);
	set_sync_i2c(&reg[i++], 0x3903, 0x84);
	set_sync_i2c(&reg[i++], 0x3903, 0x04);
	set_sync_i2c(&reg[i++], 0x3812, 0x00);

	if (gain < 2) {
		set_sync_i2c(&reg[i++], 0x3301, 0x18);
		set_sync_i2c(&reg[i++], 0x3630, 0x30);
		set_sync_i2c(&reg[i++], 0x3633, 0x23);
		set_sync_i2c(&reg[i++], 0x3622, 0xf6);
		set_sync_i2c(&reg[i++], 0x363a, 0x83);
	} else if (gain < 4) {
		set_sync_i2c(&reg[i++], 0x3301, 0x20);
		set_sync_i2c(&reg[i++], 0x3630, 0x23);
		set_sync_i2c(&reg[i++], 0x3633, 0x33);
		set_sync_i2c(&reg[i++], 0x3622, 0xf6);
		set_sync_i2c(&reg[i++], 0x363a, 0x87);
	} else if (gain < 8) {
		set_sync_i2c(&reg[i++], 0x3301, 0x28);
		set_sync_i2c(&reg[i++], 0x3630, 0x24);
		set_sync_i2c(&reg[i++], 0x3633, 0x43);
		set_sync_i2c(&reg[i++], 0x3622, 0xf6);
		set_sync_i2c(&reg[i++], 0x363a, 0x9f);
	} else {
		set_sync_i2c(&reg[i++], 0x3301, 0x38);
		set_sync_i2c(&reg[i++], 0x3630, 0x28);
		set_sync_i2c(&reg[i++], 0x3633, 0x43);
		set_sync_i2c(&reg[i++], 0x3622, 0xf6);
		set_sync_i2c(&reg[i++], 0x363a, 0x9f);
	}
	set_sync_i2c(&reg[i++], 0x3812, 0x30);
	regs->num = i;

	return RTS_ISP_OK;
}

static int sc5235_check(uint32_t isp_id)
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

	if (id == 0x5235)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops sc5235_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "sc5235",
	.get_info = sc5235_get_info,
	.get_init_info = sc5235_get_init_info,
	.start = sc5235_start,
	.get_tuned_again = sc5235_get_tuned_again,
	.get_tuned_dgain = sc5235_get_tuned_dgain,
	.get_exposure_gain_info = sc5235_get_exposure_gain_info,
	.check = sc5235_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(sc5235_ops)
