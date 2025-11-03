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

struct ps5268_status {
	float exp_step;
	float last_exposure;
	uint16_t cur_fps;
	uint16_t min_vts;
	uint16_t version;
	struct rts_isp_i2c_reg regs1[2];
};

struct ps5268_gain {
	uint8_t ana_gain_reg;
	float ana_gain;
};

static struct ps5268_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_ps5268_fps_info[] = {
	{30, 2400, 81000000},
};

static struct rts_isp_i2c_reg g_ps5268_i2c_init_regs[] = {
	// PS5268 4M
	{0x010B, 0x07},
	{0x0114, 0x12},
	{0x0115, 0xC0},
	{0x0178, 0xC0},
	{0x0179, 0x2A},
	{0x022E, 0x0E},
	{0x022F, 0x19},
	{0x022D, 0x01},
	{0x0226, 0xB1},
	{0x0227, 0x39},
	{0x021C, 0x00},
	{0x0233, 0x70},
	{0x024B, 0x05},
	{0x024D, 0x11},
	{0x0252, 0x16},
	{0x0253, 0x2C},
	{0x0254, 0x61},
	{0x0255, 0x11},
	{0x0664, 0x02},
	{0x0665, 0xAD},
	{0x0B02, 0x02},
	{0x0B0A, 0xFF},
	{0x0B0C, 0x00},
	{0x1200, 0x00},
	{0x1201, 0x01},
	{0x1300, 0x00},
	{0x1301, 0x01},
	{0x1303, 0x02},
	{0x1409, 0x1A},
	{0x140A, 0x15},
	{0x1411, 0x01},
	{0x1415, 0x05},
	{0x1417, 0x03},
	{0x1418, 0x03},
	{0x1406, 0x04},
	{0x1410, 0x02},
	{0x14B0, 0x01},
	{0x140F, 0x01},
	{0x0111, 0x01},
	{0x010F, 0x01},
};

static struct ps5268_gain gain_mapping[] = {
	{0x00, 1.0000},
	{0x01, 1.0625},
	{0x02, 1.1250},
	{0x03, 1.1875},
	{0x04, 1.2500},
	{0x05, 1.3125},
	{0x06, 1.3750},
	{0x07, 1.4375},
	{0x08, 1.5000},
	{0x09, 1.5625},
	{0x0a, 1.6250},
	{0x0b, 1.6875},
	{0x0c, 1.7500},
	{0x0d, 1.8125},
	{0x0e, 1.8750},
	{0x0f, 1.9375},
	{0x10, 2.0000},
	{0x11, 2.1250},
	{0x12, 2.2500},
	{0x13, 2.3750},
	{0x14, 2.5000},
	{0x15, 2.6250},
	{0x16, 2.7500},
	{0x17, 2.8750},
	{0x18, 3.0000},
	{0x19, 3.1250},
	{0x1a, 3.2500},
	{0x1b, 3.3750},
	{0x1c, 3.5000},
	{0x1d, 3.6250},
	{0x1e, 3.7500},
	{0x1f, 3.8750},
	{0x20, 4.0000},
	{0x21, 4.2500},
	{0x22, 4.5000},
	{0x23, 4.7500},
	{0x24, 5.0000},
	{0x25, 5.2500},
	{0x26, 5.5000},
	{0x27, 5.7500},
	{0x28, 6.0000},
	{0x29, 6.2500},
	{0x2a, 6.5000},
	{0x2b, 6.7500},
	{0x2c, 7.0000},
	{0x2d, 7.2500},
	{0x2e, 7.5000},
	{0x2f, 7.7500},
	{0x30, 8.0000},
	{0x31, 8.5000},
	{0x32, 9.0000},
	{0x33, 9.5000},
	{0x34, 10.0000},
	{0x35, 10.5000},
	{0x36, 11.0000},
	{0x37, 11.5000},
	{0x38, 12.0000},
	{0x39, 12.5000},
	{0x3a, 13.0000},
	{0x3b, 13.5000},
	{0x3c, 14.0000},
	{0x3d, 14.5000},
	{0x3e, 15.0000},
	{0x3f, 15.5000},
	{0x40, 16.0000},
	{0x41, 17.0000},
	{0x42, 18.0000},
	{0x43, 19.0000},
	{0x44, 20.0000},
	{0x45, 21.0000},
	{0x46, 22.0000},
	{0x47, 23.0000},
	{0x48, 24.0000},
	{0x49, 25.0000},
	{0x4a, 26.0000},
	{0x4b, 27.0000},
	{0x4c, 28.0000},
	{0x4d, 29.0000},
	{0x4e, 30.0000},
	{0x4f, 31.0000},
	{0x50, 32.0000},
};

static uint16_t get_sensor_gain_reg(float fgain)
{
	uint16_t reg_value = 0;
	int i;

	if (fgain >= 32.0000)
		reg_value = 0x50;
	else {
		for (i = 0; i < ((ARRAY_SIZE(gain_mapping)) - 1); i++) {
			if ((gain_mapping[i].ana_gain <= fgain) &&
			    (fgain < gain_mapping[i + 1].ana_gain)) {
				reg_value = gain_mapping[i].ana_gain_reg;
				break;
			}
		}
	}

	return reg_value;
}

