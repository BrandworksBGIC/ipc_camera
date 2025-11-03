/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2020 Eric Yang <eric_yang@realsil.com.cn>
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

struct sc401ai_gain {
	uint16_t ana_gain;
	uint16_t fine_gain;
	float total_gain;
};

struct sc401ai_status {
	float exp_step;
	float last_exposure;
	uint16_t cur_fps;
	uint16_t min_vts;
	struct rts_isp_i2c_reg regs1[2];
};

static struct sc401ai_status g_status[SUPPORTED_ISP_NUM];

static const struct fps_info g_sc401ai_fps_info[] = {
	{30, 2800, 126000000},
};

static struct rts_isp_i2c_reg g_sc401ai_30fps_i2c_init_regs[] = {
	{0x0103, 0x01}, {0x0100, 0x00}, {0x36e9, 0x80}, {0x36f9, 0x80},
	{0x301c, 0x78}, {0x301f, 0x09}, {0x3200, 0x00}, {0x3201, 0x00},
	{0x3202, 0x00}, {0x3203, 0x00}, {0x3204, 0x0a}, {0x3205, 0x07},
	{0x3206, 0x05}, {0x3207, 0xab}, {0x3208, 0x0a}, {0x3209, 0x00},
	{0x320a, 0x05}, {0x320b, 0xa0}, {0x320e, 0x05}, {0x320f, 0xdc},
	{0x3210, 0x00}, {0x3211, 0x04}, {0x3212, 0x00}, {0x3213, 0x03},
	{0x3214, 0x11}, {0x3215, 0x11}, {0x3223, 0x80}, {0x3250, 0x00},
	{0x3253, 0x08}, {0x3274, 0x01}, {0x3301, 0x20}, {0x3302, 0x18},
	{0x3303, 0x10}, {0x3304, 0x50}, {0x3306, 0x38}, {0x3308, 0x18},
	{0x3309, 0x60}, {0x330b, 0xc0}, {0x330d, 0x10}, {0x330e, 0x18},
	{0x330f, 0x04}, {0x3310, 0x02}, {0x331c, 0x04}, {0x331e, 0x41},
	{0x331f, 0x51}, {0x3320, 0x09}, {0x3333, 0x10}, {0x334c, 0x08},
	{0x3356, 0x09}, {0x3364, 0x17}, {0x338e, 0xfd}, {0x3390, 0x08},
	{0x3391, 0x18}, {0x3392, 0x38}, {0x3393, 0x20}, {0x3394, 0x20},
	{0x3395, 0x20}, {0x3396, 0x08}, {0x3397, 0x18}, {0x3398, 0x38},
	{0x3399, 0x20}, {0x339a, 0x20}, {0x339b, 0x20}, {0x339c, 0x20},
	{0x33ac, 0x10}, {0x33ae, 0x18}, {0x33af, 0x19}, {0x360f, 0x01},
	{0x3620, 0x08}, {0x3637, 0x25}, {0x363a, 0x1f}, {0x3670, 0x0a},
	{0x3671, 0x07}, {0x3672, 0x57}, {0x3673, 0x5e}, {0x3674, 0x84},
	{0x3675, 0x88}, {0x3676, 0x8a}, {0x367a, 0x58}, {0x367b, 0x78},
	{0x367c, 0x58}, {0x367d, 0x78}, {0x3690, 0x33}, {0x3691, 0x43},
	{0x3692, 0x34}, {0x369c, 0x40}, {0x369d, 0x78}, {0x36ea, 0x39},
	{0x36eb, 0x0d}, {0x36ec, 0x2c}, {0x36ed, 0x24}, {0x36fa, 0x39},
	{0x36fb, 0x33}, {0x36fc, 0x10}, {0x36fd, 0x14}, {0x3908, 0x41},
	{0x396c, 0x0e}, {0x3e00, 0x00}, {0x3e01, 0xb6}, {0x3e02, 0x00},
	{0x3e03, 0x0b}, {0x3e08, 0x03}, {0x3e09, 0x40}, {0x3e1b, 0x2a},
	{0x4509, 0x30}, {0x57a8, 0xd0}, {0x36e9, 0x23}, {0x36f9, 0x23},
	{0x3221, 0x66}, {0x0100, 0x01},
};

