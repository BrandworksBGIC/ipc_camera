/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2024 George <george_liu@realsil.com.cn>
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
	uint16_t vts;
	uint32_t clk;
};

struct ov01d1r_status {
	float exp_step;
	float last_exposure;
	uint16_t min_vts;
};

static struct ov01d1r_status g_status[SUPPORTED_ISP_NUM];
static const struct fps_info g_ov01d1r_fps_info[] = {
	{30, 1528, 1676, 76800000} //640x360 ov china mono
};

static struct rts_isp_i2c_reg g_ov01d1r_i2c_init_regs[] = {
	//;;;;;;;;;;System Setting;;;;;;;;;;
	{0xfd, 0x00},//
	{0x20, 0x00},//
	{0x20, 0x07},//
	{0x8b, 0x21},//
	{0x2e, 0x80},//
	{0x31, 0x94},//
	{0x33, 0x04},//
	{0x41, 0x88},//
	{0xfd, 0x01},//
	{0x0f, 0x01},//  ;;[0]:mipi en
	{0x17, 0x85},// ;;[7:4]:vs_delay_num [3:0]:add_delay_num
	{0x31, 0x14},// ;;COMMC1,8'b76543210
	{0xfd, 0x05},//
	{0xef, 0x60},// ;;  A2D offset  （n2p）
	{0x01, 0x01},// ;;[0]:exp &gain update command,self zero
	//;;EXP Setting;;;;;;;;;;
	{0xfd, 0x01},//
	{0x03, 0x00},//
	{0x04, 0x60},//
	{0x01, 0x01},// ;;[0]:exp &gain update command,self zero
	//;;Vblank Setting;;;;;;;;;;
	{0xfd, 0x01},//
	{0x15, 0x03},//
	{0x16, 0x93},//
	{0x01, 0x01},// ;;[0]:exp &gain update command,self zero
	//;;Hblank Setting;;;;;;;;;;
	{0xfd, 0x01},//
	{0x78, 0x00},// ;;P78 hb_lp_pwd(H8)
	{0x79, 0x6f},// ;;P79 hb_lp_pwd(L8)
	{0x7b, 0x92},//
	{0x01, 0x01},// ;;[0]:exp &gain update command,self zero
	//;;Pixel Setting;;;;;;;;;;
	{0xfd, 0x01},//
	{0x50, 0xef},// ;;P50
	{0x51, 0x09},// ;;P51
	{0x52, 0x09},// ;;P52
	{0x53, 0xef},// ;;P53
	{0x56, 0x00},// ;;P56
	{0x57, 0x02},// ;;P57
	{0x59, 0x03},// ;;P59
	{0x5a, 0x03},// ;;P5a
	{0x5c, 0x23},// ;;P5c
	{0x5e, 0x05},// ;;P5e
	{0x60, 0x0c},// ;;P60
	{0x61, 0x02},// ;;P61 tx_rb_position
	{0x62, 0x02},// ;;P62 tx_ra_position
	{0x67, 0x09},// ;;P67
	{0x6a, 0x09},// ;;P6a
	{0x76, 0x0e},// ;;P76 col_pix_sw
	{0x77, 0x46},// ;;P77
	{0x82, 0x02},// ;;P82
	{0x89, 0x0e},// ;;P89 cnt_rst
	{0x8b, 0x00},// ;;P8b
	{0x8d, 0x0c},// ;;P8d
	{0x90, 0x3f},// ;;P90
	{0x91, 0x0c},// ;;P91
	{0x92, 0x20},// ;;P92
	{0x93, 0x22},// ;;P93
	{0x94, 0x0c},// ;;P94
	{0x95, 0x4c},// ;;P95
	{0x9b, 0x01},// ;;P9b pulse1_oppsition
	{0x9c, 0x04},// ;;P9c boost_1st pulse width
	{0x9d, 0x23},// ;;P9d pulse2_oppsition
	{0x9e, 0x04},// ;;P9e boost_2nd pulse width
	{0xa0, 0x58},// ;;Pa0
	{0xa4, 0x00},// ;;Pa4
	{0xa6, 0x00},// ;;Pa6
	{0xa7, 0xe8},// ;;Pa7
	{0xa8, 0x27},// ;;Pa8
	{0xa9, 0x0e},// ;;Pa9
	{0xac, 0x35},// ;;Pac
	{0xae, 0x25},// ;;Pae
	{0xaf, 0x11},// ;;Paf
	{0xbf, 0x0a},//
	{0x49, 0x00},//
	{0xc7, 0x44},//
	{0xc8, 0x55},//
	{0xca, 0x2a},//
	{0xcb, 0x30},//
	{0xcc, 0x32},//
	{0xcd, 0x34},//
	{0xce, 0x28},//
	{0xcf, 0x2e},//
	{0xd0, 0x30},//
	{0xd1, 0x32},//
	{0xe4, 0x08},//
	//;;Timing Ctrl
	{0xfd, 0x01},//
	{0x12, 0x00},//
	{0x40, 0x02},//
	{0x41, 0x04},//
	{0x42, 0xd5},//
	{0x43, 0x04},//
	{0x44, 0x01},//
	{0x46, 0xf7},//
	{0x47, 0x4f},//
	{0x48, 0x40},//
	{0x4c, 0x5f},//
	//;;Gain Setting;;;;;;;;;;
	{0xfd, 0x01},//
	{0x05, 0x80},//  ;; Again
	{0x07, 0x80},//  ;; Again
	{0x09, 0x40},// ;; Dgain
	{0x0a, 0x00},// ;; Dgain
	{0x0b, 0x40},// ;; Dgain
	{0x0c, 0x00},// ;; Dgain
	{0x01, 0x01},// ;;[0]:exp &gain update command,self zero
	//;;SCG Setting;;;;;;;;;;
	{0xfd, 0x01},//
	{0x0d, 0x03},// ;;hcg
	{0x01, 0x01},// ;;[0]:exp &gain update command,self zero
	//;;PSNC Setting;;;;;;;;;;
	{0xfd, 0x01},//
	{0xba, 0x13},// ;;[7:0]: psnc_gain_sel_x1
	{0xbb, 0x4c},// ;;[7:0]: psnc_gain_sel_x2
	{0xbc, 0x25},// ;;[7:0]: psnc_gain_sel_x3
	{0xbd, 0xaa},// ;;[7:0]: psnc_gain_sel_x4
	{0xe5, 0x04},// ;;[5:0]: psnc_cap_op2_sel_hcg
	{0xe6, 0x04},// ;;[5:0]: psnc_cap_op2_sel_lcg
	{0xe7, 0x56},// ;;[7:0]: psnc_cload_sel_hcg
	{0xe8, 0x9d},// ;;[7:0]: psnc_cload_sel_lcg
	{0xe9, 0xa3},//
	{0xea, 0xb4},// ;;[7:4]: psnc_res_sel_hcg [3:0]psnc_res_sel_lcg
	{0xef, 0x04},//
	//;;Test Ctrl;;;;;;;;;;
	{0xfd, 0x05},//
	{0x0d, 0x00},// ;;ISP colorbar
	//;;Current&Voltage Ctrl;;;;;;;;;;
	{0xfd, 0x05},//
	{0x6f, 0x01},// ;;[2:0]ibias_sel [0]=1 64u
	{0x74, 0x95},//
	{0x75, 0xa8},// ;;[7]: adc_range 692mV
	{0x76, 0x10},//
	{0x7f, 0x61},// ;;[6]: v2i_cas [5:0]: v2i_ibias_trim
	{0x80, 0x6a},// ;;[7:4]: vref_ncp_sel [3:0]: vref_pcp_sel
	{0x81, 0x1e},// ;;[5:4]: vref_vcap_bias_sel [3:0]: vref_vcaphi_sel
	//;;DAC Ctrl;;;;;;;;;;
	{0xfd, 0x01},//
	{0xe0, 0x03},// ;;[5:0]: dac_clamp_sel_x1
	{0xe1, 0x03},// ;;[5:0]: dac_clamp_sel_x2
	{0xe2, 0x03},// ;;[5:0]: dac_clamp_sel_x3
	{0xe3, 0x03},// ;;[5:0]: dac_clamp_sel_x4
	//;;BLK Ctrl;;;;;;;;;;
	{0xfd, 0x01},//
	{0xc3, 0x55},// ;;[7:4]: bsun_rst_sel_x2_hcg  [3:0]: bsun_rst_sel_x1_hcg
	{0xc4, 0x55},// ;;[7:4]: bsun_rst_sel_x4_hcg  [3:0]: bsun_rst_sel_x3_hcg
	{0xc5, 0x00},// ;;[7:4]: bsun_sig_sel_x2_hcg  [3:0]: bsun_sig_sel_x1_hcg
	{0xc6, 0x00},// ;;[7:4]: bsun_sig_sel_x4_hcg  [3:0]: bsun_sig_sel_x3_hcg
	{0xd4, 0x55},// ;;[7:4]: bsun_rst_sel_x2_lcg  [3:0]: bsun_rst_sel_x1_lcg
	{0xd5, 0x55},// ;;[7:4]: bsun_rst_sel_x4_lcg  [3:0]: bsun_rst_sel_x3_lcg
	{0xd6, 0x00},// ;;[7:4]: bsun_sig_sel_x2_lcg  [3:0]: bsun_sig_sel_x1_lcg
	{0xd7, 0x00},// ;;[7:4]: bsun_sig_sel_x4_lcg  [3:0]: bsun_sig_sel_x3_lc
	{0xfd, 0x05},//
	{0x7b, 0x03},//
	{0x7d, 0x70},//
	//;;pd2&pd3 Ctrl
	{0xfd, 0x05},//
	{0x77, 0x7f},//
	{0x78, 0xdf},//
	{0x79, 0xf8},//
	//;;MIPI Setting;;;;;;;;;;
	{0xfd, 0x00},//
	{0x8c, 0x0c},//
	{0x8d, 0x00},// ;;[1]: mipi_p2s_pwd [0]: mipi_pwd_sel
	{0xfd, 0x02},//
	{0x51, 0x01},// ;;MIPI VFIFO start size
	{0x92, 0x00},//
	{0x95, 0x08},// ;;MIPI HS prepare
	{0x96, 0x09},// ;;MIPI HS zero
	{0x9f, 0x05},// ;;MIPI LPX
	{0x8c, 0x09},//
	{0x8d, 0x06},// ;;MIPI CK prepare
	{0x9c, 0x1b},// ;;MIPI CK zero
	{0x9f, 0x06},//
	{0xc1, 0x05},// ;;MIPI timing mannual mode
	{0xc4, 0x01},//
	{0x5e, 0x20},// ;;cut size enable
	{0xb7, 0x00},// ;;mipi colorbar;;crop
	{0xfd, 0x02},//
	{0x5e, 0x20},//
	{0xa0, 0x00},//
	{0xa1, 0x03},// ;v start
	{0xa2, 0x01},//
	{0xa3, 0x68},// ;v size 360
	{0xa4, 0x00},//
	{0xa5, 0x02},// ;h start
	{0xa6, 0x02},//
	{0xa7, 0x80},// ;h size 640
	{0xfd, 0x02},//
	{0x8e, 0x02},//
	{0x8f, 0x80},// ;h_size 640
	{0x90, 0x01},//
	{0x91, 0x68},// ;v_size 360
	//;;OTP&ISP Setting;;;;;;;;;;
	{0xfd, 0x03},//
	{0x9d, 0x0f},//
	//;;BLC Setting;;;;;;;;;;
	{0xfd, 0x07},//
	{0x00, 0xf7},//  ;; 77=BLC OFF
	{0x01, 0x60},//
	{0xfd, 0x00},//
	{0x20, 0x1f},//
	{0x36, 0x20},//
	{0xfd, 0x01},//
	{0x01, 0x01},//
};

