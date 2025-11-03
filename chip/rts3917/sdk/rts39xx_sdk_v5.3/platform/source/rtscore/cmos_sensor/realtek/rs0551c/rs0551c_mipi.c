/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2017 Grant Shen <grant_shen@realsil.com.cn>
 * Copyright (C) 2019 Sherry Cheng <sherry_cheng@realsil.com.cn>
 */

#include <stdio.h>
#include <rts_isp_sensor.h>

#define MIPI_HS_TERM 0x4

/* #define FPGA_FPS_SETTING */

#define RESOLUTION_2M
/* #define RESOLUTION_5M */
// #define RESOLUTION_6M

/*
 * config introduction
 * raw 8 & raw 10: 4 lane & 2lane-25fps 180Mhz, 1 lane-15fps 108Mhz
 * raw 12: 4 lane & 2lane-20fps 160Mhz, 1 lane-10fps 80Mhz
 */

/* #define RAW_8BIT */
#define RAW_10BIT
/* #define RAW_12BIT */
/* #define YUV_YUYV8 */

/* #define DEBUG */
#ifdef DEBUG
#define debug(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

#define SUPPORTED_ISP_NUM 1

#define L_BYTE(num) ((num) & 0xff)
#define H_BYTE(num) ((num) >> 8)

#ifdef RESOLUTION_2M
#define SENSOR_WIDTH 1920
#define SENSOR_HEIGHT 1080
#define SENSOR_VTS 1100
#ifdef FPGA_FPS_SETTING
#define FPS 5
#define PCLK 36000000
#else
#define FPS 25
#define PCLK 72000000
#endif
#endif

#ifdef RESOLUTION_5M
#define SENSOR_WIDTH 2592
#define SENSOR_HEIGHT 1944
#define SENSOR_VTS 1960
#ifdef FPGA_FPS_SETTING
#define FPS 5
#define PCLK 36000000
#else
#define FPS 25
#define PCLK 180000000
#endif
#endif

#ifdef RESOLUTION_6M
#define SENSOR_WIDTH 3072
#define SENSOR_HEIGHT 2048
#define SENSOR_VTS 2068
#ifdef FPGA_FPS_SETTING
#define FPS 5
#define PCLK 36000000
#else
#define FPS 5
#define PCLK 180000000
#endif
#endif

#ifdef RAW_8BIT
#define BIT_DEPTH SNR_8BIT
#define TYPE RAW_SENSOR
#endif
#ifdef RAW_10BIT
#define BIT_DEPTH SNR_10BIT
#define TYPE RAW_SENSOR
#endif
#ifdef RAW_12BIT
#define BIT_DEPTH SNR_12BIT
#define TYPE RAW_SENSOR
#endif
#ifdef YUV_YUYV8
#define BIT_DEPTH SNR_8BIT
#define TYPE YUV_SENSOR
#endif

struct rs0551c_mipi_config {
	enum rts_isp_sensor_bit_depth bit_depth;
	enum rts_isp_sensor_type type;
	uint16_t fps;
	uint16_t vts;
	uint32_t pclk;
	uint32_t width;
	uint32_t height;
	uint8_t lanes;
};

struct rs0551c_mipi_status {
	uint8_t lanes;
	uint8_t lanes_num;
	uint16_t hts;
	uint16_t mipi_clk;
	uint16_t divider;
};

static struct rs0551c_mipi_status g_status[SUPPORTED_ISP_NUM];

static const struct rs0551c_mipi_config g_rs0551c_mipi_config = {
	.bit_depth = BIT_DEPTH,
	.type = TYPE,
	.fps = FPS,
	.vts = SENSOR_VTS,
	.pclk = PCLK,
	.width = SENSOR_WIDTH,
	.height = SENSOR_HEIGHT,
	.lanes = MIPI_LANE0 | MIPI_LANE1,
};

static struct rts_isp_i2c_info rs0551c_mipi_i2c_info = {
	.i2c_id = 0x7a,
	.addr_len = 2,
	.data_len = 1,
};

