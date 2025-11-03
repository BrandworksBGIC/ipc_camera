/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2020 martial <martial_wu@realsil.com.cn>
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
	uint16_t vts;
	uint32_t clk;
};

struct sc5239_gain {
	uint16_t ana_gain;
	uint16_t fine_gain;
	float total_gain;
};

struct sc5239_status {
	float exp_step;
	float last_exposure;
	uint16_t cur_fps;
	uint16_t min_vts;
};

static struct sc5239_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_sc5239_fps_info[] = {
	{30, 2760, 2000, 165600000},
};

static struct rts_isp_i2c_reg g_sc5239_30fps_i2c_init_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x3039, 0xc0},
	{0x3029, 0xb4},
	{0x301f, 0x13},
	{0x302a, 0x69},
	{0x302b, 0x01},
	{0x302c, 0x00},
	{0x302d, 0x03},
	{0x3037, 0x26},
	{0x3038, 0x66},
	{0x303a, 0x29},
	{0x303b, 0x0a},
	{0x303c, 0x0e},
	{0x303d, 0x03},
	{0x3200, 0x00},
	{0x3201, 0x00},
	{0x3202, 0x00},
	{0x3203, 0x00},
	{0x3204, 0x0a},
	{0x3205, 0x2f},
	{0x3206, 0x07},
	{0x3207, 0xa7},
	{0x3208, 0x0a},
	{0x3209, 0x28},
	{0x320a, 0x07},
	{0x320b, 0xa0},
	{0x320c, 0x05},
	{0x320d, 0x64},
	{0x320e, 0x07},
	{0x320f, 0xd0},
	{0x3210, 0x00},
	{0x3211, 0x04},
	{0x3212, 0x00},
	{0x3213, 0x04},
	{0x3235, 0x0f},
	{0x3236, 0x9c},
	{0x3301, 0x38},
	{0x3303, 0x20},
	{0x3304, 0x10},
	{0x3306, 0x58},
	{0x3308, 0x10},
	{0x3309, 0x60},
	{0x330a, 0x00},
	{0x330b, 0xb8},
	{0x330d, 0x30},
	{0x330e, 0x20},
	{0x3314, 0x14},
	{0x3315, 0x02},
	{0x331b, 0x83},
	{0x331e, 0x19},
	{0x331f, 0x59},
	{0x3320, 0x01},
	{0x3321, 0x04},
	{0x3326, 0x00},
	{0x3332, 0x22},
	{0x3333, 0x20},
	{0x3334, 0x40},
	{0x3350, 0x22},
	{0x3359, 0x22},
	{0x335c, 0x22},
	{0x3364, 0x05},
	{0x3366, 0xc8},
	{0x3367, 0x08},
	{0x3368, 0x03},
	{0x3369, 0x00},
	{0x336a, 0x00},
	{0x336b, 0x00},
	{0x336c, 0x01},
	{0x336d, 0x40},
	{0x337e, 0x88},
	{0x337f, 0x03},
	{0x338f, 0x40},
	{0x33ae, 0x22},
	{0x33af, 0x22},
	{0x33b0, 0x22},
	{0x33b4, 0x22},
	{0x33b6, 0x07},
	{0x33b7, 0x17},
	{0x33b8, 0x20},
	{0x33b9, 0x20},
	{0x33ba, 0x44},
	{0x3614, 0x00},
	{0x3620, 0x28},
	{0x3621, 0xac},
	{0x3622, 0xf6},
	{0x3623, 0x08},
	{0x3624, 0x47},
	{0x3625, 0x0b},
	{0x3630, 0x30},
	{0x3631, 0x88},
	{0x3632, 0x18},
	{0x3633, 0x34},
	{0x3634, 0x86},
	{0x3635, 0x4d},
	{0x3636, 0x21},
	{0x3637, 0x20},
	{0x3638, 0x18},
	{0x3639, 0x09},
	{0x363a, 0x83},
	{0x363b, 0x02},
	{0x363c, 0x07},
	{0x363d, 0x03},
	{0x3670, 0x00},
	{0x3677, 0x86},
	{0x3678, 0x86},
	{0x3679, 0xa8},
	{0x367e, 0x08},
	{0x367f, 0x18},
	{0x3905, 0x98},
	{0x3907, 0x01},
	{0x3908, 0x11},
	{0x390a, 0x00},
	{0x391c, 0x9f},
	{0x391d, 0x00},
	{0x391e, 0x01},
	{0x391f, 0xc0},
	{0x3e01, 0xf9},
	{0x3e02, 0xa0},
	{0x3e05, 0xe0},
	{0x3e07, 0x80},
	{0x3e08, 0x03},
	{0x3e09, 0x20},
	{0x3e1e, 0x30},
	{0x3e26, 0x20},
	{0x3f00, 0x0d},
	{0x3f02, 0x05},
	{0x3f04, 0x02},
	{0x3f05, 0xaa},
	{0x3f06, 0x21},
	{0x3f08, 0x04},
	{0x4500, 0x5d},
	{0x4502, 0x10},
	{0x4509, 0x10},
	{0x4809, 0x01},
	{0x4837, 0x19},
	{0x5000, 0x20},
	{0x5002, 0x00},
	{0x6000, 0x26},
	{0x6002, 0x06},
	//2594x1946
	{0x3200, 0x00},
	{0x3201, 0x03},
	{0x3202, 0x00},
	{0x3203, 0x03},
	{0x3204, 0x0a},
	{0x3205, 0x2c},
	{0x3206, 0x07},
	{0x3207, 0xa4},
	{0x3208, 0x0a},
	{0x3209, 0x22},
	{0x320a, 0x07},
	{0x320b, 0x9a},
	{0x3210, 0x00},
	{0x3211, 0x04},
	{0x3212, 0x00},
	{0x3213, 0x04},
	{0x3039, 0x23},
	{0x3029, 0x33},
	{0x0100, 0x01},
};

