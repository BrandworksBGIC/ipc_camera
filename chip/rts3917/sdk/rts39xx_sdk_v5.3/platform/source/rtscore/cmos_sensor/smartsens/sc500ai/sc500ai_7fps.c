/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2020 Yang Wang <yang_wang@apowertec.com>
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

struct sc500ai_gain {
	uint16_t ana_gain;
	uint16_t fine_gain;
	float total_gain;
};

struct sc500ai_status {
	float exp_step;
	float last_exposure;
	uint16_t cur_fps;
	uint16_t min_vts;
	uint16_t ver_data[2];
	struct rts_isp_i2c_reg regs1[4];
};

static struct sc500ai_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_sc500ai_fps_info[] = {
	{7, 3200, 44800000},
};

static struct rts_isp_i2c_reg g_sc500ai_7fps_i2c_init_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x3018, 0x32},
	{0x3019, 0x0c},
	{0x301f, 0x43},
	{0x320e, 0x07},
	{0x320f, 0xd0},
	{0x3253, 0x0a},
	{0x3301, 0x06},
	{0x3302, 0x18},
	{0x3303, 0x10},
	{0x3304, 0x40},
	{0x3306, 0x48},
	{0x3308, 0x0c},
	{0x3309, 0x50},
	{0x330a, 0x00},
	{0x330b, 0xe0},
	{0x330d, 0x10},
	{0x330e, 0x20},
	{0x330f, 0x02},
	{0x3310, 0x02},
	{0x331c, 0x04},
	{0x331e, 0x35},
	{0x331f, 0x45},
	{0x3320, 0x09},
	{0x3333, 0x10},
	{0x334c, 0x08},
	{0x3356, 0x09},
	{0x3364, 0x17},
	{0x336d, 0x03},
	{0x3390, 0x08},
	{0x3391, 0x18},
	{0x3392, 0x38},
	{0x3393, 0x0a},
	{0x3394, 0x20},
	{0x3395, 0x20},
	{0x3396, 0x08},
	{0x3397, 0x18},
	{0x3398, 0x38},
	{0x3399, 0x0a},
	{0x339a, 0x20},
	{0x339b, 0x20},
	{0x339c, 0x20},
	{0x33ac, 0x10},
	{0x33ae, 0x10},
	{0x33af, 0x19},
	{0x360f, 0x01},
	{0x3622, 0x03},
	{0x363a, 0x1f},
	{0x363c, 0x40},
	{0x3651, 0x7d},
	{0x3670, 0x0a},
	{0x3671, 0x07},
	{0x3672, 0x17},
	{0x3673, 0x1e},
	{0x3674, 0x82},
	{0x3675, 0x64},
	{0x3676, 0x66},
	{0x367a, 0x48},
	{0x367b, 0x78},
	{0x367c, 0x58},
	{0x367d, 0x78},
	{0x3690, 0x34},
	{0x3691, 0x34},
	{0x3692, 0x54},
	{0x369c, 0x48},
	{0x369d, 0x78},
	{0x36ea, 0x32},
	{0x36eb, 0x1c},
	{0x36ec, 0x0a},
	{0x36ed, 0x14},
	{0x36fa, 0x32},
	{0x36fb, 0x35},
	{0x36fc, 0x11},
	{0x36fd, 0x14},
	{0x3904, 0x04},
	{0x3908, 0x41},
	{0x391d, 0x04},
	{0x39c2, 0x30},
	{0x3e01, 0xf9},
	{0x3e02, 0x80},
	{0x3e16, 0x00},
	{0x3e17, 0x80},
	{0x4500, 0x88},
	{0x4509, 0x20},
	{0x4837, 0x48},
	{0x5799, 0x00},
	{0x59e0, 0x60},
	{0x59e1, 0x08},
	{0x59e2, 0x3f},
	{0x59e3, 0x18},
	{0x59e4, 0x18},
	{0x59e5, 0x3f},
	{0x59e7, 0x02},
	{0x59e8, 0x38},
	{0x59e9, 0x20},
	{0x59ea, 0x0c},
	{0x59ec, 0x08},
	{0x59ed, 0x02},
	{0x59ee, 0xa0},
	{0x59ef, 0x08},
	{0x59f4, 0x18},
	{0x59f5, 0x10},
	{0x59f6, 0x0c},
	{0x59f9, 0x02},
	{0x59fa, 0x18},
	{0x59fb, 0x10},
	{0x59fc, 0x0c},
	{0x59ff, 0x02},
	{0x36e9, 0x40},
	{0x36f9, 0x20},
	{0x0100, 0x01},
};

