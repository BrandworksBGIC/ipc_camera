/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2023 Yang Wang  <yang_wang@apowertec.com>
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

struct jxf53_status {
	int hdr;
	float exp_step;
	float last_exposure;
	uint16_t min_vts;
};

static struct jxf53_status g_status[SUPPORTED_ISP_NUM];

static struct rts_isp_i2c_reg g_jxf53_30fps_linear_init_regs[] = {
	{0x12, 0x40}, {0x48, 0x8A}, {0x48, 0x0A}, {0x0E, 0x19},
	{0x0F, 0x04}, {0x10, 0x24}, {0x11, 0x80}, {0x46, 0x09},
	{0x47, 0x66}, {0x0D, 0xF2}, {0x57, 0x6A}, {0x58, 0x22},
	{0x5F, 0x41}, {0x60, 0x28}, {0xA5, 0xC0}, {0x20, 0x00},
	{0x21, 0x05}, {0x22, 0x65}, {0x23, 0x04}, {0x24, 0xC0},
	{0x25, 0x38}, {0x26, 0x43}, {0x27, 0xC6}, {0x28, 0x14},
	{0x29, 0x04}, {0x2A, 0xBB}, {0x2B, 0x14}, {0x2C, 0x02},
	{0x2D, 0x00}, {0x2E, 0x14}, {0x2F, 0x04}, {0x41, 0xC5},
	{0x42, 0x33}, {0x47, 0x46}, {0x76, 0x60}, {0x77, 0x09},
	{0x80, 0x01}, {0xAF, 0x22}, {0xAB, 0x00}, {0x1D, 0x00},
	{0x1E, 0x04}, {0x6C, 0x40}, {0x9E, 0xF8}, {0x6E, 0x2C},
	{0x70, 0x6C}, {0x71, 0x6D}, {0x72, 0x6A}, {0x73, 0x56},
	{0x74, 0x02}, {0x78, 0x9D}, {0x89, 0x01}, {0x6B, 0x20},
	{0x86, 0x40}, {0x31, 0x10}, {0x32, 0x18}, {0x33, 0xE8},
	{0x34, 0x5E}, {0x35, 0x5E}, {0x3A, 0xAF}, {0x3B, 0x00},
	{0x3C, 0xFF}, {0x3D, 0xFF}, {0x3E, 0xFF}, {0x3F, 0xBB},
	{0x40, 0xFF}, {0x56, 0x92}, {0x59, 0xAF}, {0x5A, 0x47},
	{0x61, 0x18}, {0x6F, 0x04}, {0x85, 0x5F}, {0x8A, 0x44},
	{0x91, 0x13}, {0x94, 0xA0}, {0x9B, 0x83}, {0x9C, 0xE1},
	{0xA4, 0x80}, {0xA6, 0x22}, {0xA9, 0x1C}, {0x5B, 0xE7},
	{0x5C, 0x28}, {0x5D, 0x67}, {0x5E, 0x11}, {0x62, 0x21},
	{0x63, 0x0F}, {0x64, 0xD0}, {0x65, 0x02}, {0x67, 0x49},
	{0x66, 0x00}, {0x68, 0x00}, {0x69, 0x72}, {0x6A, 0x12},
	{0x7A, 0x00}, {0x82, 0x20}, {0x8D, 0x47}, {0x8F, 0x90},
	{0x45, 0x01}, {0x97, 0x20}, {0x13, 0x81}, {0x96, 0x84},
	{0x4A, 0x01}, {0xB1, 0x00}, {0xA1, 0x0F}, {0xBE, 0x00},
	{0x7E, 0x48}, {0xB5, 0xC0}, {0x50, 0x02}, {0x49, 0x10},
	{0x7F, 0x57}, {0x90, 0x00}, {0x7B, 0x4A}, {0x7C, 0x07},
	{0x8C, 0xFF}, {0x8E, 0x00}, {0x8B, 0x01}, {0x0C, 0x00},
	{0xBC, 0x11}, {0x19, 0x20}, {0x1B, 0x4F}, {0x12, 0x00},
	{0x00, 0x10},
};