static struct sc401ai_gain gain_mapping[] = {
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
	{0x2300, 0x40, 1.469},
	{0x2300, 0x41, 1.492},
	{0x2300, 0x42, 1.515},
	{0x2300, 0x43, 1.538},
	{0x2300, 0x44, 1.561},
	{0x2300, 0x45, 1.584},
	{0x2300, 0x46, 1.607},
	{0x2300, 0x47, 1.630},
	{0x2300, 0x48, 1.653},
	{0x2300, 0x49, 1.676},
	{0x2300, 0x4a, 1.699},
	{0x2300, 0x4b, 1.721},
	{0x2300, 0x4c, 1.744},
	{0x2300, 0x4d, 1.767},
	{0x2300, 0x4e, 1.790},
	{0x2300, 0x4f, 1.813},
	{0x2300, 0x50, 1.836},
	{0x2300, 0x51, 1.859},
	{0x2300, 0x52, 1.882},
	{0x2300, 0x53, 1.905},
	{0x2300, 0x54, 1.928},
	{0x2300, 0x55, 1.951},
	{0x2300, 0x56, 1.974},
	{0x2300, 0x57, 1.997},
	{0x2300, 0x58, 2.020},
	{0x2300, 0x59, 2.043},
	{0x2300, 0x5a, 2.066},
	{0x2300, 0x5b, 2.089},
	{0x2300, 0x5c, 2.112},
	{0x2300, 0x5d, 2.135},
	{0x2300, 0x5e, 2.158},
	{0x2300, 0x5f, 2.181},
	{0x2300, 0x60, 2.204},
	{0x2300, 0x61, 2.226},
	{0x2300, 0x62, 2.249},
	{0x2300, 0x63, 2.272},
	{0x2300, 0x64, 2.295},
	{0x2300, 0x65, 2.318},
	{0x2300, 0x66, 2.341},
	{0x2300, 0x67, 2.364},
	{0x2300, 0x68, 2.387},
	{0x2300, 0x69, 2.410},
	{0x2300, 0x6a, 2.433},
	{0x2300, 0x6b, 2.456},
	{0x2300, 0x6C, 2.479},
	{0x2300, 0x6D, 2.502},
	{0x2300, 0x6E, 2.525},
	{0x2300, 0x6F, 2.548},
	{0x2300, 0x70, 2.571},
	{0x2300, 0x71, 2.594},
	{0x2300, 0x72, 2.617},
	{0x2300, 0x73, 2.640},
	{0x2300, 0x74, 2.663},
	{0x2300, 0x75, 2.686},
	{0x2300, 0x76, 2.708},
	{0x2300, 0x77, 2.731},
	{0x2300, 0x78, 2.754},
	{0x2300, 0x79, 2.777},
	{0x2300, 0x7A, 2.800},
	{0x2300, 0x7B, 2.823},
	{0x2300, 0x7C, 2.846},
	{0x2300, 0x7D, 2.869},
	{0x2300, 0x7E, 2.892},
	{0x2300, 0x7F, 2.915},
	{0x2700, 0x40, 2.938},
	{0x2700, 0x41, 2.984},
	{0x2700, 0x42, 3.030},
	{0x2700, 0x43, 3.076},
	{0x2700, 0x44, 3.122},
	{0x2700, 0x45, 3.168},
	{0x2700, 0x46, 3.213},
	{0x2700, 0x47, 3.259},
	{0x2700, 0x48, 3.305},
	{0x2700, 0x49, 3.351},
	{0x2700, 0x4A, 3.397},
	{0x2700, 0x4B, 3.443},
	{0x2700, 0x4C, 3.489},
	{0x2700, 0x4D, 3.535},
	{0x2700, 0x4E, 3.581},
	{0x2700, 0x4F, 3.627},
	{0x2700, 0x50, 3.673},
	{0x2700, 0x51, 3.718},
	{0x2700, 0x52, 3.764},
	{0x2700, 0x53, 3.810},
	{0x2700, 0x54, 3.856},
	{0x2700, 0x55, 3.902},
	{0x2700, 0x56, 3.948},
	{0x2700, 0x57, 3.994},
	{0x2700, 0x58, 4.040},
	{0x2700, 0x59, 4.086},
	{0x2700, 0x5A, 4.132},
	{0x2700, 0x5B, 4.177},
	{0x2700, 0x5C, 4.223},
	{0x2700, 0x5D, 4.269},
	{0x2700, 0x5E, 4.315},
	{0x2700, 0x5F, 4.361},
	{0x2700, 0x60, 4.407},
	{0x2700, 0x61, 4.453},
	{0x2700, 0x62, 4.499},
	{0x2700, 0x63, 4.545},
	{0x2700, 0x64, 4.591},
	{0x2700, 0x65, 4.637},
	{0x2700, 0x66, 4.682},
	{0x2700, 0x67, 4.728},
	{0x2700, 0x68, 4.774},
	{0x2700, 0x69, 4.820},
	{0x2700, 0x6A, 4.866},
	{0x2700, 0x6B, 4.912},
	{0x2700, 0x6C, 4.958},
	{0x2700, 0x6D, 5.004},
	{0x2700, 0x6E, 5.050},
	{0x2700, 0x6F, 5.096},
	{0x2700, 0x70, 5.142},
	{0x2700, 0x71, 5.187},
	{0x2700, 0x72, 5.233},
	{0x2700, 0x73, 5.279},
	{0x2700, 0x74, 5.325},
	{0x2700, 0x75, 5.371},
	{0x2700, 0x76, 5.417},
	{0x2700, 0x77, 5.463},
	{0x2700, 0x78, 5.509},
	{0x2700, 0x79, 5.555},
	{0x2700, 0x7A, 5.601},
	{0x2700, 0x7B, 5.646},
	{0x2700, 0x7C, 5.692},
	{0x2700, 0x7D, 5.738},
	{0x2700, 0x7E, 5.784},
	{0x2700, 0x7F, 5.830},
	{0x2F00, 0x40, 5.876},
	{0x2F00, 0x41, 5.968},
	{0x2F00, 0x42, 6.060},
	{0x2F00, 0x43, 6.151},
	{0x2F00, 0x44, 6.243},
	{0x2F00, 0x45, 6.335},
	{0x2F00, 0x46, 6.427},
	{0x2F00, 0x47, 6.519},
	{0x2F00, 0x48, 6.611},
	{0x2F00, 0x49, 6.702},
	{0x2F00, 0x4A, 6.794},
	{0x2F00, 0x4B, 6.886},
	{0x2F00, 0x4C, 6.978},
	{0x2F00, 0x4D, 7.070},
	{0x2F00, 0x4E, 7.161},
	{0x2F00, 0x4F, 7.253},
	{0x2F00, 0x50, 7.345},
	{0x2F00, 0x51, 7.437},
	{0x2F00, 0x52, 7.529},
	{0x2F00, 0x53, 7.620},
	{0x2F00, 0x54, 7.712},
	{0x2F00, 0x55, 7.804},
	{0x2F00, 0x56, 7.896},
	{0x2F00, 0x57, 7.988},
	{0x2F00, 0x58, 8.080},
	{0x2F00, 0x59, 8.171},
	{0x2F00, 0x5A, 8.263},
	{0x2F00, 0x5B, 8.355},
	{0x2F00, 0x5C, 8.447},
	{0x2F00, 0x5D, 8.539},
	{0x2F00, 0x5E, 8.630},
	{0x2F00, 0x5F, 8.722},
	{0x2F00, 0x60, 8.814},
	{0x2F00, 0x61, 8.906},
	{0x2F00, 0x62, 8.998},
	{0x2F00, 0x63, 9.089},
	{0x2F00, 0x64, 9.181},
	{0x2F00, 0x65, 9.273},
	{0x2F00, 0x66, 9.365},
	{0x2F00, 0x67, 9.457},
	{0x2F00, 0x68, 9.549},
	{0x2F00, 0x69, 9.640},
	{0x2F00, 0x6A, 9.732},
	{0x2F00, 0x6B, 9.824},
	{0x2F00, 0x6C, 9.916},
	{0x2F00, 0x6D, 10.008},
	{0x2F00, 0x6E, 10.099},
	{0x2F00, 0x6F, 10.191},
	{0x2F00, 0x70, 10.283},
	{0x2F00, 0x71, 10.375},
	{0x2F00, 0x72, 10.467},
	{0x2F00, 0x73, 10.558},
	{0x2F00, 0x74, 10.650},
	{0x2F00, 0x75, 10.742},
	{0x2F00, 0x76, 10.834},
	{0x2F00, 0x77, 10.926},
	{0x2F00, 0x78, 11.018},
	{0x2F00, 0x79, 11.109},
	{0x2F00, 0x7A, 11.201},
	{0x2F00, 0x7B, 11.293},
	{0x2F00, 0x7C, 11.385},
	{0x2F00, 0x7D, 11.477},
	{0x2F00, 0x7E, 11.568},
	{0x2F00, 0x7F, 11.660},
	{0x3F00, 0x40, 11.752},
	{0x3F00, 0x41, 11.936},
	{0x3F00, 0x42, 12.119},
	{0x3F00, 0x43, 12.303},
	{0x3F00, 0x44, 12.487},
	{0x3F00, 0x45, 12.670},
	{0x3F00, 0x46, 12.854},
	{0x3F00, 0x47, 13.037},
	{0x3F00, 0x48, 13.221},
	{0x3F00, 0x49, 13.405},
	{0x3F00, 0x4A, 13.588},
	{0x3F00, 0x4B, 13.772},
	{0x3F00, 0x4C, 13.956},
	{0x3F00, 0x4D, 14.139},
	{0x3F00, 0x4E, 14.323},
	{0x3F00, 0x4F, 14.506},
	{0x3F00, 0x50, 14.690},
	{0x3F00, 0x51, 14.874},
	{0x3F00, 0x52, 15.057},
	{0x3F00, 0x53, 15.241},
	{0x3F00, 0x54, 15.425},
	{0x3F00, 0x55, 15.608},
	{0x3F00, 0x56, 15.792},
	{0x3F00, 0x57, 15.975},
	{0x3F00, 0x58, 16.159},
	{0x3F00, 0x59, 16.343},
	{0x3F00, 0x5A, 16.526},
	{0x3F00, 0x5B, 16.710},
	{0x3F00, 0x5C, 16.894},
	{0x3F00, 0x5D, 17.077},
	{0x3F00, 0x5E, 17.261},
	{0x3F00, 0x5F, 17.444},
	{0x3F00, 0x60, 17.628},
	{0x3F00, 0x61, 17.812},
	{0x3F00, 0x62, 17.995},
	{0x3F00, 0x63, 18.179},
	{0x3F00, 0x64, 18.363},
	{0x3F00, 0x65, 18.546},
	{0x3F00, 0x66, 18.730},
	{0x3F00, 0x67, 18.913},
	{0x3F00, 0x68, 19.097},
	{0x3F00, 0x69, 19.281},
	{0x3F00, 0x6A, 19.464},
	{0x3F00, 0x6B, 19.648},
	{0x3F00, 0x6C, 19.832},
	{0x3F00, 0x6D, 20.015},
	{0x3F00, 0x6E, 20.199},
	{0x3F00, 0x6F, 20.382},
	{0x3F00, 0x70, 20.566},
	{0x3F00, 0x71, 20.750},
	{0x3F00, 0x72, 20.933},
	{0x3F00, 0x73, 21.117},
	{0x3F00, 0x74, 21.301},
	{0x3F00, 0x75, 21.484},
	{0x3F00, 0x76, 21.668},
	{0x3F00, 0x77, 21.851},
	{0x3F00, 0x78, 22.035},
	{0x3F00, 0x79, 22.219},
	{0x3F00, 0x7A, 22.402},
	{0x3F00, 0x7B, 22.586},
	{0x3F00, 0x7C, 22.770},
	{0x3F00, 0x7D, 22.953},
	{0x3F00, 0x7E, 23.137},
	{0x3F00, 0x7F, 23.320},
};

