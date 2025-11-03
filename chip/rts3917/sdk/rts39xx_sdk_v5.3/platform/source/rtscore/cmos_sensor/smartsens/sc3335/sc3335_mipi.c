/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2020 PingCheng Huang <pingcheng.huang@realtek.com>
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

struct sc3335_status {
	float exp_step;
	float last_exposure;
	uint16_t min_vts;
	struct rts_isp_i2c_reg regs1[2];
};

static struct sc3335_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_sc3335_fps_info[] = {
	{30, 2500, 102000000},
};

static struct rts_isp_i2c_reg g_sc3335_i2c_init_regs[] = {
//2312x1304 30fps pclk=(510*2/10=102)M
//{0x320c,0x320d} = default = 0x4e2, frame witdh = {0x320c,0x320d}  = 1250
//{0x320e,0x320f} = default = 0x550, frame height = {0x320e,0x320f} = 1360
//pclk = 2500 * 1360 * 30 = 102,000,000
	{0x0103, 0x01}, {0x0100, 0x00}, {0x36e9, 0x80}, {0x36f9, 0x80},
	{0x301f, 0x1b}, {0x3200, 0x00}, {0x3201, 0x00}, {0x3202, 0x00},
	{0x3203, 0x00}, {0x3204, 0x09}, {0x3205, 0x0b}, {0x3206, 0x05},
	{0x3207, 0x1b}, {0x3208, 0x09}, {0x3209, 0x08}, {0x320a, 0x05},
	{0x320b, 0x18}, {0x320c, 0x04}, {0x320d, 0xe2}, {0x320e, 0x05},
	{0x320f, 0x50}, {0x3210, 0x00}, {0x3211, 0x02}, {0x3212, 0x00},
	{0x3213, 0x03}, {0x3253, 0x04}, {0x3301, 0x04}, {0x3302, 0x10},
	{0x3304, 0x40}, {0x3306, 0x40}, {0x3309, 0x50}, {0x330b, 0xb6},
	{0x330e, 0x29}, {0x3310, 0x06}, {0x3314, 0x96}, {0x331e, 0x39},
	{0x331f, 0x49}, {0x3320, 0x09}, {0x3333, 0x10}, {0x334c, 0x01},
	{0x3364, 0x17}, {0x3367, 0x01}, {0x3390, 0x04}, {0x3391, 0x08},
	{0x3392, 0x38}, {0x3393, 0x05}, {0x3394, 0x09}, {0x3395, 0x16},
	{0x33ac, 0x0c}, {0x33ae, 0x1c}, {0x3622, 0x16}, {0x3637, 0x22},
	{0x363a, 0x1f}, {0x363c, 0x05}, {0x3670, 0x0e}, {0x3674, 0xb0},
	{0x3675, 0x88}, {0x3676, 0x68}, {0x3677, 0x84}, {0x3678, 0x85},
	{0x3679, 0x86}, {0x367c, 0x18}, {0x367d, 0x38}, {0x367e, 0x08},
	{0x367f, 0x18}, {0x3690, 0x43}, {0x3691, 0x43}, {0x3692, 0x44},
	{0x369c, 0x18}, {0x369d, 0x38}, {0x36ea, 0x1e}, {0x36eb, 0x0d},
	{0x36ec, 0x1c}, {0x36ed, 0x24}, {0x36fa, 0x1e}, {0x36fb, 0x00},
	{0x36fc, 0x10}, {0x36fd, 0x24}, {0x3908, 0x82}, {0x391f, 0x18},
	{0x3e01, 0xa8}, {0x3e02, 0x20}, {0x3f09, 0x48}, {0x4505, 0x08},
	{0x4509, 0x20}, {0x4819, 0x07}, {0x481b, 0x04}, {0x481d, 0x0e},
	{0x481f, 0x03}, {0x4821, 0x09}, {0x4823, 0x04}, {0x4825, 0x03},
	{0x4827, 0x03}, {0x4829, 0x06}, {0x5799, 0x00}, {0x59e0, 0x60},
	{0x59e1, 0x08}, {0x59e2, 0x3f}, {0x59e3, 0x18}, {0x59e4, 0x18},
	{0x59e5, 0x3f}, {0x59e6, 0x06}, {0x59e7, 0x02}, {0x59e8, 0x38},
	{0x59e9, 0x10}, {0x59ea, 0x0c}, {0x59eb, 0x10}, {0x59ec, 0x04},
	{0x59ed, 0x02}, {0x36e9, 0x50}, {0x36f9, 0x50}, {0x0100, 0x01},
};

static int sc3335_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 2304;
	info->modes.mode[0].size.h = 1296;
	info->modes.mode[0].fps = g_sc3335_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x30;
	info->i2c.addr_len = 2;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 5000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 0);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V2, 1000);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 2000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 5000);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_HIGH, 5000);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&down->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 0);
	set_power_item(&down->items[i++], SNR_IO_POWER, PWR_NONE, 0);
	set_power_item(&down->items[i++], SNR_CORE_POWER, PWR_NONE, 0);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, PWR_NONE, 0);
	set_power_item(&down->items[i++], SNR_HCLK, CLK_NONE, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static const struct fps_info *sc3335_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_sc3335_fps_info); i++)
		if (fps == g_sc3335_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_sc3335_fps_info))
		return NULL;

	return &g_sc3335_fps_info[i];
}

