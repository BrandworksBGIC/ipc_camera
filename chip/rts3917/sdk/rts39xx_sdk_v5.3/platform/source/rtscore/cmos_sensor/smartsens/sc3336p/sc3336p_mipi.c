/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2023 wei_mo <wei_mo@apowertec.com>
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

struct sc3336p_status {
	float exp_step;
	float last_exposure;
	uint16_t min_vts;
	struct rts_isp_i2c_reg regs1[2];
};

static struct sc3336p_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_sc3336p_fps_info[] = {
	{30, 2500, 99000000},
};

static struct rts_isp_i2c_reg g_sc3336p_i2c_init_regs[] = {
//2304x1296 30fps pclk=(495*2/10=99)M
//{0x320e,0x320f} = default = 0x528, frame height = {0x320e,0x320f} = 1320
//pclk = 5000 * 1320 * 15 = 99,000,000
	{0x36e9, 0x80}, {0x37f9, 0x80}, {0x301f, 0x02}, {0x30b8, 0x33},
	{0x320e, 0x05}, {0x320f, 0x28}, {0x3253, 0x10}, {0x325f, 0x20},
	{0x3301, 0x04}, {0x3306, 0x50}, {0x3309, 0xf8}, {0x330a, 0x00},
	{0x330b, 0xd8}, {0x3314, 0x13}, {0x331f, 0xe9}, {0x3333, 0x10},
	{0x3334, 0x40}, {0x335e, 0x06}, {0x335f, 0x0a}, {0x3364, 0x5e},
	{0x337c, 0x02}, {0x337d, 0x0e}, {0x3390, 0x01}, {0x3391, 0x03},
	{0x3392, 0x07}, {0x3393, 0x04}, {0x3394, 0x04}, {0x3395, 0x04},
	{0x3396, 0x08}, {0x3397, 0x0b}, {0x3398, 0x1f}, {0x3399, 0x04},
	{0x339a, 0x0a}, {0x339b, 0x3a}, {0x339c, 0x60}, {0x33a2, 0x04},
	{0x33ac, 0x08}, {0x33ad, 0x1c}, {0x33ae, 0x10}, {0x33af, 0x30},
	{0x33b1, 0x80}, {0x33b3, 0x48}, {0x33f9, 0x60}, {0x33fb, 0x74},
	{0x33fc, 0x4b}, {0x33fd, 0x5f}, {0x349f, 0x03}, {0x34a6, 0x4b},
	{0x34a7, 0x5f}, {0x34a8, 0x20}, {0x34a9, 0x18}, {0x34aa, 0x00},
	{0x34ab, 0xe8}, {0x34ac, 0x01}, {0x34ad, 0x00}, {0x34f8, 0x5f},
	{0x34f9, 0x18}, {0x3630, 0xc0}, {0x3631, 0x84}, {0x3632, 0x64},
	{0x3633, 0x32}, {0x363b, 0x03}, {0x363c, 0x08}, {0x3641, 0x38},
	{0x3670, 0x4e}, {0x3674, 0xf0}, {0x3675, 0xc0}, {0x3676, 0xc0},
	{0x3677, 0x86}, {0x3678, 0x86}, {0x3679, 0x86}, {0x367c, 0x48},
	{0x367d, 0x49}, {0x367e, 0x4b}, {0x367f, 0x5f}, {0x3690, 0x22},
	{0x3691, 0x22}, {0x3692, 0x33}, {0x369c, 0x4b}, {0x369d, 0x4f},
	{0x36b0, 0x87}, {0x36b1, 0x90}, {0x36b2, 0xa1}, {0x36b3, 0xc8},
	{0x36b4, 0x49}, {0x36b5, 0x4b}, {0x36b6, 0x4f}, {0x36ea, 0x0b},
	{0x36eb, 0x0d}, {0x36ec, 0x1c}, {0x36ed, 0x26}, {0x370f, 0x01},
	{0x3722, 0x09}, {0x3724, 0x41}, {0x3725, 0xc1}, {0x3771, 0x09},
	{0x3772, 0x09}, {0x3773, 0x05}, {0x377a, 0x48}, {0x377b, 0x5f},
	{0x37fa, 0x0b}, {0x37fb, 0x33}, {0x37fc, 0x11}, {0x37fd, 0x08},
	{0x3904, 0x04}, {0x3905, 0x8c}, {0x391d, 0x04}, {0x391f, 0x49},
	{0x3921, 0x20}, {0x3926, 0x21}, {0x3933, 0x80}, {0x3934, 0x08},
	{0x3935, 0x00}, {0x3936, 0x90}, {0x3937, 0x78}, {0x3938, 0x77},
	{0x3939, 0x00}, {0x393a, 0x00}, {0x393b, 0x00}, {0x393c, 0x1c},
	{0x39dc, 0x02}, {0x3e01, 0x52}, {0x3e02, 0x00}, {0x3e09, 0x00},
	{0x440e, 0x02}, {0x4509, 0x20}, {0x5780, 0x66}, {0x578d, 0x40},
	{0x5ae0, 0xfe}, {0x5ae1, 0x40}, {0x5ae2, 0x38}, {0x5ae3, 0x30},
	{0x5ae4, 0x28}, {0x5ae5, 0x38}, {0x5ae6, 0x30}, {0x5ae7, 0x28},
	{0x5ae8, 0x3f}, {0x5ae9, 0x34}, {0x5aea, 0x2c}, {0x5aeb, 0x3f},
	{0x5aec, 0x34}, {0x5aed, 0x2c}, {0x36e9, 0x53}, {0x37f9, 0x27},
	{0x3213, 0x03}, {0x0100, 0x01},
};