static struct sc5239_gain gain_mapping[] = {
	{0x0300, 0x20, 1.000},
	{0x0300, 0x21, 1.031},
	{0x0300, 0x22, 1.063},
	{0x0300, 0x23, 1.094},
	{0x0300, 0x24, 1.125},
	{0x0300, 0x25, 1.156},
	{0x0300, 0x26, 1.188},
	{0x0300, 0x27, 1.219},
	{0x0300, 0x28, 1.250},
	{0x0300, 0x29, 1.281},
	{0x0300, 0x2a, 1.313},
	{0x0300, 0x2b, 1.344},
	{0x0300, 0x2c, 1.375},
	{0x0300, 0x2d, 1.406},
	{0x0300, 0x2e, 1.438},
	{0x0300, 0x2f, 1.469},
	{0x0300, 0x30, 1.500},
	{0x0300, 0x31, 1.531},
	{0x0300, 0x32, 1.563},
	{0x0300, 0x33, 1.594},
	{0x0300, 0x34, 1.625},
	{0x0300, 0x35, 1.656},
	{0x0300, 0x36, 1.688},
	{0x0300, 0x37, 1.719},
	{0x0300, 0x38, 1.750},
	{0x0300, 0x39, 1.781},
	{0x0300, 0x3a, 1.813},
	{0x0300, 0x3b, 1.844},
	{0x0300, 0x3c, 1.875},
	{0x0300, 0x3d, 1.906},
	{0x0300, 0x3e, 1.938},
	{0x0300, 0x3f, 1.969},

	{0x0700, 0x20, 2.000},
	{0x0700, 0x21, 2.063},
	{0x0700, 0x22, 2.125},
	{0x0700, 0x23, 2.188},
	{0x0700, 0x24, 2.250},
	{0x0700, 0x25, 2.313},
	{0x0700, 0x26, 2.375},
	{0x0700, 0x27, 2.438},
	{0x0700, 0x28, 2.500},
	{0x0700, 0x29, 2.578},
	{0x0700, 0x2a, 2.625},
	{0x0700, 0x2b, 2.688},
	{0x0700, 0x2c, 2.750},
	{0x0700, 0x2d, 2.810},
	{0x0700, 0x2e, 2.875},
	{0x0700, 0x2f, 2.940},
	{0x0700, 0x30, 3.000},
	{0x0700, 0x31, 3.060},
	{0x0700, 0x32, 3.125},
	{0x0700, 0x33, 3.190},
	{0x0700, 0x34, 3.250},
	{0x0700, 0x35, 3.310},
	{0x0700, 0x36, 3.375},
	{0x0700, 0x37, 3.440},
	{0x0700, 0x38, 3.500},
	{0x0700, 0x39, 3.560},
	{0x0700, 0x3a, 3.625},
	{0x0700, 0x3b, 3.690},
	{0x0700, 0x3c, 3.750},
	{0x0700, 0x3d, 3.810},
	{0x0700, 0x3e, 3.875},
	{0x0700, 0x3f, 3.940},

