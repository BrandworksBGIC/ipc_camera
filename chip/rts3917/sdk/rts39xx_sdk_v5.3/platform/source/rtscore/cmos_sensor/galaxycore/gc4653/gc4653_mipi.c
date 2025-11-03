/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2020 Eric Yang <eric_yang@realsil.com.cn>
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

struct gc4653_status {
	int min_vts;
	float exp_step;
	float last_exposure;
	int num;
};

struct gc4653_gain_config {
	uint8_t reg_02b4;
	uint8_t reg_02b3;
	uint8_t reg_02b8;
	uint8_t reg_02b9;
	uint8_t reg_0515;
	uint8_t reg_0519;
	uint8_t reg_02d9;
	uint16_t value;
};

static struct gc4653_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_gc4653_fps_info[] = {
	{30, 4800, 216000000},
};

static struct rts_isp_i2c_reg g_gc4653_i2c_init_regs[] = {
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x0317, 0x00},
	{0x0320, 0x77},
	{0x0324, 0xc8},
	{0x0325, 0x06},
	{0x0326, 0x60},
	{0x0327, 0x03},
	{0x0334, 0x40},
	{0x0336, 0x60},
	{0x0337, 0x82},
	{0x0315, 0x25},
	{0x031c, 0xc6},
	{0x0287, 0x18},
	{0x0084, 0x00},
	{0x0087, 0x50},
	{0x029d, 0x08},
	{0x0290, 0x00},
	{0x0340, 0x05},
	{0x0341, 0xdc},
	{0x0345, 0x06},
	{0x034b, 0xb0},
	{0x0352, 0x08},
	{0x0354, 0x08},
	{0x02d1, 0xe0},
	{0x0223, 0xf2},
	{0x0238, 0xa4},
	{0x02ce, 0x7f},
	{0x0232, 0xc4},
	{0x02d3, 0x05},
	{0x0243, 0x06},
	{0x02ee, 0x30},
	{0x026f, 0x70},
	{0x0257, 0x09},
	{0x0211, 0x02},
	{0x0219, 0x09},
	{0x023f, 0x2d},
	{0x0518, 0x00},
	{0x0519, 0x01},
	{0x0515, 0x08},
	{0x02d9, 0x3f},
	{0x02da, 0x02},
	{0x02db, 0xe8},
	{0x02e6, 0x20},
	{0x021b, 0x10},
	{0x0252, 0x22},
	{0x024e, 0x22},
	{0x02c4, 0x01},
	{0x021d, 0x17},
	{0x024a, 0x01},
	{0x02ca, 0x02},
	{0x0262, 0x10},
	{0x029a, 0x20},
	{0x021c, 0x0e},
	{0x0298, 0x03},
	{0x029c, 0x00},
	{0x027e, 0x14},
	{0x02c2, 0x10},
	{0x0540, 0x20},
	{0x0546, 0x01},
	{0x0548, 0x01},
	{0x0544, 0x01},
	{0x0242, 0x1b},
	{0x02c0, 0x1b},
	{0x02c3, 0x20},
	{0x02e4, 0x10},
	{0x022e, 0x00},
	{0x027b, 0x3f},
	{0x0269, 0x0f},
	{0x02d2, 0x40},
	{0x027c, 0x08},
	{0x023a, 0x2e},
	{0x0245, 0xce},
	{0x0530, 0x20},
	{0x0531, 0x02},
	{0x0228, 0x50},
	{0x02ab, 0x00},
	{0x0250, 0x00},
	{0x0221, 0x50},
	{0x02ac, 0x00},
	{0x02a5, 0x02},
	{0x0260, 0x0b},
	{0x0216, 0x04},
	{0x0299, 0x1C},
	{0x02bb, 0x0d},
	{0x02a3, 0x02},
	{0x02a4, 0x02},
	{0x021e, 0x02},
	{0x024f, 0x08},
	{0x028c, 0x08},
	{0x0532, 0x3f},
	{0x0533, 0x02},
	{0x0277, 0xc0},
	{0x0276, 0xc0},
	{0x0239, 0xc0},
	{0x0202, 0x05},
	{0x0203, 0xd0},
	{0x0205, 0xc0},
	{0x02b0, 0x68},
	{0x0002, 0xa9},
	{0x0004, 0x01},
	{0x021a, 0x98},
	{0x0266, 0xa0},
	{0x0020, 0x01},
	{0x0021, 0x03},
	{0x0022, 0x00},
	{0x0023, 0x04},
	{0x034c, 0x0a},
	{0x034d, 0x08},
	{0x034e, 0x05},
	{0x034f, 0xa8},
	{0x0342, 0x06},
	{0x0343, 0x40},
	{0x03fe, 0x10},
	{0x03fe, 0x00},
	{0x0106, 0x78},
	{0x0108, 0x0c},
	{0x0114, 0x01},
	{0x0115, 0x12},
	{0x0180, 0x46},
	{0x0181, 0x30},
	{0x0182, 0x05},
	{0x0185, 0x01},
	{0x03fe, 0x10},
	{0x03fe, 0x00},
	{0x0100, 0x09},
	{0x000f, 0x00},
	{0x0080, 0x02},
	{0x0097, 0x0a},
	{0x0098, 0x10},
	{0x0099, 0x05},
	{0x009a, 0xb0},
	{0x0317, 0x08},
	{0x0a67, 0x80},
	{0x0a70, 0x03},
	{0x0a82, 0x00},
	{0x0a83, 0x10},
	{0x0a80, 0x2b},
	{0x05be, 0x00},
	{0x05a9, 0x01},
	{0x0313, 0x80},
	{0x05be, 0x01},
	{0x0317, 0x00},
	{0x0a67, 0x00},
};

