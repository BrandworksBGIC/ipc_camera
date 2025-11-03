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

struct ov9732_status {
	float exp_step;
	float last_exposure;
	int min_vts;
	struct rts_isp_i2c_reg regs1[2];
};

static struct ov9732_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_ov9732_fps_info[] = {
	{30, 1496, 36000000},
};

static struct rts_isp_i2c_reg g_ov9732_i2c_init_regs[] = {
	{0x0103, 0x01}, {0x0100, 0x00}, {0x3001, 0x00}, {0x3002, 0x00},
	{0x3007, 0x1f}, {0x3008, 0xff}, {0x3009, 0x02}, {0x3010, 0x00},
	{0x3011, 0x08}, {0x3014, 0x22}, {0x301e, 0x15}, {0x3030, 0x19},
	{0x3080, 0x02}, {0x3081, 0x3c}, {0x3082, 0x04}, {0x3083, 0x00},
	{0x3084, 0x02}, {0x3085, 0x01}, {0x3086, 0x01}, {0x3089, 0x01},
	{0x308a, 0x00}, {0x3103, 0x01}, {0x3600, 0xf6}, {0x3601, 0x72},
	{0x3605, 0x66}, {0x3610, 0x0c}, {0x3611, 0x60}, {0x3612, 0x35},
	{0x3654, 0x10}, {0x3655, 0x77}, {0x3656, 0x77}, {0x3657, 0x07},
	{0x3658, 0x22}, {0x3659, 0x22}, {0x365a, 0x02}, {0x3700, 0x1f},
	{0x3701, 0x10}, {0x3702, 0x0c}, {0x3703, 0x0b}, {0x3704, 0x3c},
	{0x3705, 0x51}, {0x370d, 0x20}, {0x3710, 0x0d}, {0x3782, 0x58},
	{0x3783, 0x60}, {0x3784, 0x05}, {0x3785, 0x55}, {0x37c0, 0x07},
	{0x3800, 0x00}, {0x3801, 0x04}, {0x3802, 0x00}, {0x3803, 0x04},
	{0x3804, 0x05}, {0x3805, 0x0b}, {0x3806, 0x02}, {0x3807, 0xdb},
	{0x3808, 0x05}, {0x3809, 0x04}, {0x380a, 0x02}, {0x380b, 0xd4},
	{0x380c, 0x05}, {0x380d, 0xc6}, {0x380e, 0x03}, {0x380f, 0x22},
	{0x3810, 0x00}, {0x3811, 0x04}, {0x3812, 0x00}, {0x3813, 0x04},
	{0x3816, 0x00}, {0x3817, 0x00}, {0x3818, 0x00}, {0x3819, 0x04},
	{0x3820, 0x10}, {0x3821, 0x00}, {0x382c, 0x06}, {0x3500, 0x00},
	{0x3501, 0x31}, {0x3502, 0x00}, {0x3503, 0x03}, {0x3504, 0x00},
	{0x3505, 0x00}, {0x3509, 0x10}, {0x350a, 0x00}, {0x350b, 0x40},
	{0x3d00, 0x00}, {0x3d01, 0x00}, {0x3d02, 0x00}, {0x3d03, 0x00},
	{0x3d04, 0x00}, {0x3d05, 0x00}, {0x3d06, 0x00}, {0x3d07, 0x00},
	{0x3d08, 0x00}, {0x3d09, 0x00}, {0x3d0a, 0x00}, {0x3d0b, 0x00},
	{0x3d0c, 0x00}, {0x3d0d, 0x00}, {0x3d0e, 0x00}, {0x3d0f, 0x00},
	{0x3d80, 0x00}, {0x3d81, 0x00}, {0x3d82, 0x38}, {0x3d83, 0xa4},
	{0x3d84, 0x00}, {0x3d85, 0x00}, {0x3d86, 0x1f}, {0x3d87, 0x03},
	{0x3d8b, 0x00}, {0x3d8f, 0x00}, {0x4001, 0xe0},
	// BLC level 0x4002 ~ 0x4003
	{0x4002, 0x00}, {0x4003, 0x08}, {0x4004, 0x00}, {0x4005, 0x02},
	{0x4006, 0x01}, {0x4007, 0x40}, {0x4009, 0x0b}, {0x4300, 0x03},
	{0x4301, 0xff}, {0x4304, 0x00}, {0x4305, 0x00}, {0x4309, 0x00},
	{0x4600, 0x00}, {0x4601, 0x04}, {0x4800, 0x00}, {0x4805, 0x00},
	{0x4821, 0x50}, {0x4823, 0x50}, {0x4837, 0x2d}, {0x4a00, 0x00},
	{0x4f00, 0x80}, {0x4f01, 0x10}, {0x4f02, 0x00}, {0x4f03, 0x00},
	{0x4f04, 0x00}, {0x4f05, 0x00}, {0x4f06, 0x00}, {0x4f07, 0x00},
	{0x4f08, 0x00}, {0x4f09, 0x00}, {0x5000, 0x13}, {0x500c, 0x00},
	{0x500d, 0x00}, {0x500e, 0x00}, {0x500f, 0x00}, {0x5010, 0x00},
	{0x5011, 0x00}, {0x5012, 0x00}, {0x5013, 0x00}, {0x5014, 0x00},
	{0x5015, 0x00}, {0x5016, 0x00}, {0x5017, 0x00}, {0x5080, 0x00},
	{0x5180, 0x01}, {0x5181, 0x00}, {0x5182, 0x01}, {0x5183, 0x00},
	{0x5184, 0x01}, {0x5185, 0x00}, {0x5708, 0x06}, {0x5781, 0x0e},
	{0x5783, 0x0f}, {0x0100, 0x01},
};

