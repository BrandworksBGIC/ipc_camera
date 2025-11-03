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
	uint16_t clk_div;
	uint32_t clk;
};

struct ov9715_status {
	float exp_step;
	float last_exposure;
	uint32_t min_vts;
	struct rts_isp_i2c_reg regs1[3];
};

static struct ov9715_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_ov9715_fps_info[] = {
	{1, 3632, 13, 3000000},
	{2, 3632, 6, 6000000},
	{3, 2824, 5, 7000000},
	{4, 2542, 4, 8400000},
	{5, 2542, 3, 10500000},
	{10, 1691, 2, 14000000},
	{12, 2114, 1, 21000000},
	{13, 1950, 1, 21000000},
	{14, 1812, 1, 21000000},
	{15, 1691, 1, 21000000},
	{20, 2537, 0, 42000000},
	{25, 2029, 0, 42000000},
	{30, 1691, 0, 42000000},
};

static struct rts_isp_i2c_reg g_ov9715_i2c_init_regs[] = {
	{0x1E, 0x07}, {0x5F, 0x18}, {0x69, 0x04}, {0x65, 0x2A}, {0x68, 0x0A},
	{0x39, 0x28}, {0x4D, 0x90}, {0xC1, 0x80}, {0x0c, 0x30}, {0x6d, 0x02},
	{0x60, 0x9d}, {0x96, 0xF1}, {0xBC, 0x68}, {0x12, 0x00}, {0x3B, 0x00},
	{0x97, 0x80}, {0x17, 0x25}, {0x18, 0xA2}, {0x19, 0x01}, {0x1A, 0xCA},
	{0x03, 0x00}, {0x32, 0x07}, {0x98, 0x00}, {0x99, 0x00}, {0x9A, 0x00},
	{0x57, 0x01}, {0x58, 0xC9}, {0x59, 0xA2}, {0x4C, 0x13}, {0x4B, 0x36},
	{0x3D, 0x3C}, {0x3E, 0x03}, {0xBD, 0xA0}, {0xBE, 0xC8}, {0x37, 0x03},
	{0x4E, 0x55}, {0x4F, 0x55}, {0x50, 0x55}, {0x51, 0x55}, {0x24, 0x55},
	{0x25, 0x40}, {0x26, 0xA1}, {0x5C, 0x59}, {0x5D, 0x00}, {0x11, 0x01},
	{0x2A, 0x98}, {0x2B, 0x06}, {0x2D, 0x00}, {0x2E, 0x00}, {0x13, 0xA5},
	{0x14, 0x40}, {0x4a, 0x00}, {0x49, 0xce}, {0x22, 0x03}, {0x13, 0x80},
	{0x38, 0x00}, {0xb6, 0x08}, {0x96, 0xe1}, {0x01, 0x40}, {0x02, 0x40},
	{0x05, 0x40}, {0x06, 0x00}, {0x07, 0x00},
};

static int ov9715_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 1280;
	info->modes.mode[0].size.h = 720;
	info->modes.mode[0].fps = g_ov9715_fps_info[12].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x30;
	info->i2c.addr_len = 1;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_HIGH, 0);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 10);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_3V3, 10);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 0);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 20);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_RST_GPIO, 0, 0);
	set_power_item(&down->items[i++], SNR_HCLK, 0, 20);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_IO_POWER, 0, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static const struct fps_info *ov9715_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_ov9715_fps_info); i++)
		if (fps == g_ov9715_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_ov9715_fps_info))
		return NULL;

	return &g_ov9715_fps_info[i];
}

static int ov9715_get_init_info(uint32_t isp_id,
				const struct rts_isp_sensor_mode *mode,
				struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct ov9715_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("ov9715 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = ov9715_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, clk_div: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->clk_div, fps_info->hts);

	set_init_i2c(&status->regs1[0], 0x11, fps_info->clk_div);
	set_init_i2c(&status->regs1[1], 0x2A, fps_info->hts & 0xff);
	set_init_i2c(&status->regs1[2], 0x2B, fps_info->hts >> 8);

	set_init_i2c_regs(info->sensor_regs[0], g_ov9715_i2c_init_regs, 0);
	set_init_i2c_regs(info->sensor_regs[1], status->regs1, 0);

	info->interface.interface = SNR_INTERFACE_DVP;
	info->interface.dvp.sample_rising = 1;
	info->interface.dvp.hsync_active_high = 1;
	info->interface.dvp.vsync_active_high = 1;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 1280;
	info->size.h = 721;
	info->start.x = 0;
	info->start.y = 1;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = status->min_vts = 826;
	info->max_vts = 65536;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */

	return RTS_ISP_OK;
}

static int ov9715_start(uint32_t isp_id)
{
	struct ov9715_status *status;

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

	for (i = 0; i < 4; i++) {
		if (gain >= 32) {
			gain >>= 1;
			reg_value |= 0x01 << (i + 4);
		} else {
			reg_value |= (gain - 16);
			break;
		}
	}

	return reg_value;
}

static float get_sensor_real_gain(uint8_t reg_value)
{
	int i;
	uint16_t gain;

	gain = (reg_value & 0x0f) + 16;
	reg_value >>= 4;

	for (i = 0; i < 4; i++) {
		gain <<= (reg_value & 1);
		reg_value >>= 1;
	}

	return gain / 16.0f;
}

static int ov9715_get_tuned_again(uint32_t isp_id,
				  float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int ov9715_get_tuned_dgain(uint32_t isp_id,
				  float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int ov9715_get_exposure_gain_info(uint32_t isp_id,
					 const struct rts_isp_sensor_exp_gain *exp_gain,
					 struct rts_isp_sync_regs *regs)
{
	int i;
	uint8_t gain_reg;
	uint32_t dummy;
	struct ov9715_status *status;
	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	gain_reg = get_sensor_gain_reg(exp_gain->analog_gain[0] *
				       exp_gain->digital_gain[0]);
	reg = regs->reg;

	i = 0;
	if (abs(status->last_exposure - exp_gain->exposure[0]) > 0.001f) {
		uint16_t exposure_rows;

		exposure_rows = exp_gain->exposure[0] / status->exp_step + 0.5f;
		set_sync_i2c(&reg[i++], 0x10, exposure_rows & 0xff);
		set_sync_i2c(&reg[i++], 0x16, exposure_rows >> 8);
		status->last_exposure = exp_gain->exposure[0];

		set_sync_info(&reg[i++], 1, RTS_ISP_INT_DATA_START);
	}
	dummy = exp_gain->vts - status->min_vts;
	set_sync_i2c(&reg[i++], 0x2d, dummy & 0xff);
	set_sync_i2c(&reg[i++], 0x2e, dummy >> 8);
	set_sync_i2c(&reg[i++], 0x00, gain_reg);
	regs->num = i;

	return RTS_ISP_OK;
}

static int ov9715_check(uint32_t isp_id)
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

	if (id == 0x9711)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops ov9715_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "ov9715",
	.get_info = ov9715_get_info,
	.get_init_info = ov9715_get_init_info,
	.start = ov9715_start,
	.get_tuned_again = ov9715_get_tuned_again,
	.get_tuned_dgain = ov9715_get_tuned_dgain,
	.get_exposure_gain_info = ov9715_get_exposure_gain_info,
	.check = ov9715_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(ov9715_ops)