	{0x0f00, 0x20, 4.000},
	{0x0f00, 0x21, 4.130},
	{0x0f00, 0x22, 4.250},
	{0x0f00, 0x23, 4.380},
	{0x0f00, 0x24, 4.500},
	{0x0f00, 0x25, 4.630},
	{0x0f00, 0x26, 4.750},
	{0x0f00, 0x27, 4.880},
	{0x0f00, 0x28, 5.00},
	{0x0f00, 0x29, 5.13},
	{0x0f00, 0x2a, 5.25},
	{0x0f00, 0x2b, 5.38},
	{0x0f00, 0x2c, 5.50},
	{0x0f00, 0x2d, 5.63},
	{0x0f00, 0x2e, 5.75},
	{0x0f00, 0x2f, 5.88},
	{0x0f00, 0x30, 6.00},
	{0x0f00, 0x31, 6.13},
	{0x0f00, 0x32, 6.25},
	{0x0f00, 0x33, 6.38},
	{0x0f00, 0x34, 6.50},
	{0x0f00, 0x35, 6.63},
	{0x0f00, 0x36, 6.75},
	{0x0f00, 0x37, 6.88},
	{0x0f00, 0x38, 7.00},
	{0x0f00, 0x39, 7.13},
	{0x0f00, 0x3a, 7.25},
	{0x0f00, 0x3b, 7.38},
	{0x0f00, 0x3c, 7.50},
	{0x0f00, 0x3d, 7.63},
	{0x0f00, 0x3e, 7.75},
	{0x0f00, 0x3f, 7.88},

	{0x1f00, 0x20, 8.00},
	{0x1f00, 0x21, 8.25},
	{0x1f00, 0x22, 8.50},
	{0x1f00, 0x23, 8.75},
	{0x1f00, 0x24, 9.00},
	{0x1f00, 0x25, 9.25},
	{0x1f00, 0x26, 9.50},
	{0x1f00, 0x27, 9.75},
	{0x1f00, 0x28, 10.0},
	{0x1f00, 0x29, 10.25},
	{0x1f00, 0x2a, 10.50},
	{0x1f00, 0x2b, 10.75},
	{0x1f00, 0x2c, 11.00},
	{0x1f00, 0x2d, 11.25},
	{0x1f00, 0x2e, 11.50},
	{0x1f00, 0x2f, 11.75},
	{0x1f00, 0x30, 12.00},
	{0x1f00, 0x31, 12.25},
	{0x1f00, 0x32, 12.50},
	{0x1f00, 0x33, 12.75},
	{0x1f00, 0x34, 13.00},
	{0x1f00, 0x35, 13.25},
	{0x1f00, 0x36, 13.50},
	{0x1f00, 0x37, 13.75},
	{0x1f00, 0x38, 14.00},
	{0x1f00, 0x39, 14.25},
	{0x1f00, 0x3a, 14.50},
	{0x1f00, 0x3b, 14.75},
	{0x1f00, 0x3c, 15.00},
	{0x1f00, 0x3d, 15.25},
	{0x1f00, 0x3e, 15.50},
	{0x1f00, 0x3f, 15.75},
};

static int sc5239_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 2592;
	info->modes.mode[0].size.h = 1944;
	info->modes.mode[0].fps = g_sc5239_fps_info[0].fps;
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

static const struct fps_info *sc5239_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_sc5239_fps_info); i++)
		if (fps == g_sc5239_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_sc5239_fps_info))
		return NULL;

	return &g_sc5239_fps_info[i];
}