static struct rts_isp_i2c_reg g_jxf53_15fps_hdr_init_regs[] = {
	{0x12, 0x48}, {0x48, 0x8A}, {0x48, 0x0A}, {0x0E, 0x19},
	{0x0F, 0x04}, {0x10, 0x24}, {0x11, 0x80}, {0x46, 0x0D},
	{0x47, 0x66}, {0x0D, 0xF2}, {0x57, 0x6A}, {0x58, 0x22},
	{0x5F, 0x41}, {0x60, 0x28}, {0xA5, 0xC0}, {0x20, 0x00},
	{0x21, 0x05}, {0x22, 0xCA}, {0x23, 0x08}, {0x24, 0xC0},
	{0x25, 0x38}, {0x26, 0x43}, {0x27, 0xC6}, {0x28, 0x29},
	{0x29, 0x04}, {0x2A, 0xBB}, {0x2B, 0x14}, {0x2C, 0x02},
	{0x2D, 0x00}, {0x2E, 0x14}, {0x2F, 0x04}, {0x41, 0xC5},
	{0x42, 0x33}, {0x47, 0x46}, {0x76, 0x60}, {0x77, 0x09},
	{0x80, 0x01}, {0xAF, 0x22}, {0xAB, 0x00}, {0x1D, 0x00},
	{0x1E, 0x04}, {0x6C, 0x40}, {0x9E, 0xF8}, {0x6E, 0x2C},
	{0x70, 0x6C}, {0x71, 0x6D}, {0x72, 0x6A}, {0x73, 0x56},
	{0x74, 0x02}, {0x78, 0x9D}, {0x89, 0x81}, {0x6B, 0x20},
	{0x86, 0x40}, {0x31, 0x10}, {0x32, 0x18}, {0x33, 0xE8},
	{0x34, 0x5E}, {0x35, 0x5E}, {0x3A, 0xAF}, {0x3B, 0x00},
	{0x3C, 0xFF}, {0x3D, 0xFF}, {0x3E, 0xFF}, {0x3F, 0xBB},
	{0x40, 0xFF}, {0x56, 0x92}, {0x59, 0xAF}, {0x5A, 0x47},
	{0x61, 0x18}, {0x6F, 0x04}, {0x85, 0x5F}, {0x8A, 0x44},
	{0x91, 0x13}, {0x94, 0xA0}, {0x9B, 0x83}, {0x9C, 0xE1},
	{0xA4, 0x80}, {0xA6, 0x22}, {0xA9, 0x1C}, {0x5B, 0xE7},
	{0x5C, 0x28}, {0x5D, 0x67}, {0x5E, 0x11}, {0x62, 0x21},
	{0x63, 0x0F}, {0x64, 0xD0}, {0x65, 0x02}, {0x67, 0x49},
	{0x66, 0x00}, {0x68, 0x00}, {0x69, 0x72}, {0x6A, 0x12},
	{0x7A, 0x00}, {0x82, 0x20}, {0x8D, 0x47}, {0x8F, 0x90},
	{0x45, 0x01}, {0x97, 0x20}, {0x13, 0x81}, {0x96, 0x84},
	{0x4A, 0x01}, {0xB1, 0x00}, {0xA1, 0x0F}, {0xBE, 0x00},
	{0x7E, 0x48}, {0xB5, 0xC0}, {0x50, 0x02}, {0x49, 0x10},
	{0x7F, 0x57}, {0x90, 0x00}, {0x7B, 0x4A}, {0x7C, 0x07},
	{0x8C, 0xFF}, {0x8E, 0x00}, {0x8B, 0x01}, {0x0C, 0x00},
	{0xBC, 0x11}, {0x19, 0x20}, {0x1B, 0x4F}, {0x07, 0x43},
	{0x06, 0x43}, {0x03, 0xFF}, {0x04, 0xFF}, {0x12, 0x08},
	{0x00, 0x10},
};

