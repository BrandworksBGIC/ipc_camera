/*
 * Realtek Semiconductor Corp.
 *
 * example/example_md2.c
 *
 * Copyright (C) 2019      Anakin Wang<anakin_wang@realsil.com.cn>
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>

#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>

/* code flow

+-----------+     +-----------+     +-----------+     +-------------+
|           |     |           |     |           |     |             |
| rts_av_   |     | rts_av_   |     | rts_av_   |     | rts_av_get  |
| query_md2 |====>| set_md2   |====>| poll_md2  |====>| _md2_result |
|           |     |           |     |           |     |(motion_map) |
+-----------+     +-----------+     +-----------+     |(or res bmp) |
                                                      +-------------+
*/

#define ALIGN(x, a)          (((x) + (a) - 1) & (~((a) - 1)))

struct rts_md2_attr g_attr;
struct rts_md2_ctrl g_ctrl = {
	.sensitivity = -1,
};

char *g_save_path;
int g_exit;
int g_debug;
int g_pprc = 0xff;
int d_w;
int d_h;
int show;
int roi_flag = 0;
static int rts_num;
static int rts_ret;

static void __save_result_bmp(int w, int h, int ind, void *buf);

static int __bin_string_to_dec(char *c)
{
	int len;
	int res = 0;

	len = strlen(c);
	if (*(c++) == '0')
		len--;
	if (*(c++) == 'b')
		len--;
	while(len) {
		res += (res + *(c++)-'0');
		len--;
	}

	return res;
}

const static char *optstrings = "a:b:c:e:hS:s:k:t:T:p:d:n:";
static int __parse_args(int argc, char *argv[])
{
	int opt;
	char *c;

	g_attr.sample.x = 0;
	g_attr.sample.y = 0;
	g_attr.sample.w = 192;
	g_attr.sample.h = 108;
	g_attr.sample.scale_x = 10;
	g_attr.sample.scale_y = 10;

	g_attr.bin_bits = 0;
	g_attr.nr_bins = 0;

	while ((opt = getopt(argc, argv, optstrings)) != -1) {
		switch (opt) {
		case 'a':
			g_ctrl.max_ar = atof(optarg);
			g_ctrl.min_ar = atof(argv[optind]);
			g_ctrl.cc_ratio = atof(argv[optind + 1]);
			break;
		case 'S':
			g_attr.sample.x = atoi(optarg);
			g_attr.sample.y = atoi(argv[optind]);
			g_attr.sample.w = atoi(argv[optind + 1]);
			g_attr.sample.h = atoi(argv[optind + 2]);
			g_attr.sample.scale_x = atoi(argv[optind + 3]);
			g_attr.sample.scale_y = atoi(argv[optind + 4]);
			break;
		case 's':
			g_save_path = optarg;
			mkdir(g_save_path, 0777);
			break;
		case 'b':
			g_attr.bin_bits = atoi(optarg);
			g_attr.nr_bins = atoi(argv[optind]);
			break;
		case 'c':
			g_ctrl.nr_cc_thd = atoi(optarg);
			break;
		case 'e':
			g_ctrl.sensitivity = atoi(optarg);
			break;
		case 'k':
			g_attr.skip_frames = atoi(optarg);
			break;
		case 'T':
			g_ctrl.train_enable = 1;
			g_ctrl.train_frames = atoi(optarg);
			break;
		case 't':
			g_ctrl.back_thd = atoi(optarg);
			g_ctrl.learn_thd = atoi(argv[optind]);
			g_ctrl.forget_thd = atoi(argv[optind + 1]);
			break;
		case 'd':
			d_w = atoi(optarg);
			d_h = atoi(argv[optind]);
			c = argv[optind + 1];
			show = __bin_string_to_dec(c);
			roi_flag = 1;
			break;
		case 'p':
			c = optarg;
			g_pprc = __bin_string_to_dec(c);
			break;
		case 'h':
			printf("Usage: example_md2 [options]...\n");
			printf("     -S <x y w h sx sy> | set sample region\n");
			printf("     -b <bin_bits nr_bins> | set binsetting\n");
			printf("     -k <skip_frames> | set skip\n");
			printf("     -a <max_ar min_ar cc_ratio>\n");
			printf("     -e <sensitivity>\n");
			printf("     -c <nr_cc_thd>\n");
			printf("     -T <train_frames>\n");
			printf("     -t <back_thd learn_thd forget_thd>\n");
			printf("     -d <divide_w divide_h show_block>"
						"use bit set show_block, like 0b1001\n");
			printf("     -p <pprc_mask:0b1 en postprocess,0b10 "
						"en ccinfo 0b100 en ccfilter>\n");
			printf("     -s <save_path>\n");
			printf("     -n <frame_number>\n");
			printf("example:\n");
			printf("     example_md2 -n 10\n");
			printf("\n");
			exit(0);
		case 'n':
			rts_num = atoi(optarg);
			break;
		default:
			fprintf(stderr, "unknown args!\n");
			exit(-1);
		}

	}

	printf("attr: sample %d %d %d %d %d %d, bin: %d %d\n",
			g_attr.sample.x,
			g_attr.sample.y,
			g_attr.sample.w,
			g_attr.sample.h,
			g_attr.sample.scale_x,
			g_attr.sample.scale_y,
			g_attr.bin_bits,
			g_attr.nr_bins);

	return 0;
}