static int ov9732_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 1280;
	info->modes.mode[0].size.h = 720;
	info->modes.mode[0].fps = g_ov9732_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x36;
	info->i2c.addr_len = 2;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 10);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_3V3, 10);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V8, 1000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 1000);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 1000);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_RST_GPIO, 0, 0);
	set_power_item(&down->items[i++], SNR_HCLK, 0, 0);
	set_power_item(&down->items[i++], SNR_CORE_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_IO_POWER, 0, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static const struct fps_info *ov9732_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_ov9732_fps_info); i++)
		if (fps == g_ov9732_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_ov9732_fps_info))
		return NULL;

	return &g_ov9732_fps_info[i];
}

static int ov9732_get_init_info(uint32_t isp_id,
				const struct rts_isp_sensor_mode *mode,
				struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct ov9732_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("ov9732 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = ov9732_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	set_init_i2c(&status->regs1[0], 0x380d, fps_info->hts & 0xff);
	set_init_i2c(&status->regs1[1], 0x380c, fps_info->hts >> 8);

	set_init_i2c_regs(info->sensor_regs[0], g_ov9732_i2c_init_regs, 0);
	set_init_i2c_regs(info->sensor_regs[1], status->regs1, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0;
	info->interface.mipi.hs_term = 0x3;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 1280;
	info->size.h = 721;
	info->start.x = 0;
	info->start.y = 1;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = 802;
	info->max_vts = 65536;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */
	status->min_vts = info->min_vts;

	return RTS_ISP_OK;
}

static int ov9732_start(uint32_t isp_id)
{
	struct ov9732_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static uint16_t get_sensor_gain_reg(float fgain)
{
	uint16_t gain = fgain * 16;

	return gain > 255 ? 255 : gain;
}

static float get_sensor_real_gain(uint16_t reg_value)
{
	return reg_value / 16.0f;
}

static int ov9732_get_tuned_again(uint32_t isp_id,
				  float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int ov9732_get_tuned_dgain(uint32_t isp_id,
				  float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int ov9732_get_exposure_gain_info(uint32_t isp_id,
					 const struct rts_isp_sensor_exp_gain *exp_gain,
					 struct rts_isp_sync_regs *regs)
{

	int i;
	uint16_t gain_reg;
	struct ov9732_status *status;
	struct rts_isp_sync_reg *reg;
	uint32_t vts;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	vts = exp_gain->vts;
	gain_reg = get_sensor_gain_reg(exp_gain->analog_gain[0] *
				       exp_gain->digital_gain[0]);

	reg = regs->reg;

	i = 0;
	if (abs(status->last_exposure - exp_gain->exposure[0]) > 0.001f) {
		uint32_t exposure_rows;

		exposure_rows = exp_gain->exposure[0] / status->exp_step + 0.5f;
		exposure_rows = exposure_rows << 4;
		set_sync_i2c(&reg[i++], 0x3502, exposure_rows & 0xff);
		set_sync_i2c(&reg[i++], 0x3501, exposure_rows >> 8);
		set_sync_i2c(&reg[i++], 0x3500, exposure_rows >> 16);
		status->last_exposure = exp_gain->exposure[0];

	}
	set_sync_i2c(&reg[i++], 0x380f, vts & 0xff);
	set_sync_i2c(&reg[i++], 0x380e, vts >> 8);
	set_sync_i2c(&reg[i++], 0x350a, gain_reg >> 8);
	set_sync_i2c(&reg[i++], 0x350b, gain_reg & 0xff);
	regs->num = i;

	return RTS_ISP_OK;
}

static int ov9732_check(uint32_t isp_id)
{
	int ret;
	int id;
	struct rts_isp_i2c_reg reg = {};

	reg.addr = 0x300a;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id = reg.data << 8;

	reg.addr = 0x300b;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id |= reg.data;

	if (id == 0x9732)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops ov9732_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "ov9732",
	.get_info = ov9732_get_info,
	.get_init_info = ov9732_get_init_info,
	.start = ov9732_start,
	.get_tuned_again = ov9732_get_tuned_again,
	.get_tuned_dgain = ov9732_get_tuned_dgain,
	.get_exposure_gain_info = ov9732_get_exposure_gain_info,
	.check = ov9732_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(ov9732_ops)