static struct sc500ai_gain gain_mapping[] = {
	{0x0300, 0x40, 1.000},
	{0x0300, 0x41, 1.016},
	{0x0300, 0x42, 1.031},
	{0x0300, 0x43, 1.047},
	{0x0300, 0x44, 1.063},
	{0x0300, 0x45, 1.078},
	{0x0300, 0x46, 1.094},
	{0x0300, 0x47, 1.109},
	{0x0300, 0x48, 1.125},
	{0x0300, 0x49, 1.141},
	{0x0300, 0x4a, 1.156},
	{0x0300, 0x4b, 1.172},
	{0x0300, 0x4c, 1.188},
	{0x0300, 0x4d, 1.203},
	{0x0300, 0x4e, 1.219},
	{0x0300, 0x4f, 1.234},
	{0x0300, 0x50, 1.250},
	{0x0300, 0x51, 1.266},
	{0x0300, 0x52, 1.281},
	{0x0300, 0x53, 1.297},
	{0x0300, 0x54, 1.313},
	{0x0300, 0x55, 1.328},
	{0x0300, 0x56, 1.344},
	{0x0300, 0x57, 1.359},
	{0x0300, 0x58, 1.375},
	{0x0300, 0x59, 1.391},
	{0x0300, 0x5a, 1.406},
	{0x0300, 0x5b, 1.422},
	{0x0300, 0x5c, 1.438},
	{0x0300, 0x5d, 1.453},
	{0x0300, 0x5e, 1.469},
	{0x0300, 0x5f, 1.484},
	{0x2300, 0x60, 1.500},

	{0x2300, 0x40, 1.516},
	{0x2300, 0x41, 1.540},
	{0x2300, 0x42, 1.563},
	{0x2300, 0x43, 1.587},
	{0x2300, 0x44, 1.611},
	{0x2300, 0x45, 1.634},
	{0x2300, 0x46, 1.658},
	{0x2300, 0x47, 1.682},
	{0x2300, 0x48, 1.706},
	{0x2300, 0x49, 1.729},
	{0x2300, 0x4a, 1.753},
	{0x2300, 0x4b, 1.777},
	{0x2300, 0x4c, 1.800},
	{0x2300, 0x4d, 1.824},
	{0x2300, 0x4e, 1.848},
	{0x2300, 0x4f, 1.871},
	{0x2300, 0x50, 1.895},
	{0x2300, 0x51, 1.919},
	{0x2300, 0x52, 1.942},
	{0x2300, 0x53, 1.966},
	{0x2300, 0x54, 1.990},
	{0x2300, 0x55, 2.013},
	{0x2300, 0x56, 2.037},
	{0x2300, 0x57, 2.061},
	{0x2300, 0x58, 2.085},
	{0x2300, 0x59, 2.108},
	{0x2300, 0x5a, 2.132},
	{0x2300, 0x5b, 2.156},
	{0x2300, 0x5c, 2.179},
	{0x2300, 0x5d, 2.203},
	{0x2300, 0x5e, 2.227},
	{0x2300, 0x5f, 2.250},
	{0x2300, 0x60, 2.274},
	{0x2300, 0x61, 2.298},
	{0x2300, 0x62, 2.321},
	{0x2300, 0x63, 2.345},
	{0x2300, 0x64, 2.369},
	{0x2300, 0x65, 2.392},
	{0x2300, 0x66, 2.416},
	{0x2300, 0x67, 2.440},
	{0x2300, 0x68, 2.464},
	{0x2300, 0x69, 2.487},
	{0x2300, 0x6a, 2.511},
	{0x2300, 0x6b, 2.535},
	{0x2300, 0x6c, 2.558},
	{0x2300, 0x6d, 2.582},
	{0x2300, 0x6e, 2.606},
	{0x2300, 0x6f, 2.629},
	{0x2300, 0x70, 2.653},
	{0x2300, 0x71, 2.677},
	{0x2300, 0x72, 2.700},
	{0x2300, 0x73, 2.724},
	{0x2300, 0x74, 2.748},
	{0x2300, 0x75, 2.771},
	{0x2300, 0x76, 2.795},
	{0x2300, 0x77, 2.819},
	{0x2300, 0x78, 2.843},
	{0x2300, 0x79, 2.866},
	{0x2300, 0x7a, 2.890},
	{0x2300, 0x7b, 2.914},
	{0x2300, 0x7c, 2.937},
	{0x2300, 0x7d, 2.961},
	{0x2300, 0x7e, 2.985},
	{0x2300, 0x7f, 3.008},

