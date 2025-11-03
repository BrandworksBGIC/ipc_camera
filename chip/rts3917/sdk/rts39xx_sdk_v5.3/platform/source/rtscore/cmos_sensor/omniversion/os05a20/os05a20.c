/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2017 Grant Shen <grant_shen@realsil.com.cn>
 * Copyright (C) 2019 Sherry Cheng <sherry_cheng@realsil.com.cn>
 *.Copyright (C) 2020 Bob Yin <bob_yin@realsil.com.cn>
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

struct os05a20_status {
	int min_vts;
	float exp_step;
	float last_exposure;
};

static struct os05a20_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_os05a20_fps_info[] = {
	{30, 2880, 191980800},
};

static struct rts_isp_i2c_reg g_os05a20_i2c_init_regs[] = {
	{0x0100, 0x00},
	{0x0103, 0x01},
	{0x0303, 0x01},
	{0x0305, 0x3e},
	{0x0306, 0x00},
	{0x0307, 0x00},
	{0x0308, 0x03},
	{0x0309, 0x04},
	{0x030c, 0x01},
	{0x0322, 0x01},
	{0x032a, 0x00},
	{0x031e, 0x09},
	{0x0325, 0x40},
	{0x0328, 0x07},
	{0x300d, 0x11},
	{0x300e, 0x11},
	{0x300f, 0x11},
	{0x3026, 0x00},
	{0x3027, 0x00},
	{0x3010, 0x01},
	{0x3012, 0x41},
	{0x3016, 0xf0},
	{0x3018, 0xf0},
	{0x3028, 0xf0},
	{0x301e, 0x98},
	{0x3010, 0x01},
	{0x3011, 0x04},
	{0x3031, 0xa9},
	{0x3103, 0x48},
	{0x3104, 0x01},
	{0x3106, 0x10},
	{0x3400, 0x04},
	{0x3025, 0x03},
	{0x3425, 0x01},
	{0x3428, 0x01},
	{0x3406, 0x08},
	{0x3408, 0x03},
	{0x3501, 0x08},
	{0x3502, 0xa0},
	{0x3505, 0x83},
	{0x3508, 0x00},
	{0x3509, 0x80},
	{0x350a, 0x04},
	{0x350b, 0x00},
	{0x350c, 0x00},
	{0x350d, 0x80},
	{0x350e, 0x04},
	{0x350f, 0x00},
	{0x3600, 0x00},
	{0x3626, 0xff},
	{0x3605, 0x50},
	{0x3609, 0xb5},
	{0x3610, 0x69},
	{0x360c, 0x01},
	{0x3628, 0xa4},
	{0x3629, 0x6a},
	{0x362d, 0x10},
	{0x3660, 0x43},
	{0x3661, 0x06},
	{0x3662, 0x00},
	{0x3663, 0x28},
	{0x3664, 0x0d},
	{0x366a, 0x38},
	{0x366b, 0xa0},
	{0x366d, 0x00},
	{0x366e, 0x00},
	{0x3680, 0x00},
	{0x36c0, 0x00},
	{0x3621, 0x81},
	{0x3634, 0x31},
	{0x3620, 0x00},
	{0x3622, 0x00},
	{0x362a, 0xd0},
	{0x362e, 0x8c},
	{0x362f, 0x98},
	{0x3630, 0xb0},
	{0x3631, 0xd7},
	{0x3701, 0x0f},
	{0x3737, 0x02},
	{0x3740, 0x18},
	{0x3741, 0x04},
	{0x373c, 0x0f},
	{0x373b, 0x02},
	{0x3705, 0x00},
	{0x3706, 0x50},
	{0x370a, 0x00},
	{0x370b, 0xe4},
	{0x3709, 0x4a},
	{0x3714, 0x21},
	{0x371c, 0x00},
	{0x371d, 0x08},
	{0x375e, 0x0e},
	{0x3760, 0x13},
	{0x3776, 0x10},
	{0x3781, 0x02},
	{0x3782, 0x04},
	{0x3783, 0x02},
	{0x3784, 0x08},
	{0x3785, 0x08},
	{0x3788, 0x01},
	{0x3789, 0x01},
	{0x3797, 0x04},
	{0x3798, 0x01},
	{0x3799, 0x00},
	{0x3761, 0x02},
	{0x3762, 0x0d},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x0c},
	{0x3804, 0x0e},
	{0x3805, 0xff},
	{0x3806, 0x08},
	{0x3807, 0x6f},
	{0x3808, 0x0a},
	{0x3809, 0x88},
	{0x380a, 0x07},
	{0x380b, 0xa0},
	{0x380c, 0x02},
	{0x380d, 0xd0},
	{0x380e, 0x08},
	{0x380f, 0xae},
	{0x3811, 0x10},
	{0x3813, 0x04},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3816, 0x01},
	{0x3817, 0x01},
	{0x381c, 0x00},
	{0x3820, 0x00},
	{0x3821, 0x04},
	{0x3822, 0x54},
	{0x3823, 0x18},
	{0x3826, 0x00},
	{0x3827, 0x01},
	{0x3833, 0x00},
	{0x3832, 0x02},
	{0x383c, 0x48},
	{0x383d, 0xff},
	{0x3843, 0x20},
	{0x382d, 0x08},
	{0x3d85, 0x0b},
	{0x3d84, 0x40},
	{0x3d8c, 0x63},
	{0x3d8d, 0x00},
	{0x4000, 0x78},
	{0x4001, 0x2b},
	{0x4004, 0x00},
	{0x4005, 0x40},
	{0x4028, 0x2f},
	{0x400a, 0x01},
	{0x4010, 0x12},
	{0x4008, 0x02},
	{0x4009, 0x0d},
	{0x401a, 0x58},
	{0x4050, 0x00},
	{0x4051, 0x01},
	{0x4052, 0x00},
	{0x4053, 0x80},
	{0x4054, 0x00},
	{0x4055, 0x80},
	{0x4056, 0x00},
	{0x4057, 0x80},
	{0x4058, 0x00},
	{0x4059, 0x80},
	{0x430b, 0xff},
	{0x430c, 0xff},
	{0x430d, 0x00},
	{0x430e, 0x00},
	{0x4501, 0x18},
	{0x4502, 0x00},
	{0x4643, 0x00},
	{0x4640, 0x01},
	{0x4641, 0x04},
	{0x480e, 0x00},
	{0x4813, 0x00},
	{0x4815, 0x2b},
	{0x486e, 0x36},
	{0x486f, 0x84},
	{0x4860, 0x00},
	{0x4861, 0xa0},
	{0x484b, 0x05},
	{0x4850, 0x00},
	{0x4851, 0xaa},
	{0x4852, 0xff},
	{0x4853, 0x8a},
	{0x4854, 0x08},
	{0x4855, 0x30},
	{0x4800, 0x60},
	{0x4837, 0x1d},
	{0x484a, 0x3f},
	{0x5000, 0xc9},
	{0x5001, 0x43},
	{0x5002, 0x00},
	{0x5211, 0x03},
	{0x5291, 0x03},
	{0x520d, 0x0f},
	{0x520e, 0xfd},
	{0x520f, 0xa5},
	{0x5210, 0xa5},
	{0x528d, 0x0f},
	{0x528e, 0xfd},
	{0x528f, 0xa5},
	{0x5290, 0xa5},
	{0x5004, 0x40},
	{0x5005, 0x00},
	{0x5180, 0x00},
	{0x5181, 0x10},
	{0x5182, 0x0f},
	{0x5183, 0xff},
	{0x580b, 0x03},
	{0x4d00, 0x03},
	{0x4d01, 0xe9},
	{0x4d02, 0xba},
	{0x4d03, 0x66},
	{0x4d04, 0x46},
	{0x4d05, 0xa5},
	{0x3603, 0x3c},
	{0x3703, 0x26},
	{0x3709, 0x49},
	{0x3708, 0x2d},
	{0x3719, 0x1c},
	{0x371a, 0x06},
	{0x4000, 0x79},
	{0x4837, 0x1d},
	{0x0100, 0x01},
	{0x0100, 0x01},
	{0x0100, 0x01},
	{0x0100, 0x01},
};