static int sc401ai_get_info(uint32_t isp_id, struct rts_isp_sensor_info *info)
{
	int i;
	struct rts_isp_snr_pwr *up = &info->power_up;
	struct rts_isp_snr_pwr *down = &info->power_down;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	info->modes.mode[0].hdr = RTS_ISP_HDR_NONE;
	info->modes.mode[0].size.w = 2560;
	info->modes.mode[0].size.h = 1440;
	info->modes.mode[0].fps = g_sc401ai_fps_info[0].fps;
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

static const struct fps_info *sc401ai_get_fps_info(uint16_t fps)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_sc401ai_fps_info); i++)
		if (fps == g_sc401ai_fps_info[i].fps)
			break;
	if (i == ARRAY_SIZE(g_sc401ai_fps_info))
		return NULL;

	return &g_sc401ai_fps_info[i];
}

static int sc401ai_get_init_info(uint32_t isp_id,
				 const struct rts_isp_sensor_mode *mode,
			       struct rts_isp_sensor_init_info *info)
{
	const struct fps_info *fps_info;
	struct sc401ai_status *status;
	uint16_t half_hts;

	if (isp_id >= SUPPORTED_ISP_NUM || !info)
		return -RTS_ISP_EINVAL;

	debug("sc401ai get fps %.1f init info\n", mode->fps);

