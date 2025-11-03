/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2020 Eric Yang <eric_yang@realsil.com.cn>
 */

#include <stdio.h>
#include <rts_isp_sensor.h>

/* #define DEBU */
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

struct sp2306_status {
	float exp_step;
	float last_exposure;
	uint16_t cur_fps;
	uint16_t min_vts;
	struct rts_isp_i2c_reg regs1[3];
};

static struct sp2306_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_sp2306_fps_info[] = {
	{30, 2500, 84000000},
};

static struct rts_isp_i2c_reg g_sp2306_i2c_init_regs1[] = {
	{0xfd, 0x00},
	{0x36, 0x01},
	{0xfd, 0x00},
	{0x36, 0x00},
	{0xfd, 0x00},
	{0x20, 0x00},
};

static struct rts_isp_i2c_reg g_sp2306_i2c_init_regs2[] = {
	{0xfd, 0x00},
	{0x1b, 0xc1},
	{0x1e, 0xaa},
	{0x30, 0x01},
	{0x41, 0x09},
	{0xfd, 0x01},
	{0x03, 0x01},
	{0x04, 0x54},
	{0x06, 0x00},
	{0x0a, 0x40},
	{0x24, 0xff},
	{0x01, 0x01},
	{0x11, 0x0e},
	{0x12, 0x04},
	{0x13, 0x22},
	{0x16, 0x38},
	{0x19, 0x81},
	{0x1b, 0x04},
	{0x1c, 0x44},
	{0x1e, 0x63},
	{0x1f, 0x33},
	{0x20, 0x08},
	{0x21, 0x03},
	{0x25, 0x0b},
	{0x27, 0x42},
	{0x2a, 0x00},
	{0x2c, 0x05},
	{0x38, 0x10},
	{0x50, 0x05},
	{0x51, 0x20},
	{0x52, 0x20},
	{0x55, 0x15},
	{0x57, 0x15},
	{0x59, 0x02},
	{0x5a, 0x00},
	{0x5d, 0x02},
	{0x61, 0x9e},
	{0x62, 0x9e},
	{0x63, 0x9e},
	{0x64, 0x9e},
	{0x66, 0x95},
	{0x67, 0x95},
	{0x68, 0x95},
	{0x69, 0x95},
	{0x6a, 0x05},
	{0x6b, 0x00},
	{0x71, 0xa0},
	{0x72, 0x25},
	{0x73, 0x25},
	{0x74, 0x25},
	{0x79, 0x00},
	{0x7a, 0x00},
	{0x80, 0x00},
	{0x81, 0x07},
	{0x8a, 0x20},
	{0xb8, 0xa0},
	{0xb9, 0x80},
	{0xba, 0x90},
	{0xbb, 0x80},
	{0xbc, 0x58},
	{0xbd, 0xab},
	{0xd5, 0x2a},
	{0xd6, 0x00},
	{0xd7, 0xbf},
	{0xf0, 0x40},
	{0xf1, 0x40},
	{0xf2, 0x40},
	{0xf3, 0x40},
	{0xf5, 0x04},
	{0xfa, 0x1c},
	{0xfb, 0x19},
	{0xfd, 0x02},
	{0x34, 0xff},
	{0xfd, 0x04},
	{0x26, 0x00},
	{0x27, 0x04},
	{0x28, 0x04},
	{0x29, 0x40},
	{0x2a, 0x00},
	{0x2b, 0x04},
	{0x2c, 0x07},
	{0x2d, 0x88},
	{0xfd, 0x01},
};

static int sp2306_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 1920;
	info->modes.mode[0].size.h = 1080;
	info->modes.mode[0].fps = g_sp2306_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x3c;
	info->i2c.addr_len = 1;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 0);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V5, 0);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 8000);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_HIGH, 0);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 4000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 5000);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_RST_GPIO, 0, 0);
	set_power_item(&down->items[i++], SNR_HCLK, 0, 0);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_IO_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_CORE_POWER, 0, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static const struct fps_info *sp2306_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_sp2306_fps_info); i++)
		if (fps == g_sp2306_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_sp2306_fps_info))
		return NULL;

	return &g_sp2306_fps_info[i];
}

