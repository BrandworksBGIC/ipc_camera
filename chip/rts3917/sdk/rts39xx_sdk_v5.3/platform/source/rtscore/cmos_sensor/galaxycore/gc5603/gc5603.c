/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2024 Eric Yang <eric_yang@realsil.com.cn>
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
	uint32_t hts;
	uint32_t clk;
};

struct gc5603_status {
	int min_vts;
	float exp_step;
	float last_exposure;
	int num;
};

struct gc5603_gain_config {
	uint8_t reg_0614;
	uint8_t reg_0615;
	uint8_t reg_0225;
	uint8_t reg_1467;
	uint8_t reg_1468;
	uint8_t reg_00b8;
	uint8_t reg_00b9;
	uint16_t value;
};

static struct gc5603_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_gc5603_fps_info[] = {
	{30, 3223, 169200000},
};

static struct rts_isp_i2c_reg g_gc5603_i2c_init_regs[] = {
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03fe, 0x10},
	{0x03fe, 0x00},
	{0x0a38, 0x02},
	{0x0a38, 0x03},
	{0x0a20, 0x07},
	{0x061b, 0x03},
	{0x061c, 0x50},
	{0x061d, 0x05},
	{0x061e, 0x7e},
	{0x061f, 0x03},
	{0x0a21, 0x08},
	{0x0a28, 0x01},
	{0x0a34, 0x40},
	{0x0a35, 0x11},
	{0x0a36, 0x6c},
	{0x0a37, 0x03},
	{0x0314, 0x50},
	{0x0315, 0x32},
	{0x031c, 0xce},
	{0x0219, 0x47},
	{0x0342, 0x04},
	{0x0343, 0xb0},
	{0x0340, 0x06},
	{0x0341, 0xd6},
	{0x0345, 0x02},
	{0x0347, 0x02},
	{0x0348, 0x0b},
	{0x0349, 0x98},
	{0x034a, 0x06},
	{0x034b, 0x8a},
	{0x0094, 0x0b},
	{0x0095, 0x90},
	{0x0096, 0x06},
	{0x0097, 0x82},
	{0x0099, 0x04},
	{0x009b, 0x04},
	{0x060c, 0x01},
	{0x060e, 0xd2},
	{0x060f, 0x05},
	{0x070c, 0x01},
	{0x070e, 0xd2},
	{0x070f, 0x05},
	{0x0709, 0x40},
	{0x0719, 0x40},
	{0x0909, 0x07},
	{0x0902, 0x04},
	{0x0904, 0x0b},
	{0x0907, 0x54},
	{0x0908, 0x06},
	{0x0903, 0x9d},
	{0x072a, 0x1c},
	{0x072b, 0x1c},
	{0x0724, 0x2b},
	{0x0727, 0x2b},
	{0x1466, 0x18},
	{0x1467, 0x15},
	{0x1468, 0x15},
	{0x1469, 0x70},
	{0x146a, 0xe8},
	{0x0707, 0x07},
	{0x0737, 0x0f},
	{0x0704, 0x01},
	{0x0705, 0x00},
	{0x0706, 0x01},
	{0x0715, 0x00},
	{0x0716, 0x01},
	{0x0708, 0xc8},
	{0x0718, 0xc8},
	{0x061a, 0x02},
	{0x1430, 0x80},
	{0x1407, 0x10},
	{0x1408, 0x16},
	{0x1409, 0x03},
	{0x1438, 0x01},
	{0x02ce, 0x03},
	{0x0245, 0xc9},
	{0x023a, 0x08},
	{0x02cd, 0x88},
	{0x0612, 0x02},
	{0x0613, 0xc7},
	{0x0243, 0x03},
	{0x0089, 0x03},
	{0x0002, 0xab},
	{0x0040, 0xa3},
	{0x0075, 0x64},
	{0x0004, 0x0f},
	{0x0053, 0x0a},
	{0x0205, 0x0c},
	{0x0a67, 0x80},
	{0x0a54, 0x0e},
	{0x0a65, 0x10},
	{0x0a98, 0x04},
	{0x05be, 0x00},
	{0x05a9, 0x01},
	{0x0023, 0x00},
	{0x0022, 0x00},
	{0x0025, 0x00},
	{0x0024, 0x00},
	{0x0028, 0x0b},
	{0x0029, 0x98},
	{0x002a, 0x06},
	{0x002b, 0x86},
	{0x0a83, 0xe0},
	{0x0a72, 0x02},
	{0x0a73, 0x60},
	{0x0a75, 0x41},
	{0x0a70, 0x03},
	{0x0a5a, 0x80},
	{0x0181, 0xf0},
	{0x0182, 0x05},
	{0x0185, 0x01},
	{0x0180, 0x46},
	{0x0100, 0x08},
	{0x010d, 0x74},
	{0x010e, 0x0e},
	{0x0113, 0x02},
	{0x0114, 0x03},
	{0x0115, 0x10},
	{0x0a70, 0x00},
	{0x0080, 0x02},
	{0x0a67, 0x00},
	{0x0052, 0x02},
	{0x0076, 0x01},
	{0x021a, 0x10},
	{0x0049, 0x0f},
	{0x004a, 0x3c},
	{0x004b, 0x00},
	{0x0430, 0x25},
	{0x0431, 0x25},
	{0x0432, 0x25},
	{0x0433, 0x25},
	{0x0434, 0x59},
	{0x0435, 0x59},
	{0x0436, 0x59},
	{0x0437, 0x59},
	{0x0100, 0x09},
};