static int ov01d1r_get_info(uint32_t isp_id,
		struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 640;
	info->modes.mode[0].size.h = 360;
	info->modes.mode[0].fps = g_ov01d1r_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x10;
	info->i2c.addr_len = 1;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 0);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 0);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V2, 5000);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 0);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 9000);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_HIGH, 0);

	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&down->items[i++], SNR_PWDN_GPIO, GPIO_HIGH, 0);
	set_power_item(&down->items[i++], SNR_HCLK, 0, 0);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_IO_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_CORE_POWER, 0, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static const struct fps_info *ov01d1r_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_ov01d1r_fps_info); i++)
		if (fps == g_ov01d1r_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_ov01d1r_fps_info))
		return NULL;

	return &g_ov01d1r_fps_info[i];
}

static int ov01d1r_get_init_info(uint32_t isp_id,
				 const struct rts_isp_sensor_mode *mode,
				struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct ov01d1r_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("ov01d1r get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = ov01d1r_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, clk_div: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->clk_div, fps_info->hts);

	set_init_i2c_regs(info->sensor_regs[0], g_ov01d1r_i2c_init_regs, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0;
	info->interface.mipi.hs_term = 0x5;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 640;
	info->size.h = 360;
	info->start.x = 0;
	info->start.y = 0;

	info->hts = fps_info->hts;
	info->min_vts = fps_info->vts;
	info->pclk = fps_info->clk;
	info->max_vts = 65536;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */
	status->min_vts =  fps_info->vts;

	return RTS_ISP_OK;
}

static int ov01d1r_start(uint32_t isp_id)
{
	struct ov01d1r_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static uint16_t get_sensor_gain_reg(float fgain)
{
	uint16_t reg_value = 0;

	reg_value = (uint16_t)(fgain * 16);
	if (reg_value < 0xff)
		return  reg_value;
	else
		return 0xff;
}

static float get_sensor_real_gain(uint8_t reg_value)
{
	return ((float)reg_value / 16.0);
}


static int ov01d1r_get_tuned_again(uint32_t isp_id,
				   float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int ov01d1r_get_tuned_dgain(uint32_t isp_id,
				   float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

uint32_t clip_d_word(uint32_t current, uint32_t minimum, uint32_t maximum)
{
	if (current > maximum)
		return maximum;
	if (current < minimum)
		return minimum;
	return current;
}

static int ov01d1r_get_exposure_gain_info(uint32_t isp_id,
				 const struct rts_isp_sensor_exp_gain *exp_gain,
				 struct rts_isp_sync_regs *regs)
{
	int i;
	uint16_t gain_reg;
	uint16_t total_line;
	uint16_t dummy;
	uint32_t exp_reg_value;
	struct ov01d1r_status *status;
	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	gain_reg = get_sensor_gain_reg(exp_gain->analog_gain[0] *
				       exp_gain->digital_gain[0]);
	reg = regs->reg;

	i = 0;
	if (abs(status->last_exposure - exp_gain->exposure[0]) > 0.001f) {
		uint32_t exposure;

		exposure = exp_gain->exposure[0] / status->exp_step + 0.5f;
		/* page */
		set_sync_i2c(&reg[i++], 0xfd, 1);
		/* vts */
		total_line = exp_gain->vts;
		dummy = exp_gain->vts - status->min_vts + 915;
		set_sync_i2c(&reg[i++], 0x15, dummy >> 8);
		set_sync_i2c(&reg[i++], 0x16, dummy & 0xff);

		/* exposure */
		exp_reg_value = clip_d_word(exposure, 1, total_line - 16);
		set_sync_i2c(&reg[i++], 0x03, (exp_reg_value >> 8) & 0xff);
		set_sync_i2c(&reg[i++], 0x04, exp_reg_value & 0xff);

		/* gain */
		set_sync_i2c(&reg[i++], 0x05, gain_reg);
		set_sync_i2c(&reg[i++], 0x07, gain_reg);

		status->last_exposure = exp_gain->exposure[0];

		/* end & launch group1 */
		set_sync_i2c(&reg[i++], 0x01, 0x01);
	} else {
		/* page */
		set_sync_i2c(&reg[i++], 0xfd, 1);

		/* gain */
		set_sync_i2c(&reg[i++], 0x05, gain_reg);
		set_sync_i2c(&reg[i++], 0x07, gain_reg);
		/* vts */
		dummy = exp_gain->vts - status->min_vts + 915;
		set_sync_i2c(&reg[i++], 0x15, dummy >> 8);
		set_sync_i2c(&reg[i++], 0x16, dummy & 0xff);

		/* end & launch group1 */
		set_sync_i2c(&reg[i++], 0x01, 0x01);
	}

	regs->num = i;

	return RTS_ISP_OK;
}

static int ov01d1r_check(uint32_t isp_id)
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

	if (id == 0x5601)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops ov01d1r_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "ov01d1r",
	.get_info = ov01d1r_get_info,
	.get_init_info = ov01d1r_get_init_info,
	.start = ov01d1r_start,
	.get_tuned_again = ov01d1r_get_tuned_again,
	.get_tuned_dgain = ov01d1r_get_tuned_dgain,
	.get_exposure_gain_info = ov01d1r_get_exposure_gain_info,
	.check = ov01d1r_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(ov01d1r_ops)
