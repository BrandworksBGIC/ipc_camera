/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2017 Grant Shen <grant_shen@realsil.com.cn>
 * Copyright (C) 2019 Sherry Cheng <sherry_cheng@realsil.com.cn>
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

struct gc2053_status {
	int min_vts;
	float exp_step;
	float last_exposure;
	int num;
	struct rts_isp_i2c_reg regs1[3];
};

struct gc2053_gain_config {
	uint8_t reg_b4;
	uint8_t reg_b3;
	uint8_t reg_b8;
	uint8_t reg_b9;
	uint16_t value;
};

static struct gc2053_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_gc2053_fps_info[] = {
	{30, 2200, 74250000},
};

static struct rts_isp_i2c_reg g_gc2053_i2c_init_regs[] = {
	/****system****/
	{0xfe, 0x80},
	{0xfe, 0x80},
	{0xfe, 0x80},
	{0xfe, 0x00},
	{0xf2, 0x00},
	{0xf3, 0x00},
	{0xf4, 0x36},
	{0xf5, 0xc0},
	{0xf6, 0x44},
	{0xf7, 0x01},
	{0xf8, 0x2c},
	{0xf9, 0x42},
	{0xfc, 0x8e},
	/****CISCTL & ANALOG****/
	{0xfe, 0x00},
	{0x87, 0x18}, //[6]aec_delay_mode
	{0xee, 0x30}, //[5:4]dwen_sramen
	{0xd0, 0xb7}, //ramp_en
	{0x03, 0x04},
	{0x04, 0x10},
	{0x05, 0x04}, //05
	{0x06, 0x4c}, //60//[11:0]hb
	{0x07, 0x00},
	{0x08, 0x0c},
	{0x09, 0x00},
	{0x0a, 0x02}, //cisctl row start
	{0x0b, 0x00},
	{0x0c, 0x02}, //cisctl col start
	{0x12, 0xe2}, //vsync_ahead_mode
	{0x13, 0x16},
	{0x19, 0x0a}, //ad_pipe_num
	{0x21, 0x1c}, //eqc1fc_eqc2fc_sw
	{0x28, 0x0a}, //16//eqc2_c2clpen_sw
	{0x29, 0x24}, //eq_post_width
	{0x2b, 0x04}, //c2clpen --eqc2
	{0x32, 0xf8}, //[5]txh_en ->avdd28
	{0x37, 0x03}, //[3:2]eqc2sel=0
	{0x39, 0x17}, //[3:0]rsgl
	{0x44, 0x40}, //0e//post_tx_width
	{0x46, 0x0d},
	{0x4b, 0x20}, //rst_tx_width
	{0x4e, 0x08}, //12//ramp_t1_width
	{0x55, 0x20}, //read_tx_width_pp
	{0x66, 0x05}, //18//stspd_width_r1
	{0x67, 0x05}, //40//5//stspd_width_r
	{0x77, 0x00}, //dacin  offset x31
	{0x78, 0x20}, //dacin offset
	{0x7c, 0xb3}, //[1:0] co1comp
	{0x8c, 0x12}, //12 ramp_t1_ref
	{0x8d, 0x92},
	{0x90, 0x00},
	{0x41, 0x04},
	{0x42, 0x64},
	{0x9d, 0x10},
	{0xce, 0x6c}, //70//78//[4:2]c1isel
	{0xd0, 0xd7},
	{0xd2, 0x41}, //[5:3]c2clamp
	{0xd3, 0x54}, //{0x39[7]=0,0xd3[3]=1 rsgh=vref
	{0xe6, 0x40}, //ramps offset
	/*gain*/
	{0xb6, 0xC0},//80->dynamic dpc ===c0
	{0xb0, 0x58},
	/*blk*/
	{0x26, 0x20},
	{0xfe, 0x01},
	{0x40, 0x23},
	{0x60, 0x40}, //[7:0]WB_offset
	/*window*/
	{0xfe, 0x01},
	{0x94, 0x01},
	{0x95, 0x04},
	{0x96, 0x38}, //[10:0]out_height//40
	{0x97, 0x07},
	{0x98, 0x88}, //[11:0]out_width
	/*ISP*/
	{0xfe, 0x01},
	{0x01, 0x04}, //[3]dpc blending mode
	{0x02, 0x89}, //[7:0]BFF_sram_mode
	{0x04, 0x01}, //[0]DD_en
	{0x50, 0x1c},
	{0x89, 0x03},
	/*dpc*/
	{0xfe, 0x04},
	{0x28, 0x86},
	{0x29, 0x86},
	{0x2a, 0x86},
	{0x2b, 0x68},
	{0x2c, 0x68},
	{0x2d, 0x68},
	{0x2e, 0x68},
	{0x2f, 0x68},
	{0x30, 0x4f},
	{0x31, 0x68},
	{0x32, 0x67},
	{0x33, 0x66},
	{0x34, 0x66},
	{0x35, 0x66},
	{0x36, 0x66},
	{0x37, 0x66},
	{0x38, 0x62},
	{0x39, 0x62},
	{0x3a, 0x62},
	{0x3b, 0x62},
	{0x3c, 0x62},
	{0x3d, 0x62},
	{0x3e, 0x62},
	{0x3f, 0x62},
	/****DVP & MIPI****/
	{0xfe, 0x01},
	{0x9a, 0x03},
	{0x99, 0x01},
	{0xfe, 0x00},
	{0x7b, 0x2a},
	{0x23, 0x2d},
	{0xfe, 0x03},
	{0x01, 0x27},
	{0x02, 0x56},
	{0x03, 0xb6},
	{0x12, 0x88},
	{0x13, 0x07},
	{0x15, 0x12},
	{0xfe, 0x00},
	{0x3e, 0x91},
};

