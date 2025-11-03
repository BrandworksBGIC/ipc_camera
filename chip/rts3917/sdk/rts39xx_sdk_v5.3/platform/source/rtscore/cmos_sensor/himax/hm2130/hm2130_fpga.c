/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2020 Sally Peng <sally_peng@realsil.com.cn>
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

struct hm2130_status {
	float row_time;
	float last_exposure;
	int min_vts;
};

static struct hm2130_status g_status[SUPPORTED_ISP_NUM];

static struct rts_isp_i2c_reg g_hm2130_i2c_init_regs_setting1[] = {
	{0x0103, 0x00},
};

static struct rts_isp_i2c_reg g_hm2130_i2c_init_regs_setting2[] = {
	{0x0304, 0x2A},
#if 0// input 24M
	{0x0305, 0x2C},
	{0x0307, 0x30},
	{0x0303, 0x04},
	{0x0309, 0x00},
	{0x030A, 0x0A},
	{0x030D, 0x02},
	{0x030F, 0x10},
#else //input 6M
	{0x0305, 0x23},
	{0x0307, 0x19},  //0x1B
	{0x0303, 0x04},
	{0x0309, 0x00},
	{0x030A, 0x0A},
	{0x030D, 0x01},
	{0x030F, 0x0F},
#endif
	{0x5268, 0x01},
	{0x5264, 0x24},
	{0x5265, 0x92},
	{0x5266, 0x23},
	{0x5267, 0x07},
	{0x5269, 0x02},
	{0x0100, 0x02},
	{0x0100, 0x02},
	{0x0111, 0x01},
	{0x0112, 0x0A},
	{0x0113, 0x0A},
	{0x4B20, 0xCE},
	{0x4B18, 0x12},
	{0x4B02, 0x05},
	{0x4B43, 0x07},
	{0x4B05, 0x1C},
	{0x4B0E, 0x00},
	{0x4B0F, 0x0D},
	{0x4B06, 0x06},
	{0x4B39, 0x0B},
	{0x4B42, 0x07},
	{0x4B03, 0x0C},
	{0x4B04, 0x07},
	{0x4B3A, 0x0B},
	{0x4B51, 0x80},
	{0x4B52, 0x09},
};