static struct gc4653_gain_config g_gc4653_gain_config[] = {
	{0x00, 0x00, 0x01, 0x00, 0x30, 0x1e, 0x5C, 64},
	{0x00, 0x20, 0x01, 0x0B, 0x30, 0x1e, 0x5C, 75},
	{0x00, 0x01, 0x01, 0x19, 0x30, 0x1d, 0x5B, 89},
	{0x00, 0x21, 0x01, 0x2A, 0x30, 0x1e, 0x5C, 106},
	{0x00, 0x02, 0x02, 0x00, 0x30, 0x1e, 0x5C, 128},
	{0x00, 0x22, 0x02, 0x17, 0x30, 0x1d, 0x5B, 151},
	{0x00, 0x03, 0x02, 0x33, 0x20, 0x16, 0x54, 179},
	{0x00, 0x23, 0x03, 0x14, 0x20, 0x17, 0x55, 212},
	{0x00, 0x04, 0x04, 0x00, 0x20, 0x17, 0x55, 256},
	{0x00, 0x24, 0x04, 0x2F, 0x20, 0x19, 0x57, 303},
	{0x00, 0x05, 0x05, 0x26, 0x20, 0x19, 0x57, 358},
	{0x00, 0x25, 0x06, 0x28, 0x20, 0x1b, 0x59, 424},
	{0x00, 0x0c, 0x08, 0x00, 0x20, 0x1d, 0x5B, 512},
	{0x00, 0x2C, 0x09, 0x1E, 0x20, 0x1f, 0x5D, 606},
	{0x00, 0x0D, 0x0B, 0x0C, 0x20, 0x21, 0x5F, 716},
	{0x00, 0x2D, 0x0D, 0x11, 0x20, 0x24, 0x62, 849},
	{0x00, 0x1C, 0x10, 0x00, 0x20, 0x26, 0x64, 1024},
};

static int gc4653_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 2560;
	info->modes.mode[0].size.h = 1440;
	info->modes.mode[0].fps = g_gc4653_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x29;
	info->i2c.addr_len = 2;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 1000);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V2, 1000);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 1000);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_27M, 1000);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_HIGH, 1000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 1000);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_RST_GPIO, 0, 10000);
	set_power_item(&down->items[i++], SNR_PWDN_GPIO, 0, 100000);
	set_power_item(&down->items[i++], SNR_HCLK, 0, 10000);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, 0, 5000);
	set_power_item(&down->items[i++], SNR_CORE_POWER, 0, 5000);
	set_power_item(&down->items[i++], SNR_IO_POWER, 0, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static const struct fps_info *gc4653_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_gc4653_fps_info); i++)
		if (fps == g_gc4653_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_gc4653_fps_info))
		return NULL;

	return &g_gc4653_fps_info[i];
}

