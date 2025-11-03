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

struct jxk06_status {
	float exp_step;
	float last_exposure;
	uint16_t min_vts;
	struct rts_isp_i2c_reg regs1[2];
};

static struct jxk06_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_jxk06_fps_info[] = {
	{30, 3840, 172800000},
};

static struct rts_isp_i2c_reg g_jxk06_30fps_i2c_init_regs[] = {
	{0x12, 0x70},
	{0x48, 0x86},
	{0x48, 0x06},
	{0x0E, 0x11},
	{0x0F, 0x04},
	{0x10, 0x48},
	{0x11, 0x80},
	{0x46, 0x08},
	{0x7F, 0x5E},
	{0x0D, 0xA0},
	{0x57, 0x67},
	{0x58, 0x1F},
	{0x5F, 0x41},
	{0x60, 0x20},
	{0x20, 0xC0},
	{0x21, 0x03},
	{0x22, 0xDC},
	{0x23, 0x05},
	{0x24, 0x80},
	{0x25, 0xA0},
	{0x26, 0x52},
	{0x27, 0xBC},
	{0x28, 0x15},
	{0x29, 0x03},
	{0x2A, 0xB6},
	{0x2B, 0x13},
	{0x2C, 0x00},
	{0x2D, 0x00},
	{0x2E, 0x6E},
	{0x2F, 0x04},
	{0x41, 0x06},
	{0x42, 0x05},
	{0x47, 0x46},
	{0x76, 0x80},
	{0x77, 0x0C},
	{0x80, 0x01},
	{0xAF, 0x12},
	{0xAA, 0x84},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x40},
	{0x9E, 0xF8},
	{0x0C, 0x00},
	{0x6E, 0x2C},
	{0x70, 0xF9},
	{0x71, 0xDD},
	{0x72, 0xD5},
	{0x73, 0x5A},
	{0x74, 0x02},
	{0x78, 0x1C},
	{0x89, 0x01},
	{0x6B, 0x20},
	{0x86, 0x40},
	{0x6F, 0x00},
	{0x30, 0x8D},
	{0x31, 0x08},
	{0x32, 0x20},
	{0x33, 0x5C},
	{0x34, 0x30},
	{0x35, 0x30},
	{0x3A, 0xB9},
	{0x56, 0x92},
	{0x59, 0x40},
	{0x5A, 0x01},
	{0x61, 0x00},
	{0x64, 0xC0},
	{0x85, 0x44},
	{0x8A, 0x00},
	{0x91, 0x58},
	{0x94, 0xE0},
	{0x9B, 0x8F},
	{0xA6, 0x02},
	{0xA7, 0xA0},
	{0xA9, 0x48},
	{0x45, 0x09},
	{0x5B, 0xA5},
	{0x5C, 0x8C},
	{0x5D, 0x97},
	{0x5E, 0x48},
	{0x65, 0x32},
	{0x66, 0x80},
	{0x67, 0x44},
	{0x68, 0x00},
	{0x69, 0x74},
	{0x6A, 0x2B},
	{0x7A, 0x82},
	{0x8D, 0x6F},
	{0x8F, 0x90},
	{0xA4, 0xC7},
	{0xA5, 0xAF},
	{0xB7, 0x61},
	{0x97, 0x20},
	{0x13, 0x81},
	{0x96, 0x84},
	{0x4A, 0x01},
	{0x7E, 0x4C},
	{0x50, 0x02},
	{0x93, 0x00},
	{0xB5, 0x4C},
	{0xB1, 0x00},
	{0xA1, 0x0F},
	{0xA3, 0x40},
	{0x49, 0x10},
	{0x8C, 0xFF},
	{0x8E, 0x00},
	{0x8B, 0x01},
	{0xBC, 0x11},
	{0x82, 0x00},
	{0x9F, 0x50},
	{0x19, 0x20},
	{0x1B, 0x4F},
	{0x12, 0x30},
	{0x48, 0x86},
	{0x48, 0x06},
	{0x00, 0x10},
};

static int jxk06_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 2560;
	info->modes.mode[0].size.h = 1440;
	info->modes.mode[0].fps = g_jxk06_fps_info[0].fps;
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

static const struct fps_info *jxk06_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_jxk06_fps_info); i++)
		if (fps == g_jxk06_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_jxk06_fps_info))
		return NULL;

	return &g_jxk06_fps_info[i];
}

static int jxk06_get_init_info(uint32_t isp_id,
			       const struct rts_isp_sensor_mode *mode,
			       struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct jxk06_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("jxk06 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = jxk06_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->hts);

	set_init_i2c(&status->regs1[0], 0x20, (fps_info->hts >> 2)  & 0xff);
	set_init_i2c(&status->regs1[1], 0x21, (fps_info->hts >> 2)  >> 8);

	set_init_i2c_regs(info->sensor_regs[0], g_jxk06_30fps_i2c_init_regs, 0);
	set_init_i2c_regs(info->sensor_regs[1], status->regs1, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0 | MIPI_LANE1;
	info->interface.mipi.hs_term = 0x5;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;
	info->interface.type_config.raw.bayer = SNR_GBRG;

	info->size.w = 2560;
	info->size.h = 1440;
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

static int jxk06_start(uint32_t isp_id)
{
	struct jxk06_status *status;

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

static int jxk06_get_tuned_again(uint32_t isp_id,
				 float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int jxk06_get_tuned_dgain(uint32_t isp_id,
				 float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int jxk06_get_exposure_gain_info(uint32_t isp_id,
			const struct rts_isp_sensor_exp_gain *exp_gain,
			struct rts_isp_sync_regs *regs)
{

	int i;
	uint16_t total_line;
	uint8_t gain_reg;
	struct jxk06_status *status;
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

static int jxk06_get_mirror_flip(uint32_t isp_id,
		   const struct rts_isp_mirror_flip *mf_info,
		   struct rts_isp_sync_regs *regs)
{
	int i = 0;
	uint32_t val1 = 0x30;
	uint32_t val3 = 0x04;
	uint32_t val4 = 0xBC;

	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !mf_info || !regs)
	return -RTS_ISP_EINVAL;

	rts_isp_drop_frames(isp_id, 1);

	reg = regs->reg;

	if (mf_info->mirror) {
		val1 &= 0x10;
		val3 = ~0x04;
		val4 = val4 + 1;
	}
	if (mf_info->flip) {
		val1 &= 0x20;
		val3 = 0x04;
		val4 = 0xBC;
	}
	if ((mf_info->mirror) && (mf_info->flip)) {
		val1 = 0x00;
		val3 =  ~0x04;
		val4 = val4 + 1;
	}

	set_sync_i2c_mask(&reg[i++], 0x12, val1, 0x30);
	set_sync_i2c_mask(&reg[i++], 0xAA, val3, 0x0F);
	set_sync_i2c_mask(&reg[i++], 0x27, val4, 0xFF);

	regs->num = i;

	return RTS_ISP_OK;
}

static int jxk06_check(uint32_t isp_id)
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

	if (id == 0x0852)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops jxk06_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "jxk06",
	.get_info = jxk06_get_info,
	.get_init_info = jxk06_get_init_info,
	.start = jxk06_start,
	.get_tuned_again = jxk06_get_tuned_again,
	.get_tuned_dgain = jxk06_get_tuned_dgain,
	.get_exposure_gain_info = jxk06_get_exposure_gain_info,
	.get_mirror_flip = jxk06_get_mirror_flip,
	.check = jxk06_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(jxk06_ops)
