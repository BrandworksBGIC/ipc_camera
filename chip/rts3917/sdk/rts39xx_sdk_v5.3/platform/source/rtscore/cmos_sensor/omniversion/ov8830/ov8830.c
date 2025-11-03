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
	uint32_t hts;
	uint32_t clk;
};

struct ov8830_status {
	int min_vts;
	float exp_step;
	float last_exposure;
};

static struct ov8830_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_ov8830_fps_info[] = {
	{15, 3608, 136000000},
};

static struct rts_isp_i2c_reg g_ov8830_i2c_init_regs[] = {
	{0x0100, 0x00}, {0x0102, 0x01}, {0x3001, 0x2a}, {0x3002, 0x88},
	{0x3005, 0x00}, {0x3011, 0x41}, {0x3015, 0x08}, {0x301b, 0xb4},
	{0x301d, 0x02}, {0x3021, 0x00}, {0x3022, 0x00}, {0x3081, 0x02},
	{0x3083, 0x01}, {0x3090, 0x01}, {0x3091, 0x12}, {0x3094, 0x00},
	{0x3092, 0x00}, {0x3093, 0x00}, {0x3098, 0x02}, {0x3099, 0x1e},
	{0x309a, 0x00}, {0x309b, 0x00}, {0x30a2, 0x01}, {0x30b0, 0x05},
	{0x30b2, 0x00}, {0x30b3, 0x4b}, {0x30b4, 0x04}, {0x30b5, 0x04},
	{0x30b6, 0x01}, {0x3104, 0xa1}, {0x3106, 0x01}, {0x3400, 0x04},
	{0x3401, 0x00}, {0x3402, 0x04}, {0x3403, 0x00}, {0x3404, 0x04},
	{0x3405, 0x00}, {0x3406, 0x01}, {0x3500, 0x00}, {0x3501, 0x9a},
	{0x3502, 0x80}, {0x3503, 0x07}, {0x3504, 0x00}, {0x3505, 0x30},
	{0x3506, 0x00}, {0x3507, 0x10}, {0x3508, 0x80}, {0x3509, 0x10},
	{0x350a, 0x00}, {0x350b, 0x38}, {0x3600, 0x78}, {0x3601, 0x0a},
	{0x3602, 0x9c}, {0x3604, 0x38}, {0x3612, 0x00}, {0x3620, 0x64},
	{0x3621, 0xb5}, {0x3622, 0x03}, {0x3625, 0x64}, {0x3630, 0x55},
	{0x3631, 0xd2}, {0x3632, 0x00}, {0x3633, 0x34}, {0x3634, 0x03},
	{0x364d, 0x00}, {0x364f, 0x00}, {0x3660, 0x80}, {0x3662, 0x10},
	{0x3665, 0x00}, {0x3666, 0x00}, {0x3667, 0x00}, {0x366a, 0x80},
	{0x366c, 0x00}, {0x366d, 0x00}, {0x366e, 0x00}, {0x366f, 0x20},
	{0x3680, 0xe0}, {0x3681, 0x00}, {0x3701, 0x14}, {0x3702, 0xbf},
	{0x3703, 0x8c}, {0x3704, 0x78}, {0x3705, 0x02}, {0x3708, 0xe3},
	{0x3709, 0xc3}, {0x370a, 0x00}, {0x370b, 0x20}, {0x370c, 0x0c},
	{0x370d, 0x11}, {0x370e, 0x00}, {0x370f, 0x00}, {0x3710, 0x00},
	{0x371c, 0x01}, {0x371f, 0x0c}, {0x3721, 0x00}, {0x3724, 0x10},
	{0x3726, 0x00}, {0x372a, 0x01}, {0x3730, 0x18}, {0x3738, 0x22},
	{0x3739, 0x08}, {0x373a, 0x51}, {0x373b, 0x02}, {0x373c, 0x20},
	{0x373f, 0x02}, {0x3740, 0x42}, {0x3741, 0x02}, {0x3742, 0x18},
	{0x3743, 0x01}, {0x3744, 0x02}, {0x3747, 0x10}, {0x374c, 0x04},
	{0x3751, 0xf0}, {0x3752, 0x00}, {0x3753, 0x00}, {0x3754, 0xc0},
	{0x3755, 0x00}, {0x3756, 0x1a}, {0x3758, 0x00}, {0x3759, 0x0f},
	{0x375c, 0x04}, {0x3767, 0x01}, {0x376b, 0x44}, {0x3774, 0x10},
	{0x3776, 0x00}, {0x377f, 0x08}, {0x3780, 0x22}, {0x3781, 0x0c},
	{0x3784, 0x2c}, {0x3785, 0x1e}, {0x3786, 0x16}, {0x378f, 0xf5},
	{0x3791, 0xb0}, {0x3795, 0x00}, {0x3796, 0x64}, {0x3797, 0x11},
	{0x3798, 0x30}, {0x3799, 0x41}, {0x379a, 0x07}, {0x379b, 0xb0},
	{0x379c, 0x0c}, {0x37c5, 0x00}, {0x37c6, 0xa0}, {0x37c7, 0x00},
	{0x37c9, 0x00}, {0x37ca, 0x00}, {0x37cb, 0x00}, {0x37cc, 0x00},
	{0x37cd, 0x00}, {0x37ce, 0x01}, {0x37cf, 0x00}, {0x37d1, 0x01},
	{0x37de, 0x00}, {0x37df, 0x00}, {0x3800, 0x00}, {0x3801, 0x0c},
	{0x3802, 0x00}, {0x3803, 0x0c}, {0x3804, 0x0c}, {0x3805, 0xd3},
	{0x3806, 0x09}, {0x3807, 0xa3}, {0x3808, 0x0c}, {0x3809, 0xc4},
	{0x380a, 0x09}, {0x380b, 0x94}, {0x380c, 0x0e}, {0x380d, 0x18},
	{0x380e, 0x09}, {0x380f, 0xb4}, {0x3810, 0x00}, {0x3811, 0x02},
	{0x3812, 0x00}, {0x3813, 0x02}, {0x3814, 0x11}, {0x3815, 0x11},
	{0x3820, 0x10}, {0x3821, 0x0e}, {0x3823, 0x00}, {0x3824, 0x00},
	{0x3825, 0x00}, {0x3826, 0x00}, {0x3827, 0x00}, {0x382a, 0x04},
	{0x3a04, 0x09}, {0x3a05, 0xa9}, {0x3a06, 0x00}, {0x3a07, 0xf8},
	{0x3b00, 0x00}, {0x3b02, 0x00}, {0x3b03, 0x00}, {0x3b04, 0x00},
	{0x3b05, 0x00}, {0x3d00, 0x00}, {0x3d01, 0x00}, {0x3d02, 0x00},
	{0x3d03, 0x00}, {0x3d04, 0x00}, {0x3d05, 0x00}, {0x3d06, 0x00},
	{0x3d07, 0x00}, {0x3d08, 0x00}, {0x3d09, 0x00}, {0x3d0a, 0x00},
	{0x3d0b, 0x00}, {0x3d0c, 0x00}, {0x3d0d, 0x00}, {0x3d0e, 0x00},
	{0x3d0f, 0x00}, {0x3d80, 0x00}, {0x3d81, 0x00}, {0x3d84, 0x00},
	{0x4000, 0x18}, {0x4001, 0x04}, {0x4002, 0x45}, {0x4004, 0x08},
	{0x4005, 0x18}, {0x4006, 0x20}, {0x4008, 0x20}, {0x4009, 0x10},
	{0x404f, 0xf0}, {0x4100, 0x10}, {0x4101, 0x12}, {0x4102, 0x24},
	{0x4103, 0x00}, {0x4104, 0x5b}, {0x4307, 0x30}, {0x4315, 0x00},
	{0x4511, 0x05}, {0x4512, 0x01}, {0x4805, 0x21}, {0x4806, 0x00},
	{0x481f, 0x36}, {0x4831, 0x6c}, {0x4837, 0x0b}, {0x4a00, 0xaa},
	{0x4a03, 0x01}, {0x4a05, 0x08}, {0x4a0a, 0x88}, {0x4d03, 0xbb},
	{0x5000, 0x06}, {0x5001, 0x01}, {0x5002, 0x80}, {0x5003, 0x20},
	{0x5013, 0x00}, {0x5046, 0x4a}, {0x5780, 0x1c}, {0x5786, 0x20},
	{0x5787, 0x10}, {0x5788, 0x18}, {0x578a, 0x04}, {0x578b, 0x02},
	{0x578c, 0x02}, {0x578e, 0x06}, {0x578f, 0x02}, {0x5790, 0x02},
	{0x5791, 0xff}, {0x5a08, 0x02}, {0x5e00, 0x00}, {0x5e10, 0x0c},
};

