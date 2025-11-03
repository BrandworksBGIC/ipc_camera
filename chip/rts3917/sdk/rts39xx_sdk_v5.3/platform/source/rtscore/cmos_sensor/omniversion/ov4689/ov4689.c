/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2017 Grant Shen <grant_shen@realsil.com.cn>
 * Copyright (C) 2019 Sherry Cheng <sherry_cheng@realsil.com.cn>
 * Copyright (C) 2020 Yinna Liu <yinna_liu@realsil.com.cn>
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

struct ov4689_status {
	int min_vts;
	float exp_step;
	float last_exposure;
};

static struct ov4689_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_ov4689_fps_info[] = {
	{30, 4324, 201600000},
};

static struct rts_isp_i2c_reg g_ov4689_i2c_init_regs[] = {
	{0x0103, 0x01},
	{0x3638, 0x00},
	{0x0300, 0x00},
	{0x0302, 0x15},
	{0x0303, 0x00},
	{0x0304, 0x03},
	{0x030b, 0x00},
	{0x030d, 0x1e},
	{0x030e, 0x04},
	{0x030f, 0x01},
	{0x0312, 0x01},
	{0x031e, 0x00},
	{0x3000, 0x20},
	{0x3002, 0x00},
	{0x3018, 0x72},
	{0x3020, 0x93},
	{0x3021, 0x03},
	{0x3022, 0x01},
	{0x3031, 0x0a},
	{0x303f, 0x0c},
	{0x3305, 0xf1},
	{0x3307, 0x04},
	{0x3309, 0x29},
	{0x3500, 0x00},
	{0x3501, 0x60},
	{0x3502, 0x00},
	{0x3503, 0x04},
	{0x3504, 0x00},
	{0x3505, 0x00},
	{0x3506, 0x00},
	{0x3507, 0x00},
	{0x3508, 0x00},
	{0x3509, 0x80},
	{0x350a, 0x00},
	{0x350b, 0x00},
	{0x350c, 0x00},
	{0x350d, 0x00},
	{0x350e, 0x00},
	{0x350f, 0x80},
	{0x3510, 0x00},
	{0x3511, 0x00},
	{0x3512, 0x00},
	{0x3513, 0x00},
	{0x3514, 0x00},
	{0x3515, 0x80},
	{0x3516, 0x00},
	{0x3517, 0x00},
	{0x3518, 0x00},
	{0x3519, 0x00},
	{0x351a, 0x00},
	{0x351b, 0x80},
	{0x351c, 0x00},
	{0x351d, 0x00},
	{0x351e, 0x00},
	{0x351f, 0x00},
	{0x3520, 0x00},
	{0x3521, 0x80},
	{0x3522, 0x08},
	{0x3524, 0x08},
	{0x3526, 0x08},
	{0x3528, 0x08},
	{0x352a, 0x08},
	{0x3602, 0x00},
	{0x3603, 0x40},
	{0x3604, 0x02},
	{0x3605, 0x00},
	{0x3606, 0x00},
	{0x3607, 0x00},
	{0x3609, 0x12},
	{0x360a, 0x40},
	{0x360c, 0x08},
	{0x360f, 0xe5},
	{0x3608, 0x8f},
	{0x3611, 0x00},
	{0x3613, 0xf7},
	{0x3616, 0x58},
	{0x3619, 0x99},
	{0x361b, 0x60},
	{0x361c, 0x7a},
	{0x361e, 0x79},
	{0x361f, 0x02},
	{0x3632, 0x00},
	{0x3633, 0x10},
	{0x3634, 0x10},
	{0x3635, 0x10},
	{0x3636, 0x15},
	{0x3646, 0x86},
	{0x364a, 0x0b},
	{0x3700, 0x17},
	{0x3701, 0x22},
	{0x3703, 0x10},
	{0x370a, 0x37},
	{0x3705, 0x00},
	{0x3706, 0x63},
	{0x3709, 0x3c},
	{0x370b, 0x01},
	{0x370c, 0x30},
	{0x3710, 0x24},
	{0x3711, 0x0c},
	{0x3716, 0x00},
	{0x3720, 0x28},
	{0x3729, 0x7b},
	{0x372a, 0x84},
	{0x372b, 0xbd},
	{0x372c, 0xbc},
	{0x372e, 0x52},
	{0x373c, 0x0e},
	{0x373e, 0x33},
	{0x3743, 0x10},
	{0x3744, 0x88},
	{0x3745, 0xc0},
	{0x374a, 0x43},
	{0x374c, 0x00},
	{0x374e, 0x23},
	{0x3751, 0x7b},
	{0x3752, 0x84},
	{0x3753, 0xbd},
	{0x3754, 0xbc},
	{0x3756, 0x52},
	{0x375c, 0x00},
	{0x3760, 0x00},
	{0x3761, 0x00},
	{0x3762, 0x00},
	{0x3763, 0x00},
	{0x3764, 0x00},
	{0x3767, 0x04},
	{0x3768, 0x04},
	{0x3769, 0x08},
	{0x376a, 0x08},
	{0x376b, 0x20},
	{0x376c, 0x00},
	{0x376d, 0x00},
	{0x376e, 0x00},
	{0x3773, 0x00},
	{0x3774, 0x51},
	{0x3776, 0xbd},
	{0x3777, 0xbd},
	{0x3781, 0x18},
	{0x3783, 0x25},
	{0x3798, 0x1b},
	{0x3800, 0x00},
	{0x3801, 0x08},
	{0x3802, 0x00},
	{0x3803, 0x04},
	{0x3804, 0x0a},
	{0x3805, 0x97},
	{0x3806, 0x05},
	{0x3807, 0xfb},
	{0x3808, 0x0a},
	{0x3809, 0x84},
	{0x380a, 0x05},
	{0x380b, 0xf4},
	{0x380c, 0x0a},
	{0x380d, 0x18},
	{0x380e, 0x06},
	{0x380f, 0x12},
	{0x3810, 0x00},
	{0x3811, 0x08},
	{0x3812, 0x00},
	{0x3813, 0x04},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3819, 0x01},
	{0x3820, 0x00},
	{0x3821, 0x06},
	{0x3829, 0x00},
	{0x382a, 0x01},
	{0x382b, 0x01},
	{0x382d, 0x7f},
	{0x3830, 0x04},
	{0x3836, 0x01},
	{0x3837, 0x00},
	{0x3841, 0x02},
	{0x3846, 0x08},
	{0x3847, 0x07},
	{0x3d85, 0x36},
	{0x3d8c, 0x71},
	{0x3d8d, 0xcb},
	{0x3f0a, 0x00},
	{0x4000, 0xf1},
	{0x4001, 0x40},
	{0x4002, 0x04},
	{0x4003, 0x14},
	{0x400e, 0x00},
	{0x4011, 0x00},
	{0x401a, 0x00},
	{0x401b, 0x00},
	{0x401c, 0x00},
	{0x401d, 0x00},
	{0x401f, 0x00},
	{0x4020, 0x00},
	{0x4021, 0x10},
	{0x4022, 0x07},
	{0x4023, 0xcf},
	{0x4024, 0x09},
	{0x4025, 0x60},
	{0x4026, 0x09},
	{0x4027, 0x6f},
	{0x4028, 0x00},
	{0x4029, 0x02},
	{0x402a, 0x06},
	{0x402b, 0x04},
	{0x402c, 0x02},
	{0x402d, 0x02},
	{0x402e, 0x0e},
	{0x402f, 0x04},
	{0x4302, 0xff},
	{0x4303, 0xff},
	{0x4304, 0x00},
	{0x4305, 0x00},
	{0x4306, 0x00},
	{0x4308, 0x02},
	{0x4500, 0x6c},
	{0x4501, 0xc4},
	{0x4502, 0x40},
	{0x4503, 0x01},
	{0x4601, 0xA7},
	{0x4800, 0x04},
	{0x4813, 0x08},
	{0x481f, 0x40},
	{0x4829, 0x78},
	{0x4837, 0x10},
	{0x4b00, 0x2a},
	{0x4b0d, 0x00},
	{0x4d00, 0x04},
	{0x4d01, 0x42},
	{0x4d02, 0xd1},
	{0x4d03, 0x93},
	{0x4d04, 0xf5},
	{0x4d05, 0xc1},
	{0x5000, 0xf3},
	{0x5001, 0x11},
	{0x5004, 0x00},
	{0x500a, 0x00},
	{0x500b, 0x00},
	{0x5032, 0x00},
	{0x5040, 0x00},
	{0x5050, 0x0c},
	{0x5500, 0x00},
	{0x5501, 0x10},
	{0x5502, 0x01},
	{0x5503, 0x0f},
	{0x8000, 0x00},
	{0x8001, 0x00},
	{0x8002, 0x00},
	{0x8003, 0x00},
	{0x8004, 0x00},
	{0x8005, 0x00},
	{0x8006, 0x00},
	{0x8007, 0x00},
	{0x8008, 0x00},
	{0x3638, 0x00},
	{0x0100, 0x01},
	{0x0300, 0x60},

};

