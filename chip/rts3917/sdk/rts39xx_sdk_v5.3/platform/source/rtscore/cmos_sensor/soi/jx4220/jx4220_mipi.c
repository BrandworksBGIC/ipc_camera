/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2025 Benno Ma  <benno_ma@realsil.com.cn>
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

struct jx4220_status {
	float exp_step;
	float last_exposure;
	uint16_t min_vts;
};

static struct jx4220_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_jx4220_fps_info[] = {
	{30, 3840, 172800000},
};

static struct rts_isp_i2c_reg g_jx4220_30fps_i2c_init_regs[] = {
	{0x12, 0x60},
	{0xAD, 0x01},
	{0xAD, 0x00},
	{0x0E, 0x11},
	{0x0F, 0x44},
	{0x10, 0x48},
	{0x62, 0x00},
	{0x0C, 0x80},
	{0x0D, 0x11},
	{0x64, 0x14},
	{0x65, 0x20},
	{0xBE, 0x0C},
	{0xBF, 0x30},
	{0xCB, 0x08},
	{0x61, 0x1E},
	{0x1B, 0x81},
	{0xEC, 0x20},
	{0x20, 0xC0},
	{0x21, 0x03},
	{0x22, 0xDC},
	{0x23, 0x05},
	{0x24, 0x80},
	{0x25, 0xA0},
	{0x26, 0x52},
	{0x27, 0x08},
	{0x28, 0x10},
	{0x29, 0x00},
	{0x2B, 0x10},
	{0x2C, 0x08},
	{0x2D, 0x0F},
	{0x2E, 0x7A},
	{0x2F, 0x14},
	{0x30, 0x42},
	{0x87, 0xC6},
	{0x9D, 0xB1},
	{0xAC, 0x00},
	{0x06, 0x33},
	{0x1D, 0x00},
	{0x1E, 0x10},
	{0x3A, 0xD9},
	{0x3B, 0xBA},
	{0x3C, 0x8E},
	{0x3D, 0x69},
	{0x3F, 0x13},
	{0x3E, 0x12},
	{0x42, 0x12},
	{0x43, 0x00},
	{0x71, 0x20},
	{0x76, 0x04},
	{0x70, 0x10},
	{0x44, 0x24},
	{0x60, 0x6E},
	{0x7E, 0x0C},
	{0x9F, 0x4D},
	{0x5F, 0x68},
	{0xAB, 0x60},
	{0x31, 0x0A},
	{0x32, 0x10},
	{0x33, 0xD8},
	{0x35, 0xA0},
	{0x36, 0xCF},
	{0x37, 0x14},
	{0x9E, 0x0E},
	{0xA3, 0x80},
	{0xA6, 0x01},
	{0xB0, 0x12},
	{0xB1, 0x20},
	{0xB2, 0xA4},
	{0xB5, 0x16},
	{0xB6, 0x33},
	{0xB7, 0xFF},
	{0xB8, 0x14},
	{0xB9, 0x0B},
	{0xBA, 0xA6},
	{0xBB, 0x8B},
	{0xBC, 0xC6},
	{0xBD, 0x9F},
	{0xC3, 0xE0},
	{0xC5, 0x8F},
	{0xC6, 0xCF},
	{0xCC, 0x18},
	{0xE9, 0x15},
	{0x58, 0x60},
	{0x5A, 0x64},
	{0x5B, 0x30},
	{0x5D, 0x11},
	{0x67, 0x11},
	{0x68, 0x03},
	{0x6A, 0x08},
	{0x6C, 0x01},
	{0x6D, 0x04},
	{0xC4, 0x88},
	{0xEA, 0x14},
	{0xEB, 0xC1},
	{0xE1, 0xF8},
	{0x13, 0x30},
	{0x80, 0x00},
	{0x81, 0x10},
	{0xFB, 0x00},
	{0xFC, 0x32},
	{0xFA, 0x01},
	{0x49, 0x10},
	{0x82, 0xFF},
	{0x83, 0x01},
	{0x89, 0x00},
	{0x8C, 0xFF},
	{0x85, 0x00},
	{0xB4, 0x00},
	{0xC0, 0x00},
	{0xAF, 0x00},
	{0xE5, 0x60},
	{0x89, 0x00},
	{0x12, 0x20},
	{0xFF, 0x01},
	{0x76, 0xF5},
	{0x77, 0x03},
	{0x78, 0xF2},
	{0x79, 0xF0},
	{0x7A, 0x28},
	{0x7B, 0x28},
	{0x7C, 0x14},
	{0xFF, 0x00},
	{0x11, 0xCF},
	{0xBB, 0x9B},
	{0xB6, 0x2B},
	{0xB3, 0x7A},
	{0xB4, 0x01},
	{0xD3, 0x22},
	{0xD2, 0x20},
};