static int sp2306_get_init_info(uint32_t isp_id,
				const struct rts_isp_sensor_mode *mode,
			       struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct sp2306_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("sp2306 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = sp2306_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, clk_div: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->clk_div, fps_info->hts);

	set_init_i2c_regs(info->sensor_regs[0], g_sp2306_i2c_init_regs1, 5000);
	set_init_i2c_regs(info->sensor_regs[1], g_sp2306_i2c_init_regs2, 0);
	set_init_i2c_regs(info->sensor_regs[2], status->regs1, 0);

	info->interface.interface = SNR_INTERFACE_DVP;
	info->interface.dvp.sample_rising = 1;
	info->interface.dvp.hsync_active_high = 1;
	info->interface.dvp.vsync_active_high = 0;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 1920;
	info->size.h = 1081;
	info->start.x = 0;
	info->start.y = 1;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = status->min_vts = 1120;
	info->max_vts = 65535;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */
	status->cur_fps = mode->fps;

	return RTS_ISP_OK;
}

static int sp2306_start(uint32_t isp_id)
{
	struct sp2306_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static uint16_t get_sensor_gain_reg(float fgain)
{
	uint16_t reg_value = 0;

	reg_value = (uint16_t)(fgain * 16.0f);
	if (reg_value > 16 * 15.5)
		reg_value = 0xf8;

	return reg_value;
}

static float get_sensor_real_gain(uint16_t reg_value)
{
	float gain;

	gain = (float)(reg_value>>4) + (float)(reg_value&0xf)/16.0f;

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

static int sp2306_get_tuned_again(uint32_t isp_id,
				  float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int sp2306_get_tuned_dgain(uint32_t isp_id,
				  float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int sp2306_get_exposure_gain_info(uint32_t isp_id,
					const struct rts_isp_sensor_exp_gain *exp_gain,
					struct rts_isp_sync_regs *regs)
{
	int i;
	int exp_set;
	uint16_t total_line;
	uint16_t line_dummy;
	uint16_t gain_reg;
	float exp_reg_value_float;
	uint32_t exp_reg_value;
	float gain;
	struct sp2306_status *status;
	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	gain = exp_gain->analog_gain[0] * exp_gain->digital_gain[0];
	gain_reg = get_sensor_gain_reg(gain);
	total_line = exp_gain->vts;
	total_line = (total_line + 1) / 2 * 2;
	reg = regs->reg;

	exp_reg_value_float =
		exp_gain->exposure[0] / status->exp_step + 0.5f;
	exp_reg_value =
		clip_d_word(exp_reg_value_float, 0, total_line);

	i = 0;
	line_dummy = total_line - 1117;
	set_sync_i2c(&reg[i++], 0xfd, 1);

	set_sync_i2c(&reg[i++], 0x05, (line_dummy >> 8));
	set_sync_i2c(&reg[i++], 0x06, (line_dummy & 0xff));

	exp_set = abs(status->last_exposure - exp_gain->exposure[0]) > 0.001f;
	if (exp_set) {
		set_sync_i2c(&reg[i++], 0x03, exp_reg_value >> 8);
		set_sync_i2c(&reg[i++], 0x04, exp_reg_value & 0xff);
		status->last_exposure = exp_gain->exposure[0];
	}
	set_sync_i2c(&reg[i++], 0x24, gain_reg);
	set_sync_i2c(&reg[i++], 1, 1);

	set_sync_info(&reg[i++], 1, RTS_ISP_INT_DATA_START);

	regs->num = i;

	return RTS_ISP_OK;
}

static int sp2306_check(uint32_t isp_id)
{
	int ret;
	int id;
	struct rts_isp_i2c_reg reg;

	/* page */
	reg.addr = 0xfd;
	reg.data = 0x00;
	ret = rts_isp_write_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;

	reg.addr = 0x02;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id = reg.data << 8;

	reg.addr = 0x03;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id |= reg.data;

	if (id == 0x2306)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}


static const struct rts_isp_sensor_ops sp2306_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "sp2306",
	.get_info = sp2306_get_info,
	.get_init_info = sp2306_get_init_info,
	.start = sp2306_start,
	.get_tuned_again = sp2306_get_tuned_again,
	.get_tuned_dgain = sp2306_get_tuned_dgain,
	.get_exposure_gain_info = sp2306_get_exposure_gain_info,
	.check = sp2306_check
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(sp2306_ops)
