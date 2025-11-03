/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2023 Yang Wang  <yang_wang@apowertec.com>
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

struct gc2083_status {
	int min_vts;
	float exp_step;
	float last_exposure;
	int num;
};

struct gc2083_gain_config {
	uint8_t reg_00d0;
	uint8_t reg_0155;
	uint8_t reg_0410;
	uint8_t reg_0414;
	uint8_t reg_00b8;
	uint8_t reg_00b9;
	uint8_t reg_0dc1;
	uint16_t value;
};

static struct gc2083_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_gc2083_fps_info[] = {
	{30, 2800, 94500000},
};

static struct rts_isp_i2c_reg g_gc2083_i2c_init_regs[] = {
	/****system****/
	{0x03fe, 0xf0},
	{0x03fe, 0xf0},
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03f2, 0x00},
	{0x03f3, 0x00},
	{0x03f4, 0x36},
	{0x03f5, 0xc0},
	{0x03f6, 0x24},
	{0x03f7, 0x01},
	{0x03f8, 0x2a},//2c
	{0x03f9, 0x43},
	{0x03fc, 0x8e},
	{0x0381, 0x07},
	{0x00d7, 0x29},
	/****CISCTL & ANALOG****/
	{0x0d6d, 0x18},
	{0x00d5, 0x03},
	{0x0082, 0x01},
	{0x0db3, 0xd4},
	{0x0db0, 0x0d},
	{0x0db5, 0x96},
	{0x0d03, 0x02},
	{0x0d04, 0x02},
	{0x0d05, 0x05},
	{0x0d06, 0x78},//1466 //1400
	{0x0d07, 0x00},
	{0x0d08, 0x11},//17
	{0x0d09, 0x00},
	{0x0d0a, 0x02},
	{0x000b, 0x00},
	{0x000c, 0x00},
	{0x0d0d, 0x04},
	{0x0d0e, 0x40},//1088
	{0x000f, 0x07},
	{0x0010, 0x90},//1936
	{0x0017, 0x0c},
	{0x0d73, 0x92},
	{0x0076, 0x00},
	{0x0d76, 0x00},
	{0x0d41, 0x04},
	{0x0d42, 0x65},//frame length 1125
	{0x0d7a, 0x10},
	{0x0d19, 0x31},
	{0x0d25, 0x0b},
	{0x0d20, 0x60},
	{0x0d27, 0x03},
	{0x0d29, 0x60},
	{0x0d43, 0x10},
	{0x0d49, 0x10},
	{0x0d55, 0x18},
	{0x0dc2, 0x44},
	{0x0058, 0x3c},
	{0x00d8, 0x68},
	{0x00d9, 0x14},
	{0x00da, 0xc1},
	{0x0050, 0x18},
	{0x0db6, 0x3d},
	{0x00d2, 0xbc},
	{0x0d66, 0x42},
	{0x008c, 0x05},//7
	{0x008d, 0xa8},//ff
	/*gain*/
	{0x007a, 0x60},//global gain
	{0x00d0, 0x00},
	{0x0dc1, 0x00},
	/*isp*/
	{0x0102, 0xa9},//89
	{0x0158, 0x00},
	{0x0107, 0xa6},
	{0x0108, 0xa9},
	{0x0109, 0xa8},
	{0x010a, 0xa7},
	{0x010b, 0xff},
	{0x010c, 0xff},
	{0x0428, 0x86},
	{0x0429, 0x86},
	{0x042a, 0x86},
	{0x042b, 0x68},
	{0x042c, 0x68},
	{0x042d, 0x68},
	{0x042e, 0x68},
	{0x042f, 0x68},
	{0x0430, 0x4f},
	{0x0431, 0x68},
	{0x0432, 0x67},
	{0x0433, 0x66},
	{0x0434, 0x66},
	{0x0435, 0x66},
	{0x0436, 0x66},
	{0x0437, 0x66},
	{0x0438, 0x62},
	{0x0439, 0x62},
	{0x043a, 0x62},
	{0x043b, 0x62},
	{0x043c, 0x62},
	{0x043d, 0x62},
	{0x043e, 0x62},
	{0x043f, 0x62},
	/*dark sun*/
	{0x0077, 0x01},
	{0x0078, 0x65},
	{0x0079, 0x04},
	{0x0067, 0xa0},
	{0x0054, 0xff},
	{0x0055, 0x02},
	{0x0056, 0x00},
	{0x0057, 0x04},
	{0x005a, 0xff},
	{0x005b, 0x07},
	/*blk*/
	{0x0026, 0x01},
	{0x0152, 0x02},
	{0x0153, 0x50},
	{0x0155, 0x93},
	{0x0410, 0x16},
	{0x0411, 0x16},
	{0x0412, 0x16},
	{0x0413, 0x16},
	{0x0414, 0x6f},
	{0x0415, 0x6f},
	{0x0416, 0x6f},
	{0x0417, 0x6f},
	{0x04e0, 0x18},
	/*window*/
	{0x0192, 0x00},
	{0x0194, 0x03},
	{0x0195, 0x04},
	{0x0196, 0x38},//1088
	{0x0197, 0x07},
	{0x0198, 0x80},//1928
	/****DVP & MIPI****/
	{0x0201, 0x27},
	{0x0202, 0x53},//0x50
	{0x0203, 0xce},//0xb6//0x8e
	{0x0204, 0x40},
	{0x0212, 0x07},
	{0x0213, 0x80},
	{0x0215, 0x12},
	{0x0229, 0x05},
	{0x0237, 0x03},
	{0x023e, 0x99},
};

