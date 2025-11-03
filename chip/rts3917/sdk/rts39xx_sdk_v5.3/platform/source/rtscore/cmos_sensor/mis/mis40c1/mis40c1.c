/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2023 George Liu <george_liu@realsil.com.cn>
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

#define ANALOG_GAIN_CHOOSE 0 //1_64xgain or 0_16xgain

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))
#define abs(x) ((x) >= 0 ? (x) : -(x))

struct fps_info {
	uint16_t fps;
	uint16_t hts;
	uint32_t clk;
};

struct mis40c1_status {
	float exp_step;
	float last_exposure;
	uint16_t cur_fps;
	uint16_t min_vts;
	struct rts_isp_i2c_reg regs1[2];
};

static struct mis40c1_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_mis40c1_fps_info[] = {
	{30, 3300, 148500000},
};

static struct rts_isp_i2c_reg g_mis40c1_30fps_i2c_init_regs[] = {
	{0x302d, 0x01},
	{0xffff, 0x50},
	{0x301f, 0x01},
	{0x3c1d, 0x0a},
	{0x3c1e, 0x00},
	{0x3c1f, 0x05},
	{0x3c20, 0xa0},
	{0x3106, 0xdc},
	{0x3105, 0x05},
	{0x3108, 0xe4},
	{0x3107, 0x0c},
	{0x310a, 0x04},
	{0x3109, 0x00},
	{0x310c, 0xa3},
	{0x310b, 0x05},
	{0x310e, 0x04},
	{0x310d, 0x00},
	{0x3110, 0x03},
	{0x310f, 0x0a},
	{0x3112, 0x0c},
	{0x61a8, 0x00},
	{0x4201, 0x01},
	{0x4200, 0x6e},
	{0x4203, 0x04},
	{0x4210, 0x00},
	{0x420a, 0x01},
	{0x4202, 0x01},
	{0x4208, 0x01},
	{0x4204, 0x01},
	{0x6101, 0x3c},
	{0x6100, 0x00},
	{0x6105, 0xff},
	{0x6104, 0x1f},
	{0x6103, 0xc3},
	{0x6102, 0x0c},
	{0x6107, 0xff},
	{0x6106, 0x1f},
	{0x6109, 0x7c},
	{0x6108, 0x04},
	{0x610d, 0xff},
	{0x610c, 0x1f},
	{0x610b, 0xa5},
	{0x610a, 0x05},
	{0x610f, 0xff},
	{0x610e, 0x1f},
	{0x6111, 0x00},
	{0x6110, 0x00},
	{0x6113, 0x35},
	{0x6112, 0x02},
	{0x6115, 0x6d},
	{0x6114, 0x04},
	{0x6117, 0xa8},
	{0x6116, 0x04},
	{0x6119, 0x7c},
	{0x6118, 0x04},
	{0x611b, 0xb7},
	{0x611a, 0x04},
	{0x611d, 0x1e},
	{0x611c, 0x00},
	{0x611f, 0x99},
	{0x611e, 0x04},
	{0x6121, 0x00},
	{0x6120, 0x00},
	{0x6123, 0xd2},
	{0x6122, 0x0c},
	{0x6125, 0x00},
	{0x6124, 0x00},
	{0x6127, 0xd2},
	{0x6126, 0x0c},
	{0x6129, 0x1e},
	{0x6128, 0x00},
	{0x612b, 0xab},
	{0x612a, 0x0c},
	{0x612d, 0x00},
	{0x612c, 0x00},
	{0x612f, 0x84},
	{0x612e, 0x04},
	{0x6131, 0x3c},
	{0x6130, 0x00},
	{0x6133, 0xbe},
	{0x6132, 0x01},
	{0x6135, 0x3c},
	{0x6134, 0x00},
	{0x6137, 0xaf},
	{0x6136, 0x01},
	{0x61ad, 0x17},
	{0x61ac, 0x02},
	{0x61b1, 0x5e},
	{0x61b0, 0x04},
	{0x61af, 0x35},
	{0x61ae, 0x02},
	{0x61b3, 0x39},
	{0x61b2, 0x06},
	{0x6139, 0x70},
	{0x6138, 0x02},
	{0x613d, 0x74},
	{0x613c, 0x06},
	{0x613b, 0x40},
	{0x613a, 0x04},
	{0x613f, 0xb4},
	{0x613e, 0x0c},
	{0x6141, 0x61},
	{0x6140, 0x02},
	{0x6143, 0x4f},
	{0x6142, 0x04},
	{0x6145, 0x66},
	{0x6144, 0x06},
	{0x6147, 0xc3},
	{0x6146, 0x0c},
	{0x6149, 0x52},
	{0x6148, 0x02},
	{0x614d, 0x57},
	{0x614c, 0x06},
	{0x614b, 0x3c},
	{0x614a, 0x04},
	{0x614f, 0xb0},
	{0x614e, 0x0c},
	{0x6151, 0x00},
	{0x6150, 0x00},
	{0x6155, 0x52},
	{0x6154, 0x02},
	{0x6159, 0x57},
	{0x6158, 0x06},
	{0x6153, 0x77},
	{0x6152, 0x00},
	{0x6157, 0x49},
	{0x6156, 0x04},
	{0x615b, 0xbd},
	{0x615a, 0x0c},
	{0x615d, 0x38},
	{0x615c, 0x03},
	{0x6161, 0x3c},
	{0x6160, 0x07},
	{0x615f, 0x40},
	{0x615e, 0x04},
	{0x6163, 0xb4},
	{0x6162, 0x0c},
	{0x6165, 0x00},
	{0x6164, 0x00},
	{0x6169, 0x7c},
	{0x6168, 0x04},
	{0x6167, 0x70},
	{0x6166, 0x02},
	{0x616b, 0x74},
	{0x616a, 0x06},
	{0x616d, 0xf9},
	{0x616c, 0x01},
	{0x6171, 0x5e},
	{0x6170, 0x04},
	{0x616f, 0x49},
	{0x616e, 0x04},
	{0x6173, 0xbd},
	{0x6172, 0x0c},
	{0x6175, 0x00},
	{0x6174, 0x00},
	{0x6177, 0x0f},
	{0x6176, 0x00},
	{0x6179, 0x01},
	{0x6178, 0x00},
	{0x617b, 0xbd},
	{0x617a, 0x0c},
	{0x617d, 0x00},
	{0x617c, 0x00},
	{0x617f, 0x29},
	{0x617e, 0x01},
	{0x6181, 0x00},
	{0x6180, 0x00},
	{0x6183, 0x29},
	{0x6182, 0x01},
	{0x6185, 0x00},
	{0x6184, 0x00},
	{0x6187, 0x29},
	{0x6186, 0x01},
	{0x61b5, 0x5e},
	{0x61b4, 0x04},
	{0x61b7, 0x6d},
	{0x61b6, 0x04},
	{0x6189, 0xd2},
	{0x6188, 0x0c},
	{0x618b, 0xe1},
	{0x618a, 0x0c},
	{0x618d, 0xd0},
	{0x618c, 0x00},
	{0x6191, 0x7c},
	{0x6190, 0x04},
	{0x618f, 0xee},
	{0x618e, 0x00},
	{0x6193, 0x99},
	{0x6192, 0x04},
	{0x6195, 0x0d},
	{0x6194, 0x0d},
	{0x61a3, 0xd0},
	{0x61a2, 0x01},
	{0x61a5, 0xcc},
	{0x61a4, 0x05},
	//BLC
	{0x5400, 0x2B},
	{0x5403, 0x08},
	{0x5406, 0x00},
	{0x5407, 0x40},
	{0x5408, 0x3e},
	//DPC ON 20},230821
	{0x3902, 0x02},
	//Added in 2022/10/31 by chenzheng
	{0x3a04, 0x10},
	{0x3a17, 0x00},
	{0x3a18, 0x00},
	{0x3048, 0x01},
	{0x6207, 0x00},
	{0x6208, 0x00},
	{0x3103, 0x03},
	{0x3104, 0xe0},
	{0x3118, 0x01},
	{0x3700, 0x01},
	//Offset 0x20 20230809
	{0x3701, 0x00},
	{0x3702, 0x20},
	{0x3703, 0x00},
	{0x3704, 0x20},
	{0x3705, 0x00},
	{0x3706, 0x20},
	{0x3707, 0x00},
	{0x3708, 0x20},
	{0x300c, 0x01},
	{0x610b, 0x40},
	{0x613d, 0x70},
	//Added in 20230717 by huangyuanxi
	{0x61a1, 0x90}, //128x竖纹
	{0x6202, 0x0c}, //去高倍太阳黑子
	{0x3a03, 0x39},
	{0x3a02, 0xfc}, //0xfd功耗最小基础上PAC OFF 20230810
	{0x3a08, 0x0c},
	{0x3048, 0x01},
	//PAC对称
	{0x61ae, 0x02},
	{0x61af, 0x50},
	{0x61b2, 0x06},
	{0x61b3, 0x50},
	{0x3040, 0x01},
	{0x6200, 0x09}, //CM电流次小 优化横带
	{0x6201, 0x09}, //RCS电流次小 优化横带
	//优化灯管横带   20230821
	{0x3a02, 0x2d},
	{0x3a03, 0x19},
	{0x6200, 0x00},
	{0x6201, 0x09},
	{0x6124, 0x1f},
	{0x6125, 0xff},
	{0x6126, 0x00},
	{0x6127, 0x00},
	//Eclp电流减小弱化亮点问题
	{0x3a13, 0x39},
	{0x3a15, 0x08},
	//高温闪烁/BLC不准/画面异常   20230824
	{0x3a02, 0x2d},
	{0x5400, 0x23},
	{0x5407, 0x10},
	{0x3a0c, 0x07},
	{0x3040, 0x01},
	//上电竖纹问题优化   20230828
	{0x6199, 0x90},
	{0x619b, 0x90},
	{0x619d, 0x90},
	{0x619f, 0x90},
	{0x61a1, 0x90},
	//IIC片内切换并降低时钟频率   20230829
	{0x420c, 0x00},
	{0x420d, 0x0a},
	//1x 部分芯片竖线问题   20230829
	{0x3a01, 0x01},
	//优化灯管横带问题   20230831
	{0x6209, 0x36},
	{0x302d, 0x00},
};