static int jx4220_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 2560;
	info->modes.mode[0].size.h = 1440;
	info->modes.mode[0].fps = 30;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x40;
	info->i2c.addr_len = 1;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 1000);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 1000);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V2, 1000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 1000);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 1000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 20000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 5000);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_HIGH, 5000);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 5000);
	set_power_item(&down->items[i++], SNR_RST_GPIO, 0, 1000);
	set_power_item(&down->items[i++], SNR_HCLK, 0, 1000);
	set_power_item(&down->items[i++], SNR_IO_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_CORE_POWER, 0, 1);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, 0, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static const struct fps_info *jx4220_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_jx4220_fps_info); i++)
		if (fps == g_jx4220_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_jx4220_fps_info))
		return NULL;

	return &g_jx4220_fps_info[i];

}

static int jx4220_get_init_info(uint32_t isp_id,
			       const struct rts_isp_sensor_mode *mode,
			       struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct jx4220_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("jx4220 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = jx4220_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->hts);

	set_init_i2c_regs(info->sensor_regs[0],
			g_jx4220_30fps_i2c_init_regs, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0	| MIPI_LANE1;
	info->interface.mipi.hs_term = 0x8;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

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

static int jx4220_start(uint32_t isp_id)
{
	struct jx4220_status *status;

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

	if (gain > 496)
		gain = 496;
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

static int jx4220_get_tuned_again(uint32_t isp_id,
				 float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int jx4220_get_tuned_dgain(uint32_t isp_id,
				 float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int jx4220_get_exposure_gain_info(uint32_t isp_id,
				const struct rts_isp_sensor_exp_gain *exp_gain,
				struct rts_isp_sync_regs *regs)
{

	int i;
	uint16_t total_line;
	uint16_t gain_reg;
	struct jx4220_status *status;
	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	gain_reg = get_sensor_gain_reg(exp_gain->analog_gain[0] *
				       exp_gain->digital_gain[0]);
	total_line = exp_gain->vts;
	reg = regs->reg;
	i = 0;
	set_sync_i2c(&reg[i++], 0xFF, 0x01);
	set_sync_i2c(&reg[i++], 0x00, 0x00);
	set_sync_i2c(&reg[i++], 0x01, gain_reg);

	if (abs(status->last_exposure - exp_gain->exposure[0]) > 0.001f) {
		uint16_t exposure_rows;

		exposure_rows = exp_gain->exposure[0] / status->exp_step + 0.5f;
		set_sync_i2c(&reg[i++], 0x02, 0x01);
		set_sync_i2c(&reg[i++], 0x03, exposure_rows & 0xff);
		set_sync_i2c(&reg[i++], 0x04, 0x02);
		set_sync_i2c(&reg[i++], 0x05, exposure_rows >> 8);
		status->last_exposure = exp_gain->exposure[0];
	}
	set_sync_i2c(&reg[i++], 0x06, 0x22);
	set_sync_i2c(&reg[i++], 0x07, total_line & 0xff);
	set_sync_i2c(&reg[i++], 0x08, 0x23);
	set_sync_i2c(&reg[i++], 0x09, total_line >> 8);
	set_sync_i2c(&reg[i++], 0xFF, 0x00);
	set_sync_i2c(&reg[i++], 0x1F, 0x81);
	regs->num = i;

	return RTS_ISP_OK;
}

static int jx4220_get_mirror_flip(uint32_t isp_id,
				   const struct rts_isp_mirror_flip *mf_info,
				   struct rts_isp_sync_regs *regs)
{
	int i = 0;
	uint32_t val = 0x10;
	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !mf_info || !regs)
		return -RTS_ISP_EINVAL;

	rts_isp_drop_frames(isp_id, 1);

	reg = regs->reg;
	if (mf_info->mirror)
		val |= 0x00;
	if (mf_info->flip)
		val ^= 0x10;

	set_sync_i2c_mask(&reg[i++], 0x12, val, 0x30);
	regs->num = i;

	return RTS_ISP_OK;
}

static int jx4220_check(uint32_t isp_id)
{
	int ret;
	int id;
	struct rts_isp_i2c_reg reg;

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

	if (id == 0x0866)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops jx4220_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "jx4220",
	.get_info = jx4220_get_info,
	.get_init_info = jx4220_get_init_info,
	.start = jx4220_start,
	.get_tuned_again = jx4220_get_tuned_again,
	.get_tuned_dgain = jx4220_get_tuned_dgain,
	.get_exposure_gain_info = jx4220_get_exposure_gain_info,
	.get_mirror_flip = jx4220_get_mirror_flip,
	.check = jx4220_check,
};


RTS_ISP_DEFINE_SENSOR_PLUGIN(jx4220_ops)