static struct gc2083_gain_config g_gc2083_gain_table[] = {
	{0x00,  0x03,  0x11,  0x6f,  0x01,  0x00, 0x00, 64},
	{0x10,  0x03,  0x11,  0x6f,  0x01,  0x0c, 0x00, 76},
	{0x01,  0x03,  0x11,  0x6f,  0x01,  0x1a, 0x00, 90},
	{0x11,  0x03,  0x11,  0x6f,  0x01,  0x2b, 0x00, 107},
	{0x02,  0x03,  0x11,  0x6f,  0x02,  0x00, 0x00, 128},
	{0x12,  0x03,  0x11,  0x6f,  0x02,  0x18, 0x00, 152},
	{0x03,  0x03,  0x11,  0x6f,  0x02,  0x33, 0x00, 179},
	{0x13,  0x03,  0x11,  0x6f,  0x03,  0x15, 0x00, 213},
	{0x04,  0x03,  0x11,  0x6f,  0x04,  0x00, 0x00, 256},
	{0x14,  0x03,  0x11,  0x6f,  0x04,  0xe0, 0x00, 305},
	{0x05,  0x03,  0x11,  0x6f,  0x05,  0x26, 0x00, 358},
	{0x15,  0x03,  0x11,  0x6f,  0x06,  0x2b, 0x00, 427},
	{0x44,  0x03,  0x11,  0x6f,  0x08,  0x00, 0x00, 512},
	{0x54,  0x03,  0x11,  0x6f,  0x09,  0x22, 0x00, 610},
	{0x45,  0x03,  0x11,  0x6f,  0x0b,  0x0d, 0x00, 717},
	{0x55,  0x03,  0x11,  0x6f,  0x0d,  0x16, 0x00, 854},
	{0x04,  0x19,  0x16,  0x6f,  0x10,  0x00, 0x01, 1024},
};

static int gc2083_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 1920;
	info->modes.mode[0].size.h = 1080;
	info->modes.mode[0].fps = g_gc2083_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x37;
	info->i2c.addr_len = 2;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 1000);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_NONE, 0);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 1000);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_27M, 1000);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_HIGH, 1000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 1000);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_RST_GPIO, 0, 10000);
	set_power_item(&down->items[i++], SNR_PWDN_GPIO, 0, 100000);
	set_power_item(&down->items[i++], SNR_HCLK, 0, 10000);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, 0, 5000);
	set_power_item(&down->items[i++], SNR_CORE_POWER, 0, 5000);
	set_power_item(&down->items[i++], SNR_IO_POWER, 0, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static const struct fps_info *gc2083_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_gc2083_fps_info); i++)
		if (fps == g_gc2083_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_gc2083_fps_info))
		return NULL;
	return &g_gc2083_fps_info[i];
}