	{0x2700, 0x40, 3.032},
	{0x2700, 0x41, 3.079},
	{0x2700, 0x42, 3.127},
	{0x2700, 0x43, 3.174},
	{0x2700, 0x44, 3.222},
	{0x2700, 0x45, 3.269},
	{0x2700, 0x46, 3.316},
	{0x2700, 0x47, 3.364},
	{0x2700, 0x48, 3.411},
	{0x2700, 0x49, 3.458},
	{0x2700, 0x4a, 3.506},
	{0x2700, 0x4b, 3.553},
	{0x2700, 0x4c, 3.601},
	{0x2700, 0x4d, 3.648},
	{0x2700, 0x4e, 3.695},
	{0x2700, 0x4f, 3.743},
	{0x2700, 0x50, 3.790},
	{0x2700, 0x51, 3.837},
	{0x2700, 0x52, 3.885},
	{0x2700, 0x53, 3.932},
	{0x2700, 0x54, 3.980},
	{0x2700, 0x55, 4.027},
	{0x2700, 0x56, 4.074},
	{0x2700, 0x57, 4.122},
	{0x2700, 0x58, 4.169},
	{0x2700, 0x59, 4.216},
	{0x2700, 0x5a, 4.264},
	{0x2700, 0x5b, 4.311},
	{0x2700, 0x5c, 4.359},
	{0x2700, 0x5d, 4.406},
	{0x2700, 0x5e, 4.453},
	{0x2700, 0x5f, 4.501},
	{0x2700, 0x60, 4.548},
	{0x2700, 0x61, 4.595},
	{0x2700, 0x62, 4.643},
	{0x2700, 0x63, 4.690},
	{0x2700, 0x64, 4.738},
	{0x2700, 0x65, 4.785},
	{0x2700, 0x66, 4.832},
	{0x2700, 0x67, 4.880},
	{0x2700, 0x68, 4.927},
	{0x2700, 0x69, 4.974},
	{0x2700, 0x6a, 5.022},
	{0x2700, 0x6b, 5.069},
	{0x2700, 0x6c, 5.117},
	{0x2700, 0x6d, 5.164},
	{0x2700, 0x6e, 5.211},
	{0x2700, 0x6f, 5.259},
	{0x2700, 0x70, 5.306},
	{0x2700, 0x71, 5.353},
	{0x2700, 0x72, 5.401},
	{0x2700, 0x73, 5.448},
	{0x2700, 0x74, 5.496},
	{0x2700, 0x75, 5.543},
	{0x2700, 0x76, 5.590},
	{0x2700, 0x77, 5.638},
	{0x2700, 0x78, 5.685},
	{0x2700, 0x79, 5.732},
	{0x2700, 0x7a, 5.780},
	{0x2700, 0x7b, 5.827},
	{0x2700, 0x7c, 5.875},
	{0x2700, 0x7d, 5.922},
	{0x2700, 0x7e, 5.969},
	{0x2700, 0x7f, 6.017},