	status = &g_status[isp_id];
	fps_info = sc401ai_get_fps_info(mode->fps);
	if (!fps_info)
		return -RTS_ISP_EINVAL;

	debug("fps: %u, pclk: %u, clk_div: %u, hts: %u\n",
	      fps_info->fps, fps_info->clk, fps_info->clk_div, fps_info->hts);

	half_hts = (fps_info->hts) / 2;
	set_init_i2c(&status->regs1[0], 0x320d, half_hts & 0xff);
	set_init_i2c(&status->regs1[1], 0x320c, half_hts >> 8);

	set_init_i2c_regs(info->sensor_regs[0],
		g_sc401ai_30fps_i2c_init_regs, 0);

	set_init_i2c_regs(info->sensor_regs[1], status->regs1, 0);

	info->interface.interface = SNR_INTERFACE_MIPI;
	info->interface.mipi.lanes = (MIPI_LANE0 | MIPI_LANE1 |
				      MIPI_LANE2 | MIPI_LANE3);
	info->interface.mipi.hs_term = 0x3;
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

static int sc401ai_start(uint32_t isp_id)
{
	struct sc401ai_status *status;

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

	if (fgain >= 15.975) {
		reg_value = 0x3F57;
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

	if (reg_value >= 0x3F57)
		gain = 15.975;
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

static int sc401ai_get_tuned_again(uint32_t isp_id,
				   float again[RTS_ISP_HDR_CHAN_MAX])
{
	int gain_reg;

	if (isp_id >= SUPPORTED_ISP_NUM || !again)
		return -RTS_ISP_EINVAL;

	gain_reg = get_sensor_gain_reg(again[0]);
	again[0] = get_sensor_real_gain(gain_reg);

	return RTS_ISP_OK;
}

static int sc401ai_get_tuned_dgain(uint32_t isp_id,
				   float dgain[RTS_ISP_HDR_CHAN_MAX])
{
	if (isp_id >= SUPPORTED_ISP_NUM || !dgain)
		return -RTS_ISP_EINVAL;

	dgain[0] = 1.0f;

	return RTS_ISP_OK;
}

static int sc401ai_get_exposure_gain_info(uint32_t isp_id,
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
	struct sc401ai_status *status;
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
		clip_d_word(exp_reg_value_float, 4, (2 * total_line - 8));
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
	if (gain < 2.0f)
		set_sync_i2c(&reg[i++], 0x363a, 0x0b);
	else if (gain >= 2.0f)
		set_sync_i2c(&reg[i++], 0x363a, 0x17);
	set_sync_i2c(&reg[i++], 0x3812, 0x30);
	regs->num = i;

	return RTS_ISP_OK;
}

static int sc401ai_get_mirror_flip(uint32_t isp_id,
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
		val |= 0x6;
	if (mf_info->flip)
		val |= 0x60;
	reg = regs->reg;
	set_sync_i2c_mask(&reg[i++], 0x3221, ~val, 0x66);
	regs->num = i;

	return RTS_ISP_OK;
}

static int sc401ai_check(uint32_t isp_id)
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

	if (id == 0xcd2e)
		return RTS_ISP_OK;
	else
		return -RTS_ISP_EINVAL;
}

static const struct rts_isp_sensor_ops sc401ai_ops = {
	.api_version = SENSOR_API_VERSION,
	.name = "sc401ai",
	.get_info = sc401ai_get_info,
	.get_init_info = sc401ai_get_init_info,
	.start = sc401ai_start,
	.get_tuned_again = sc401ai_get_tuned_again,
	.get_tuned_dgain = sc401ai_get_tuned_dgain,
	.get_exposure_gain_info = sc401ai_get_exposure_gain_info,
	.get_mirror_flip = sc401ai_get_mirror_flip,
	.check = sc401ai_check,
};

RTS_ISP_DEFINE_SENSOR_PLUGIN(sc401ai_ops)
