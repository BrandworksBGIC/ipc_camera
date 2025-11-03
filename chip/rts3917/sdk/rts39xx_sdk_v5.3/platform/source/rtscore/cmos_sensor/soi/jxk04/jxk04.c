/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2020 Hengzhi Liu  <george_liu@realsil.com.cn>
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

struct jxk04_status {
	float exp_step;
	float last_exposure;
	uint16_t min_vts;
	struct rts_isp_i2c_reg regs1[2];
};

static struct jxk04_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_jxk04_fps_info[] = {
	{30, 3000, 144000000},
};

static struct rts_isp_i2c_reg g_jxk04_30fps_i2c_init_regs[] = {
	{0x12, 0x40},
	{0x48, 0x8B},
	{0x48, 0x0B},
	{0x0E, 0x11},
	{0x0F, 0x04},
	{0x10, 0x1E},
	{0x11, 0x80},
	{0x0D, 0xA0},
	{0x57, 0xC0},
	{0x58, 0x30},
	{0x5F, 0x01},
	{0x60, 0x19},
	{0x61, 0x10},
	{0x20, 0x77},
	{0x21, 0x01},
	{0x22, 0x40},
	{0x23, 0x06},
	{0x24, 0x51},
	{0x25, 0xF2},
	{0x26, 0x51},
	{0x27, 0x40},
	{0x28, 0x15},
	{0x29, 0x01},
	{0x2B, 0x11},
	{0x2C, 0x01},
	{0x2D, 0x00},
	{0x2E, 0x82},
	{0x2F, 0x44},
	{0x41, 0xA2},
	{0x42, 0x02},
	{0x46, 0x18},
	{0x47, 0x42},
	{0x80, 0x03},
	{0xAF, 0x12},
	{0xBD, 0x88},
	{0xBE, 0x0A},
	{0x9B, 0x83},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x40},
	{0x70, 0x69},
	{0x71, 0x4C},
	{0x72, 0x29},
	{0x73, 0x25},
	{0x75, 0x2B},
	{0x74, 0x12},
	{0x89, 0x0F},
	{0x0C, 0x20},
	{0x6B, 0x00},
	{0x86, 0x00},
	{0x9E, 0x80},
	{0x78, 0x14},
	{0x2A, 0x3A},
	{0x30, 0x8C},
	{0x31, 0x0C},
	{0x32, 0x16},
	{0x33, 0x15},
	{0x34, 0x2F},
	{0x35, 0x2F},
	{0x3A, 0xA0},
	{0x3B, 0x00},
	{0x3C, 0x54},
	{0x3D, 0x56},
	{0x3E, 0xC8},
	{0x56, 0x12},
	{0x59, 0x38},
	{0x85, 0x1F},
	{0x8A, 0x04},
	{0x91, 0x22},
	{0x9C, 0xE1},
	{0x5B, 0xE0},
	{0x5C, 0x4E},
	{0x5D, 0xE4},
	{0x5E, 0x22},
	{0x64, 0xE0},
	{0x65, 0x07},
	{0x66, 0x04},
	{0x67, 0x21},
	{0x68, 0x00},
	{0x69, 0xF4},
	{0x6A, 0x42},
	{0x7A, 0x90},
	{0x82, 0x21},
	{0x8F, 0x90},
	{0x9D, 0x70},
	{0xBF, 0x01},
	{0x57, 0x0F},
	{0xBF, 0x00},
	{0x97, 0xFA},
	{0x13, 0x81},
	{0x96, 0x04},
	{0x4A, 0x05},
	{0x7E, 0xCD},
	{0xA7, 0x84},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x7B, 0x4A},
	{0x7C, 0x12},
	{0x7F, 0x57},
	{0x62, 0x21},
	{0x90, 0x00},
	{0x8C, 0xFF},
	{0x8D, 0xC7},
	{0x8E, 0x00},
	{0x8B, 0x01},
	{0xBF, 0x01},
	{0x4E, 0x00},
	{0xBF, 0x00},
	{0xA3, 0x20},
	{0xA0, 0x01},
	{0x81, 0x74},
	{0xA2, 0xB1},
	{0x19, 0x20},
	{0x12, 0x00},
};

static int jxk04_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 2688;
	info->modes.mode[0].size.h = 1520;
	info->modes.mode[0].fps = g_jxk04_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x40;
	info->i2c.addr_len = 1;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_HIGH, 0);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 1000);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 1000);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V5, 1000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 1000);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 1000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 20000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 5000);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 5000);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_PWDN_GPIO, GPIO_HIGH, 5000);
	set_power_item(&down->items[i++], SNR_RST_GPIO, 0, 1000);
	set_power_item(&down->items[i++], SNR_HCLK, 0, 1000);
	set_power_item(&down->items[i++], SNR_IO_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_CORE_POWER, 0, 1);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, 0, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static const struct fps_info *jxk04_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_jxk04_fps_info); i++)
		if (fps == g_jxk04_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_jxk04_fps_info))
		return NULL;

	return &g_jxk04_fps_info[i];

}