static int ov4689_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 2688;
	info->modes.mode[0].size.h = 1520;
	info->modes.mode[0].fps = g_ov4689_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x36;
	info->i2c.addr_len = 2;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 0);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 10);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_HIGH, 0);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 100);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 100);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V2, 100);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 1000);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_HCLK, 0, 1000);
	set_power_item(&down->items[i++], SNR_RST_GPIO, 0, 0);
	set_power_item(&down->items[i++], SNR_CORE_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_IO_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_PWDN_GPIO, 0, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static const struct fps_info *ov4689_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_ov4689_fps_info); i++)
		if (fps == g_ov4689_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_ov4689_fps_info))
		return NULL;

	return &g_ov4689_fps_info[i];
}

static int ov4689_get_init_info(uint32_t isp_id,
				const struct rts_isp_sensor_mode *mode,
				struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct ov4689_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("ov4689 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = ov4689_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->hts);

	set_init_i2c_regs(info->sensor_regs[0], g_ov4689_i2c_init_regs, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = (MIPI_LANE0 | MIPI_LANE1 |
				      MIPI_LANE2 | MIPI_LANE3);
	info->interface.mipi.hs_term = 0x6;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 2696;
	info->size.h = 1528;
	info->start.x = 4;
	info->start.y = 5;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = 1554;
	info->max_vts = 65536;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */
	status->min_vts = info->min_vts;

	return RTS_ISP_OK;
}

static int ov4689_start(uint32_t isp_id)
{
	struct ov4689_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static uint16_t get_sensor_gain_reg(float fgain)
{
	uint16_t gain;

	fgain = fgain > 16 ? 16 : fgain;
	gain = ((int)fgain << 7) | (int)((fgain - (int)fgain) * 128);
	return gain;
}

static float get_sensor_real_gain(uint16_t reg_value)
{
	float fgain;

	fgain = (float)(reg_value >> 7) + (reg_value & 0x7f) / 128.0f;
	return fgain;
}

static int ov4689_get_tuned_again(uint32_t isp_id,
				  float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int ov4689_get_tuned_dgain(uint32_t isp_id,
				  float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int ov4689_get_exposure_gain_info(uint32_t isp_id,
					 const struct rts_isp_sensor_exp_gain *exp_gain,
					 struct rts_isp_sync_regs *regs)
{
	int i;
	uint16_t gain_reg;
	uint32_t vts;
	struct ov4689_status *status;
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
		set_sync_i2c(&reg[i++], 0x3508, (gain_reg & 0700) >> 8);
		set_sync_i2c(&reg[i++], 0x3509, gain_reg & 0xff);
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
		set_sync_i2c(&reg[i++], 0x3508, (gain_reg & 0700) >> 8);
		set_sync_i2c(&reg[i++], 0x3509, gain_reg & 0xff);
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

static int ov4689_check(uint32_t isp_id)
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

	if (id == 0x4688)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops ov4689_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "ov4689",
	.get_info = ov4689_get_info,
	.get_init_info = ov4689_get_init_info,
	.start = ov4689_start,
	.get_tuned_again = ov4689_get_tuned_again,
	.get_tuned_dgain = ov4689_get_tuned_dgain,
	.get_exposure_gain_info = ov4689_get_exposure_gain_info,
	.check = ov4689_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(ov4689_ops)