static int mis40c1_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 2560;
	info->modes.mode[0].size.h = 1440;
	info->modes.mode[0].fps = g_mis40c1_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x30;
	info->i2c.addr_len = 2;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 0);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_27M, 5000);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 1000);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V2, 1000);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 3000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 10000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 0);
	up->num = i;
	i = 0;
	set_power_item(&down->items[i++], SNR_HCLK, 0, 0);
	set_power_item(&down->items[i++], SNR_IO_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_CORE_POWER, 0, 0);
	set_power_item(&down->items[i++], SNR_ANALOG_POWER, 0, 0);
	down->num = i;

	return RTS_ISP_OK;
}

static const struct fps_info *mis40c1_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_mis40c1_fps_info); i++)
		if (fps == g_mis40c1_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_mis40c1_fps_info))
		return NULL;

	return &g_mis40c1_fps_info[i];
}

static int mis40c1_get_init_info(uint32_t isp_id,
				 const struct rts_isp_sensor_mode *mode,
			       struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct mis40c1_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("mis40c1 get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = mis40c1_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, clk_div: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->clk_div, fps_info->hts);

	set_init_i2c_regs(info->sensor_regs[0],
		g_mis40c1_30fps_i2c_init_regs, 0);

	set_init_i2c_regs(info->sensor_regs[1], status->regs1, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0 | MIPI_LANE1;
	info->interface.mipi.hs_term = 0x4;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 2560;
	info->size.h = 1440;
	info->start.x = 0;
	info->start.y = 0;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = status->min_vts = 1500;
	info->max_vts = 65535;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */
	status->cur_fps = mode->fps;

	return RTS_ISP_OK;
}

static int mis40c1_start(uint32_t isp_id)
{
	struct mis40c1_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static uint16_t get_sensor_gain_reg(float fgain)
{
	uint16_t reg_value = 0;

	if (fgain >= 16)
		reg_value = 0x03c0;
	else
		reg_value = (uint16_t)(0x0400 - 1024 / fgain);

#if ANALOG_GAIN_CHOOSE
	if (fgain >= 64)
		reg_value = 0x03f0;
	else
		reg_value = (uint16_t)(0x0400 - 1024 / fgain);
#endif

	return reg_value;
}

static float get_sensor_real_gain(uint16_t reg_value)
{
	float gain = 0.0;

	if (reg_value >= 0x03c0)
		gain = 16.00;
	else
		gain = (float)1024 / (1024 - reg_value);

#if ANALOG_GAIN_CHOOSE
	if (reg_value >= 0x03f0)
		gain = 64.00;
	else
		gain = (float)1024 / (1024 - reg_value);
#endif

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

static int mis40c1_get_tuned_again(uint32_t isp_id,
				   float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int mis40c1_get_tuned_dgain(uint32_t isp_id,
				   float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int mis40c1_get_exposure_gain_info(uint32_t isp_id,
			const struct rts_isp_sensor_exp_gain *exp_gain,
					struct rts_isp_sync_regs *regs)
{
	int i;
	int exp_set;
	uint16_t gain_reg;
	uint16_t total_line;
	float exp_reg_value_float;
	uint32_t exp_reg_value;
	float gain;
	struct mis40c1_status *status;
	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	gain = exp_gain->analog_gain[0] * exp_gain->digital_gain[0];
	gain_reg = get_sensor_gain_reg(gain);

	total_line = exp_gain->vts;
	total_line = (total_line + 1) / 2 * 2;
	exp_reg_value_float = 1.0
			* exp_gain->exposure[0] / status->exp_step + 0.5f;
	exp_reg_value = clip_d_word(exp_reg_value_float, 2, total_line - 3);

	reg = regs->reg;

	i = 0;
	set_sync_i2c(&reg[i++], 0x3105, (total_line >> 8));
	set_sync_i2c(&reg[i++], 0x3106, (total_line & 0xff));
	exp_set = abs(status->last_exposure - exp_gain->exposure[0]) > 0.001f;

	if (exp_set) {
		set_sync_i2c(&reg[i++], 0x3100, (exp_reg_value >> 8));
		set_sync_i2c(&reg[i++], 0x3101, (exp_reg_value & 0xff));
		status->last_exposure = exp_gain->exposure[0];
	}

	set_sync_i2c(&reg[i++], 0x3103, (gain_reg >> 8));
	set_sync_i2c(&reg[i++], 0x3104, (gain_reg & 0xff));
	set_sync_i2c(&reg[i++], 0x300c, 0x01);

	regs->num = i;

	return RTS_ISP_OK;
}

static int mis40c1_check(uint32_t isp_id)
{
	int ret;
	int id;
	struct rts_isp_i2c_reg reg = {};

	reg.addr = 0x3000;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id = reg.data << 8;

	reg.addr = 0x3001;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	id |= reg.data;

	if (id == 0x0000)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops mis40c1_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "mis40c1",
	.get_info = mis40c1_get_info,
	.get_init_info = mis40c1_get_init_info,
	.start = mis40c1_start,
	.get_tuned_again = mis40c1_get_tuned_again,
	.get_tuned_dgain = mis40c1_get_tuned_dgain,
	.get_exposure_gain_info = mis40c1_get_exposure_gain_info,
	.check = mis40c1_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(mis40c1_ops)