static int jxk04_get_init_info(uint32_t isp_id,
			       const struct rts_isp_sensor_mode *mode,
			       struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct jxk04_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("jxk04 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = jxk04_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->hts);

	set_init_i2c(&status->regs1[0], 0x20, (fps_info->hts >> 3)  & 0xff);
	set_init_i2c(&status->regs1[1], 0x21, (fps_info->hts >> 3)  >> 8);

	set_init_i2c_regs(info->sensor_regs[0], g_jxk04_30fps_i2c_init_regs, 0);
	set_init_i2c_regs(info->sensor_regs[1], status->regs1, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0
			| MIPI_LANE1 | MIPI_LANE2 | MIPI_LANE3;
	info->interface.mipi.hs_term = 0x3;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 2696;
	info->size.h = 1522;
	info->start.x = 0;
	info->start.y = 1;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = 1600;
	info->max_vts = 65536;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */
	status->min_vts = info->min_vts;

	return RTS_ISP_OK;
}

static int jxk04_start(uint32_t isp_id)
{
	struct jxk04_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static uint8_t get_sensor_gain_reg(float fgain)
{
	int i;
	uint8_t reg_value = 0;
	uint16_t gain = fgain * 16;

	if (gain > 248)
		gain = 248;
	for (i = 0; i < 5; i++) {
		if (gain >= 32) {
			gain >>= 1;
			reg_value += (1 << 4);
		} else {
			reg_value += (gain - 16);
			break;
		}
	}

	return reg_value;
}

static float get_sensor_real_gain(uint16_t reg_value)
{
	int i;
	uint16_t gain;

	gain = (reg_value & 0x0f) + 16;
	reg_value >>= 4;

	for (i = 0; i < 3; i++) {
		if (reg_value & 1)
			gain <<= (1 << i);
		reg_value >>= 1;
	}

	return gain / 16.0f;
}

static int jxk04_get_tuned_again(uint32_t isp_id,
				 float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int jxk04_get_tuned_dgain(uint32_t isp_id,
				 float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int jxk04_get_exposure_gain_info(uint32_t isp_id,
					const struct rts_isp_sensor_exp_gain *exp_gain,
					struct rts_isp_sync_regs *regs)
{

	int i;
	uint16_t total_line;
	uint8_t gain_reg;
	struct jxk04_status *status;
	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	gain_reg = get_sensor_gain_reg(exp_gain->analog_gain[0] *
				       exp_gain->digital_gain[0]);
	total_line = exp_gain->vts;
	reg = regs->reg;
	i = 0;
	set_sync_i2c(&reg[i++], 0xC0, 0x00);
	set_sync_i2c(&reg[i++], 0xC1, gain_reg);
	if (abs(status->last_exposure - exp_gain->exposure[0]) > 0.001f) {
		uint16_t exposure_rows;

		exposure_rows = exp_gain->exposure[0] / status->exp_step + 0.5f;
		set_sync_i2c(&reg[i++], 0xC2, 0x01);
		set_sync_i2c(&reg[i++], 0xC3, exposure_rows & 0xff);
		set_sync_i2c(&reg[i++], 0xC4, 0x02);
		set_sync_i2c(&reg[i++], 0xC5, exposure_rows >> 8);
		status->last_exposure = exp_gain->exposure[0];
	}
	set_sync_i2c(&reg[i++], 0xC6, 0x22);
	set_sync_i2c(&reg[i++], 0xC7, total_line & 0xff);
	set_sync_i2c(&reg[i++], 0xC8, 0x23);
	set_sync_i2c(&reg[i++], 0xC9, total_line >> 8);
	set_sync_i2c_mask(&reg[i++], 0x1F, 0x80, 0x80);
	regs->num = i;

	return RTS_ISP_OK;
}

static int jxk04_check(uint32_t isp_id)
{
	int ret;
	int id;
	struct rts_isp_i2c_reg reg = {};

	reg.addr = 0x0a;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id = reg.data << 8;

	reg.addr = 0x0b;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id |= reg.data;

	if (id == 0x0404)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops jxk04_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "jxk04",
	.get_info = jxk04_get_info,
	.get_init_info = jxk04_get_init_info,
	.start = jxk04_start,
	.get_tuned_again = jxk04_get_tuned_again,
	.get_tuned_dgain = jxk04_get_tuned_dgain,
	.get_exposure_gain_info = jxk04_get_exposure_gain_info,
	.check = jxk04_check,
};


RTS_ISP_DEFINE_SENSOR_PLUGIN(jxk04_ops)