static struct rts_isp_i2c_reg g_hm2130_i2c_init_regs_setting3[] = {
	//	delay(5ms)
	{0x4B52, 0xC9},
	{0x4B57, 0x07},
	{0x4B68, 0x6B},
	{0x0350, 0x37},
	{0x5030, 0x11},
#if 0 //input 24M
	{0x5032, 0x02},
	{0x5033, 0x05},
	{0x5034, 0x00},
	{0x5035, 0x40},
#else//input 6M
	{0x5032, 0x07},
	{0x5033, 0x00},
	{0x5034, 0x00},
	{0x5035, 0x70},
	{0x5036, 0x10},
	{0x5037, 0x10},
	{0x5038, 0x01},
	{0x5039, 0x00},
	{0x503A, 0x01},
	{0x503B, 0x00},
	{0x0346, 0x00},
	{0x0347, 0x01},
	{0x034A, 0x04},
	{0x034B, 0x3C},
#endif
	{0x5229, 0x90},
	{0x5061, 0x00},
	{0x5062, 0x94},
	{0x50F5, 0x06},
	{0x5230, 0x00},
	{0x526C, 0x00},
	{0x520B, 0x41},
	{0x5254, 0x08},
	{0x522B, 0x78},
	{0x4144, 0x08},
	{0x4148, 0x03},
	{0x4024, 0x40},
	{0x4B66, 0x00},
	{0x0340, 0x04},
	{0x0341, 0x4e},
	{0x0342, 0x04},
	{0x0343, 0x88},
	{0x034C, 0x07},
	{0x034D, 0x88},
	{0x034E, 0x04},
	{0x034F, 0x40},
	{0x0101, 0x00},
	{0x4020, 0x10},
	{0x300D, 0x10},
	{0x300F, 0x10},
	{0x50DD, 0x01},	//GAIN STRETEGY: 0=SMIA, 1=HII
	{0x0350, 0x37},
	{0x4131, 0x01},
	{0x4132, 0x20},
	{0x5011, 0x00},
	{0x5015, 0x00},
	{0x501D, 0x1C},
	{0x501E, 0x00},
	{0x501F, 0x20},
	{0x50D5, 0xF0},
	{0x50D7, 0x12},
	{0x50BB, 0x14},
	{0x5040, 0x07},
	{0x50B7, 0x00},
	{0x50B8, 0x10},
	{0x50B9, 0xFF},
	{0x50BA, 0xFF},
	{0x5200, 0x26},
	{0x5201, 0x00},
	{0x5202, 0x00},
	{0x5203, 0x00},
	{0x5217, 0x01},
	{0x5219, 0x01},
	{0x5234, 0x01},
	{0x526B, 0x03},
	{0x4C00, 0x00},
	{0x0310, 0x00},
	{0x4B31, 0x06},
	{0x4B3B, 0x02},
	{0x4B44, 0x0C},
	{0x4B45, 0x01},
	{0x50A1, 0x00},
	{0x50AA, 0x2D},
	{0x50AC, 0x60},
	{0x50AB, 0x02},
	{0x50A0, 0xB0},
	{0x50A2, 0x12},
	{0x50AF, 0x00},
	{0x5208, 0x55},
	{0x5209, 0x03},
	{0x520D, 0x40},
	{0x5214, 0x38},
	{0x5215, 0x03},
	{0x5216, 0x00},
	{0x521A, 0x10},
	{0x521B, 0x24},
	{0x5232, 0x04},
	{0x5233, 0x03},
	{0x5106, 0xF0},
	{0x510E, 0xC1},
	{0x5166, 0xF0},
	{0x516E, 0xC1},
	{0x5196, 0xF0},
	{0x519E, 0xC1},
	{0x51C0, 0x80},
	{0x51C4, 0x80},
	{0x51C8, 0x80},
	{0x51CC, 0x80},
	{0x51D0, 0x80},
	{0x51D4, 0x80},
	{0x51D8, 0x80},
	{0x51DC, 0x80},
	{0x51C1, 0x03},
	{0x51C5, 0x13},
	{0x51C9, 0x13},
	{0x51CD, 0x13},
	{0x51D1, 0x13},
	{0x51D5, 0x17},
	{0x51D9, 0x1B},
	{0x51DD, 0x1B},
	{0x51C2, 0x4B},
	{0x51C6, 0x4B},
	{0x51CA, 0x4B},
	{0x51CE, 0x49},
	{0x51D2, 0x49},
	{0x51D6, 0x49},
	{0x51DA, 0x49},
	{0x51DE, 0x49},
	{0x51C3, 0x10},
	{0x51C7, 0x20},
	{0x51CB, 0x08},
	{0x51CF, 0x00},
	{0x51D3, 0x00},
	{0x51D7, 0x10},
	{0x51DB, 0x10},
	{0x51DF, 0x00},
	{0x51E0, 0xF4},
	{0x51E2, 0x94},
	{0x51E4, 0x94},
	{0x51E6, 0x94},
	{0x51E1, 0x00},
	{0x51E3, 0x00},
	{0x51E5, 0x00},
	{0x51E7, 0x00},
	{0x5264, 0x23},
	{0x5265, 0x07},
	{0x5266, 0x24},
	{0x5267, 0x92},
	{0x5268, 0x01},
	{0xBAA2, 0xC0},
	{0xBAA2, 0x40},
	{0xBA90, 0x01},
	{0xBA93, 0x02},
	{0x3110, 0x0B},
	{0x373E, 0x8A},
	{0x373F, 0x8A},
	{0x3701, 0x05},
	{0x3709, 0x05},
	{0x3703, 0x04},
	{0x370B, 0x04},
	{0x3713, 0x00},
	{0x3717, 0x00},
	{0x5043, 0x01},
	{0x5040, 0x01},
	{0x5044, 0x07},
	{0x6000, 0x0F},
	{0x6001, 0xFF},
	{0x6002, 0x1F},
	{0x6003, 0xFF},
	{0x6004, 0x82},
	{0x6005, 0x00},
	{0x6006, 0x00},
	{0x6007, 0x00},
	{0x6008, 0x00},
	{0x6009, 0x00},
	{0x600A, 0x00},
	{0x600B, 0x00},
	{0x600C, 0x00},
	{0x600D, 0x20},
	{0x600E, 0x00},
	{0x600F, 0xA1},
	{0x6010, 0x01},
	{0x6011, 0x00},
	{0x6012, 0x0F},
	{0x6013, 0x00},
	{0x6014, 0x24},
	{0x6015, 0x00},
	{0x6016, 0x3F},
	{0x6017, 0x00},
	{0x6018, 0x73},
	{0x6019, 0x00},
	{0x601A, 0xC3},
	{0x601B, 0x01},
	{0x601C, 0x4F},
	{0x0000, 0x00},
	{0x0104, 0x01},
	{0x0104, 0x00},
	{0x0100, 0x03},
};