static int gc2083_get_init_info(uint32_t isp_id,
				const struct rts_isp_sensor_mode *mode,
				struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct gc2083_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("gc2083 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = gc2083_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->hts);

	set_init_i2c_regs(info->sensor_regs[0], g_gc2083_i2c_init_regs, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0 | MIPI_LANE1;
	info->interface.mipi.hs_term = 0x6;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 1920;
	info->size.h = 1080;
	info->start.x = 0;
	info->start.y = 0;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = status->min_vts = 1125;
	info->max_vts = 65536;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */

	return RTS_ISP_OK;
}

static int gc2083_start(uint32_t isp_id)
{
	struct gc2083_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static uint16_t get_sensor_gain_reg(float fgain, struct gc2083_status *status)
{
	int i;
	uint16_t gain = fgain * 64;

	if (gain >= 1024) {
		gain = 1024;
		status->num = 16;
	} else {
		for (i = 0; i < ARRAY_SIZE(g_gc2083_gain_table) - 1; i++) {
			if (gain >= g_gc2083_gain_table[i].value &&
			    gain < g_gc2083_gain_table[i + 1].value) {
				gain = g_gc2083_gain_table[i].value;
				status->num = i;
				break;
			}
		}
	}

	return gain;
}

static float get_sensor_real_gain(uint16_t reg_value)
{
	return reg_value / 64.0f;
}

static int gc2083_get_tuned_again(uint32_t isp_id,
				  float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;
	struct gc2083_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	gain_reg = get_sensor_gain_reg(again[0], status);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int gc2083_get_tuned_dgain(uint32_t isp_id,
				  float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int gc2083_get_exposure_gain_info(uint32_t isp_id,
				const struct rts_isp_sensor_exp_gain *exp_gain,
				struct rts_isp_sync_regs *regs)
{
	int i;
	uint32_t vts;
	struct gc2083_status *status;
	struct rts_isp_sync_reg *reg;
	uint16_t exposure_rows;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	vts = exp_gain->vts;
	reg = regs->reg;

	i = 0;
	if (abs(status->last_exposure - exp_gain->exposure[0]) > 0.001f) {
		exposure_rows = exp_gain->exposure[0] / status->exp_step + 0.5f;
		/*set exptime*/
		set_sync_i2c(&reg[i++], 0x0d03, exposure_rows >> 8);
		set_sync_i2c(&reg[i++], 0x0d04, exposure_rows & 0xff);
		status->last_exposure = exp_gain->exposure[0];
	}
	/*set gain*/
	set_sync_i2c(&reg[i++], 0x00d0,
		g_gc2083_gain_table[status->num].reg_00d0);

	set_sync_i2c(&reg[i++], 0x0155,
		g_gc2083_gain_table[status->num].reg_0155);
	set_sync_i2c(&reg[i++], 0x0410,
		g_gc2083_gain_table[status->num].reg_0410);
	set_sync_i2c(&reg[i++], 0x0411,
		g_gc2083_gain_table[status->num].reg_0410);
	set_sync_i2c(&reg[i++], 0x0412,
		g_gc2083_gain_table[status->num].reg_0410);
	set_sync_i2c(&reg[i++], 0x0413,
		g_gc2083_gain_table[status->num].reg_0410);
	set_sync_i2c(&reg[i++], 0x0414,
		g_gc2083_gain_table[status->num].reg_0414);
	set_sync_i2c(&reg[i++], 0x0415,
		g_gc2083_gain_table[status->num].reg_0414);
	set_sync_i2c(&reg[i++], 0x0416,
		g_gc2083_gain_table[status->num].reg_0414);
	set_sync_i2c(&reg[i++], 0x0417,
		g_gc2083_gain_table[status->num].reg_0414);

	set_sync_i2c(&reg[i++], 0x00b8,
		g_gc2083_gain_table[status->num].reg_00b8);
	set_sync_i2c(&reg[i++], 0x00b9,
		g_gc2083_gain_table[status->num].reg_00b9);

	set_sync_i2c(&reg[i++], 0x031d, 0x2e);
	set_sync_i2c(&reg[i++], 0x0dc1,
		g_gc2083_gain_table[status->num].reg_0dc1);
	set_sync_i2c(&reg[i++], 0x031d, 0x28);
	/*set frame length*/
	set_sync_i2c(&reg[i++], 0x0d41, vts >> 8);
	set_sync_i2c(&reg[i++], 0x0d42, vts & 0xff);
	regs->num = i;

	return RTS_ISP_OK;
}

static int gc2083_get_mirror_flip(uint32_t isp_id,
				   const struct rts_isp_mirror_flip *mf_info,
				   struct rts_isp_sync_regs *regs)
{
	int i = 0;
	uint32_t val = 0;
	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !mf_info || !regs)
		return -RTS_ISP_EINVAL;

	rts_isp_drop_frames(isp_id, 1);
	if (mf_info->mirror)
		val |= 0x1;
	if (mf_info->flip)
		val |= 0x2;
	reg = regs->reg;
	set_sync_i2c_mask(&reg[i++], 0x0015, val, 0x03);
	set_sync_i2c_mask(&reg[i++], 0x0d15, val, 0x03);
	regs->num = i;

	return RTS_ISP_OK;
}

static int gc2083_check(uint32_t isp_id)
{
	int ret;
	int id;
	struct rts_isp_i2c_reg reg = {};

	reg.addr = 0x03f0;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id = reg.data << 8;

	reg.addr = 0x03f1;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id |= reg.data;

	if (id == 0x2083)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops gc2083_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "gc2083",
	.get_info = gc2083_get_info,
	.get_init_info = gc2083_get_init_info,
	.start = gc2083_start,
	.get_tuned_again = gc2083_get_tuned_again,
	.get_tuned_dgain = gc2083_get_tuned_dgain,
	.get_exposure_gain_info = gc2083_get_exposure_gain_info,
	.get_mirror_flip = gc2083_get_mirror_flip,
	.check = gc2083_check,
};


RTS_ISP_DEFINE_SENSOR_PLUGIN(gc2083_ops)
