#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <rts_nn.h>
#include <rts_nn_types.h>
#include <rts_nn_log.h>
#include <rts_nn_utils.h>

#include <rtsavapi.h>
#include <rtscamkit.h>
#include <rtsvideo.h>

static char *model_name;
static char *model_path;
static int log_level = 0;

static int vin_id = 10;
static int vin_chn = -1;
static int vin_buf_num = 2;
//static int frame_w = 192;
//static int frame_h = 192;
static int frame_w = 256;
static int frame_h = 160;
static int fps = 3;
static int g_exit;

static void print_usage(char *name)
{
	printf("Usage %s:\n", name);
	printf("%s model_name model_path [frame_w] [frame_h] [fps] [log_level]\n",
			name);

	printf("\nExample:\n");
	printf("%s od_tiny ./od_tiny.mnn\n", name);
	printf("%s odlite ./rts_nn_odlite.data 416 416\n", name);
}

static int parse_args(int argc, char *argv[])
{
	int ret = 0;

	if (argc < 3 || argc > 7)
		return -1;

	model_name = argv[1];
	model_path = argv[2];

	if (argc >= 4)
		frame_w = atof(argv[3]);
	if (argc >= 5)
		frame_h = atoi(argv[4]);
	if (argc >= 6)
		fps = atoi(argv[5]);
	if (argc >= 7)
		log_level = atoi(argv[6]);

	return ret;
}

static inline float diff_timeval(struct timeval start, struct timeval end)
{
	return (end.tv_sec - start.tv_sec) * 1000.0
		+ (end.tv_usec - start.tv_usec) / 1000.0;
}

static void Termination(int sign)
{
	g_exit = 1;
}

static int __start_stream(void)
{
	struct rts_vin_attr attr = {0};
	struct rts_av_profile pro;
	int ret = 0;

	ret = rts_av_init();
	if (ret)
		goto err;

	attr.vin_buf_num = vin_buf_num;
	attr.vin_id = vin_id;
	vin_chn = rts_av_create_vin_chn(&attr);
	if (vin_chn < 0) {
		ret = -1;
		goto err;
	}

	pro.fmt = RTS_V_FMT_RGB;
	pro.video.width = frame_w;
	pro.video.height = frame_h;
	pro.video.numerator = 1;
	pro.video.denominator = fps;

	ret = rts_av_set_profile(vin_chn, &pro);
	if (ret < 0)
		goto err;

	ret = rts_av_enable_chn(vin_chn);
	if (ret)
		goto err;

	ret = rts_av_start_recv(vin_chn);
	if (ret)
		goto err;

	return 0;

err:
	if (ret)
		printf("%s\n", rts_strerrno(ret));

	return ret;
}

int main(int argc, char *argv[])
{
	int ret = 0;
	rts_nn_handle handle = NULL;
	struct rts_nn_cfg cfg = {0};
	struct rts_nn_image img = {0};
	struct rts_nn_classes cls;
	struct rts_nn_od_res *res = NULL;
	int count = 0;

	signal(SIGINT, Termination);
	signal(SIGTERM, Termination);

	ret = parse_args(argc, argv);
	if (ret) {
		print_usage(argv[0]);
		goto exit;
	}

	ret = __start_stream();
	if (ret) {
		printf("create video stream failed!\n");
		goto exit;
	}

	if (log_level) {
		rts_nn_set_log_level(log_level);
		rts_nn_set_log_mask(RTS_NN_LOG_MASK_CONS);
	}

	/* 1. init network */
	strcpy(cfg.model_name, model_name);
	strcpy(cfg.model_path, model_path);
	ret = rts_nn_init(&handle, &cfg);
	if (ret) {
		printf("init nn failed: %d\n", ret);
		goto exit;
	}

	ret = rts_nn_get_classes(handle, &cls);
	if (ret < 0) {
		printf("get nn classes error\n");
		goto exit;
	}

	/* 2. setting input image */
	img.attr.fmt = RTS_NN_RGB_PLANAR;
	img.attr.w = frame_w;
	img.attr.h = frame_h;
	img.attr.c = 3;

	while (!g_exit) {
		struct rts_av_buffer *buffer;

		usleep(1000);

		ret = rts_av_poll(vin_chn);
		if (ret)
			continue;

		ret = rts_av_recv(vin_chn, &buffer);
		if (ret)
			continue;

		if (buffer) {

			printf("\nframe %d\n", count++);
			img.virt[0] = buffer->vm_addr;
			img.phy[0] = buffer->phy_addr;

			/* 3. run nn */
			ret = rts_nn_od_run(handle, &img, &res);
			if (ret || res == NULL) {
				printf("run network failed, ret: %d\n", ret);
				rts_av_put_buffer(buffer);
				buffer = NULL;
				continue;
			}

			/* 4. print and save result */
			for (int i = 0; i < res->num; ++i) {
				printf("od %s, prob: %.2f, (%d, %d) (%d, %d)\n",
						cls.names[res->bboxes[i].id],
						res->bboxes[i].score,
						res->bboxes[i].x1,
						res->bboxes[i].y1,
						res->bboxes[i].x2,
						res->bboxes[i].y2);
			}
		}

		rts_av_put_buffer(buffer);
		buffer = NULL;
	}

exit:
	rts_nn_release(&handle);

	if (vin_chn >= 0)
		rts_av_destroy_chn(vin_chn);
	rts_av_release();
	return 0;
}