static struct rts_isp_i2c_reg g_ov8830_i2c_init_2nd_regs[] = {
	{0x3011, 0x41}, {0x3015, 0x08}, {0x3090, 0x03}, {0x3091, 0x22},
	{0x3092, 0x01}, {0x3093, 0x00}, {0x30b3, 0x3c}, {0x30b4, 0x04},
	{0x30b5, 0x04}, {0x30b6, 0x01}, {0x3500, 0x00}, {0x3501, 0x9c},
	{0x3502, 0x80}, {0x380F, 0xD0}, {0x4837, 0x0d}, {0x0100, 0x01},
};

static int ov8830_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 3072;
	info->modes.mode[0].size.h = 2048;
	info->modes.mode[0].fps = g_ov8830_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x36;
	info->i2c.addr_len = 2;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_HIGH, 0);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 20000);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 0);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_3V0, 2000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 0);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 10000);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_RST_GPIO, 0, 0);
	set_power_item(&down->items[i++], SNR_HCLK, 0, 0);
	set_power_item(&down->items[i++], SNR_IO_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, 0, 0);
	down->num = i;

	info->focus.min = 100;
	info->focus.max = 747;

	return RTS_ISP_OK;
}

static const struct fps_info *ov8830_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_ov8830_fps_info); i++)
		if (fps == g_ov8830_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_ov8830_fps_info))
		return NULL;

	return &g_ov8830_fps_info[i];
}

