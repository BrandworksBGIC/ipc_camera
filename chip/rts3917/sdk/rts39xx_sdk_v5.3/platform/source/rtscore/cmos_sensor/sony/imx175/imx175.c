/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2020 Sally Peng <sally_peng@realsil.com.cn>
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

struct imx175_status {
	float exp_step;
	float last_exposure;
	int min_vts;
	struct rts_isp_i2c_reg regs1[2];
};

static struct imx175_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info  g_imx175_fps_info[] = {
	{15, 3840, 144000000},
};

static struct rts_isp_i2c_reg g_imx175_i2c_init_regs[] = {
	{0x0100, 0x00},
	{0x0202, 0x09},
	{0x0203, 0xC0},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0305, 0x06},
	{0x0309, 0x05},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0x5A},
	{0x0340, 0x09},
	{0x0341, 0xC4},
	{0x0342, 0x0F},
	{0x0343, 0x00},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x00},
	{0x0347, 0x00},
	{0x0348, 0x0C},
	{0x0349, 0xCF},
	{0x034A, 0x09},
	{0x034B, 0x9F},
	{0x034C, 0x0C},
	{0x034D, 0xD0},
	{0x034E, 0x09},
	{0x034F, 0xA0},
	{0x0390, 0x00},
	{0x3020, 0x10},
	{0x302D, 0x03},
	{0x302F, 0x80},
	{0x3032, 0xA3},
	{0x3033, 0x20},
	{0x3034, 0x24},
	{0x3041, 0x15},
	{0x3042, 0x87},
	{0x3050, 0x35},
	{0x3056, 0x57},
	{0x305D, 0x41},
	{0x3097, 0x69},
	{0x3109, 0x41},
	{0x3148, 0x3F},
	{0x3302, 0x0 },
	{0x330F, 0x07},
	{0x3344, 0x3F},
	{0x3345, 0x1F},
	{0x3364, 0x00},
	{0x3368, 0x18},
	{0x3369, 0x00},
	{0x3370, 0x67},
	{0x3371, 0x17},
	{0x3372, 0x47},
	{0x3373, 0x1F},
	{0x3374, 0x1F},
	{0x3375, 0x17},
	{0x3376, 0x77},
	{0x3377, 0x27},
	{0x33C8, 0x00},
	{0x33D4, 0x0C},
	{0x33D5, 0xD0},
	{0x33D6, 0x09},
	{0x33D7, 0xA0},
	{0x4100, 0x0E},
	{0x4104, 0x32},
	{0x4105, 0x32},
	{0x4108, 0x01},
	{0x4109, 0x7C},
	{0x410A, 0x00},
	{0x410B, 0x00},
	{0x0100, 0x01},
};

static struct rts_isp_i2c_reg g_imx175_i2c_focus_regs[5] = {
	{0xec, 0xa3},
	{0xa1, 0x0d},
	{0xf2, 0x00},
	{0xdc, 0x51},
};

static int imx175_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 3072;
	info->modes.mode[0].size.h = 2048;
	info->modes.mode[0].fps = g_imx175_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x10;
	info->i2c.addr_len = 2;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V7, 10);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 0);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 10);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 100);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V2, 10);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_RST_GPIO, 0, 0);
	set_power_item(&down->items[i++], SNR_CORE_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_HCLK, 0, 0);
	set_power_item(&down->items[i++], SNR_IO_POWER, 0, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static const struct fps_info *imx175_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_imx175_fps_info); i++)
		if (fps == g_imx175_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_imx175_fps_info))
		return NULL;

	return &g_imx175_fps_info[i];
}