static float get_sensor_real_gain(uint16_t reg_value)
{
	float gain = 0.0;
	int i;

	if (reg_value >= 0x50)
		gain = 32.0000;
	else {
		for (i = 0; i < ((ARRAY_SIZE(gain_mapping)) - 1); i++) {
			if (reg_value == gain_mapping[i].ana_gain_reg) {
				gain = gain_mapping[i].ana_gain;
				break;
			}
		}
	}

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

static int ps5268_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 1920;
	info->modes.mode[0].size.h = 1080;
	info->modes.mode[0].fps = g_ps5268_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x48;
	info->i2c.addr_len = 2;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 0);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V2, 0);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 0);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_3V3, 0);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 0);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_RST_GPIO, 0, 0);
	set_power_item(&down->items[i++], SNR_HCLK, 0, 0);
	set_power_item(&down->items[i++], SNR_IO_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_CORE_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, 0, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static const struct fps_info *ps5268_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_ps5268_fps_info); i++)
		if (fps == g_ps5268_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_ps5268_fps_info))
		return NULL;

	return &g_ps5268_fps_info[i];
}

static int ps5268_get_init_info(uint32_t isp_id,
				const struct rts_isp_sensor_mode *mode,
			       struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct ps5268_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	fps_info = ps5268_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0 | MIPI_LANE1;
	info->interface.mipi.hs_term = 0x6;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 1928;
	info->size.h = 1088;
	info->start.x = 0;
	info->start.y = 1;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = status->min_vts = 1124;
	info->max_vts = 65535;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */
	status->cur_fps = mode->fps;

	return RTS_ISP_OK;
}

/*
static int read_reg(int addr)
{
	struct rts_isp_i2c_reg reg;

	reg.addr = addr;
	rts_isp_read_sensor_reg(0, &reg);
	return reg.data;
}
*/
static void write_reg(int addr, int data)
{
	struct rts_isp_i2c_reg reg;

	reg.addr = addr;
	reg.data = data;

	(void)rts_isp_write_sensor_reg(0, &reg);
}

static int ps5268_start(uint32_t isp_id)
{
	int i;
	struct ps5268_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;
	//int val;
	//uint8_t check_ready;
	//float again;
	//uint8_t index;
	//uint16_t expline;

	//if (isp_id >= SUPPORTED_ISP_NUM)
	//	return -RTS_ISP_EINVAL;

	//do {
	//	val = read_reg(0x0f37);
	//	printf("status: %d\n", val);
	//	check_ready = (val >> 6) & 0;
	//} while (check_ready);
	//expline = read_reg(0x0f3c) << 8 | read_reg(0x0f3d);
	//expline = 1124 - expline;
	//index = read_reg(0x0f38);
	//again = gain_mapping[index].ana_gain;
	//debug("expline: %u, gain: %.2f\n", expline, again);

	for (i = 0; i < ARRAY_SIZE(g_ps5268_i2c_init_regs); i++)
		write_reg(g_ps5268_i2c_init_regs[i].addr,
			  g_ps5268_i2c_init_regs[i].data);

	return RTS_ISP_OK;
}

static int ps5268_get_tuned_again(uint32_t isp_id,
				  float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int ps5268_get_tuned_dgain(uint32_t isp_id,
				  float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int ps5268_get_exposure_gain_info(uint32_t isp_id,
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
	struct ps5268_status *status;
	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	gain = exp_gain->analog_gain[0] * exp_gain->digital_gain[0];
	gain_reg = get_sensor_gain_reg(gain);

	total_line = exp_gain->vts;
	total_line = (total_line - 1) / 2 * 2;
	reg = regs->reg;

	exp_reg_value_float =
		exp_gain->exposure[0] / status->exp_step + 0.5f;
	exp_reg_value =
		clip_d_word(exp_reg_value_float, 2, total_line);

	i = 0;

	line_dummy = total_line + 1 - exp_reg_value;
	line_dummy = clip_d_word(line_dummy, 2, total_line);

	exp_set = abs(status->last_exposure - exp_gain->exposure[0]) > 0.001f;
	if (exp_set) {
		set_sync_i2c(&reg[i++], 0x116, (total_line & 0xff00) >> 8);
		set_sync_i2c(&reg[i++], 0x117, (total_line & 0xff));

		set_sync_i2c(&reg[i++], 0x12B, (gain_reg & 0xff));

		set_sync_i2c(&reg[i++], 0x118, line_dummy >> 8);
		set_sync_i2c(&reg[i++], 0x119, line_dummy & 0xff);
		set_sync_i2c(&reg[i++], 0x111, 0x01);
		status->last_exposure = exp_gain->exposure[0];

	} else {
		set_sync_i2c(&reg[i++], 0x116, (total_line & 0xff00) >> 8);
		set_sync_i2c(&reg[i++], 0x117, (total_line & 0xff));

		set_sync_i2c(&reg[i++], 0x12B, (gain_reg & 0xff));
		set_sync_i2c(&reg[i++], 0x111, 0x01);
	}

	regs->num = i;

	return RTS_ISP_OK;
}

static int ps5268_check(uint32_t isp_id)
{
	int ret;
	int id;
	struct rts_isp_i2c_reg reg = {};

	reg.addr = 0x0100;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id = reg.data << 8;

	reg.addr = 0x0101;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id |= reg.data;

	if (id == 0x5268)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}


static const struct rts_isp_sensor_ops ps5268_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "ps5268",
	.get_info = ps5268_get_info,
	.get_init_info = ps5268_get_init_info,
	.start = ps5268_start,
	.get_tuned_again = ps5268_get_tuned_again,
	.get_tuned_dgain = ps5268_get_tuned_dgain,
	.get_exposure_gain_info = ps5268_get_exposure_gain_info,
	.check = ps5268_check
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(ps5268_ops)
