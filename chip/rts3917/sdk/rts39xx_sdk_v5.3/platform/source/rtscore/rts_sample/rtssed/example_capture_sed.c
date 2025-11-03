#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsaudio.h>

#include <rts_nn_log.h>
#include <rts_nn.h>
#include <rts_nn_types.h>

#include "rts_nn_sed.h"

#define SAMPLERATE		16000
#define CHANNELS		1
#define FORMAT			16
#define PERIOD_FRAMES_MS	50
#define CNT_MAX			20
#define CHECK_SAMPLENUMS	16000
#define CHECK_BYTES		(CHECK_SAMPLENUMS * CHANNELS * FORMAT / 8)

struct option longopts[] = {
	{"model", required_argument, NULL, 'm'},
	{0, 0, 0, 0}
};

struct __sed_sample_ctx {
	int capture_ch;
	struct rts_audio_attr attr;
	rts_nn_handle sed_handle;
	unsigned char *data;
	int buffer_index;
};

static int g_exit;
static void Termination(int sign)
{
	g_exit = 1;
}

static void print_help_info(char *filename)
{
	fprintf(stdout, "DESCRIPTION:\n");
	fprintf(stdout, "\taudio capture + sed\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "EXAMPLE:\n");
	fprintf(stdout, "\t%s -m sed_16k.model\n", filename);
}

static void sed_sample_stop(struct __sed_sample_ctx *ctx)
{
	rts_av_stop_recv(ctx->capture_ch);
	rts_av_disable_chn(ctx->capture_ch);
	rts_av_destroy_chn(ctx->capture_ch);

	if (ctx->sed_handle) {
		rts_nn_release(&ctx->sed_handle);
		ctx->sed_handle = NULL;
	}
	if (ctx->data) {
		free(ctx->data);
		ctx->data = NULL;
	}
	ctx->buffer_index = 0;
}

static int sed_sample_start(struct __sed_sample_ctx *ctx, char *model_path)
{
	struct rts_audio_capture_vqe c_vqe = {0};
	struct rts_nn_cfg sed_cfg = {0};
	int ret = 0;

	snprintf(ctx->attr.dev_node, sizeof(ctx->attr.dev_node), "hw:0,1");
	ctx->attr.rate = SAMPLERATE;
	ctx->attr.format = FORMAT;
	ctx->attr.channels = CHANNELS;
	ctx->attr.period_frames = PERIOD_FRAMES_MS * SAMPLERATE / 1000;
	ctx->capture_ch = rts_av_create_audio_capture_chn(&ctx->attr);
	if (ctx->capture_ch < 0) {
		ret = ctx->capture_ch;
		goto exit;
	}

	rts_av_get_audio_capture_vqe(ctx->capture_ch, &c_vqe);
	c_vqe.aecns_enable = 1;
	c_vqe.aecns_attr.aec_enable = 0;
	c_vqe.aecns_attr.ns_enable = 1;
	rts_av_set_audio_capture_vqe(ctx->capture_ch, &c_vqe);
	rts_av_enable_chn(ctx->capture_ch);
	rts_av_start_recv(ctx->capture_ch);

	strcpy(sed_cfg.model_name, "sed_svm");
	strcpy(sed_cfg.model_path, model_path);
	ret = rts_nn_init(&ctx->sed_handle, &sed_cfg);
	if (ret < 0) {
		printf("init sed_handle failed!\n");
		goto exit;
	}

	ctx->data = (unsigned char *)calloc(1, CHECK_BYTES);
	if (!ctx->data) {
		ret = -12;
		printf("calloc data failed!\n");
		goto exit;
	}

	ctx->buffer_index = 0;

	return 0;

exit:
	rts_av_destroy_chn(ctx->capture_ch);
	sed_sample_stop(ctx);

	return ret;
}

static int sed_sample_run(struct __sed_sample_ctx *ctx,
			struct rts_av_buffer *input,
			float *res)
{
	unsigned char *src = NULL, *dst = NULL;
	struct rts_nn_audio audios_in = {0};
	struct rts_nn_sed_res sed_res = {0};

	src = (unsigned char *)(input->vm_addr);
	dst = &(ctx->data[ctx->buffer_index * input->bytesused]);
	memcpy(dst, src, input->bytesused);
	++ctx->buffer_index;

	if (ctx->buffer_index == CNT_MAX) {
		audios_in.attr.fmt = RTS_NN_PCM_S16LE_MONO;
		audios_in.attr.samplerate = SAMPLERATE;
		audios_in.attr.sample_cnt = CHECK_SAMPLENUMS;
		audios_in.quantized = 0;
		audios_in.virt[0] = (void *)(ctx->data);

		rts_nn_sed_run(ctx->sed_handle,
				&audios_in,
				&sed_res);

		*res = sed_res.result[0];

		ctx->buffer_index = 0;
		memset(ctx->data, 0, CHECK_BYTES);

		return 0;
	} else {
		return -19;
	}
}

static int sed_sample_test(struct __sed_sample_ctx *ctx)
{
	struct rts_av_buffer *buffer = NULL;
	int ret = -1;
	float result = 0;

	ret = rts_av_recv_block(ctx->capture_ch, &buffer, 100);
	if (ret < 0)
		return ret;

	if (buffer) {
		ret = sed_sample_run(ctx, buffer, &result);
		if (ret == 0)
			printf("sed_sample_run: %.4f\n", result);
	}
	rts_av_put_buffer(buffer);

	return 0;
}

int main(int argc, char *argv[])
{
	int c, ret = -1;
	struct __sed_sample_ctx ctx = {0};
	char *model_path = NULL;

	signal(SIGINT, Termination);
	signal(SIGTERM, Termination);
	// rts_set_log_mask(RTS_LOG_MASK_CONS);

	while ((c = getopt_long(argc, argv, "hm:", longopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			print_help_info(argv[0]);
			return 0;
		case 'm':
			model_path = optarg;
			break;
		default:
			printf("wrong command arguments\n");
			goto exit;
		}
	}

	ret = rts_av_init();
	if (ret < 0) {
		printf("fail to rts_av_init\n");
		return ret;
	}

	ret = sed_sample_start(&ctx, model_path);
	if (ret < 0) {
		printf("fail to sed_sample_start, ret = %d\n", ret);
		goto exit;
	}

	while (!g_exit) {
		sed_sample_test(&ctx);
		usleep(1000);
	}

exit:
	sed_sample_stop(&ctx);
	rts_av_release();

	return 0;
}