static int imx175_get_init_info(uint32_t isp_id,
				const struct rts_isp_sensor_mode *mode,
			       struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct imx175_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("imx175 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = imx175_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	set_init_i2c(&status->regs1[0], 0x0343, fps_info->hts & 0xff);
	set_init_i2c(&status->regs1[1], 0x0342, fps_info->hts >> 8);

	set_init_i2c_regs(info->sensor_regs[0], g_imx175_i2c_init_regs, 0);
	set_init_i2c_regs(info->sensor_regs[1], status->regs1, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0 | MIPI_LANE1 |
					MIPI_LANE2 | MIPI_LANE3;
	info->interface.mipi.hs_term = 0x4;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 3280;
	info->size.h = 2416;
	info->start.x = 105;
	info->start.y = 184;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = 2500;
	info->max_vts = 65535;

	status->exp_step = 1e6 * info->hts / info->pclk;
	status->min_vts = info->min_vts;

	return RTS_ISP_OK;
}

static int imx175_start(uint32_t isp_id)
{
	struct imx175_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static void get_sensor_gain_reg(float fgain,
		uint16_t *analog_gain_reg, uint16_t *digital_gain_reg)
{
	uint16_t gain = fgain * 16;

	if (gain > 128) {
		*analog_gain_reg = 256 - 4096 / 128;
		*digital_gain_reg = gain / 128.0f * 256;

		if (*digital_gain_reg >= 0x1000)
			*digital_gain_reg = 0x0fff;
	} else {
		*analog_gain_reg = 256 - 4096 / gain;
		*digital_gain_reg = 0x100;
	}
}

static float get_sensor_real_gain(uint16_t again_reg, uint16_t dgain_reg)
{
	return 256.0 / (256.0 - again_reg) * dgain_reg / 256.0;
}

static int imx175_get_tuned_again(uint32_t isp_id,
				  float again[RTS_ISP_HDR_CHAN_MAX])
{
	uint16_t again_reg;
	uint16_t dgain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	get_sensor_gain_reg(again[0], &again_reg, &dgain_reg);
	again[0] = get_sensor_real_gain(again_reg, dgain_reg);

	return RTS_ISP_OK;
}

static int imx175_get_tuned_dgain(uint32_t isp_id,
				  float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int imx175_get_exposure_gain_info(uint32_t isp_id,
					const struct rts_isp_sensor_exp_gain *exp_gain,
					struct rts_isp_sync_regs *regs)
{
	int i;
	struct imx175_status *status;
	struct rts_isp_sync_reg *reg;
	uint16_t vts;
	uint16_t again_reg;
	uint16_t dgain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	vts = exp_gain->vts;
	get_sensor_gain_reg(exp_gain->analog_gain[0] * exp_gain->digital_gain[0],
						&again_reg, &dgain_reg);

	reg = regs->reg;

	i = 0;
	set_sync_i2c(&reg[i++], 0x0104, 0x01);
	if (abs(status->last_exposure - exp_gain->exposure[0]) > 0.001f) {
		uint16_t exposure_rows;

		exposure_rows = exp_gain->exposure[0] / status->exp_step;
		set_sync_i2c(&reg[i++], 0x0203, exposure_rows & 0xff);
		set_sync_i2c(&reg[i++], 0x0202, exposure_rows >> 8);
		status->last_exposure = exp_gain->exposure[0];

	}

	set_sync_i2c(&reg[i++], 0x0341, vts & 0xff);
	set_sync_i2c(&reg[i++], 0x0340, vts >> 8);
	set_sync_i2c(&reg[i++], 0x0205, again_reg);

	set_sync_i2c(&reg[i++], 0x20E, dgain_reg >> 8);
	set_sync_i2c(&reg[i++], 0x20F, dgain_reg & 0xff);

	set_sync_i2c(&reg[i++], 0x210, dgain_reg >> 8);
	set_sync_i2c(&reg[i++], 0x211, dgain_reg & 0xff);

	set_sync_i2c(&reg[i++], 0x212, dgain_reg >> 8);
	set_sync_i2c(&reg[i++], 0x213, dgain_reg & 0xff);

	set_sync_i2c(&reg[i++], 0x214, dgain_reg >> 8);
	set_sync_i2c(&reg[i++], 0x215, dgain_reg & 0xff);

	set_sync_i2c(&reg[i++], 0x0104, 0);
	regs->num = i;

	return RTS_ISP_OK;
}

static int imx175_set_focus(uint32_t isp_id, uint32_t position)
{
	int value;
	struct rts_isp_i2c_regs regs;
	static struct rts_isp_i2c_reg *reg;
	static const struct rts_isp_i2c_info info = {
		.i2c_id = 0xc,
		.addr_len = 1,
		.data_len = 1,
	};

	value = position << 4;
	reg = g_imx175_i2c_focus_regs;
	reg[4].addr = value >> 8;
	reg[4].data = value & 0xff;

	regs.num = 5;
	regs.regs = reg;
	regs.udelay = 0;

	return rts_isp_write_i2c_regs(&info, &regs);
}

static int imx175_check(uint32_t isp_id)
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

	if (id == 0x0175)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops imx175_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "imx175",
	.get_info = imx175_get_info,
	.get_init_info = imx175_get_init_info,
	.start = imx175_start,
	.get_tuned_again = imx175_get_tuned_again,
	.get_tuned_dgain = imx175_get_tuned_dgain,
	.get_exposure_gain_info = imx175_get_exposure_gain_info,
	.set_focus = imx175_set_focus,
	.check = imx175_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(imx175_ops)