static int sc5239_get_init_info(uint32_t isp_id,
				const struct rts_isp_sensor_mode *mode,
			       struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct sc5239_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("sc5239 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = sc5239_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, clk_div: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->clk_div, fps_info->hts);

	set_init_i2c_regs(info->sensor_regs[0],
		g_sc5239_30fps_i2c_init_regs, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0 | MIPI_LANE1;
	info->interface.mipi.hs_term = 0x3;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 2594;
	info->size.h = 1946;
	info->start.x = 0;
	info->start.y = 0;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = status->min_vts = fps_info->vts;
	info->max_vts = 65535;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */
	status->cur_fps = mode->fps;

	return RTS_ISP_OK;
}

static int sc5239_start(uint32_t isp_id)
{
	struct sc5239_status *status;

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

	if (fgain >= 15.75) {
		reg_value = 0x1f3f;
	} else {
		for (i = 0; i < ((ARRAY_SIZE(gain_mapping)) - 1); i++) {
			if ((gain_mapping[i].total_gain <= fgain) &&
			    (fgain < gain_mapping[i + 1].total_gain)) {
				reg_value = gain_mapping[i].ana_gain |
					    gain_mapping[i].fine_gain;
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

	if (reg_value >= 0x1f3f)
		gain = 15.75;
	else {
		for (i = 0; i < ((ARRAY_SIZE(gain_mapping)) - 1); i++) {
			if (reg_value == (gain_mapping[i].ana_gain |
			    gain_mapping[i].fine_gain)) {
				gain = gain_mapping[i].total_gain;
				break;
			}
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

static int sc5239_get_tuned_again(uint32_t isp_id,
				  float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int sc5239_get_tuned_dgain(uint32_t isp_id,
				  float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int sc5239_get_exposure_gain_info(uint32_t isp_id,
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
	struct sc5239_status *status;
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
		clip_d_word(exp_reg_value_float, 1, (2 * total_line - 8));
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
	set_sync_i2c(&reg[i++], 0x3812, 0x00);
	if (gain >= 15.75f) {
		set_sync_i2c(&reg[i++], 0x3301, 0x44);
		set_sync_i2c(&reg[i++], 0x3630, 0x19);
		set_sync_i2c(&reg[i++], 0x3633, 0x45);
		set_sync_i2c(&reg[i++], 0x3622, 0x16);
		set_sync_i2c(&reg[i++], 0x363a, 0x9f);
	} else if (gain >= 8.0f) {
		set_sync_i2c(&reg[i++], 0x3301, 0x30);
		set_sync_i2c(&reg[i++], 0x3630, 0x16);
		set_sync_i2c(&reg[i++], 0x3633, 0x33);
		set_sync_i2c(&reg[i++], 0x3622, 0xf6);
		set_sync_i2c(&reg[i++], 0x363a, 0x9f);
	} else if (gain >= 4.0f) {
		set_sync_i2c(&reg[i++], 0x3301, 0x28);
		set_sync_i2c(&reg[i++], 0x3630, 0x24);
		set_sync_i2c(&reg[i++], 0x3633, 0x33);
		set_sync_i2c(&reg[i++], 0x3622, 0xf6);
		set_sync_i2c(&reg[i++], 0x363a, 0x9f);
	} else if (gain >= 2.0f) {
		set_sync_i2c(&reg[i++], 0x3301, 0x24);
		set_sync_i2c(&reg[i++], 0x3630, 0x23);
		set_sync_i2c(&reg[i++], 0x3633, 0x33);
		set_sync_i2c(&reg[i++], 0x3622, 0xf6);
		set_sync_i2c(&reg[i++], 0x363a, 0x87);
	} else {
		set_sync_i2c(&reg[i++], 0x3301, 0x1e);
		set_sync_i2c(&reg[i++], 0x3630, 0x30);
		set_sync_i2c(&reg[i++], 0x3633, 0x23);
		set_sync_i2c(&reg[i++], 0x3622, 0xf6);
		set_sync_i2c(&reg[i++], 0x363a, 0x83);
	}
	set_sync_i2c(&reg[i++], 0x3812, 0x30);
	regs->num = i;

	return RTS_ISP_OK;
}

static int sc5239_check(uint32_t isp_id)
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

static const struct rts_isp_sensor_ops sc5239_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "sc5239",
	.get_info = sc5239_get_info,
	.get_init_info = sc5239_get_init_info,
	.start = sc5239_start,
	.get_tuned_again = sc5239_get_tuned_again,
	.get_tuned_dgain = sc5239_get_tuned_dgain,
	.get_exposure_gain_info = sc5239_get_exposure_gain_info,
	.check = sc5239_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(sc5239_ops)