static int ov8830_get_init_info(uint32_t isp_id,
				const struct rts_isp_sensor_mode *mode,
				struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct ov8830_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("ov8830 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = ov8830_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, clk_div: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->clk_div, fps_info->hts);

	set_init_i2c_regs(info->sensor_regs[0], g_ov8830_i2c_init_regs, 0);
	set_init_i2c_regs(info->sensor_regs[1], g_ov8830_i2c_init_2nd_regs, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = (MIPI_LANE0 | MIPI_LANE1 |
				      MIPI_LANE2 | MIPI_LANE3);
	info->interface.mipi.hs_term = 0x5;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 3268;
	info->size.h = 2452;
	info->start.x = 98;
	info->start.y = 203;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = 2512;
	info->max_vts = 65536;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */
	status->min_vts = info->min_vts;

	return RTS_ISP_OK;
}

static int ov8830_start(uint32_t isp_id)
{
	struct ov8830_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static uint8_t get_sensor_gain_reg(float fgain)
{
	uint16_t gain = fgain * 16;

	return gain > 255 ? 255 : gain;
}

static float get_sensor_real_gain(uint8_t reg_value)
{
	return reg_value / 16.0f;
}

static int ov8830_get_tuned_again(uint32_t isp_id,
				  float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int ov8830_get_tuned_dgain(uint32_t isp_id,
				  float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int ov8830_get_exposure_gain_info(uint32_t isp_id,
					 const struct rts_isp_sensor_exp_gain *exp_gain,
					 struct rts_isp_sync_regs *regs)
{
	int i;
	uint8_t gain_reg;
	uint32_t vts;
	struct ov8830_status *status;
	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	vts = exp_gain->vts;
	gain_reg = get_sensor_gain_reg(exp_gain->analog_gain[0] *
				       exp_gain->digital_gain[0]);
	reg = regs->reg;

	i = 0;
	if (abs(status->last_exposure - exp_gain->exposure[0]) > 0.001f) {
		uint32_t exposure_reg;

		exposure_reg = exp_gain->exposure[0] * 16 / status->exp_step;
		if (exposure_reg > 16) {
			uint8_t tmp = exposure_reg & 0xf;

			exposure_reg = exposure_reg & ~0xf;
			if (tmp >= 8)
				exposure_reg += 0x10;
		}
		/* group */
		set_sync_i2c(&reg[i++], 0x3208, 0x01);
		/* exposure */
		set_sync_i2c(&reg[i++], 0x3500, exposure_reg >> 16);
		set_sync_i2c(&reg[i++], 0x3501, (exposure_reg & 0xff00) >> 8);
		set_sync_i2c(&reg[i++], 0x3502, exposure_reg & 0xff);
		status->last_exposure = exp_gain->exposure[0];
		/* gain */
		set_sync_i2c(&reg[i++], 0x350b, gain_reg);
		/* end group */
		set_sync_i2c(&reg[i++], 0x3208, 0x11);
		set_sync_i2c(&reg[i++], 0x3208, 0xA1);

		set_sync_info(&reg[i++], 1, RTS_ISP_INT_DATA_START);

		/* group */
		set_sync_i2c(&reg[i++], 0x3208, 0x00);
		/* dummy */
		set_sync_i2c(&reg[i++], 0x380e, vts >> 8);
		set_sync_i2c(&reg[i++], 0x380f, vts & 0xff);
		/* end group */
		set_sync_i2c(&reg[i++], 0x3208, 0x10);
		set_sync_i2c(&reg[i++], 0x3208, 0xA0);
	} else {
		/* group */
		set_sync_i2c(&reg[i++], 0x3208, 0x00);
		/* gain */
		set_sync_i2c(&reg[i++], 0x350b, gain_reg);
		/* dummy */
		set_sync_i2c(&reg[i++], 0x380e, vts >> 8);
		set_sync_i2c(&reg[i++], 0x380f, vts & 0xff);
		/* end group */
		set_sync_i2c(&reg[i++], 0x3208, 0x10);
		set_sync_i2c(&reg[i++], 0x3208, 0xA0);
	}
	regs->num = i;

	return RTS_ISP_OK;
}

static int ov8830_set_focus(uint32_t isp_id, uint32_t position)
{
	int value;
	struct rts_isp_i2c_reg reg[1];
	static const struct rts_isp_i2c_info info = {
		.i2c_id = 0xc,
		.addr_len = 1,
		.data_len = 1,
	};

	value = position << 4;
	reg[0].addr = value >> 8;
	reg[0].data = value & 0xff;

	return rts_isp_write_i2c_reg(&info, reg);
}

static int ov8830_check(uint32_t isp_id)
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

	if (id == 0x8830)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops ov8830_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "ov8830",
	.get_info = ov8830_get_info,
	.get_init_info = ov8830_get_init_info,
	.start = ov8830_start,
	.get_tuned_again = ov8830_get_tuned_again,
	.get_tuned_dgain = ov8830_get_tuned_dgain,
	.get_exposure_gain_info = ov8830_get_exposure_gain_info,
	.set_focus = ov8830_set_focus,
	.check = ov8830_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(ov8830_ops)