static int hm2130_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_LINE_2TO1;
	info->modes.mode[0].size.w = 1920;
	info->modes.mode[0].size.h = 1080;
	info->modes.mode[0].fps = 7.04;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x24;
	info->i2c.addr_len = 2;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 10);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_3V3, 10);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V5, 1000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 1000);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_6M, 1000);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_RST_GPIO, 0, 0);
	set_power_item(&down->items[i++], SNR_HCLK, 0, 0);
	set_power_item(&down->items[i++], SNR_CORE_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_IO_POWER, 0, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static int hm2130_get_init_info(uint32_t isp_id,
				const struct rts_isp_sensor_mode *mode,
				struct rts_isp_sensor_init_info *info)
{
	struct hm2130_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("hm2130 get fps %.1f init info\n", mode->fps);
	status = &g_status[isp_id];

	set_init_i2c_regs(info->sensor_regs[0],
				g_hm2130_i2c_init_regs_setting1, 1000);
	set_init_i2c_regs(info->sensor_regs[1],
				g_hm2130_i2c_init_regs_setting2, 6000);
	set_init_i2c_regs(info->sensor_regs[2],
				g_hm2130_i2c_init_regs_setting3, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.hdr = MIPI_HDR_VC;
	info->interface.mipi.lanes = MIPI_LANE0 | MIPI_LANE1;
	info->interface.mipi.hs_term = 0x9;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 1920;
	info->size.h = 1080;
	info->start.x = 0;
	info->start.y = 0;

	info->hts = 2320;
	info->pclk = 36000000;
	info->min_vts = status->min_vts = 1102 * 2;
	info->max_vts = info->min_vts;

	status->row_time = 1e6 * info->hts / info->pclk; /* us */

	return RTS_ISP_OK;
}

static int hm2130_start(uint32_t isp_id)
{
	struct hm2130_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static int hm2130_get_exposure_range(uint32_t isp_id, uint32_t vts,
				     float ratio[RTS_ISP_HDR_CHAN_MAX - 1],
				     float min_exposure[RTS_ISP_HDR_CHAN_MAX],
				     float max_exposure[RTS_ISP_HDR_CHAN_MAX])
{
	struct hm2130_status *status;
	uint32_t tmp1;
	uint32_t tmp2;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	tmp1 = 112;
	tmp2 = (uint32_t)((vts - 8) / (ratio[0] + 1));
	tmp1 = tmp1 < tmp2 ? tmp1 : tmp2;
	max_exposure[1] = tmp1 * status->row_time;
	min_exposure[1] = status->row_time;
	max_exposure[0] = max_exposure[1] * ratio[0];
	min_exposure[0] = min_exposure[1] * ratio[0];

	return RTS_ISP_OK;
}

static uint16_t get_sensor_gain_reg(float fgain)
{
	uint16_t fgaintemp;
	uint16_t rough = 1;
	uint16_t fine = 0;
	uint16_t gain = fgain * 128;

	fgaintemp = (gain > 15.5 * 128) ?
			(15.5 * 128) : ((gain < 128) ? 128 : gain);

	while (fgaintemp > 255) {
		rough++;
		fgaintemp = fgaintemp >> 1;
	}
	fine = (fgaintemp - 128) >> 3;
	fgaintemp = ((rough << 4) | (fine));

	return fgaintemp;
}

static float get_sensor_real_gain(uint16_t reg_value)
{
	uint16_t rough = 128;

	while (reg_value >= 32) {
		rough = rough << 1;
		reg_value -= 16;
	}
	reg_value = (rough * reg_value) / 16;

	return reg_value / 128.0f;
}

static int hm2130_get_tuned_again(uint32_t isp_id,
				  float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);
	gain_reg = get_sensor_gain_reg(again[1]);
	again[1] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int hm2130_get_tuned_dgain(uint32_t isp_id,
				  float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;
	dgain[1] = 1.0f;

	return RTS_ISP_OK;
}

static int hm2130_get_exposure_gain_info(uint32_t isp_id,
			const struct rts_isp_sensor_exp_gain *exp_gain,
			struct rts_isp_sync_regs *regs)
{
	int i;
	uint16_t total_line;
	struct hm2130_status *status;
	struct rts_isp_sync_reg *reg;
	uint16_t exp_cnt[2];
	uint16_t gain_reg[2];

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	total_line = exp_gain->vts >> 1;
	for (i = 0; i < 2; i++) {
		exp_cnt[i] = exp_gain->exposure[i] / status->row_time;
		gain_reg[i] = get_sensor_gain_reg(exp_gain->analog_gain[i] *
						exp_gain->digital_gain[i]);
	}

	reg = regs->reg;
	i = 0;
	/* set vts */
	set_sync_i2c(&reg[i++], 0x0341, total_line & 0xff);
	set_sync_i2c(&reg[i++], 0x0340, total_line >> 8);
	/* set Long Exposuretime setting */
	set_sync_i2c(&reg[i++], 0x5033, exp_cnt[0] & 0xff);
	set_sync_i2c(&reg[i++], 0x5032, exp_cnt[0] >> 8);
	/* set short Exposuretime setting */
	set_sync_i2c(&reg[i++], 0x5035, exp_cnt[1] & 0xff);
	set_sync_i2c(&reg[i++], 0x5034, exp_cnt[1] >> 8);
	/* set gain */
	set_sync_i2c(&reg[i++], 0x5037, gain_reg[0] & 0xff);
	set_sync_i2c(&reg[i++], 0x5036, gain_reg[1] & 0xff);

	set_sync_i2c(&reg[i++], 0x0104, 0x01);
	set_sync_i2c(&reg[i++], 0x0104, 0x00);

	regs->num = i;

	return RTS_ISP_OK;
}

static const struct rts_isp_sensor_ops hm2130_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "hm2130",
	.get_info = hm2130_get_info,
	.get_init_info = hm2130_get_init_info,
	.start = hm2130_start,
	.get_exposure_range = hm2130_get_exposure_range,
	.get_tuned_again = hm2130_get_tuned_again,
	.get_tuned_dgain = hm2130_get_tuned_dgain,
	.get_exposure_gain_info = hm2130_get_exposure_gain_info,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(hm2130_ops)