static void __set_ctrl(struct rts_md2_ctrl *pctrl)
{
	if (g_ctrl.train_enable) {
		pctrl->train_enable = g_ctrl.train_enable;
		pctrl->train_frames = g_ctrl.train_frames;
	}

	if (g_ctrl.sensitivity >= 0)
		pctrl->sensitivity = g_ctrl.sensitivity;
	if (g_ctrl.back_thd) {
		pctrl->back_thd = g_ctrl.back_thd;
		pctrl->learn_thd = g_ctrl.learn_thd;
		pctrl->forget_thd = g_ctrl.forget_thd;
	}
	if (g_ctrl.max_ar) {
		pctrl->max_ar = g_ctrl.max_ar;
		pctrl->min_ar = g_ctrl.min_ar;
		pctrl->cc_ratio = g_ctrl.cc_ratio;
	}
	if (g_ctrl.nr_cc_thd)
		pctrl->nr_cc_thd = g_ctrl.nr_cc_thd;
}


void sighandle(int sig)
{
	g_exit = 1;
}


void __draw_rect(uint8_t *d, int w, int h,
		int l, int u, int r, int b, int isbitmap)
{
	int s1 = u * w;
	int s2 = b * w;

	if (isbitmap) {
		for (int i = s1 + l; i <= s1 + r; i++)
			d[i/8] |= (1 << (i%8));
		for (int i = s2 + l; i <= s2 + r; i++)
			d[i/8] |= (1 << (i%8));
		for (int i = s1 + l; i <= s2 + l; i += w)
			d[i/8] |= (1 << (i%8));
		for (int i = s1 + r; i <= s2 + r; i += w)
			d[i/8] |= (1 << (i%8));
	} else {
		for (int i = s1 + l; i <= s1 + r; i++)
			d[i] = 255;
		for (int i = s2 + l; i <= s2 + r; i++)
			d[i] = 255;
		for (int i = s1 + l; i <= s2 + l; i += w)
			d[i] = 255;
		for (int i = s1 + r; i <= s2 + r; i += w)
			d[i] = 255;
	}
}

static void __set_bit(void *img, uint32_t index, uint8_t y)
{

	((uint8_t *)img)[index] = y;
}

static void __set_block(void *img, int x_start, int w, int b_w, int b_h, int y)
{
	int i, j, index, tmp;

	for (j = 0; j < b_h; j++) {
		tmp = j * w;
		for (i = 0; i < b_w; i++) {
			index = x_start + tmp + i;
			__set_bit(img, index, y);
		}
	}
}

static void __set_roi_map(uint8_t *roi_buf, int divide_w, int divide_h, int val)
{
	uint8_t *p;
	uint8_t *q;
	int i, j, s, k;
	uint32_t w = g_attr.sample.w;
	uint32_t h = g_attr.sample.h;
	uint32_t d_w = w / divide_w;
	uint32_t d_h = h / divide_h;

	for (j = 0; j < divide_h; j++) {
		for (i = 0; i < divide_w; i++) {
			s = i * d_w + j * d_h * w;
			k = i + j * divide_h;
			if ((val >> k) & 1) {
				__set_block(roi_buf, s, w, d_w, d_h, 1);
			} else
				__set_block(roi_buf, s, w, d_w, d_h, 0);
		}
	}
}