static struct gc5603_gain_config g_gc5603_gain_config[] = {
	{0x00, 0x00, 0x04, 0x15, 0x15, 0x01, 0x00, 64},
	{0x90, 0x02, 0x04, 0x15, 0x15, 0x01, 0x0A, 76},
	{0x00, 0x00, 0x00, 0x15, 0x15, 0x01, 0x12, 82},
	{0x90, 0x02, 0x00, 0x15, 0x15, 0x01, 0x20, 96},
	{0x01, 0x00, 0x00, 0x15, 0x15, 0x01, 0x30, 114},
	{0x91, 0x02, 0x00, 0x15, 0x15, 0x02, 0x05, 135},
	{0x02, 0x00, 0x00, 0x15, 0x15, 0x02, 0x19, 153},
	{0x92, 0x02, 0x00, 0x16, 0x16, 0x02, 0x3F, 180},
	{0x03, 0x00, 0x00, 0x16, 0x16, 0x03, 0x20, 220},
	{0x93, 0x02, 0x00, 0x17, 0x17, 0x04, 0x0A, 260},
	{0x00, 0x00, 0x01, 0x18, 0x18, 0x05, 0x02, 311},
	{0x90, 0x02, 0x01, 0x19, 0x19, 0x05, 0x39, 367},
	{0x01, 0x00, 0x01, 0x19, 0x19, 0x06, 0x3C, 435},
	{0x91, 0x02, 0x01, 0x19, 0x19, 0x08, 0x0D, 513},
	{0x02, 0x00, 0x01, 0x1a, 0x1a, 0x09, 0x21, 581},
	{0x92, 0x02, 0x01, 0x1a, 0x1a, 0x0B, 0x0F, 686},
	{0x03, 0x00, 0x01, 0x1c, 0x1c, 0x0D, 0x17, 839},
	{0x93, 0x02, 0x01, 0x1c, 0x1c, 0x0F, 0x33, 990},
	{0x04, 0x00, 0x01, 0x1d, 0x1d, 0x12, 0x30, 1162},
	{0x94, 0x02, 0x01, 0x1d, 0x1d, 0x16, 0x10, 1371},
	{0x05, 0x00, 0x01, 0x1e, 0x1e, 0x1A, 0x19, 1585},
	{0x95, 0x02, 0x01, 0x1e, 0x1e, 0x1F, 0x13, 1870},
	{0x06, 0x00, 0x01, 0x20, 0x20, 0x25, 0x08, 2186},
	{0x96, 0x02, 0x01, 0x20, 0x20, 0x2C, 0x03, 2596},
	{0xb6, 0x04, 0x01, 0x20, 0x20, 0x34, 0x0F, 3010},
	{0x86, 0x06, 0x01, 0x20, 0x20, 0x3D, 0x3D, 3582},
};

static int gc5603_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 2944;
	info->modes.mode[0].size.h = 1664;
	info->modes.mode[0].fps = g_gc5603_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x31;
	info->i2c.addr_len = 2;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 1000);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V2, 1000);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 1000);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 1000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 1000);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_RST_GPIO, 0, 10000);
	set_power_item(&down->items[i++], SNR_HCLK, 0, 10000);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, 0, 5000);
	set_power_item(&down->items[i++], SNR_CORE_POWER, 0, 5000);
	set_power_item(&down->items[i++], SNR_IO_POWER, 0, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static const struct fps_info *gc5603_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_gc5603_fps_info); i++)
		if (fps == g_gc5603_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_gc5603_fps_info))
		return NULL;

	return &g_gc5603_fps_info[i];
}