static int sc3335_get_init_info(uint32_t isp_id,
				const struct rts_isp_sensor_mode *mode,
			       struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct sc3335_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("sc3335 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = sc3335_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, clk_div: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->clk_div, fps_info->hts);

	set_init_i2c_regs(info->sensor_regs[0], g_sc3335_i2c_init_regs, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0 | MIPI_LANE1;
	info->interface.mipi.hs_term = 0x05;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 2304;
	info->size.h = 1296;
	info->start.x = 0;
	info->start.y = 0;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = status->min_vts = 1360;
	info->max_vts = 65535 - info->min_vts;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */

	return RTS_ISP_OK;
}

static int sc3335_start(uint32_t isp_id)
{
	struct sc3335_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static uint16_t get_sensor_gain_reg(float fgain)
{
	uint16_t reg_value = 0;

	if (fgain >= 15.875) {
		reg_value = 0x1f7f;
	} else {
		if (fgain >= 8.0)
			reg_value = (uint16_t)(fgain * 8.0f) | 0x1f00;
		else if (fgain >= 4.0)
			reg_value = (uint16_t)(fgain * 16.0f) | 0x0f00;
		else if (fgain >= 2.0)
			reg_value = (uint16_t)(fgain * 32.0f) | 0x0700;
		else
			reg_value = (uint16_t)(fgain * 64.0f) | 0x0300;
	}

	return reg_value;
}



static float get_sensor_real_gain(uint16_t reg_value)
{
	float gain;

	gain = ((reg_value & 0xff) / 64.0f) *
		((((reg_value >> 8) & 0x1f) >> 2) + 1);

	return gain;
}


static uint32_t clip_d_word(uint32_t current, uint32_t minimum,
			    uint32_t maximum)
{
	if (current > maximum)
		return maximum;
	if (current < minimum)
		return minimum;
	return current;
}

static int sc3335_get_tuned_again(uint32_t isp_id,
				  float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int sc3335_get_tuned_dgain(uint32_t isp_id,
				  float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int sc3335_get_exposure_gain_info(uint32_t isp_id,
					const struct rts_isp_sensor_exp_gain *exp_gain,
					struct rts_isp_sync_regs *regs)
{

	int i;
	uint16_t total_line;
	uint16_t gain_reg;
	float exp_reg_value_float;
	uint32_t exp_reg_value;
	struct sc3335_status *status;
	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	gain_reg = get_sensor_gain_reg(exp_gain->analog_gain[0] *
				       exp_gain->digital_gain[0]);

	total_line = exp_gain->vts;

	exp_reg_value_float =
		2.0 * exp_gain->exposure[0] / status->exp_step + 0.5f;
	exp_reg_value =
		clip_d_word(exp_reg_value_float, 3,
			    (2 * total_line-10));
	exp_reg_value = exp_reg_value << 4;

	total_line = (total_line + 1) / 2 * 2;

	reg = regs->reg;

	i = 0;

	if (abs(status->last_exposure - exp_gain->exposure[0]) > 0.0001f) {
		total_line = exp_gain->vts;

		//total length
		set_sync_i2c(&reg[i++], 0x320e, (total_line & 0xff00) >> 8);
		set_sync_i2c(&reg[i++], 0x320f, (total_line & 0xff));

		//set exposure time
		set_sync_i2c(&reg[i++], 0x3e00,
			     (exp_reg_value & 0xff0000) >> 16);
		set_sync_i2c(&reg[i++], 0x3e01, (exp_reg_value & 0xff00) >> 8);
		set_sync_i2c(&reg[i++], 0x3e02, exp_reg_value & 0xff);
		status->last_exposure = exp_gain->exposure[0];

		//finegain, corsegain at 0x3e09[6:0]
		set_sync_i2c(&reg[i++], 0x3e08, (gain_reg & 0xff00) >> 8);
		set_sync_i2c(&reg[i++], 0x3e09, (gain_reg & 0xff));

	} else {
		total_line = exp_gain->vts;
		//total length
		set_sync_i2c(&reg[i++], 0x320e, (total_line & 0xff00) >> 8);
		set_sync_i2c(&reg[i++], 0x320f, (total_line & 0xff));

		set_sync_i2c(&reg[i++], 0x3e08, (gain_reg & 0xff00) >> 8);
		set_sync_i2c(&reg[i++], 0x3e09, (gain_reg & 0xff));
	}

	set_sync_i2c(&reg[i++], 0x3812, 0x00); //group hold
	if (exp_gain->analog_gain[0] < 2) {
		set_sync_i2c(&reg[i++], 0x363c, 0x05);
		set_sync_i2c(&reg[i++], 0x330e, 0x29);
	} else if (exp_gain->analog_gain[0] < 8) {
		set_sync_i2c(&reg[i++], 0x363c, 0x07);
		set_sync_i2c(&reg[i++], 0x330e, 0x25);
	} else {
		set_sync_i2c(&reg[i++], 0x363c, 0x07);
		set_sync_i2c(&reg[i++], 0x330e, 0x18);
	}
	// high temperature DPC logic
	if (exp_gain->analog_gain[0] >= 15.875)
		set_sync_i2c(&reg[i++], 0x5799, 0x07);
	else if (exp_gain->analog_gain[0] <= 10)
		set_sync_i2c(&reg[i++], 0x5799, 0x00);
	set_sync_i2c(&reg[i++], 0x3812, 0x30); //group enable

	regs->num = i;

	return RTS_ISP_OK;
}

static int sc3335_check(uint32_t isp_id)
{
	int ret;
	int id;
	struct rts_isp_i2c_reg reg = {};

	reg.addr = 0x3107;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id = reg.data << 8;

	reg.addr = 0x3108;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id |= reg.data;

	if (id == 0xcc1a)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops sc3335_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "sc3335",
	.get_info = sc3335_get_info,
	.get_init_info = sc3335_get_init_info,
	.start = sc3335_start,
	.get_tuned_again = sc3335_get_tuned_again,
	.get_tuned_dgain = sc3335_get_tuned_dgain,
	.get_exposure_gain_info = sc3335_get_exposure_gain_info,
	.check = sc3335_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(sc3335_ops)
