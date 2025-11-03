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

struct imx132_status {
	float exp_step;
	float last_exposure;
	struct rts_isp_i2c_reg regs0[2];
	struct rts_isp_i2c_reg regs2[3];
};

static struct imx132_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info  g_imx132_fps_info[] = {
	{ 5, 3570, 8, 20250000},
	{10, 3570, 4, 40500000},
	{15, 2380, 4, 40500000},
	{20, 3570, 2, 81000000},
	{25, 2856, 2, 81000000},
	{30, 2380, 2, 81000000},
};

#define USE_2LANE 1
#if USE_2LANE
static struct rts_isp_i2c_reg g_imx132_i2c_init_regs[] = {
	{0x0307, 0x22}, {0x30A4, 0x02}, {0x303C, 0x4B}, {0x3087, 0x53},
	{0x308B, 0x5A}, {0x3094, 0x11}, {0x309D, 0xA4}, {0x30C6, 0x00},
	{0x30C7, 0x00}, {0x3118, 0x2F}, {0x312A, 0x00}, {0x312B, 0x0B},
	{0x312C, 0x0B}, {0x312D, 0x13}, {0x3032, 0x40}, {0x0340, 0x04},
	{0x0341, 0x64}, {0x0342, 0x09}, {0x0343, 0x4c}, {0x0344, 0x00},
	{0x0345, 0x00}, {0x0346, 0x00}, {0x0347, 0x1C}, {0x0348, 0x07},
	{0x0349, 0xB7}, {0x034A, 0x04}, {0x034B, 0x67}, {0x034C, 0x07},
	{0x034D, 0xB8}, {0x034E, 0x04}, {0x034F, 0x4c}, {0x0381, 0x01},
	{0x0383, 0x01}, {0x0385, 0x01}, {0x0387, 0x01}, {0x303D, 0x10},
	{0x303E, 0x4A}, {0x3048, 0x00}, {0x304C, 0x2F}, {0x304D, 0x02},
	{0x309B, 0x00}, {0x309E, 0x41}, {0x30A0, 0x10}, {0x30A1, 0x0B},
	{0x30B2, 0x00}, {0x30D5, 0x00}, {0x30D6, 0x00}, {0x30D7, 0x00},
	{0x30DE, 0x00}, {0x3102, 0x0C}, {0x3103, 0x33}, {0x3104, 0x30},
	{0x3105, 0x00}, {0x3106, 0xCA}, {0x315C, 0x3D}, {0x315D, 0x3C},
	{0x316E, 0x3E}, {0x316F, 0x3D}, {0x3301, 0x00}, {0x3318, 0x61},
	{0x0202, 0x00}, {0x0203, 0xD0}, {0x0112, 0x0a}, {0x0113, 0x0a},
};
#else
static const struct rts_isp_i2c_reg g_imx132_i2c_init_regs[] = {
	{0x0307, 0x43}, {0x0101, 0x03}, {0x303E, 0x5A}, {0x3105, 0x00},
	{0x3104, 0x18}, {0x3107, 0x00}, {0x3106, 0x65}, {0x3318, 0x61},
	{0x3301, 0x01}, {0x0112, 0x0a}, {0x0113, 0x0a}, {0x0340, 0x04},
	{0x0341, 0x64}, {0x0342, 0x09}, {0x0343, 0x4c}, {0x0344, 0x00},
	{0x0345, 0x00}, {0x0346, 0x00}, {0x0347, 0x1C}, {0x0348, 0x07},
	{0x0349, 0xB7}, {0x034A, 0x04}, {0x034B, 0x93}, {0x034C, 0x07},
	{0x034D, 0xB8}, {0x034E, 0x04}, {0x034F, 0x4c}, {0x0202, 0x00},
	{0x0203, 0xD0},
};
#endif

static int imx132_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 1920;
	info->modes.mode[0].size.h = 1080;
	info->modes.mode[0].fps = g_imx132_fps_info[5].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x36;
	info->i2c.addr_len = 2;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V7, 0);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V2, 0);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 1);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 1);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 10);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_RST_GPIO, 0, 1);
	set_power_item(&down->items[i++], SNR_HCLK, 0, 1);
	set_power_item(&down->items[i++], SNR_IO_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_CORE_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, 0, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static const struct fps_info *imx132_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_imx132_fps_info); i++)
		if (fps == g_imx132_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_imx132_fps_info))
		return NULL;

	return &g_imx132_fps_info[i];
}