static int gc4653_get_init_info(uint32_t isp_id,
				const struct rts_isp_sensor_mode *mode,
				struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct gc4653_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("gc4653 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = gc4653_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->hts);

	set_init_i2c_regs(info->sensor_regs[0], g_gc4653_i2c_init_regs, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = (MIPI_LANE0 | MIPI_LANE1);
	info->interface.mipi.hs_term = 0x6;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 2568;
	info->size.h = 1448;
	info->start.x = 0;
	info->start.y = 0;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = 1500;
	info->max_vts = 65536;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */
	status->min_vts = info->min_vts;

	return RTS_ISP_OK;
}

static int gc4653_start(uint32_t isp_id)
{
	struct gc4653_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static uint16_t get_sensor_gain_reg(float fgain, struct gc4653_status *status)
{
	int i;
	uint16_t gain = fgain * 64;

	if (gain >= 1024) {
		gain = 1024;
		status->num = 16;
	} else {
		for (i = 0; i < ARRAY_SIZE(g_gc4653_gain_config) - 1; i++) {
			if (gain >= g_gc4653_gain_config[i].value &&
			    gain < g_gc4653_gain_config[i + 1].value) {
				gain = g_gc4653_gain_config[i].value;
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

static int gc4653_get_tuned_again(uint32_t isp_id,
				  float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;
	struct gc4653_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	gain_reg = get_sensor_gain_reg(again[0], status);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int gc4653_get_tuned_dgain(uint32_t isp_id,
				  float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int gc4653_get_exposure_gain_info(uint32_t isp_id,
					 const struct rts_isp_sensor_exp_gain *exp_gain,
					 struct rts_isp_sync_regs *regs)
{
	int i;
	uint32_t vts;
	struct gc4653_status *status;
	struct rts_isp_sync_reg *reg;
	uint16_t exposure_rows;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	vts = exp_gain->vts;
	reg = regs->reg;

	i = 0;
	if (abs(status->last_exposure - exp_gain->exposure[0]) > 0.001f) {
		exposure_rows = exp_gain->exposure[0] / status->exp_step + 0.5f;
		set_sync_i2c(&reg[i++], 0x0202, exposure_rows >> 8);
		set_sync_i2c(&reg[i++], 0x0203, exposure_rows & 0xff);
		status->last_exposure = exp_gain->exposure[0];
	}
	set_sync_i2c(&reg[i++], 0x02b3,
				g_gc4653_gain_config[status->num].reg_02b3);
	set_sync_i2c(&reg[i++], 0x02b4,
				g_gc4653_gain_config[status->num].reg_02b4);
	set_sync_i2c(&reg[i++], 0x02b8,
				g_gc4653_gain_config[status->num].reg_02b8);
	set_sync_i2c(&reg[i++], 0x02b9,
				g_gc4653_gain_config[status->num].reg_02b9);
	set_sync_i2c(&reg[i++], 0x0515,
				g_gc4653_gain_config[status->num].reg_0515);
	set_sync_i2c(&reg[i++], 0x0519,
				g_gc4653_gain_config[status->num].reg_0519);
	set_sync_i2c(&reg[i++], 0x02d9,
				g_gc4653_gain_config[status->num].reg_02d9);
	set_sync_i2c(&reg[i++], 0x0340, vts >> 8);
	set_sync_i2c(&reg[i++], 0x0341, vts & 0xff);
	regs->num = i;

	return RTS_ISP_OK;
}

static int gc4653_check(uint32_t isp_id)
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

	if (id == 0x4653)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops gc4653_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "gc4653",
	.get_info = gc4653_get_info,
	.get_init_info = gc4653_get_init_info,
	.start = gc4653_start,
	.get_tuned_again = gc4653_get_tuned_again,
	.get_tuned_dgain = gc4653_get_tuned_dgain,
	.get_exposure_gain_info = gc4653_get_exposure_gain_info,
	.check = gc4653_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(gc4653_ops)