static int jxf53_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	i = 0;
	info->modes.mode[i].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[i].size.w = 1920;
	info->modes.mode[i].size.h = 1080;
	info->modes.mode[i].fps = 30;
	i++;
	info->modes.mode[i].hdr = RTS_ISP_HDR_LINE_2TO1;
	info->modes.mode[i].size.w = 1920;
	info->modes.mode[i].size.h = 1080;
	info->modes.mode[i].fps = 15;
	i++;
	info->modes.num = i;

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
	set_power_item(&down->items[i++], SNR_CORE_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, 0, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static int jxf53_get_init_info(uint32_t isp_id,
			       const struct rts_isp_sensor_mode *mode,
			       struct rts_isp_sensor_init_info *info)
{
	struct jxf53_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("jxf53 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	status->hdr = mode->hdr;

	if (mode->hdr == RTS_ISP_HDR_LINE_2TO1) {
		debug("hdr line 2to1 mode\n");
		set_init_i2c_regs(info->sensor_regs[0],
			g_jxf53_15fps_hdr_init_regs, 0);

		info->interface.interface = SNR_INTERFACE_MIPI;
		info->interface.mipi.hdr = MIPI_HDR_VC;
		info->interface.mipi.lanes = MIPI_LANE0	| MIPI_LANE1;
		info->interface.mipi.hs_term = 0x3;
		info->interface.type = RAW_SENSOR;
		info->interface.bit_depth = SNR_10BIT;
		info->interface.type_config.raw.bayer = SNR_BGGR;

		info->size.w = 1920;
		info->size.h = 1080;
		info->start.x = 0;
		info->start.y = 0;

		info->hts = 5120;
		info->pclk = 86400000;
		info->min_vts = 1125;
		info->max_vts = info->min_vts * 2;/*min_fps = 7.5*/

		status->min_vts = info->min_vts;
		status->exp_step = 1e6 * info->hts / 2 / info->pclk; /* us */
	} else {
		debug("linear mode\n");
		set_init_i2c_regs(info->sensor_regs[0],
			g_jxf53_30fps_linear_init_regs, 0);

		info->interface.interface = SNR_INTERFACE_MIPI;
		info->interface.mipi.hdr = MIPI_HDR_NONE;
		info->interface.mipi.lanes = MIPI_LANE0	| MIPI_LANE1;
		info->interface.mipi.hs_term = 0x3;
		info->interface.type = RAW_SENSOR;
		info->interface.bit_depth = SNR_10BIT;

		info->size.w = 1920;
		info->size.h = 1080;
		info->start.x = 0;
		info->start.y = 0;

		info->hts = 2560;
		info->pclk = 86400000;
		info->min_vts = 1125;
		info->max_vts = 65536;

		status->min_vts = info->min_vts;
		status->exp_step = 1e6 * info->hts / info->pclk; /* us */
	}

	return RTS_ISP_OK;
}

static int jxf53_start(uint32_t isp_id)
{
	struct jxf53_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}
static int jxf53_get_exposure_range(uint32_t isp_id, uint32_t vts,
				     float ratio[RTS_ISP_HDR_CHAN_MAX - 1],
				     float min_exposure[RTS_ISP_HDR_CHAN_MAX],
				     float max_exposure[RTS_ISP_HDR_CHAN_MAX])
{
	struct jxf53_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	if (status->hdr == RTS_ISP_HDR_NONE) {
		min_exposure[0] = status->exp_step;
		max_exposure[0] = (vts - 4) * status->exp_step;
	} else {
		uint32_t tmp1;
		uint32_t tmp2;

		tmp1 = (uint32_t)((vts * 2 - 8) / (ratio[0] + 1));
		/*reg_0x05(short exp lines) < reg_0x06 *2 +1 -5*/
		tmp2 = 67 * 2 + 1 - 5;
		tmp1 = tmp1 < tmp2 ? tmp1 : (tmp2 - 1);
		max_exposure[1] = tmp1 * status->exp_step;
		min_exposure[1] = status->exp_step;
		max_exposure[0] = max_exposure[1] * ratio[0];
		min_exposure[0] = min_exposure[1] * ratio[0];
	}

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

static int jxf53_get_tuned_again(uint32_t isp_id,
				 float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;
	struct jxf53_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);
	if (status->hdr == RTS_ISP_HDR_LINE_2TO1) {
		gain_reg = get_sensor_gain_reg(again[1]);
		again[1] = get_sensor_real_gain(gain_reg);
	}

	return RTS_ISP_OK;
}

static int jxf53_get_tuned_dgain(uint32_t isp_id,
				 float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	struct jxf53_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	dgain[0] = 1.0f;
	if (status->hdr == RTS_ISP_HDR_LINE_2TO1)
		dgain[1] = 1.0f;

	return RTS_ISP_OK;
}

static int jxf53_get_exposure_gain_info(uint32_t isp_id,
				const struct rts_isp_sensor_exp_gain *exp_gain,
				struct rts_isp_sync_regs *regs)
{
	int i;
	uint16_t gain_reg;
	uint32_t exp_rows[2];
	uint32_t total_line;
	struct jxf53_status *status;
	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	if (status->hdr == RTS_ISP_HDR_NONE) {
		gain_reg = get_sensor_gain_reg(exp_gain->analog_gain[0] *
				       exp_gain->digital_gain[0]);
		total_line = exp_gain->vts;
		reg = regs->reg;
		i = 0;
		/* set gain */
		set_sync_i2c(&reg[i++], 0xC0, 0x00);
		set_sync_i2c(&reg[i++], 0xC1, gain_reg);
		/* set exposure */
		exp_rows[0] = exp_gain->exposure[0] / status->exp_step + 0.5f;
		set_sync_i2c(&reg[i++], 0xC2, 0x01);
		set_sync_i2c(&reg[i++], 0xC3, exp_rows[0] & 0xff);
		set_sync_i2c(&reg[i++], 0xC4, 0x02);
		set_sync_i2c(&reg[i++], 0xC5, exp_rows[0] >> 8);
		status->last_exposure = exp_gain->exposure[0];
		/* set vts */
		set_sync_i2c(&reg[i++], 0xC6, 0x22);
		set_sync_i2c(&reg[i++], 0xC7, total_line & 0xff);
		set_sync_i2c(&reg[i++], 0xC8, 0x23);
		set_sync_i2c(&reg[i++], 0xC9, total_line >> 8);
		/* group write trigger */
		set_sync_i2c_mask(&reg[i++], 0x1F, 0x80, 0x80);
		regs->num = i;
	} else if (status->hdr == RTS_ISP_HDR_LINE_2TO1) {
		gain_reg = get_sensor_gain_reg(exp_gain->analog_gain[0] *
				       exp_gain->digital_gain[0]);
		exp_rows[0] = exp_gain->exposure[0] / status->exp_step;
		if (exp_rows[0] % 2 == 0)
			exp_rows[0] += 1;
		exp_rows[1] = exp_gain->exposure[1] / status->exp_step + 0.5f;
		total_line = exp_gain->vts * 2;
		reg = regs->reg;
		i = 0;
		/* set gain */
		set_sync_i2c(&reg[i++], 0xC0, 0x00);
		set_sync_i2c(&reg[i++], 0xC1, gain_reg);
		/* set long exposure */
		set_sync_i2c(&reg[i++], 0xC2, 0x01);
		set_sync_i2c(&reg[i++], 0xC3, exp_rows[0] & 0xff);
		set_sync_i2c(&reg[i++], 0xC4, 0x02);
		set_sync_i2c(&reg[i++], 0xC5, exp_rows[0] >> 8);
		/* set short exposure */
		set_sync_i2c(&reg[i++], 0xC6, 0x05);
		set_sync_i2c(&reg[i++], 0xC7, exp_rows[1] & 0xff);
		set_sync_i2c(&reg[i++], 0xC8, 0x08);
		set_sync_i2c(&reg[i++], 0xC9, (exp_rows[1] >> 8) & 0x01);
		/* set vts */
		set_sync_i2c(&reg[i++], 0xCA, 0x22);
		set_sync_i2c(&reg[i++], 0xCB, total_line & 0xff);
		set_sync_i2c(&reg[i++], 0xCC, 0x23);
		set_sync_i2c(&reg[i++], 0xCD, total_line >> 8);
		/* group write trigger */
		set_sync_i2c_mask(&reg[i++], 0x1F, 0x80, 0x80);
		regs->num = i;
	}
	return RTS_ISP_OK;
}

static int jxf53_check(uint32_t isp_id)
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

	if (id == 0x0842)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops jxf53_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "jxf53",
	.get_info = jxf53_get_info,
	.get_init_info = jxf53_get_init_info,
	.start = jxf53_start,
	.get_exposure_range = jxf53_get_exposure_range,
	.get_tuned_again = jxf53_get_tuned_again,
	.get_tuned_dgain = jxf53_get_tuned_dgain,
	.get_exposure_gain_info = jxf53_get_exposure_gain_info,
	.check = jxf53_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(jxf53_ops)