static int imx132_get_init_info(uint32_t isp_id,
				const struct rts_isp_sensor_mode *mode,
			       struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct imx132_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("imx132 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = imx132_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, clk_div: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->clk_div, fps_info->hts);

	set_init_i2c(&status->regs0[0], 0x0103, 0x01);
	set_init_i2c(&status->regs0[1], 0x0305, fps_info->clk_div);

	set_init_i2c(&status->regs2[0], 0x0343, fps_info->hts & 0xff);
	set_init_i2c(&status->regs2[1], 0x0342, fps_info->hts >> 8);
	set_init_i2c(&status->regs2[2], 0x0100, 0x01);

	set_init_i2c_regs(info->sensor_regs[0], status->regs0, 0);
	set_init_i2c_regs(info->sensor_regs[1], g_imx132_i2c_init_regs, 0);
	set_init_i2c_regs(info->sensor_regs[2], status->regs2, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
#if USE_2LANE
	info->interface.mipi.lanes = MIPI_LANE0 | MIPI_LANE1;
#else
	info->interface.mipi.lanes = MIPI_LANE0;
#endif
	info->interface.mipi.hs_term = 0x6;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 1921;
	info->size.h = 1080;
	info->start.x = 1;
	info->start.y = 0;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = 1200;
	info->max_vts = 65535;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */

	return RTS_ISP_OK;
}

static int imx132_start(uint32_t isp_id)
{
	struct imx132_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static void get_sensor_gain_reg_value(float fgain,
				      uint16_t *again_reg, uint16_t *dgain_reg)
{
	uint16_t gain = fgain * 16;
	uint16_t again = gain < 128 ? gain : 128;

	*again_reg = 256 - 4096 / again;
	*dgain_reg = gain * 256 / again;
}

static float get_sensor_real_gain(uint16_t again_reg, uint16_t dgain_reg)
{
	return 256.0f / (256 - again_reg) * dgain_reg / 256.0f;
}

static int imx132_get_tuned_again(uint32_t isp_id,
				  float again[RTS_ISP_HDR_CHAN_MAX])
{
	uint16_t again_reg;
	uint16_t dgain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	get_sensor_gain_reg_value(again[0], &again_reg, &dgain_reg);
	again[0] = get_sensor_real_gain(again_reg, dgain_reg);

	return RTS_ISP_OK;
}

static int imx132_get_tuned_dgain(uint32_t isp_id,
				  float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int imx132_get_exposure_gain_info(uint32_t isp_id,
					const struct rts_isp_sensor_exp_gain *exp_gain,
					struct rts_isp_sync_regs *regs)
{
	int i;
	uint16_t again_reg;
	uint16_t dgain_reg;
	float gain;
	struct imx132_status *status;
	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	gain = exp_gain->analog_gain[0] * exp_gain->digital_gain[0];
	get_sensor_gain_reg_value(gain, &again_reg, &dgain_reg);
	reg = regs->reg;

	i = 0;
	set_sync_i2c(&reg[i++], 0x0104, 0x01);
	if (abs(status->last_exposure - exp_gain->exposure[0]) > 0.001f) {
		uint16_t exposure_rows;

		exposure_rows = exp_gain->exposure[0] / status->exp_step + 0.5f;
		set_sync_i2c(&reg[i++], 0x0202, exposure_rows >> 8);
		set_sync_i2c(&reg[i++], 0x0203, exposure_rows & 0xff);
		status->last_exposure = exp_gain->exposure[0];
	}
	set_sync_i2c(&reg[i++], 0x0204, again_reg >> 8);
	set_sync_i2c(&reg[i++], 0x0205, again_reg & 0xff);

	set_sync_i2c(&reg[i++], 0x020E, dgain_reg >> 8);
	set_sync_i2c(&reg[i++], 0x020F, dgain_reg & 0xff);
	set_sync_i2c(&reg[i++], 0x0210, dgain_reg >> 8);
	set_sync_i2c(&reg[i++], 0x0211, dgain_reg & 0xff);
	set_sync_i2c(&reg[i++], 0x0212, dgain_reg >> 8);
	set_sync_i2c(&reg[i++], 0x0213, dgain_reg & 0xff);
	set_sync_i2c(&reg[i++], 0x0214, dgain_reg >> 8);
	set_sync_i2c(&reg[i++], 0x0215, dgain_reg & 0xff);

	set_sync_i2c(&reg[i++], 0x0104, 0x00);
	regs->num = i;

	return RTS_ISP_OK;
}

static int imx132_check(uint32_t isp_id)
{
	int ret;
	int id;
	struct rts_isp_i2c_reg reg = {};

	reg.addr = 0x0000;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id = reg.data << 8;

	reg.addr = 0x0001;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id |= reg.data;

	if (id == 0x0132)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops imx132_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "imx132",
	.get_info = imx132_get_info,
	.get_init_info = imx132_get_init_info,
	.start = imx132_start,
	.get_tuned_again = imx132_get_tuned_again,
	.get_tuned_dgain = imx132_get_tuned_dgain,
	.get_exposure_gain_info = imx132_get_exposure_gain_info,
	.check = imx132_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(imx132_ops)