static int rs0551c_mipi_init(uint32_t isp_id)
{
	struct rs0551c_mipi_status *status;
	uint8_t lanes;

	debug("rs0551c mipi init\n");
	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	status->lanes = lanes = g_rs0551c_mipi_config.lanes;
	while (lanes) {
		status->lanes_num++;
		lanes &= lanes - 1;
	};

	return RTS_ISP_OK;
}

static int rs0551c_mipi_get_info(uint32_t isp_id,
				 struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = g_rs0551c_mipi_config.width;
	info->modes.mode[0].size.h = g_rs0551c_mipi_config.height;
	info->modes.mode[0].fps = g_rs0551c_mipi_config.fps;
	info->modes.num = 1;

	info->i2c = rs0551c_mipi_i2c_info;

	i = 0;
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 20000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 0);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 0);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 20000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 100000);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_RST_GPIO, 0, 100);
	set_power_item(&down->items[i++], SNR_HCLK, 0, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static int rs0551c_mipi_get_pclk_vts(struct rs0551c_mipi_status *status)
{
	int data_width;

	data_width = g_rs0551c_mipi_config.bit_depth;

	status->mipi_clk = g_rs0551c_mipi_config.pclk * data_width /
			   status->lanes_num / 1000000;

	status->hts = (g_rs0551c_mipi_config.pclk / g_rs0551c_mipi_config.vts /
		       g_rs0551c_mipi_config.fps) & ~1;

	if (data_width / status->lanes_num >= 8)
		status->divider = 4;
	else if (data_width / status->lanes_num >= 4)
		status->divider = 2;
	else
		status->divider = 1;

	return RTS_ISP_OK;
}

static int rs0551c_mipi_get_init_info(uint32_t isp_id,
				      const struct rts_isp_sensor_mode *mode,
				      struct rts_isp_sensor_init_info *info)
{
	struct rs0551c_mipi_status *status;
	int ret = RTS_ISP_OK;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("rs0551c mipi get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];

	ret = rs0551c_mipi_get_pclk_vts(status);
	if (ret)
		goto out;
	debug("fps: %u, pclk: %u, hts: %u\n", g_rs0551c_mipi_config.mode->fps,
	      g_rs0551c_mipi_config.pclk, status->hts);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = g_rs0551c_mipi_config.lanes;
	info->interface.mipi.hs_term = MIPI_HS_TERM;
	info->interface.type = g_rs0551c_mipi_config.type;
	info->interface.bit_depth = g_rs0551c_mipi_config.bit_depth;
	if (info->interface.type == YUV_SENSOR)
		info->interface.type_config.yuv.order.yuv422 = SNR_YUYV;

	info->size.w = g_rs0551c_mipi_config.width;
	info->size.h = g_rs0551c_mipi_config.height;
	info->start.x = 0;
	info->start.y = 0;

	info->hts = status->hts;
	info->pclk = g_rs0551c_mipi_config.pclk;
	info->min_vts = g_rs0551c_mipi_config.vts;
	info->max_vts = g_rs0551c_mipi_config.vts;

out:
	return ret;
}


static int rs0551c_mipi_get_tuned_again(uint32_t isp_id,
					float again[RTS_ISP_HDR_CHAN_MAX])
{
	return RTS_ISP_OK;
}

static int rs0551c_mipi_get_tuned_dgain(uint32_t isp_id,
					float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	return RTS_ISP_OK;
}

static int rs0551c_mipi_get_exposure_gain_info(uint32_t isp_id,
					const struct rts_isp_sensor_exp_gain *exp_gain,
					struct rts_isp_sync_regs *regs)
{
	return RTS_ISP_OK;
}

static inline int write_snr_reg(uint16_t addr, uint16_t data)
{
	struct rts_isp_i2c_reg reg[1];

	reg[0].addr = addr;
	reg[0].data = data;
	return rts_isp_write_i2c_reg(&rs0551c_mipi_i2c_info, reg);
}

static int rs0551c_mipi_tx_aphy_init(void)
{
	// cmu debug enable
	write_snr_reg(0x042a, 0x00);
	// mipi power enable
	write_snr_reg(0x0410, 0x00);
	// CMU enable
	write_snr_reg(0x0409, 0x01);
	// tx power enable
	write_snr_reg(0x0400, 0x01);
	// cmu power value setting
	write_snr_reg(0x0426, 0x00);
	return RTS_ISP_OK;
}

static int rs0551c_mipi_tx_dphy_init(struct rs0551c_mipi_status *status)
{
	uint16_t tx_cfg, tx_type;

	if (g_rs0551c_mipi_config.type == YUV_SENSOR) {
		write_snr_reg(0x0301, 0x00);
		tx_type = 0x1e;
	} else {
		switch (g_rs0551c_mipi_config.bit_depth) {
		case SNR_8BIT:
			write_snr_reg(0x0301, 0x00);
			tx_type = 0x2a;
			break;
		case SNR_12BIT:
			write_snr_reg(0x0301, 0x02);
			tx_type = 0x2c;
			break;
		case SNR_10BIT:
		default:
			write_snr_reg(0x0301, 0x01);
			tx_type = 0x2b;
			break;
		}
	}

	tx_cfg = ((status->lanes & 0x0C) << 4) | (status->lanes & 0x03);
	// MIPI TX LANE CONFIG
	write_snr_reg(0x0302, tx_cfg);
	// MIPI TX DATA SRC 0-FIXP 1-CCS
	write_snr_reg(0x0311, 0x00);
	// MIPI TX LANE Num 0-1 1-2 2-3 3-4
	write_snr_reg(0x0317, status->lanes_num - 1);
	// MIPI TX LANE Mapping
	write_snr_reg(0x0318, 0xE4);
	// MIPI TX TYPE
	write_snr_reg(0x0319, tx_type);
	// MIPI TX LDO CONFIG
	write_snr_reg(0x0418, 0x07);
	return RTS_ISP_OK;
}

static int rs0551c_mipi_tx_aphy_freq_set(struct rs0551c_mipi_status *status)
{
	uint16_t ssc_nc;

	ssc_nc = status->mipi_clk / 12 - 2;
	// txclk=mipiclk/8
	// fix mipi tx clock soure 0= mipi_clk/2	1=mipi_clk/4
	write_snr_reg(0x0467, 0x00);
	// fix mipi tx clock sel 0= clk_src/4 1=clk_src/2
	write_snr_reg(0x031a, 0x00);
	// fpclk=mipiclk/2/(byFpDiv-1)
	// fixp pattern clock source
	// sel 0=clk_src 1=clk_src/2 2=clk_src/4 3=SSOR_SYSCLK
	write_snr_reg(0x021a, status->divider / 2);
	// PLL pre-div=0, post-div=0
	// PLL Pre-div 0-1 1-2 2-4 4-8
	write_snr_reg(0x0415, 0x00);
	// PLL Post-div
	write_snr_reg(0x0424, 0x00);
	// 0=vco bypass post-div 1= div
	write_snr_reg(0x042E, 0x00);
	write_snr_reg(0x0434, 0x00);
	write_snr_reg(0x0435, 0x00);
	write_snr_reg(0x0436, L_BYTE(ssc_nc));
	write_snr_reg(0x0437, H_BYTE(ssc_nc));
	return RTS_ISP_OK;
}

static uint8_t rs0551c_mipi_tx_get_ui_num(uint8_t const min_time,
					  uint8_t const min_ui_num,
					  uint16_t const mipi_clk)
{
	// Unit:8UI
	return (((float)min_time * mipi_clk/1000 + min_ui_num) / 8) + 1;
}

// mipi_clk = bitclk(include rising and failing)
static int rs0551c_mipi_tx_timing_init(uint16_t const mipi_clk)
{
	uint16_t tx_init_time;
	// UI = mipi_clk
	// Unit: 8UI
	// MIPI Tx HS EXIT Time more than 100nS
	write_snr_reg(0x0305, rs0551c_mipi_tx_get_ui_num(100, 0, mipi_clk));
	tx_init_time = 100 * mipi_clk / 8 + 1;
	// MIPI TX Init Time more than 100uS
	write_snr_reg(0x0303, L_BYTE(tx_init_time));
	write_snr_reg(0x0304, H_BYTE(tx_init_time));
	// MIPI TX LPX Time more than 50ns
	write_snr_reg(0x0306, rs0551c_mipi_tx_get_ui_num(50, 0, mipi_clk));
	// MIPI CK POST Time more than 60nS + 52UI
	write_snr_reg(0x0307, rs0551c_mipi_tx_get_ui_num(60, 52, mipi_clk));
	// MIPI CK PRPR Time 38nS~ 95nS
	write_snr_reg(0x0308, rs0551c_mipi_tx_get_ui_num(68, 0, mipi_clk));
	// MIPI CK ZERO Time CK_ZERO_TIME + CK_PRPR_TIME > 300nS
	write_snr_reg(0x0309, rs0551c_mipi_tx_get_ui_num(232, 0, mipi_clk));
	// MIPI CK PRE Time more than 8UI
	write_snr_reg(0x030A, 2);
	// MIPI HS PRPR Time 40nS+4UI ~ 85UI+6UI
	write_snr_reg(0x030B, rs0551c_mipi_tx_get_ui_num(45, 4, mipi_clk));
	// MIPI HS ZERO Time HS_ZERO_TIME + HS_PRPR_TIME > 145nS + 10UI
	write_snr_reg(0x030C, rs0551c_mipi_tx_get_ui_num(100, 6, mipi_clk));
	// MIPI TX TRAIL Time more than 60nS
	write_snr_reg(0x030D, rs0551c_mipi_tx_get_ui_num(60, 0, mipi_clk));
	// MIPI TX VLD Time more than 60nS	need small than HS_ZERO_TIME
	write_snr_reg(0x030E, rs0551c_mipi_tx_get_ui_num(60, 0, mipi_clk));
	// MIPI TX IF CFG0
	write_snr_reg(0x030E, 0x00);
	return RTS_ISP_OK;
}

static int rs0551c_mipi_clk_set(struct rs0551c_mipi_status *status)
{
	int ret;

	ret = rs0551c_mipi_tx_aphy_freq_set(status);
	if (ret)
		goto out;
	ret = rs0551c_mipi_tx_timing_init(status->mipi_clk);
out:
	return ret;
}

static int rs0551c_mipi_fixp_set(struct rs0551c_mipi_status *status)
{
	uint16_t frame_width, frame_height, fixp_blk_w, fixp_blk_h;
	uint16_t dummy_pixel, dummy_line;
	int shift;
	unsigned long pclk_fixp;

	if (g_rs0551c_mipi_config.type == YUV_SENSOR)
		shift = 1;
	else
		shift = 0;

	frame_width = g_rs0551c_mipi_config.width << shift;
	frame_height = g_rs0551c_mipi_config.height;
	// Frame width
	write_snr_reg(0x0200, H_BYTE(frame_width));
	write_snr_reg(0x0201, L_BYTE(frame_width));
	// Frame height
	write_snr_reg(0x0202, H_BYTE(frame_height));
	write_snr_reg(0x0203, L_BYTE(frame_height));
	// FIXP MARGIN WIDTH
	write_snr_reg(0x0217, 0x14);
	// FIXP MARGIN HEIGHT
	write_snr_reg(0x0218, 0x14);
	// FIXP MARGIN SIZE H
	write_snr_reg(0x0219, 0);

	fixp_blk_w = (g_rs0551c_mipi_config.width - 0x14 * 7) / 8;
	fixp_blk_h = (g_rs0551c_mipi_config.height - 0x14 * 5) / 6;
	// FIXP BLOCK WIDTH
	write_snr_reg(0x0214, L_BYTE(fixp_blk_w));
	// FIXP BLOCK HEIGHT
	write_snr_reg(0x0215, L_BYTE(fixp_blk_h));
	// FIXP BLOCK SIZE H
	write_snr_reg(0x0216,
		     (H_BYTE(fixp_blk_w) << 4) | H_BYTE(fixp_blk_h));
	// FIXP BLOCK START X
	write_snr_reg(0x0211, L_BYTE(fixp_blk_w));
	// FIXP BLOCK START Y
	write_snr_reg(0x0212, L_BYTE(fixp_blk_h));
	// FIXP BLOCK START SIZE H
	write_snr_reg(0x0213,
		     (H_BYTE(fixp_blk_w) << 4) | H_BYTE(fixp_blk_h));
	// Dummy line
	dummy_line = g_rs0551c_mipi_config.vts - g_rs0551c_mipi_config.height;
	write_snr_reg(0x0206, H_BYTE(dummy_line));
	write_snr_reg(0x0207, L_BYTE(dummy_line));

	pclk_fixp = (g_rs0551c_mipi_config.pclk *
		     g_rs0551c_mipi_config.bit_depth /
		     status->lanes_num / status->divider / 2);

	int hts = (pclk_fixp / g_rs0551c_mipi_config.vts /
		   g_rs0551c_mipi_config.fps) & ~1;

	if (hts < (g_rs0551c_mipi_config.width << shift) ||
	    (g_rs0551c_mipi_config.width << shift) > 4095)
		return -RTS_ISP_EINVAL;

	dummy_pixel = hts - (g_rs0551c_mipi_config.width << shift);
	write_snr_reg(0x0204, H_BYTE(dummy_pixel));
	write_snr_reg(0x0205, L_BYTE(dummy_pixel));
	// speed ctrl = 0
	write_snr_reg(0x0208, 0);

	// Frame Num=0
	write_snr_reg(0x0209, 0);

	return RTS_ISP_OK;
}

static int rs0551c_mipi_run(struct rs0551c_mipi_status *status)
{
	// fixp ctrl
	if (g_rs0551c_mipi_config.type == YUV_SENSOR)
		write_snr_reg(0x0210, 0xC0);
	else
		write_snr_reg(0x0210, 0x80);
	// MIPI TX Enable
	write_snr_reg(0x0300, 0x01);
	return RTS_ISP_OK;
}

static int rs0551c_mipi_start(uint32_t isp_id)
{
	int ret;
	struct rs0551c_mipi_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;
	status = &g_status[isp_id];

	ret = rs0551c_mipi_tx_aphy_init();
	if (ret)
		goto out;
	ret = rs0551c_mipi_tx_dphy_init(status);
	if (ret)
		goto out;
	ret = rs0551c_mipi_clk_set(status);
	if (ret)
		goto out;
	ret = rs0551c_mipi_fixp_set(status);
	if (ret)
		goto out;
	ret = rs0551c_mipi_run(status);
out:
	return ret;
}

static int rs0551c_mipi_stop(uint32_t isp_id)
{
	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	// MIPI TX Disable
	write_snr_reg(0x0300, 0x00);
	// fixp ctrl stop
	write_snr_reg(0x0210, 0x00);

	return RTS_ISP_OK;
}

static const struct rts_isp_sensor_ops rs0551c_mipi_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "rs0551c_mipi",
	.init = rs0551c_mipi_init,
	.get_info = rs0551c_mipi_get_info,
	.get_init_info = rs0551c_mipi_get_init_info,
	.get_tuned_again = rs0551c_mipi_get_tuned_again,
	.get_tuned_dgain = rs0551c_mipi_get_tuned_dgain,
	.get_exposure_gain_info = rs0551c_mipi_get_exposure_gain_info,
	.start = rs0551c_mipi_start,
	.stop = rs0551c_mipi_stop,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(rs0551c_mipi_ops)
