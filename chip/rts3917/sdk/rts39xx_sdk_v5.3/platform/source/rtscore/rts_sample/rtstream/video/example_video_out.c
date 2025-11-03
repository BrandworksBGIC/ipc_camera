/*
 * Realtek Semiconductor Corp.
 *
 * example_video_out.c
 *
 * Copyright (C) 2020	<wil_shi@realsil.com.cn>
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>

/* code flow

+-------------+         +-------------+
|             |         |             |
|    vin_ch   |========>|   vout_ch   |
|  (src_img)  |         |  (dst_img)  |
+-------------+         +-------------+

*/

struct __dbg_option {
	int vin_id;
	uint32_t width;
	uint32_t height;
	uint32_t fps;
	uint32_t buf_num;

	uint32_t x;
	uint32_t y;
	uint32_t crop_x;
	uint32_t crop_y;
	uint32_t crop_w;
	uint32_t crop_h;
};

static int g_exit;
static void Termination(int sign)
{
	g_exit = 1;
}

void print_help_info(void)
{
	fprintf(stdout, "DESCRIPTION:\n");
	fprintf(stdout,
		"\texample for video out, not support for RTS3903/RTS3913\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "\texample_video_out");
	fprintf(stdout, " [w] [h] [x] [y] [crop_x] [crop_y]");
	fprintf(stdout, " [crop_w] [crop_h] [opt fps] [opt buf_num]\n");
	fprintf(stdout, "EXAMPLE:\n");
	fprintf(stdout, "\texample_video_out 1280 800 0 0 100 100 640 480\n");
	fprintf(stdout, "\texample_video_out 1280 800 0 0 100 100 640 480 5\n");
}


int test_stream(struct __dbg_option *option)
{
	struct rts_vin_attr vin_attr;
	struct rts_av_profile profile;
	struct rts_vout_attr outattr;

	int vin = -1;
	int out = -1;
	int ret;

	vin_attr.vin_id = option->vin_id;
	vin_attr.vin_buf_num = option->buf_num;
	vin = rts_av_create_vin_chn(&vin_attr);
	if (vin < 0) {
		RTS_ERR("fail to create vin chn, ret = %d\n", vin);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}
	RTS_INFO("vin chn : %d\n", vin);

	profile.fmt = RTS_V_FMT_YUV420SEMIPLANAR;
	profile.video.width = option->width;
	profile.video.height = option->height;
	profile.video.numerator = 1;
	profile.video.denominator = option->fps;
	ret = rts_av_set_profile(vin, &profile);
	if (ret) {
		RTS_ERR("set vin profile fail, ret = %d\n", ret);
		goto exit;
	}

	outattr.display.x = option->x;
	outattr.display.y = option->y;
	outattr.crop.start_x = option->crop_x;
	outattr.crop.start_y = option->crop_y;
	outattr.crop.width = option->crop_w;
	outattr.crop.height = option->crop_h;
	out = rts_av_create_vout_chn(&outattr);
	if (out < 0) {
		RTS_ERR("fail to create video out chn, ret = %d\n", out);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}

	rts_av_bind(vin, out);

	ret = rts_av_enable_chn(vin);
	if (ret)
		goto exit;


	ret = rts_av_enable_chn(out);
	if (ret)
		goto exit;

	while (!g_exit)
		usleep(1000);

	rts_av_disable_chn(vin);
	rts_av_disable_chn(out);

	rts_av_unbind(vin, out);
exit:

	if (out >= 0) {
		rts_av_destroy_chn(out);
		out = -1;
	}

	if (vin >= 0) {
		rts_av_destroy_chn(vin);
		vin = -1;
	}

	return ret;
}

int main(int argc, char *argv[])
{
	struct __dbg_option option;
	int c;
	int ret;

	if (argc < 9) {
		if (argc >= 2) {
			if (!strcmp(argv[1], "-h")
				|| !strcmp(argv[1], "--help")) {
				print_help_info();
				return 0;
			}
		}
		printf("too few parameter\n");
		printf("use -h to get help info\n");
		return -1;
	}

	rts_set_log_mask(RTS_LOG_MASK_CONS);

	signal(SIGINT, Termination);
	signal(SIGTERM, Termination);

	ret = rts_av_init();
	if (ret)
		return ret;

	option.vin_id = 0;
	option.width = (int)strtol(argv[1], NULL, 0);
	option.height = (int)strtol(argv[2], NULL, 0);
	option.fps = argc < 10 ? 5 : (int)strtol(argv[9], NULL, 0);
	option.buf_num = argc < 11 ? 2 : (int)strtol(argv[10], NULL, 0);

	option.x = (int)strtol(argv[3], NULL, 0);
	option.y = (int)strtol(argv[4], NULL, 0);
	option.crop_x = (int)strtol(argv[5], NULL, 0);
	option.crop_y = (int)strtol(argv[6], NULL, 0);
	option.crop_w = (int)strtol(argv[7], NULL, 0);
	option.crop_h = (int)strtol(argv[8], NULL, 0);


	RTS_INFO("%d %d %d %d %d %d %d %d\n",
			option.width, option.height,
			option.x, option.y,
			option.crop_x, option.crop_y,
			option.crop_w, option.crop_h);

	test_stream(&option);

	rts_av_release();

	return RTS_OK;
}