static int os05a20_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 2592;
	info->modes.mode[0].size.h = 1944;
	info->modes.mode[0].fps = g_os05a20_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x36;
	info->i2c.addr_len = 2;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 1000);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 1000);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 1000);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V2, 1000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 5000);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 20000);
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

static const struct fps_info *os05a20_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_os05a20_fps_info); i++)
		if (fps == g_os05a20_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_os05a20_fps_info))
		return NULL;

	return &g_os05a20_fps_info[i];
}

static int os05a20_get_init_info(uint32_t isp_id,
				 const struct rts_isp_sensor_mode *mode,
				struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct os05a20_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("os05a20 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = os05a20_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->hts);

	set_init_i2c_regs(info->sensor_regs[0], g_os05a20_i2c_init_regs, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = (MIPI_LANE0 | MIPI_LANE1 |
				      MIPI_LANE2 | MIPI_LANE3);
	info->interface.mipi.hs_term = 0x03;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 2696;
	info->size.h = 1952;
	info->start.x = 0;
	info->start.y = 1;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = 2222;
	info->max_vts = 65536;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */
	status->min_vts = info->min_vts;

	return RTS_ISP_OK;
}

static int os05a20_start(uint32_t isp_id)
{
	struct os05a20_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static uint16_t get_sensor_gain_reg(float fgain)
{
	uint16_t reg;

	fgain = fgain > 15.5 ? 15.5 : fgain;
	reg = ((int)fgain << 7) | (int)((fgain - (int)fgain) * 128);
	return reg;
}

static float get_sensor_real_gain(uint16_t reg_value)
{
	float fgain;

	fgain = (float)(reg_value >> 7) + (reg_value & 0x7f) / 128.0f;
	return fgain;
}

static int os05a20_get_tuned_again(uint32_t isp_id,
				   float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int os05a20_get_tuned_dgain(uint32_t isp_id,
				   float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int os05a20_get_exposure_gain_info(uint32_t isp_id,
					 const struct rts_isp_sensor_exp_gain *exp_gain,
					 struct rts_isp_sync_regs *regs)
{
	int i;
	uint16_t gain_reg;
	uint32_t vts;
	struct os05a20_status *status;
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
		uint16_t exposure_reg;

		exposure_reg = exp_gain->exposure[0] / status->exp_step + 0.5f;

		/* exposure */
		set_sync_i2c(&reg[i++], 0x3501, exposure_reg  >> 8);
		set_sync_i2c(&reg[i++], 0x3502, exposure_reg & 0xff);
		status->last_exposure = exp_gain->exposure[0];

		/* gain */
		set_sync_i2c(&reg[i++], 0x3508, gain_reg >> 8);
		set_sync_i2c(&reg[i++], 0x3509, gain_reg & 0xff);

		/* dummy */
		set_sync_i2c(&reg[i++], 0x380e, vts >> 8);
		set_sync_i2c(&reg[i++], 0x380f, vts & 0xff);

	} else {
		/* gain */
		set_sync_i2c(&reg[i++], 0x3508, gain_reg >> 8);
		set_sync_i2c(&reg[i++], 0x3509, gain_reg & 0xff);
		/* dummy */
		set_sync_i2c(&reg[i++], 0x380e, vts >> 8);
		set_sync_i2c(&reg[i++], 0x380f, vts & 0xff);
	}
	regs->num = i;

	return RTS_ISP_OK;
}

static int os05a20_check(uint32_t isp_id)
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

	if (id == 0x5305)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops os05a20_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "os05a20",
	.get_info = os05a20_get_info,
	.get_init_info = os05a20_get_init_info,
	.start = os05a20_start,
	.get_tuned_again = os05a20_get_tuned_again,
	.get_tuned_dgain = os05a20_get_tuned_dgain,
	.get_exposure_gain_info = os05a20_get_exposure_gain_info,
	.check = os05a20_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(os05a20_ops)