static int sc3336p_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 2304;
	info->modes.mode[0].size.h = 1296;
	info->modes.mode[0].fps = g_sc3336p_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x30;
	info->i2c.addr_len = 2;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 5000);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V5, 5000);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 5000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 5000);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 5000);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&down->items[i++], SNR_HCLK, CLK_NONE, 0);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, PWR_NONE, 0);
	set_power_item(&down->items[i++], SNR_CORE_POWER, PWR_NONE, 0);
	set_power_item(&down->items[i++], SNR_IO_POWER, PWR_NONE, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static const struct fps_info *sc3336p_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_sc3336p_fps_info); i++)
		if (fps == g_sc3336p_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_sc3336p_fps_info))
		return NULL;

	return &g_sc3336p_fps_info[i];
}

static int sc3336p_get_init_info(uint32_t isp_id,
				const struct rts_isp_sensor_mode *mode,
			       struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct sc3336p_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("sc3336p get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = sc3336p_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, clk_div: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->clk_div, fps_info->hts);

	set_init_i2c_regs(info->sensor_regs[0], g_sc3336p_i2c_init_regs, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0 | MIPI_LANE1;
	info->interface.mipi.hs_term = 0x5;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 2304;
	info->size.h = 1296;
	info->start.x = 0;
	info->start.y = 0;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = status->min_vts = 1320;
	info->max_vts = 65535 - info->min_vts;

	set_init_i2c(&status->regs1[0], 0x320d, ((fps_info->hts)>>1) & 0xff);
	set_init_i2c(&status->regs1[1], 0x320c, ((fps_info->hts)>>1) >> 8);

	set_init_i2c_regs(info->sensor_regs[1], status->regs1, 0);

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */

	return RTS_ISP_OK;
}

static int sc3336p_start(uint32_t isp_id)
{
	struct sc3336p_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static uint16_t get_sensor_gain_reg(float fgain)
{
	uint16_t reg_value = 0x00;

	if (fgain >= 48.64f)
		reg_value = 0x5f;
	else if (fgain >= 24.32f)
		reg_value = 0x4f;
	else if (fgain >= 12.16f)
		reg_value = 0x4b;
	else if (fgain >= 6.08f)
		reg_value = 0x49;
	else if (fgain >= 3.04f)
		reg_value = 0x48;
	else if (fgain >= 1.52f)
		reg_value = 0x40;
	else if (fgain >= 1.00f)
		reg_value = 0x00;
	return reg_value;
}

static float get_sensor_real_gain(uint16_t reg_value)
{
	float gain = 1.0;

	if (reg_value >= 0x5f)
		gain = 48.64;
	else if (reg_value >= 0x4f)
		gain = 24.32;
	else if (reg_value >= 0x4b)
		gain = 12.16;
	else if (reg_value >= 0x49)
		gain = 6.08;
	else if (reg_value >= 0x48)
		gain = 3.04;
	else if (reg_value >= 0x40)
		gain = 1.52;
	else
		gain = 1.00;
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

static int sc3336p_get_tuned_again(uint32_t isp_id,
				  float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int sc3336p_get_tuned_dgain(uint32_t isp_id,
				  float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int sc3336p_get_exposure_gain_info(uint32_t isp_id,
		const struct rts_isp_sensor_exp_gain *exp_gain,
					struct rts_isp_sync_regs *regs)
{
	int i;
	int exp_set;
	uint16_t total_line;
	uint16_t gain_reg;
	float exp_reg_value_float;
	uint32_t exp_reg_value;
	float gain;
	struct sc3336p_status *status;
	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	gain = exp_gain->analog_gain[0] * exp_gain->digital_gain[0];
	gain_reg = get_sensor_gain_reg(gain);
	total_line = exp_gain->vts;
	total_line = (total_line + 1) / 2 * 2;
	reg = regs->reg;

	i = 0;

	set_sync_i2c(&reg[i++], 0x320e, (total_line >> 8));
	set_sync_i2c(&reg[i++], 0x320f, (total_line & 0xff));
	exp_set = abs(status->last_exposure - exp_gain->exposure[0]) > 0.001f;

	if (exp_set) {
		exp_reg_value_float =
			exp_gain->exposure[0] / status->exp_step + 0.5f;
		exp_reg_value =
			clip_d_word(exp_reg_value_float, 2, (total_line - 8));
		exp_reg_value *= 16;
		set_sync_i2c(&reg[i++], 0x3e00, exp_reg_value >> 16);
		set_sync_i2c(&reg[i++], 0x3e01, (exp_reg_value & 0xff00) >> 8);
		set_sync_i2c(&reg[i++], 0x3e02, exp_reg_value & 0xff);
		status->last_exposure = exp_gain->exposure[0];
	}
	set_sync_i2c(&reg[i++], 0x3e09, (gain_reg & 0xff));

	regs->num = i;

	return RTS_ISP_OK;
}

static int sc3336p_check(uint32_t isp_id)
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

	if (id == 0x9c41)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops sc3336p_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "sc3336p",
	.get_info = sc3336p_get_info,
	.get_init_info = sc3336p_get_init_info,
	.start = sc3336p_start,
	.get_tuned_again = sc3336p_get_tuned_again,
	.get_tuned_dgain = sc3336p_get_tuned_dgain,
	.get_exposure_gain_info = sc3336p_get_exposure_gain_info,
	.check = sc3336p_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(sc3336p_ops)