static void *enable_md2(void *arg)
{
	struct rts_md2_ctrl *pctrl = NULL;
	int ret;
	int frame_ind = 0;
	uint8_t *roi_buf = NULL;
	uint32_t roi_len;

	ret = rts_av_query_md2(&pctrl, &g_attr);
	if (ret) {
		RTS_ERR("query md2 failed [%d]\n", ret);
		goto exit;
	}

	if (roi_flag) {
		roi_len = g_attr.sample.w * g_attr.sample.h;
		roi_len = ALIGN(roi_len, 8);

		roi_buf = (uint8_t *)rts_malloc(roi_len);
		if (!roi_buf)
			goto exit;

		__set_roi_map(roi_buf, d_w, d_h, show);

		RTS_S_C_VAR(pctrl->roi.map, roi_buf, uint8_t *);
	}
	__set_ctrl(pctrl);
	ret = rts_av_set_md2(pctrl);
	if (ret)
		goto exit;

	while (!g_exit) {
		int wret = 0;
		struct rts_md2_result res = {0};


		res.flags = g_pprc;

		wret = rts_av_poll_md2(pctrl, 1000);
		if (wret == RTS_FALSE) {
			RTS_ERR("poll md2 timeout\n");
			continue;
		}

		if (g_save_path)
			res.flags |= RTS_MD2_RESULT_FL_ENABLE_MOTION_MAP;

		wret = rts_av_get_md2_result(pctrl, &res);
		if (!wret) {
			printf("get_md2_result motion_cnt [%d]\n",
				res.motion_cnt);

			if ((g_pprc & RTS_MD2_RESULT_FL_ENABLE_CC_INFO)) {
				printf("cc_info length %03d:\n",
						res.cc_info.cc_len);
				for (int i = 0; i < res.cc_info.cc_len; i++) {

					printf("\tcc_%03d  %d\n", i,
						res.cc_info.cc[i].pixel_cnt);
					__draw_rect(res.motion_map,
							g_attr.sample.w,
							g_attr.sample.h,
							res.cc_info.cc[i].l,
							res.cc_info.cc[i].u,
							res.cc_info.cc[i].r,
							res.cc_info.cc[i].b,
							0
							);
				}
			}

			if (g_save_path)
				__save_result_bmp(g_attr.sample.w,
						g_attr.sample.h,
						++frame_ind,
						res.motion_map);
		}
	}
exit:
	RTS_SAFE_RELEASE(pctrl, rts_av_release_md2);
	RTS_SAFE_DELETE(roi_buf);
	rts_ret = ret;

	return NULL;
}

int test_stream(void)
{
	struct rts_vin_attr vin_attr = {0};
	struct rts_av_profile profile;
	struct rts_h265_attr h265_attr = {0};
	uint32_t number = 0;
	int vin = -1;
	int enc = -1;
	int ret;
	pthread_t tid;

	vin_attr.vin_id = 0;
	vin_attr.vin_buf_num = 1;
	vin_attr.vin_mode = RTS_AV_VIN_RING_MODE;
	vin = rts_av_create_vin_chn(&vin_attr);
	if (vin < 0) {
		RTS_ERR("fail to create vin chn, ret = %d\n", vin);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}
	RTS_INFO("vin chn : %d\n", vin);

	profile.fmt = RTS_V_FMT_YUV420SEMIPLANAR;
	profile.video.width = 1280;
	profile.video.height = 720;
	profile.video.numerator = 1;
	profile.video.denominator = 15;
	ret = rts_av_set_profile(vin, &profile);
	if (ret) {
		RTS_ERR("set vin profile fail, ret = %d\n", ret);
		goto exit;
	}

	h265_attr.level = H265_LEVEL_5;
	h265_attr.tier = 0;
	h265_attr.rotation = RTS_AV_ROTATION_0;
	enc = rts_av_create_h265_chn(&h265_attr);
	if (enc < 0) {
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		RTS_ERR("create h265 chn fail, ret = %d\n", ret);
		goto exit;
	}
	RTS_INFO("h265 chn : %d\n", enc);

	ret = rts_av_bind(vin, enc);
	if (ret) {
		RTS_ERR("fail to bind vin and encode, ret %d\n", ret);
		goto exit;
	}

	rts_av_enable_chn(vin);
	rts_av_enable_chn(enc);
	rts_av_start_recv(enc);

	pthread_create(&tid, NULL, enable_md2, NULL);

	while (!g_exit) {
		struct rts_av_buffer *buffer = NULL;

		if (rts_av_recv_block(enc, &buffer, 100))
			continue;

		if (buffer) {
			number++;
			rts_av_put_buffer(buffer);
		}

		if (rts_num > 0 && number >= rts_num)
			break;
	}

	g_exit = 1;
	pthread_join(tid, NULL);
	if (rts_ret)
		ret = rts_ret;

	rts_av_stop_recv(enc);
	rts_av_disable_chn(vin);
	rts_av_disable_chn(enc);
	rts_av_unbind(vin, enc);

	RTS_INFO("\n");
	RTS_INFO("get %d frames\n", number);
exit:
	if (vin >= 0) {
		rts_av_destroy_chn(vin);
		vin = -1;
	}
	if (enc >= 0) {
		rts_av_destroy_chn(enc);
		enc = -1;
	}

	return ret;
}