	{0x2f00, 0x40, 6.064},
	{0x2f00, 0x41, 6.159},
	{0x2f00, 0x42, 6.254},
	{0x2f00, 0x43, 6.348},
	{0x2f00, 0x44, 6.443},
	{0x2f00, 0x45, 6.538},
	{0x2f00, 0x46, 6.633},
	{0x2f00, 0x47, 6.727},
	{0x2f00, 0x48, 6.822},
	{0x2f00, 0x49, 6.917},
	{0x2f00, 0x4a, 7.012},
	{0x2f00, 0x4b, 7.106},
	{0x2f00, 0x4c, 7.201},
	{0x2f00, 0x4d, 7.296},
	{0x2f00, 0x4e, 7.391},
	{0x2f00, 0x4f, 7.485},
	{0x2f00, 0x50, 7.580},
	{0x2f00, 0x51, 7.675},
	{0x2f00, 0x52, 7.770},
	{0x2f00, 0x53, 7.864},
	{0x2f00, 0x54, 7.959},
	{0x2f00, 0x55, 8.054},
	{0x2f00, 0x56, 8.149},
	{0x2f00, 0x57, 8.243},
	{0x2f00, 0x58, 8.338},
	{0x2f00, 0x59, 8.433},
	{0x2f00, 0x5a, 8.528},
	{0x2f00, 0x5b, 8.622},
	{0x2f00, 0x5c, 8.717},
	{0x2f00, 0x5d, 8.812},
	{0x2f00, 0x5e, 8.907},
	{0x2f00, 0x5f, 9.001},
	{0x2f00, 0x60, 9.096},
	{0x2f00, 0x61, 9.191},
	{0x2f00, 0x62, 9.286},
	{0x2f00, 0x63, 9.380},
	{0x2f00, 0x64, 9.475},
	{0x2f00, 0x65, 9.570},
	{0x2f00, 0x66, 9.665},
	{0x2f00, 0x67, 9.759},
	{0x2f00, 0x68, 9.854},
	{0x2f00, 0x69, 9.949},
	{0x2f00, 0x6a, 10.044},
	{0x2f00, 0x6b, 10.138},
	{0x2f00, 0x6c, 10.233},
	{0x2f00, 0x6d, 10.328},
	{0x2f00, 0x6e, 10.423},
	{0x2f00, 0x6f, 10.517},
	{0x2f00, 0x70, 10.612},
	{0x2f00, 0x71, 10.707},
	{0x2f00, 0x72, 10.802},
	{0x2f00, 0x73, 10.896},
	{0x2f00, 0x74, 10.991},
	{0x2f00, 0x75, 11.086},
	{0x2f00, 0x76, 11.181},
	{0x2f00, 0x77, 11.275},
	{0x2f00, 0x78, 11.370},
	{0x2f00, 0x79, 11.465},
	{0x2f00, 0x7a, 11.560},
	{0x2f00, 0x7b, 11.654},
	{0x2f00, 0x7c, 11.749},
	{0x2f00, 0x7d, 11.844},
	{0x2f00, 0x7e, 11.939},
	{0x2f00, 0x7f, 12.033},

	{0x3f00, 0x40, 12.128},
	{0x3f00, 0x41, 12.318},
	{0x3f00, 0x42, 12.507},
	{0x3f00, 0x43, 12.697},
	{0x3f00, 0x44, 12.886},
	{0x3f00, 0x45, 13.076},
	{0x3f00, 0x46, 13.265},
	{0x3f00, 0x47, 13.455},
	{0x3f00, 0x48, 13.644},
	{0x3f00, 0x49, 13.834},
	{0x3f00, 0x4a, 14.023},
	{0x3f00, 0x4b, 14.213},
	{0x3f00, 0x4c, 14.402},
	{0x3f00, 0x4d, 14.592},
	{0x3f00, 0x4e, 14.781},
	{0x3f00, 0x4f, 14.971},
	{0x3f00, 0x50, 15.160},
	{0x3f00, 0x51, 15.350},
	{0x3f00, 0x52, 15.539},
	{0x3f00, 0x53, 15.729},
	{0x3f00, 0x54, 15.918},
	{0x3f00, 0x55, 16.108},
	{0x3f00, 0x56, 16.297},
	{0x3f00, 0x57, 16.487},
	{0x3f00, 0x58, 16.676},
	{0x3f00, 0x59, 16.866},
	{0x3f00, 0x5A, 17.055},
	{0x3f00, 0x5B, 17.245},
	{0x3f00, 0x5C, 17.434},
	{0x3f00, 0x5D, 17.624},
	{0x3f00, 0x5E, 17.813},
	{0x3f00, 0x5F, 18.003},
	{0x3f00, 0x60, 18.192},
	{0x3f00, 0x61, 18.382},
	{0x3f00, 0x62, 18.571},
	{0x3f00, 0x63, 18.761},
	{0x3f00, 0x64, 18.950},
	{0x3f00, 0x65, 19.140},
	{0x3f00, 0x66, 19.329},
	{0x3f00, 0x67, 19.519},
	{0x3f00, 0x68, 19.708},
	{0x3f00, 0x69, 19.898},
	{0x3f00, 0x6A, 20.087},
	{0x3f00, 0x6B, 20.277},
	{0x3f00, 0x6C, 20.466},
	{0x3f00, 0x6d, 20.656},
	{0x3f00, 0x6e, 20.845},
	{0x3f00, 0x6f, 21.035},
	{0x3f00, 0x70, 21.224},
	{0x3f00, 0x71, 21.414},
	{0x3f00, 0x72, 21.603},
	{0x3f00, 0x73, 21.793},
	{0x3f00, 0x74, 21.982},
	{0x3f00, 0x75, 22.172},
	{0x3f00, 0x76, 22.361},
	{0x3f00, 0x77, 22.551},
	{0x3f00, 0x78, 22.740},
	{0x3f00, 0x79, 22.930},
	{0x3f00, 0x7A, 23.119},
	{0x3f00, 0x7B, 23.309},
	{0x3f00, 0x7C, 23.498},
	{0x3f00, 0x7d, 23.688},
	{0x3f00, 0x7e, 23.877},
	{0x3f00, 0x7f, 24.067},


};