static int gc5603_get_init_info(uint32_t isp_id,
				const struct rts_isp_sensor_mode *mode,
				struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct gc5603_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("gc5603 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = gc5603_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->hts);

	set_init_i2c_regs(info->sensor_regs[0], g_gc5603_i2c_init_regs, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = (MIPI_LANE0 |
		MIPI_LANE1 | MIPI_LANE2 | MIPI_LANE3);
	info->interface.mipi.hs_term = 0x6;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 2960;
	info->size.h = 1666;
	info->start.x = 0;
	info->start.y = 1;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = 1750;
	info->max_vts = 65536;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */
	status->min_vts = info->min_vts;

	return RTS_ISP_OK;
}

static int gc5603_start(uint32_t isp_id)
{
	struct gc5603_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static uint16_t get_sensor_gain_reg(float fgain,
				struct gc5603_status *status)
{
	int i;
	uint16_t gain = fgain * 64;

	if (gain >= 3582) {
		gain = 3582;
		status->num = 25;
	} else {
		for (i = 0; i < ARRAY_SIZE(g_gc5603_gain_config) - 1; i++) {
			if (gain >= g_gc5603_gain_config[i].value &&
				gain < g_gc5603_gain_config[i + 1].value) {
				gain = g_gc5603_gain_config[i].value;
				status->num = i;
				break;
			}
		}
	}
	return gain;
}

static float get_sensor_real_gain(uint16_t reg_value)
{
	return reg_value / 64.0f;
}

static uint32_t clip_d_word(uint32_t current,
					uint32_t minimum, uint32_t maximum)
{
	if (current > maximum)
		return maximum;
	if (current < minimum)
		return minimum;
	return current;
}

static int gc5603_get_tuned_again(uint32_t isp_id,
				  float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;
	struct gc5603_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	gain_reg = get_sensor_gain_reg(again[0], status);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int gc5603_get_tuned_dgain(uint32_t isp_id,
				  float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int gc5603_get_exposure_gain_info(uint32_t isp_id,
			 const struct rts_isp_sensor_exp_gain *exp_gain,
			 struct rts_isp_sync_regs *regs)
{
	int i;
	uint32_t vts;
	struct gc5603_status *status;
	struct rts_isp_sync_reg *reg;
	float exposure_rows;
	uint16_t exp_reg_value;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	vts = exp_gain->vts;
	reg = regs->reg;

	exposure_rows = exp_gain->exposure[0] / status->exp_step + 0.5f;
	exp_reg_value = clip_d_word(exposure_rows, 1, vts - 8);

	i = 0;
	if (abs(status->last_exposure - exp_gain->exposure[0]) > 0.001f) {
		set_sync_i2c(&reg[i++], 0x0202, exp_reg_value >> 8);
		set_sync_i2c(&reg[i++], 0x0203, exp_reg_value & 0xff);
		status->last_exposure = exp_gain->exposure[0];
	}

	set_sync_i2c(&reg[i++], 0x031d, 0x2d);
	set_sync_i2c(&reg[i++], 0x0614,
				g_gc5603_gain_config[status->num].reg_0614);
	set_sync_i2c(&reg[i++], 0x0615,
				g_gc5603_gain_config[status->num].reg_0615);
	set_sync_i2c(&reg[i++], 0x0225,
				g_gc5603_gain_config[status->num].reg_0225);
	set_sync_i2c(&reg[i++], 0x031d, 0x28);

	set_sync_i2c(&reg[i++], 0x1467,
				g_gc5603_gain_config[status->num].reg_1467);
	set_sync_i2c(&reg[i++], 0x1468,
				g_gc5603_gain_config[status->num].reg_1468);
	set_sync_i2c(&reg[i++], 0x00b8,
				g_gc5603_gain_config[status->num].reg_00b8);
	set_sync_i2c(&reg[i++], 0x00b9,
				g_gc5603_gain_config[status->num].reg_00b9);

	set_sync_i2c(&reg[i++], 0x0340, vts >> 8);
	set_sync_i2c(&reg[i++], 0x0341, vts & 0xff);
	regs->num = i;

	return RTS_ISP_OK;
}

static int gc5603_check(uint32_t isp_id)
{
	int ret;
	int id;
	struct rts_isp_i2c_reg reg = {};

	reg.addr = 0x03f0;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id = reg.data << 8;

	reg.addr = 0x03f1;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id |= reg.data;

	if (id == 0x5603)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops gc5603_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "gc5603",
	.get_info = gc5603_get_info,
	.get_init_info = gc5603_get_init_info,
	.start = gc5603_start,
	.get_tuned_again = gc5603_get_tuned_again,
	.get_tuned_dgain = gc5603_get_tuned_dgain,
	.get_exposure_gain_info = gc5603_get_exposure_gain_info,
	.check = gc5603_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(gc5603_ops)