int main(int argc, char *argv[])
{
	int ret;

	ret = __parse_args(argc, argv);
	if (ret)
		return ret;

	rts_set_log_mask(RTS_LOG_MASK_CONS);

	signal(SIGINT, sighandle);
	signal(SIGTERM, sighandle);

	ret = rts_av_init();
	if (ret) {
		RTS_ERR("rts_av_init fail\n");
		return ret;
	}

	ret = test_stream();

	rts_av_release();

	if (ret)
		printf("Fail\n");
	else
		printf("Success\n");

	return ret;
}

struct bitmap_header {
	struct file_header {
		char filetype[2];
		uint32_t filesize;
		uint16_t reserved1;
		uint16_t reserved2;
		uint32_t data_offset;
	} __attribute__((packed)) fh;

	struct bmp_header {
		int32_t bmpheader_size;
		int32_t width;
		int32_t height;
		int16_t planes;
		int16_t bitcount_perpix;
		int32_t compression;
		int32_t sizeimage;
		int32_t xpixpermeter;
		int32_t ypixpermeter;
		int32_t clrused;
		int32_t clrimportant;
	} __attribute__((packed)) bh;
} __attribute__((packed));

struct rgb_pattern {
	unsigned char rgb[4];
};

static const struct rgb_pattern const pt_bw[] = {
	{ {0, 0, 0, 0} },
	{ {255, 255, 255, 0} },
};

static const unsigned char const reverse_table[] = {
	[0x0] = 0x0,
	[0x1] = 0x8,
	[0x2] = 0x4,
	[0x3] = 0xc,
	[0x4] = 0x2,
	[0x5] = 0xa,
	[0x6] = 0x6,
	[0x7] = 0xe,
	[0x8] = 0x1,
	[0x9] = 0x9,
	[0xa] = 0x5,
	[0xb] = 0xd,
	[0xc] = 0x3,
	[0xd] = 0xb,
	[0xe] = 0x7,
	[0xf] = 0xf,
};

static uint8_t reverse_byte(uint8_t b)
{
	return ((reverse_table[b & 0xf] << 4) | reverse_table[(b >> 4)]);
}

/*
 *- if left-up is 0,0, h must be <0
 *- if left-bottom is 0,0, h must be >0
 *- now can only save 2-value pic, every pixel is a bit
 *- in a byte, higher bit means left bit, lower bit means right bit
 */
static void save_bmp(int w, int h, int isbitmap, void *buf, FILE *fstream)
{
	struct bitmap_header bhr;
	int abs_h = h < 0 ? -h : h;
	int row_size = (w * 1 + 31) / 32 * 4;
	int flag_quick = !(w % 8);

	memset(&bhr, 0, sizeof(bhr));
	bhr.fh.filetype[0] = 'B';
	bhr.fh.filetype[1] = 'M';
	bhr.fh.filesize = row_size * abs_h + sizeof(bhr) + sizeof(pt_bw[0]) * 2;
	bhr.fh.data_offset = sizeof(bhr) + sizeof(pt_bw[0]) * 2;
	bhr.bh.bmpheader_size = sizeof(bhr.bh);
	bhr.bh.width = w;
	bhr.bh.height = h;
	bhr.bh.planes = 1;
	bhr.bh.bitcount_perpix = 1;

	fwrite(&bhr, sizeof(bhr), 1, fstream);
	fwrite(pt_bw, sizeof(pt_bw[0]), 2, fstream);

	unsigned char *src = buf;
	unsigned char *row = malloc(row_size);

	if (!row)
		return;

	if (!isbitmap) {
		for (int i = 0; i < abs_h; i++) {
			int s = i * w;

			memset(row, 0, row_size);
			for (int j = 0; j < w; j++)
				if (src[s + j])
					row[j / 8] |= (1 << (7 - (j % 8)));
			fwrite(row, 1, row_size, fstream);
		}
		goto out;

	}

	for (int i = 0; i < abs_h; i++) {
		memset(row, 0, row_size);
		if (!flag_quick) {
			for (int j = 0; j < w; j++) {
				int index = i * w + j;
				int b = index / 8;
				int off = index % 8;
				int bit = (src[b] >> off) & 0x1;

				if (bit)
					row[j / 8] |= (1 << (7 - (j % 8)));
			}
		} else {
			for (int j = 0; j < w / 8; j++) {
				int byte_index = j + i * w / 8;

				row[j] = reverse_byte(src[byte_index]);
			}
		}
		fwrite(row, 1, row_size, fstream);
	}

out:
	if (row)
		free(row);
}

static void __save_result_bmp(int w, int h, int ind, void *buf)
{
	char filename[64];
	FILE *f;

	sprintf(filename, "%s/%d.bmp", g_save_path, ind);
	f = fopen(filename, "wb+");
	save_bmp(w, -h, 0, buf, f);
	fclose(f);
}