static struct gc2053_gain_config g_gc2035_gain_config[] = {
	{0x00, 0x00, 0x01, 0x00, 64},
	{0x00, 0x10, 0x01, 0x0c, 76},
	{0x00, 0x20, 0x01, 0x1b, 90},
	{0x00, 0x30, 0x01, 0x2c, 106},
	{0x00, 0x40, 0x01, 0x3f, 128},
	{0x00, 0x50, 0x02, 0x16, 152},
	{0x00, 0x60, 0x02, 0x35, 179},
	{0x00, 0x70, 0x03, 0x16, 212},
	{0x00, 0x80, 0x04, 0x02, 256},
	{0x00, 0x90, 0x04, 0x31, 303},
	{0x00, 0xa0, 0x05, 0x32, 358},
	{0x00, 0xb0, 0x06, 0x35, 425},
	{0x00, 0xc0, 0x08, 0x04, 512},
	{0x00, 0x5a, 0x09, 0x19, 607},
	{0x00, 0x83, 0x0b, 0x0f, 717},
	{0x00, 0x93, 0x0d, 0x12, 849},
	{0x00, 0x84, 0x10, 0x00, 1024},
};

static int gc2053_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 1920;
	info->modes.mode[0].size.h = 1080;
	info->modes.mode[0].fps = g_gc2053_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x37;
	info->i2c.addr_len = 1;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 1000);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V2, 1000);
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

static const struct fps_info *gc2053_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_gc2053_fps_info); i++)
		if (fps == g_gc2053_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_gc2053_fps_info))
		return NULL;
	return &g_gc2053_fps_info[i];
}

static int gc2053_get_init_info(uint32_t isp_id,
				const struct rts_isp_sensor_mode *mode,
				struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct gc2053_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("gc2053 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = gc2053_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->hts);

	set_init_i2c(&status->regs1[0], 0xfe, 0x00);
	set_init_i2c(&status->regs1[1], 0x05, (fps_info->hts >> 1) >> 8);
	set_init_i2c(&status->regs1[2], 0x06, (fps_info->hts >> 1) & 0xff);

	set_init_i2c_regs(info->sensor_regs[0], g_gc2053_i2c_init_regs, 0);
	set_init_i2c_regs(info->sensor_regs[1], status->regs1, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0 | MIPI_LANE1;
	info->interface.mipi.hs_term = 0x6;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 1921;
	info->size.h = 1080;
	info->start.x = 1;
	info->start.y = 0;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = status->min_vts = 1125;
	info->max_vts = 65536;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */

	return RTS_ISP_OK;
}

static int gc2053_start(uint32_t isp_id)
{
	struct gc2053_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static uint16_t get_sensor_gain_reg(float fgain, struct gc2053_status *status)
{
	int i;
	uint16_t gain = fgain * 64;

	if (gain >= 1024) {
		gain = 1024;
		status->num = 16;
	} else {
		for (i = 0; i < ARRAY_SIZE(g_gc2035_gain_config) - 1; i++) {
			if (gain >= g_gc2035_gain_config[i].value &&
			    gain < g_gc2035_gain_config[i + 1].value) {
				gain = g_gc2035_gain_config[i].value;
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

static int gc2053_get_tuned_again(uint32_t isp_id,
				  float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;
	struct gc2053_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	gain_reg = get_sensor_gain_reg(again[0], status);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int gc2053_get_tuned_dgain(uint32_t isp_id,
				  float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int gc2053_get_exposure_gain_info(uint32_t isp_id,
					 const struct rts_isp_sensor_exp_gain *exp_gain,
					 struct rts_isp_sync_regs *regs)
{
	int i;
	uint32_t vts;
	struct gc2053_status *status;
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
		set_sync_i2c(&reg[i++], 0xfe, 0x00);
		set_sync_i2c(&reg[i++], 0x03, exposure_rows >> 8);
		set_sync_i2c(&reg[i++], 0x04, exposure_rows & 0xff);
		status->last_exposure = exp_gain->exposure[0];
	}
	set_sync_i2c(&reg[i++], 0xfe, 0x00);
	set_sync_i2c(&reg[i++], 0xb3, g_gc2035_gain_config[status->num].reg_b3);
	set_sync_i2c(&reg[i++], 0xb4, g_gc2035_gain_config[status->num].reg_b4);
	set_sync_i2c(&reg[i++], 0xb8, g_gc2035_gain_config[status->num].reg_b8);
	set_sync_i2c(&reg[i++], 0xb9, g_gc2035_gain_config[status->num].reg_b9);
	set_sync_i2c(&reg[i++], 0x41, vts >> 8);
	set_sync_i2c(&reg[i++], 0x42, vts & 0xff);
	regs->num = i;

	return RTS_ISP_OK;
}

static int gc2053_check(uint32_t isp_id)
{
	int ret;
	int id;
	struct rts_isp_i2c_reg reg = {};

	reg.addr = 0xf0;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id = reg.data << 8;

	reg.addr = 0xf1;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id |= reg.data;

	if (id == 0x2053)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops gc2053_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "gc2053",
	.get_info = gc2053_get_info,
	.get_init_info = gc2053_get_init_info,
	.start = gc2053_start,
	.get_tuned_again = gc2053_get_tuned_again,
	.get_tuned_dgain = gc2053_get_tuned_dgain,
	.get_exposure_gain_info = gc2053_get_exposure_gain_info,
	.check = gc2053_check,
};


RTS_ISP_DEFINE_SENSOR_PLUGIN(gc2053_ops)