static int sc500ai_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 2880;
	info->modes.mode[0].size.h = 1616;
	info->modes.mode[0].fps = g_sc500ai_fps_info[0].fps;
	info->modes.num = 1;

	info->i2c.i2c_id = 0x30;
	info->i2c.addr_len = 2;
	info->i2c.data_len = 1;

	i = 0;
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_LOW, 0);
	set_power_item(&up->items[i++], SNR_IO_POWER, PWR_1V8, 1000);
	set_power_item(&up->items[i++], SNR_CORE_POWER, PWR_1V2, 1000);
	set_power_item(&up->items[i++], SNR_ANALOG_POWER, PWR_2V8, 3000);
	set_power_item(&up->items[i++], SNR_RST_GPIO, GPIO_HIGH, 3000);
	set_power_item(&up->items[i++], SNR_PWDN_GPIO, GPIO_HIGH, 5000);
	set_power_item(&up->items[i++], SNR_HCLK, CLK_24M, 5000);
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

static const struct fps_info *sc500ai_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_sc500ai_fps_info); i++)
		if (fps == g_sc500ai_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_sc500ai_fps_info))
		return NULL;

	return &g_sc500ai_fps_info[i];
}

static int sc500ai_get_init_info(uint32_t isp_id,
				 const struct rts_isp_sensor_mode *mode,
			       struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct sc500ai_status *status;
	uint16_t half_hts;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("sc500ai get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = sc500ai_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, clk_div: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->clk_div, fps_info->hts);

	half_hts = (fps_info->hts) / 2;
	set_init_i2c(&status->regs1[0], 0x320d, half_hts & 0xff);
	set_init_i2c(&status->regs1[1], 0x320c, half_hts >> 8);

	set_init_i2c_regs(info->sensor_regs[0],
		g_sc500ai_7fps_i2c_init_regs, 0);

	set_init_i2c(&status->regs1[2], 0x336d, status->ver_data[0]);
	set_init_i2c(&status->regs1[3], 0x363c, status->ver_data[1]);

	set_init_i2c_regs(info->sensor_regs[1], status->regs1, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = MIPI_LANE0 | MIPI_LANE1;
	info->interface.mipi.hs_term = 0x3;
	info->interface.type = RAW_SENSOR;
	info->interface.bit_depth = SNR_10BIT;

	info->size.w = 2880;
	info->size.h = 1620;
	info->start.x = 0;
	info->start.y = 1;

	info->hts = fps_info->hts;
	info->pclk = fps_info->clk;
	info->min_vts = status->min_vts = 2000;
	info->max_vts = 65535;

	status->exp_step = 1e6 * info->hts / info->pclk; /* us */
	status->cur_fps = mode->fps;

	return RTS_ISP_OK;
}

static int sc500ai_start(uint32_t isp_id)
{
	struct sc500ai_status *status;

	if (isp_id >= SUPPORTED_ISP_NUM)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];

	status->last_exposure = 0;

	return RTS_ISP_OK;
}

static uint16_t get_sensor_gain_reg(float fgain)
{
	uint16_t reg_value = 0;
	int i;

	if (fgain >= 15.918) {
		reg_value = 0x3f54;
	} else {
		for (i = 0; i < ((ARRAY_SIZE(gain_mapping)) - 1); i++) {
			if ((gain_mapping[i].total_gain <= fgain) &&
			    (fgain < gain_mapping[i + 1].total_gain)) {
				reg_value = gain_mapping[i].ana_gain |
					    gain_mapping[i].fine_gain;
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

	if (reg_value >= 0x3f54)
		gain = 15.918;
	else {
		for (i = 0; i < ((ARRAY_SIZE(gain_mapping)) - 1); i++) {
			if (reg_value == (gain_mapping[i].ana_gain |
			    gain_mapping[i].fine_gain)) {
				gain = gain_mapping[i].total_gain;
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

static int sc500ai_get_tuned_again(uint32_t isp_id,
				   float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int sc500ai_get_tuned_dgain(uint32_t isp_id,
				   float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int sc500ai_get_exposure_gain_info(uint32_t isp_id,
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
	struct sc500ai_status *status;
	struct rts_isp_sync_reg *reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !exp_gain || !regs)
		return -RTS_ISP_EINVAL;

	status = &g_status[isp_id];
	gain = exp_gain->analog_gain[0] * exp_gain->digital_gain[0];
	gain_reg = get_sensor_gain_reg(gain);

	total_line = exp_gain->vts;

	exp_reg_value_float =
		2.0 * exp_gain->exposure[0] / status->exp_step + 0.5f;
	exp_reg_value =
		clip_d_word(exp_reg_value_float, 1, (2 * total_line - 10));
	exp_reg_value = exp_reg_value << 4;

	total_line = (total_line + 1) / 2 * 2;
	reg = regs->reg;

	i = 0;
	set_sync_i2c(&reg[i++], 0x320e, (total_line >> 8));
	set_sync_i2c(&reg[i++], 0x320f, (total_line & 0xff));
	exp_set = abs(status->last_exposure - exp_gain->exposure[0]) > 0.001f;
	if (exp_set) {
		set_sync_i2c(&reg[i++], 0x3e00, exp_reg_value >> 16);
		set_sync_i2c(&reg[i++], 0x3e01, (exp_reg_value & 0xff00) >> 8);
		set_sync_i2c(&reg[i++], 0x3e02, exp_reg_value & 0xff);
		status->last_exposure = exp_gain->exposure[0];
	}
	set_sync_i2c(&reg[i++], 0x3e08, (gain_reg >> 8));
	set_sync_i2c(&reg[i++], 0x3e09, (gain_reg & 0xff));
	set_sync_i2c(&reg[i++], 0x3812, 0x00);
	if (gain >= 30.0f)
		set_sync_i2c(&reg[i++], 0x5799, 0x07);
	else if (gain <= 20.0f)
		set_sync_i2c(&reg[i++], 0x5799, 0x00);
	set_sync_i2c(&reg[i++], 0x3812, 0x30);
	regs->num = i;

	return RTS_ISP_OK;
}

static int sc500ai_check(uint32_t isp_id)
{
	int ret;
	int id;
	struct rts_isp_i2c_reg reg = {};
	struct sc500ai_status *status;

	/* ic version logic */
	status = &g_status[isp_id];
	reg.addr = 0x3109;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	if (reg.data == 1)
		status->ver_data[0] = 0x23;
	else
		status->ver_data[0] = 0x03;

	reg.addr = 0x3040;
	ret = rts_isp_read_sensor_reg(isp_id, &reg);
	if (ret)
		return ret;
	if (reg.data == 0)
		status->ver_data[1] = 0x42;
	else
		status->ver_data[1] = 0x40;

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

	if (id == 0xce1f)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops sc500ai_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "sc500ai",
	.get_info = sc500ai_get_info,
	.get_init_info = sc500ai_get_init_info,
	.start = sc500ai_start,
	.get_tuned_again = sc500ai_get_tuned_again,
	.get_tuned_dgain = sc500ai_get_tuned_dgain,
	.get_exposure_gain_info = sc500ai_get_exposure_gain_info,
	.check = sc500ai_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(sc500ai_ops)
