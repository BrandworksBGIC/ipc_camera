/*
 *  Copyright (C) 2019 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include <pthread.h>
#include <rtsavisp.h>
#include <errno.h>
#include <string.h>
#include <json-c/json.h>

#include "rf_msg.h"
#include "video.h"
#include "ringbuffer.h"
#include "mjpeg.h"
#include "common.h"
#include "h26x_parse.h"

enum stream_status {
	STREAM_STATUS_STOP,
	STREAM_STATUS_PLAYING,
	STREAM_STATUS_PAUSE,
};

enum rf_av_rotation {
	RF_AV_ROTATION_0 = 0,
	RF_AV_ROTATION_90R = 1,
	RF_AV_ROTATION_90L = 2,
	RF_AV_ROTATION_180 = 3,
};

enum rf_h264_profile {
	RF_H264_PROFILE_UNKNOWN,
	RF_H264_PROFILE_BASE,
	RF_H264_PROFILE_MAIN,
	RF_H264_PROFILE_HIGH
};

enum rf_h264_level {
	RF_H264_LEVEL_UNKNOWN = 0,
	RF_H264_LEVEL_1,
	RF_H264_LEVEL_1_b,
	RF_H264_LEVEL_1_1,
	RF_H264_LEVEL_1_2,
	RF_H264_LEVEL_1_3,
	RF_H264_LEVEL_2,
	RF_H264_LEVEL_2_1,
	RF_H264_LEVEL_2_2,
	RF_H264_LEVEL_3,
	RF_H264_LEVEL_3_1,
	RF_H264_LEVEL_3_2,
	RF_H264_LEVEL_4,
	RF_H264_LEVEL_4_1,
	RF_H264_LEVEL_4_2,
	RF_H264_LEVEL_5,
	RF_H264_LEVEL_5_1,
	RF_H264_LEVEL_RESERVED
};

enum rf_h265_level {
	RF_H265_LEVEL_0 =    0,    //firmware determines a level
	RF_H265_LEVEL_1 =   10,
	RF_H265_LEVEL_2 =   20,
	RF_H265_LEVEL_2_1 = 21,
	RF_H265_LEVEL_3 =   30,
	RF_H265_LEVEL_3_1 = 31,
	RF_H265_LEVEL_4 =   40,
	RF_H265_LEVEL_4_1 = 41,
	RF_H265_LEVEL_5 =   50,
	RF_H265_LEVEL_5_1 = 51,
	RF_H265_LEVEL_RESERVED
};

enum rf_av_mirror {
	RF_AV_MIRROR_NO = 0,
	RF_AV_MIRROR_VER,
	RF_AV_MIRROR_HOR,
	RF_AV_MIRROR_HOR_VER,
};

enum rf_bitrate_mode {
	RF_BITRATE_MODE_CBR		= (1 << 1),
	RF_BITRATE_MODE_VBR		= (1 << 2),
	RF_BITRATE_MODE_C_VBR		= (1 << 3),
	RF_BITRATE_MODE_S_VBR		= (1 << 4)
};

enum rf_gop_mode {
	RF_GOP_MODE_NORMAL		= (1 << 1),
	RF_GOP_MODE_SP			= (1 << 2)
};

struct video_context *g_video_ctx[MAX_STREAM_NUM];

struct video_config g_config[MAX_STREAM_NUM] = {
	{
		.codec_type = 0,
		.isp_config = {
			.isp_id = 0,
			.isp_buf_num = 2,
			.vin_mode = RTS_AV_VIN_RING_MODE,
			.width = 1280,
			.height = 720,
			.framerate = 15,
		},
		.h264_config = {
			.level = H264_LEVEL_4,
			.qp = 26,
			.bitrate = 2097152,        // 2 * 1024 * 1024
			.gop = 15,
			.rotation = RF_AV_ROTATION_0,
		},
		.h265_config = {
			.level = H265_LEVEL_4,
			.tier = MAIN_TIER,
			.bitrate = 2097152,       // 2 * 1024 * 1024
			.gop = 15,
			.rotation = RF_AV_ROTATION_0,
			.mirror = RF_AV_MIRROR_NO,
		}
	},
	{
		.codec_type = 0,
		.isp_config = {
			.isp_id = 1,
			.isp_buf_num = 2,
			.vin_mode = RTS_AV_VIN_FRAME_MODE,
			.width = 640,
			.height = 360,
			.framerate = 15,
		},
		.h264_config = {
			.level = H264_LEVEL_4,
			.qp = 26,
			.bitrate = 524288,        //512 * 1024
			.gop = 15,
			.rotation = RF_AV_ROTATION_0,
		},
		.h265_config = {
			.level = H265_LEVEL_4,
			.tier = MAIN_TIER,
			.bitrate = 524288,           //512 * 1024
			.gop = 15,
			.rotation = RF_AV_ROTATION_0,
			.mirror = RF_AV_MIRROR_NO,
		}
	}
};

int check_cfg(struct rts_vin_attr *isp_attr)
{
	if (isp_attr->vin_id != 0 &&
			isp_attr->vin_mode == RTS_AV_VIN_RING_MODE) {
		RTS_ERR("vin ring mode only work in vin_id 0\n");
		return RTS_RETURN(RTS_E_INVALID_ARG);
	}
	if (isp_attr->vin_id > 1 &&
			isp_attr->vin_mode == RTS_AV_VIN_DIRECT_MODE) {
		RTS_ERR("vin direct mode only work in vin_id 0/1\n");
		return RTS_RETURN(RTS_E_INVALID_ARG);
	}
	if (isp_attr->vin_id < 0 && isp_attr->vin_mode > 2) {
		RTS_ERR("vin mode range [0~2]\n");
		return RTS_RETURN(RTS_E_INVALID_ARG);
	}

	return RTS_OK;
}

static int update_video_config_from_json(
	int index, struct video_config *conf, int vmode_flag)
{
	int ret, i = 0;
	int val[8] = {};
	char *path = RAINBOW_CONFIG;
	struct json_object *root_obj = NULL;
	struct json_object *profiles_obj = NULL;
	struct json_object *isp_obj = NULL;
	struct json_object *h264_obj = NULL;
	struct json_object *h265_obj = NULL;
	struct json_object *codec_type_obj = NULL;

	root_obj = json_object_from_file(path);
	if (!root_obj)
		ret = -1;

	if (index == 0) {
		ret = json_object_object_get_ex(
				root_obj, "profile1", &profiles_obj);
	}
	if (index == 1) {
		ret = json_object_object_get_ex(
				root_obj, "profile2", &profiles_obj);
	}

	ret = json_object_object_get_ex(
			profiles_obj, "codec_type", &codec_type_obj);
	if (ret) {
		conf->codec_type = json_object_get_int(codec_type_obj);//0:H264, 1:H265
	}

	ret = json_object_object_get_ex(profiles_obj, "isp_config", &isp_obj);
	if (ret) {
		json_object_object_foreach(isp_obj, isp_key, isp_val) {
		val[i] = json_object_get_int(isp_val);
		i = i + 1;
		}
		conf->isp_config.isp_id = val[0];
		conf->isp_config.isp_buf_num = val[1];
		conf->isp_config.vin_mode = val[2];
		conf->isp_config.width = val[3];
		conf->isp_config.height = val[4];
		conf->isp_config.framerate = val[5];
		i = 0;
	}
	if (vmode_flag == 1)
		return conf->isp_config.vin_mode;

	ret = json_object_object_get_ex(profiles_obj, "h264_config", &h264_obj);
	if (ret) {
		json_object_object_foreach(h264_obj, h264_key, h264_val) {
		val[i] = json_object_get_int(h264_val);
		i = i + 1;
		}
		conf->h264_config.level = val[0];
		conf->h264_config.qp = val[1];
		conf->h264_config.bitrate = val[2];
		conf->h264_config.gop = val[3];
		conf->h264_config.rotation = val[4];
		i = 0;
	}

	ret = json_object_object_get_ex(profiles_obj, "h265_config", &h265_obj);
	if (ret) {
		json_object_object_foreach(h265_obj, h265_key, h265_val) {
		val[i] = json_object_get_int(h265_val);
		i = i + 1;
		}
		conf->h265_config.level = val[0];
		conf->h265_config.tier = val[1];
		conf->h265_config.bitrate = val[2];
		conf->h265_config.gop = val[3];
		conf->h265_config.rotation = val[4];
		conf->h265_config.mirror = val[5];
		i = 0;
	}

	if (root_obj)
		json_object_put(root_obj);

	return ret;
}

static int update_encoder_attr_to_json(int index, struct video_config *conf)
{
	int ret, i = 0;
	char *path = RAINBOW_CONFIG;
	struct json_object *root_obj = NULL;
	struct json_object *profiles_obj = NULL;
	struct json_object *encoder_obj = NULL;

	root_obj = json_object_from_file(path);
	if (!root_obj)
		ret = -1;

	if (index == 0) {
		ret = json_object_object_get_ex(
				root_obj, "profile1", &profiles_obj);
	}
	if (index == 1) {
		ret = json_object_object_get_ex(
				root_obj, "profile2", &profiles_obj);
	}

	json_object_object_foreach(profiles_obj, key, val)
	{
		if (strcmp(key, "codec_type") == 0)
			json_object_object_add(profiles_obj, key, json_object_new_int(conf->codec_type));
	}

	if(conf->codec_type == 0)
	{
		ret = json_object_object_get_ex(profiles_obj, "h264_config", &encoder_obj);
		if (ret) {
			json_object_object_foreach(encoder_obj, key, val) {
				if (strcmp(key, "h264_bitrate") == 0) {
					json_object_object_add(encoder_obj, key, json_object_new_int(conf->h264_config.bitrate));
				} else if (strcmp(key, "h264_gop") == 0) {
					json_object_object_add(encoder_obj, key, json_object_new_int(conf->h264_config.gop));
				}
			}
		}
	}
	else
	{
		ret = json_object_object_get_ex(profiles_obj, "h265_config", &encoder_obj);
		if (ret) {
			json_object_object_foreach(encoder_obj, key, val) {
				if (strcmp(key, "h265_bitrate") == 0) {
					json_object_object_add(encoder_obj, key, json_object_new_int(conf->h265_config.bitrate));
				} else if (strcmp(key, "h265_gop") == 0) {
					json_object_object_add(encoder_obj, key, json_object_new_int(conf->h265_config.gop));
				}
			}
		}
	}


	json_object_to_file(RAINBOW_CONFIG, root_obj);

	if (root_obj)
		json_object_put(root_obj);
}

static int update_isp_attr_to_json(int index, struct video_config *conf)
{
	int ret;
	char *path = RAINBOW_CONFIG;
	struct json_object *root_obj = NULL;
	struct json_object *profiles_obj = NULL;
	struct json_object *isp_obj = NULL;

	root_obj = json_object_from_file(path);
	if (!root_obj)
		ret = -1;

	if (index == 0) {
		ret = json_object_object_get_ex(
				root_obj, "profile1", &profiles_obj);
	}
	if (index == 1) {
		ret = json_object_object_get_ex(
				root_obj, "profile2", &profiles_obj);
	}

	ret = json_object_object_get_ex(profiles_obj, "isp_config", &isp_obj);
	if (ret) {
		json_object_object_foreach(isp_obj, isp_key, isp_val) {
			if (strcmp(isp_key, "width") == 0) {
				json_object_object_add(isp_obj, isp_key, json_object_new_int(conf->isp_config.width));
			} else if (strcmp(isp_key, "height") == 0) {
				json_object_object_add(isp_obj, isp_key, json_object_new_int(conf->isp_config.height));
			} else if (strcmp(isp_key, "framerate") == 0) {
				json_object_object_add(isp_obj, isp_key, json_object_new_int(conf->isp_config.framerate));
			}
		}
	}

	json_object_to_file(RAINBOW_CONFIG, root_obj);

	if (root_obj)
		json_object_put(root_obj);

	return ret;
}

static int __init_sys_vmem(int vin_mode_0, int vin_mode_1)
{
	int ret = 0;
	struct rts_sys_vmem_cfg cfg = {0};
	int share = 0;
	int status = 0;

	status = rts_av_sys_vmem_status();

	if (status == RTS_SYS_VMEM_STATUS_ON)
		goto out;

	/* 1-channel */
	cfg.stream[0].enable = 1;
	cfg.stream[0].fmt = RTS_V_FMT_YUV420SEMIPLANAR;
	cfg.stream[0].width = 0;
	cfg.stream[0].height = 0;

	/* vin */
	cfg.stream[0].module[0].type = RTS_AV_ID_VIN;
	cfg.stream[0].module[0].cnt = 1;
	cfg.stream[0].module[0].mode = vin_mode_0;
	if (vin_mode_0 == 0) {
		cfg.stream[0].module[0].outbuf.num = 2;
		cfg.stream[0].module[0].outbuf.setted = 1;
	}

	/* h26x */
	cfg.stream[0].module[1].type = RTS_AV_ID_H264;
	cfg.stream[0].module[1].cnt = 1;
	cfg.stream[0].module[1].outbuf.setted = 1;
	cfg.stream[0].module[1].outbuf.shared = share;
	cfg.stream[0].module[1].outbuf.num = 1;
	cfg.stream[0].module[1].outbuf.size = 0; // default size

	/* 2-channel */
	cfg.stream[1].enable = 1;
	cfg.stream[1].fmt = RTS_V_FMT_YUV420SEMIPLANAR;
	cfg.stream[1].width = 0;
	cfg.stream[1].height = 0;

	/* vin */
	cfg.stream[1].module[0].type = RTS_AV_ID_VIN;
	cfg.stream[1].module[0].cnt = 1;
	cfg.stream[1].module[0].mode = vin_mode_1;
	if (vin_mode_1 == 0) {
		cfg.stream[1].module[0].outbuf.num = 2;
		cfg.stream[1].module[0].outbuf.setted = 1;
	}

	/* h26x */
	cfg.stream[1].module[1].type = RTS_AV_ID_H264;
	cfg.stream[1].module[1].cnt = 1;
	cfg.stream[1].module[1].outbuf.setted = 1;
	cfg.stream[1].module[1].outbuf.shared = 0;
	cfg.stream[1].module[1].outbuf.num = 1;
	cfg.stream[1].module[1].outbuf.size = 0; // default size

	/* mjpeg */
	cfg.stream[1].module[2].type = RTS_AV_ID_MJPGENC;
	cfg.stream[1].module[2].cnt = 1;
	cfg.stream[1].module[2].outbuf.setted = 1;
	cfg.stream[1].module[2].outbuf.shared = share;
	cfg.stream[1].module[2].outbuf.num = 1;
	cfg.stream[1].module[2].outbuf.size = 0; // default size

	ret = rts_av_sys_vmem_set_conf(&cfg);
	if (ret) {
		RTS_ERR("failed to set sysmem cfg, ret:%d\n", ret);
		return ret;
	}

	ret = rts_av_sys_vmem_init();
	if (ret) {
		RTS_ERR("failed to init sysmem cfg, ret:%d\n", ret);
		return ret;
	}

out:
	return ret;
}

static void __release_sys_vmem(void)
{
	int status = 0;

	status = rts_av_sys_vmem_status();

	if (status == RTS_SYS_VMEM_STATUS_OFF)
		return;

	rts_av_sys_vmem_release();
}

static void stop_capture_thread(
	struct video_context *context)
{
	if (context->tid) {
		context->config.cur_status = STREAM_STATUS_STOP;
		pthread_join(context->tid, NULL);
	}
	context->tid = 0;
}

static void stop_stream(
	struct video_context *context)
{
	if (context->h264_ch >= 0) {
		rts_av_stop_recv(context->h264_ch);
		rts_av_disable_chn(context->h264_ch);
	}
	if (context->h265_ch >= 0) {
		rts_av_stop_recv(context->h265_ch);
		rts_av_disable_chn(context->h265_ch);
	}
	if (context->mjpeg_ch >= 0) {
		rts_av_disable_chn(context->mjpeg_ch);
	}
	if (context->isp_ch >= 0)
		rts_av_disable_chn(context->isp_ch);
}

static int start_stream(
	struct video_context *context)
{
	int ret;

	if (context->isp_ch >= 0) {
		ret = rts_av_enable_chn(context->isp_ch);
		if (ret) {
			RTS_ERR("enable isp ch error\n");
			return RF_ERR_CHN_ENABLE;
		}
	}
	if (context->h264_ch >= 0) {
		rts_av_start_recv(context->h264_ch);
		ret = rts_av_enable_chn(context->h264_ch);
		if (ret) {
			RTS_ERR("enable h264 ch error\n");
			return RF_ERR_CHN_ENABLE;
		}
	}
	if (context->h265_ch >= 0) {
		rts_av_start_recv(context->h265_ch);
		ret = rts_av_enable_chn(context->h265_ch);
		if (ret) {
			RTS_ERR("enable h265 ch error\n");
			return RF_ERR_CHN_ENABLE;
		}
	}

	return RF_ERR_OK;
}

static void delete_stream(
	struct video_context *context)
{
	if (context->h264_ch >= 0) {
		rts_av_destroy_chn(context->h264_ch);
		context->h264_ch = -1;
	}
	if (context->h265_ch >= 0) {
		rts_av_destroy_chn(context->h265_ch);
		context->h265_ch = -1;
	}
	if (context->mjpeg_ch >= 0) {
		rts_av_destroy_chn(context->mjpeg_ch);
		context->mjpeg_ch = -1;
	}
	if (context->isp_ch >= 0) {
		rts_av_destroy_chn(context->isp_ch);
		context->isp_ch = -1;
	}
}

static int create_stream(
	int index,
	struct video_context *context)
{
	int res;
	int ret = RF_ERR_OK;
	struct video_config *config = &g_config[index];
	struct rts_vin_attr isp_attr = {0};
	struct rts_av_profile isp_profile = {0};
	struct rts_jpgenc_attr jpg_attr = {0};

	pthread_mutex_lock(&context->mutex);

	res = update_video_config_from_json(index, config, 0);
	if (res == -1)
		printf("update with default parameter\n");

	isp_attr.vin_id = config->isp_config.isp_id;
	context->isp_id = config->isp_config.isp_id;
	isp_attr.vin_buf_num = config->isp_config.isp_buf_num;
	isp_attr.vin_mode = config->isp_config.vin_mode;
	ret = check_cfg(&isp_attr);
	if (RTS_IS_ERR(ret)) {
		RTS_ERR("fail to check cfg, ret %d\n", ret);
		return ret;
	}
	context->isp_ch = rts_av_create_vin_chn(&isp_attr);
	if (context->isp_ch < 0) {
		RTS_ERR("creat isp channel error\n");
		ret = RF_ERR_CREAT_STREAM;
		goto failed;
	}

	isp_profile.fmt = RTS_V_FMT_YUV420SEMIPLANAR;
	isp_profile.video.width = config->isp_config.width;
	isp_profile.video.height = config->isp_config.height;
	isp_profile.video.numerator = 1;
	isp_profile.video.denominator = config->isp_config.framerate;
	ret = rts_av_set_profile(context->isp_ch, &isp_profile);
	if (ret) {
		RTS_ERR("set profile error\n");
		ret = RF_ERR_CREAT_STREAM;
		goto failed;
	}
	if (index == 1) {
		jpg_attr.rotation = RF_AV_ROTATION_0;
		context->mjpeg_ch = rts_av_create_mjpeg_chn(&jpg_attr);
		if (context->mjpeg_ch < 0) {
			RTS_ERR("creat mjpeg channel error\n");
			ret = RF_ERR_CREAT_STREAM;
			goto failed;
		}
		ret = rts_av_bind(context->isp_ch, context->mjpeg_ch);
		if (ret) {
			RTS_ERR("bind mjpeg to isp error\n");
			ret = RF_ERR_CREAT_STREAM;
			goto failed;
		}
	}

	if (g_config[index].codec_type == 0) {
		struct rts_h264_attr h264_attr;
		memset(&h264_attr, 0, sizeof(h264_attr));
		h264_attr.level = config->h264_config.level;
		h264_attr.rotation = config->h264_config.rotation;
		h264_attr.mirror = RTS_AV_MIRROR_NO;

		context->h264_ch = rts_av_create_h264_chn(&h264_attr);
		if (context->h264_ch < 0) {
			RTS_ERR("creat h264 channel error\n");
			ret = RF_ERR_CREAT_STREAM;
			goto failed;
		}

		ret = rts_av_bind(context->isp_ch,
			context->h264_ch);
		if (ret) {
			RTS_ERR("bind h264 to isp error\n");
			ret = RF_ERR_CREAT_STREAM;
			goto failed;
		}
		struct rts_h264_ctrl *h264_ctl = NULL;

		rts_av_query_h264_ctrl(context->h264_ch, &h264_ctl);
		rts_av_get_h264_ctrl(h264_ctl);

		h264_ctl->forced_idr_header_enable = 1;
		h264_ctl->qp = config->h264_config.qp;
		h264_ctl->bitrate = config->h264_config.bitrate;
		h264_ctl->gop = config->h264_config.gop;

		ret = rts_av_set_h264_ctrl(h264_ctl);
		if (ret) {
			rts_av_release_h264_ctrl(h264_ctl);
			RTS_ERR("Failed to set h264 ctrl, ret %d\n",
				ret);
			ret = RF_ERR_CREAT_STREAM;
			goto failed;
		}
		rts_av_release_h264_ctrl(h264_ctl);
	} else {
		struct rts_h265_attr h265_attr;
		memset(&h265_attr, 0, sizeof(h265_attr));
		h265_attr.level = config->h265_config.level;
		h265_attr.tier = config->h265_config.tier;
		h265_attr.rotation = config->h265_config.rotation;
		h265_attr.mirror = config->h265_config.mirror;

		context->h265_ch = rts_av_create_h265_chn(&h265_attr);
		if (context->h265_ch < 0) {
			RTS_ERR("creat h265 channel error\n");
			ret = RF_ERR_CREAT_STREAM;
			goto failed;
		}

		ret = rts_av_bind(context->isp_ch, context->h265_ch);
		if (ret) {
			RTS_ERR("bind h265 to isp error\n");
			ret = RF_ERR_CREAT_STREAM;
			goto failed;
		}

		struct rts_h265_ctrl *h265_ctl = NULL;

		rts_av_query_h265_ctrl(context->h265_ch, &h265_ctl);
		rts_av_get_h265_ctrl(h265_ctl);

		h265_ctl->forced_idr_header_enable = 1;
		h265_ctl->gop = config->h265_config.gop;
		h265_ctl->bitrate = config->h265_config.bitrate;
		ret = rts_av_set_h265_ctrl(h265_ctl);
		if (ret) {
			rts_av_release_h265_ctrl(h265_ctl);
			RTS_ERR("Failed to set h265 ctrl, ret %d\n",
				ret);
			ret = RF_ERR_CREAT_STREAM;
			goto failed;
		}
		rts_av_release_h265_ctrl(h265_ctl);
	}

	pthread_mutex_unlock(&context->mutex);

	return RF_ERR_OK;

failed:
	pthread_mutex_unlock(&context->mutex);

	delete_stream(context);

	return ret;
}

static void uninit_output(
	struct video_context *context)
{

}

static int init_output(
	struct video_context *context)
{
	int ret = RF_ERR_OK;


	return ret;
}

static int v_packet_set(packet_t *v_pack, struct rts_av_buffer *v_buff)
{
	static long long index;
	RTS_ASSERT(v_pack);
	RTS_ASSERT(v_buff);

	v_pack->vm_addr = v_buff->vm_addr;
	v_pack->length = v_buff->bytesused;
	v_pack->timestamp = v_buff->timestamp;
	v_pack->flags = v_buff->flags;
	v_pack->type = v_buff->type;
	v_pack->index = index++;

	return RF_ERR_OK;
}

static int put_buf_to_rb(struct rts_av_buffer *vbuffer,
		void *v_handle, packet_t *v_pack)
{
	int ret;
	struct rts_av_buffer *vbuf = vbuffer;
	void *handle = v_handle;
	packet_t *pack = v_pack;

	if (v_handle == NULL)
		return RF_ERR_RB_WRITE;

	ret = v_packet_set(pack, vbuf);
	ret = rts_ringbuffer_write_packet(handle, pack);
	if (ret != RTS_OK) {
		RTS_ERR("write ring buffer fail\n");
		return RF_ERR_RB_WRITE;
	}
	return RF_ERR_OK;
}

static int set_ringbuffer_fmt(struct video_context *context)
{
	int ret = RF_ERR_OK;
	int ch_id = context->isp_id;
	stream_format format;
	void *handle = context->ringbuf;

	if (g_config[ch_id].codec_type == 0) {
		format.fmt = RB_V_FMT_H264;
		format.video.width = g_config[ch_id].isp_config.width;
		format.video.height = g_config[ch_id].isp_config.height;
		format.video.numerator = 1;
		format.video.denominator = g_config[ch_id].isp_config.framerate;
		format.video.timebase_numerator = 1;
		format.video.timebase_denominator = 1000000;
		format.video.qp = g_config[ch_id].h264_config.qp;
		format.video.gop = g_config[ch_id].h264_config.gop;
		format.video.bitrate = g_config[ch_id].h264_config.bitrate;
	} else {
		format.fmt = RB_V_FMT_H265;
		format.video.width = g_config[ch_id].isp_config.width;
		format.video.height = g_config[ch_id].isp_config.height;
		format.video.numerator = 1;
		format.video.denominator = g_config[ch_id].isp_config.framerate;
		format.video.timebase_numerator = 1;
		format.video.timebase_denominator = 1000000;
		format.video.gop = g_config[ch_id].h265_config.gop;
		format.video.bitrate = g_config[ch_id].h265_config.bitrate;
	}

	context->config.rebuild_rb = 0;

	ret = rts_ringbuffer_set_stream_format(handle, &format);
	if (ret != RF_ERR_OK)
		return RF_ERR_RB_SET_FMT;

	return ret;
}

static int rf_check_config(struct video_context *context)
{
	int refresh_rb, rebuild_rb, cur_status, priv_status;
	int chn_num;
	int ch_id = context->isp_id;

	if(g_config[ch_id].codec_type == 0)
		chn_num = context->h264_ch;
	else
		chn_num = context->h265_ch;

	rebuild_rb = context->config.rebuild_rb;
	refresh_rb = context->config.refresh_rb;
	cur_status = context->config.cur_status;
	priv_status = context->config.priv_status;

	if (refresh_rb) {
		pthread_mutex_lock(&context->mutex);
		context->config.refresh_rb = 0;
		pthread_mutex_unlock(&context->mutex);
		set_ringbuffer_fmt(context);
	}

	if (cur_status != priv_status) {
		switch (cur_status) {
		case STREAM_STATUS_STOP:
			return 0;

		case STREAM_STATUS_PAUSE:
			rts_av_stop_recv(chn_num);
			context->config.priv_status
				= STREAM_STATUS_PAUSE;
			return 1;

		case STREAM_STATUS_PLAYING:
			rts_av_start_recv(chn_num);
			context->config.priv_status
				= STREAM_STATUS_PLAYING;
			return 1;

		default:
			return 0;
		}
	}
	return 1;
}

#define RINGBUFFER_VIDEO_FILENAME_FORMAT "/var/tmp/capture_video_profile%c.shm"

static void *capture_thread(void *args)
{
	int index = (int)args;
	int chn_num;
	char profile_index;
	struct video_context *context =
		g_video_ctx[index];
	struct rts_av_buffer *buffer = NULL;
	packet_t v_pack;
	int ret;

	profile_index = '0' + index + 1;
	snprintf(context->shm_video, sizeof(context->shm_video),
			RINGBUFFER_VIDEO_FILENAME_FORMAT, profile_index);

	context->ringbuf = rts_ringbuffer_init
		(context->shm_video,
		VIDEO_SHM_BUFF);
	if (!context->ringbuf) {
		RTS_ERR("video ring buffer init failed\n");
		goto err;
	}
	set_ringbuffer_fmt(context);

	if (g_config[index].codec_type == 0) {
		chn_num = context->h264_ch;
		save_sps_pps_info(index, chn_num);
	} else {
		chn_num = context->h265_ch;
		save_vps_sps_pps_info(index, chn_num);
	}

	write_h26x_info_to_ringbuf(MAX_STREAM_NUM);

	while (rf_check_config(context)) {
		if (rts_av_recv_block(chn_num, &buffer, 100))
			continue;
		if (buffer) {
			ret = put_buf_to_rb(buffer, context->ringbuf, &v_pack);

			rts_av_put_buffer(buffer);
			buffer = NULL;
		}
	}

err:
	rts_ringbuffer_release(context->ringbuf);
	buffer = NULL;
	return NULL;
}

static struct video_context *create_context(void)
{
	struct video_context *context = NULL;
	context = calloc(1, sizeof(struct video_context));
	pthread_mutex_init(&context->mutex, NULL);

	context->isp_ch = -1;
	context->h264_ch = -1;
	context->h265_ch = -1;
	context->mjpeg_ch = -1;

	return context;
}

static void release_context(struct video_context *context)
{
	if (context) {
		pthread_mutex_destroy(&context->mutex);
		free(context);
	}
}

static int get_isp_attr(struct video_context *context,
		struct rf_isp_attr *isp_attr)
{
	int ret;
	int ch_id = context->isp_id;
	struct rf_isp_attr *attr = isp_attr;
	struct rts_av_profile isp_profile;

	ret = rts_av_get_profile(context->isp_ch, &isp_profile);
	if (ret) {
		RTS_ERR("get profile error\n");
		ret = RF_ERR_GET_ISP_CTL;
		goto failed;
	}

	attr->width = isp_profile.video.width;
	attr->height = isp_profile.video.height;
	attr->framerate = isp_profile.video.denominator;

	g_config[ch_id].isp_config.width = isp_profile.video.width;
	g_config[ch_id].isp_config.height = isp_profile.video.height;
	g_config[ch_id].isp_config.framerate = isp_profile.video.denominator;

	return RF_ERR_OK;

failed:
	pthread_mutex_unlock(&context->mutex);

	if (context->isp_ch >= 0) {
		rts_av_destroy_chn(context->isp_ch);
		context->isp_ch = -1;
	}
	if (context->h264_ch >= 0) {
		rts_av_destroy_chn(context->h264_ch);
		context->h264_ch = -1;
	}
	if (context->h265_ch >= 0) {
		rts_av_destroy_chn(context->h265_ch);
		context->h264_ch = -1;
	}

	return ret;

}

static int set_isp_attr(struct video_context *context,
		struct rf_isp_attr *isp_attr)
{
	int ret;
	int ch_id = context->isp_id;
	struct rf_isp_attr *attr = isp_attr;
	struct rts_av_profile isp_profile;

	isp_profile.fmt = RTS_V_FMT_YUV420SEMIPLANAR;
	isp_profile.video.width = attr->width;
	isp_profile.video.height = attr->height;
	isp_profile.video.numerator = 1;
	isp_profile.video.denominator = attr->framerate;
	rts_av_disable_chn(context->isp_ch);
	ret = rts_av_set_profile(context->isp_ch, &isp_profile);
	if (ret) {
		RTS_ERR("set profile error\n");
		ret = RF_ERR_SET_ISP_CTL;
		goto failed;
	}
	ret = rts_av_enable_chn(context->isp_ch);
	g_config[ch_id].isp_config.width = attr->width;
	g_config[ch_id].isp_config.height = attr->height;
	g_config[ch_id].isp_config.framerate = attr->framerate;

	context->config.refresh_rb = 1;

	return RF_ERR_OK;

failed:
	if (context->isp_ch >= 0) {
		rts_av_destroy_chn(context->isp_ch);
		context->isp_ch = -1;
	}
	if (context->h264_ch >= 0) {
		rts_av_destroy_chn(context->h264_ch);
		context->h264_ch = -1;
	}
	if (context->h265_ch >= 0) {
		rts_av_destroy_chn(context->h265_ch);
		context->h265_ch = -1;
	}

	return ret;
}

static int get_encode_key_frame(struct video_context *context)
{
	int ret = RF_ERR_OK;
	int ch_id = context->isp_id;

	if(g_config[ch_id].codec_type == 0)
		ret = rts_av_request_h264_key_frame(context->h264_ch);
	else
		ret = rts_av_request_h265_key_frame(context->h265_ch);

	if (ret != RF_ERR_OK)
		return RF_ERR_GET_KEY_FRAME;

	return RF_ERR_OK;
}

static int snapshot_and_save(struct rf_snapshot *snapshot)
{
	int ret = RF_ERR_OK;
	struct rf_snapshot *rf_snapshot = snapshot;

	ret = rts_av_enable_chn(g_video_ctx[1]->mjpeg_ch);
	if (ret != RF_ERR_OK)
		goto exit;

	ret = rf_set_mjpeg_ctrl();
	if (ret != RF_ERR_OK)
		return ret;

	ret = rf_set_mjpeg_callback(0, rf_snapshot->number,
				rf_snapshot->interval);
	if (ret != RF_ERR_OK)
		return ret;

	return ret;

exit:
	rf_release_mjpeg_ctrl();
	rts_av_disable_chn(g_video_ctx[1]->mjpeg_ch);
	return RF_ERR_SNAPSHOT;
}

static int get_isp_ability(struct video_context *context,
		struct rf_isp_ability *isp_ability)
{
	int ret, len = 0;
	struct rf_isp_ability *rf_ability = isp_ability;
	struct rts_isp_ability *ability;
	struct rts_fract max_fps = {0};

	ret = rts_av_query_isp_ability(context->isp_ch, &ability);
	if (ret != RF_ERR_OK)
		return RF_ERR_QUERY_ISP;

	rf_ability->fmt_number = ability->fmt_number;

	for (int i = 0; i < MAX_FMT_NUM_STREAM; i++)
		rf_ability->formats[i] = *(ability->pformats++);

	rf_ability->resolution.min_width = ability->resolution.min_width;
	rf_ability->resolution.max_width = ability->resolution.max_width;
	rf_ability->resolution.step_width = ability->resolution.step_width;
	rf_ability->resolution.min_height = ability->resolution.min_height;
	rf_ability->resolution.max_height = ability->resolution.max_height;
	rf_ability->resolution.step_height = ability->resolution.step_height;

	if (ability->frmival_type == RTS_V4L2_FRMSIZE_TYPE_DISCRETE) {
		rf_ability->frmival_num = ability->frmival_num;
		len = rf_ability->frmival_num * sizeof(struct rts_fract);
		memcpy(rf_ability->frmivals, ability->pfrmivals, len);
	} else if (ability->frmival_type == RTS_V4L2_FRMSIZE_TYPE_STEPWISE) {
		int min, max, i, cnt = 0;

		max = ability->frmival.max.denominator / ability->frmival.max.numerator;
		min = ability->frmival.min.denominator / ability->frmival.min.numerator;

		rf_ability->frmival_num = max - min + 1;

		for (i = min; i <= max; i++) {
			rf_ability->frmivals[cnt].denominator = i;
			rf_ability->frmivals[cnt].numerator = 1;
			cnt++;
		}
	}

	rts_av_release_isp_ability(ability);

	return RF_ERR_OK;
}

static int get_encoder_attr(struct video_context *context,
		struct rf_encode_attr *encode_attr)
{
	int ret;
	int ch_id = context->isp_id;
	int encodec_chn;
	struct rf_encode_attr *rf_attr = encode_attr;

	rf_attr->codec_type = g_config[ch_id].codec_type;

	if (g_config[ch_id].codec_type == 0) {
		struct rts_h264_ctrl *h264_attr;

		encodec_chn = context->h264_ch;
		ret = rts_av_query_h264_ctrl(encodec_chn, &h264_attr);
		if (ret != RF_ERR_OK)
			return RF_ERR_GET_ENCODE_ATTR;

		ret = rts_av_get_h264_ctrl(h264_attr);
		if (ret != RF_ERR_OK)
			return RF_ERR_GET_ENCODE_ATTR;

		rf_attr->bitrate_mode = h264_attr->bitrate_mode;
		rf_attr->gop_mode = h264_attr->gop_mode;

		rf_attr->bitrate = h264_attr->bitrate;
		rf_attr->max_bitrate = h264_attr->max_bitrate;
		rf_attr->min_bitrate = h264_attr->min_bitrate;

		rf_attr->qp = h264_attr->qp;
		rf_attr->max_qp = h264_attr->max_qp;
		rf_attr->min_qp = h264_attr->min_qp;

		rf_attr->gop = h264_attr->gop;
		rf_attr->super_p_period = h264_attr->super_p_period;

		g_config[ch_id].h264_config.bitrate = h264_attr->bitrate;
		g_config[ch_id].h264_config.qp = h264_attr->qp;
		g_config[ch_id].h264_config.gop = h264_attr->gop;

		rts_av_release_h264_ctrl(h264_attr);
 	} else {
		struct rts_h265_ctrl *h265_attr;

		encodec_chn = context->h265_ch;
		ret = rts_av_query_h265_ctrl(encodec_chn, &h265_attr);
		if (ret != RF_ERR_OK)
			return RF_ERR_GET_ENCODE_ATTR;

		ret = rts_av_get_h265_ctrl(h265_attr);
		if (ret != RF_ERR_OK)
			return RF_ERR_GET_ENCODE_ATTR;

		rf_attr->bitrate_mode = h265_attr->bitrate_mode;
		rf_attr->gop_mode = h265_attr->gop_mode;

		rf_attr->bitrate = h265_attr->bitrate;
		rf_attr->max_bitrate = h265_attr->max_bitrate;
		rf_attr->min_bitrate = h265_attr->min_bitrate;

		rf_attr->qp = h265_attr->qp;
		rf_attr->max_qp = h265_attr->max_qp;
		rf_attr->min_qp = h265_attr->min_qp;

		rf_attr->gop = h265_attr->gop;
		rf_attr->super_p_period = h265_attr->super_p_period;

		g_config[ch_id].h265_config.bitrate = h265_attr->bitrate;
		g_config[ch_id].h265_config.gop = h265_attr->gop;

		rts_av_release_h265_ctrl(h265_attr);
	}

	return RF_ERR_OK;
}

static int set_encoder_attr(struct video_context *context,
		struct rf_encode_attr *encode_attr)
{
	int ret;
	int ch_id = context->isp_id;
	int encodec_chn;
	struct rf_encode_attr *rf_attr = encode_attr;

	g_config[ch_id].codec_type = rf_attr->codec_type;

	if (g_config[ch_id].codec_type == 0) {
		struct rts_h264_ctrl *h264_attr;

		encodec_chn = context->h264_ch;
		ret = rts_av_query_h264_ctrl(encodec_chn, &h264_attr);
		if (ret != RF_ERR_OK)
			return RF_ERR_GET_ENCODE_ATTR;

		h264_attr->gop_mode = rf_attr->gop_mode;

		h264_attr->bitrate = rf_attr->bitrate;
		h264_attr->max_bitrate = rf_attr->max_bitrate;
		h264_attr->min_bitrate = rf_attr->min_bitrate;

		h264_attr->qp = rf_attr->qp;
		h264_attr->max_qp = rf_attr->max_qp;
		h264_attr->min_qp = rf_attr->min_qp;

		h264_attr->gop = rf_attr->gop;
		h264_attr->super_p_period = rf_attr->super_p_period;

		rts_av_disable_chn(context->isp_ch);

		ret = rts_av_set_h264_ctrl(h264_attr);
		if (ret != RF_ERR_OK)
			return RF_ERR_SET_ENCODE_ATTR;

		ret = rts_av_enable_chn(context->isp_ch);

		rts_av_release_h264_ctrl(h264_attr);

		if (rf_attr->bitrate != g_config[ch_id].h264_config.bitrate)
			context->config.rebuild_rb = 1;

		g_config[ch_id].h264_config.bitrate = rf_attr->bitrate;
		g_config[ch_id].h264_config.qp = rf_attr->qp;
		g_config[ch_id].h264_config.gop = rf_attr->gop;

		context->config.refresh_rb = 1;
	} else {
		struct rts_h265_ctrl *h265_attr;

		encodec_chn = context->h265_ch;
		ret = rts_av_query_h265_ctrl(encodec_chn, &h265_attr);
		if (ret != RF_ERR_OK)
			return RF_ERR_GET_ENCODE_ATTR;

		h265_attr->gop_mode = rf_attr->gop_mode;

		h265_attr->bitrate = rf_attr->bitrate;
		h265_attr->max_bitrate = rf_attr->max_bitrate;
		h265_attr->min_bitrate = rf_attr->min_bitrate;

		h265_attr->qp = rf_attr->qp;
		h265_attr->max_qp = rf_attr->max_qp;
		h265_attr->min_qp = rf_attr->min_qp;

		h265_attr->gop = rf_attr->gop;
		h265_attr->super_p_period = rf_attr->super_p_period;

		rts_av_disable_chn(context->isp_ch);

		ret = rts_av_set_h265_ctrl(h265_attr);
		if (ret != RF_ERR_OK)
			return RF_ERR_SET_ENCODE_ATTR;

		ret = rts_av_enable_chn(context->isp_ch);

		rts_av_release_h265_ctrl(h265_attr);

		if (rf_attr->bitrate != g_config[ch_id].h265_config.bitrate)
			context->config.rebuild_rb = 1;

		g_config[ch_id].h265_config.bitrate = rf_attr->bitrate;
		g_config[ch_id].h265_config.gop = rf_attr->gop;

		context->config.refresh_rb = 1;
	}

	return RF_ERR_OK;
}

static int get_encoder_ability(struct rf_encode_ability *encode_ability)
{
	int ret;
	struct rf_encode_ability *enc_ability = encode_ability;
	if (!enc_ability)
		return 0;

#ifdef RTS_ENABLE_H1
		enc_ability->profile.minimum = RF_H264_PROFILE_BASE;
		enc_ability->profile.maximum = RF_H264_PROFILE_HIGH;
		enc_ability->profile.step = 1;

		enc_ability->level.minimum = RF_H264_LEVEL_1;
		enc_ability->level.maximum = RF_H264_LEVEL_5_1;
		enc_ability->level.step = 1;
#endif
#if (defined RTS_ENABLE_W420) || (defined RTS_ENABLE_W521)
		enc_ability->level.minimum = RF_H265_LEVEL_1;
		enc_ability->level.maximum = RF_H265_LEVEL_5_1;
		enc_ability->level.step = 1;
#endif

	enc_ability->rotate.minimum = RF_AV_ROTATION_0;
	enc_ability->rotate.maximum = RF_AV_ROTATION_180;
	enc_ability->rotate.step = 1;

	enc_ability->gop_mode.minimum = RF_GOP_MODE_NORMAL;
	enc_ability->gop_mode.maximum = RF_GOP_MODE_SP;
	enc_ability->gop_mode.step = 1;

	enc_ability->bitrate_mode.minimum = RF_BITRATE_MODE_CBR;
	enc_ability->bitrate_mode.maximum = RF_BITRATE_MODE_S_VBR;
	enc_ability->bitrate_mode.step = 1;

	enc_ability->gop[0].minimum = RF_MIN_GOP;
	enc_ability->gop[0].maximum = RF_MAX_GOP_NORMAL;

	enc_ability->gop[1].minimum = RF_MIN_GOP;
	enc_ability->gop[1].maximum = RF_MAX_GOP_BSMART;

	enc_ability->super_p_period[0].minimum = enc_ability->gop[0].minimum;
	enc_ability->super_p_period[0].maximum = enc_ability->gop[0].maximum;
	enc_ability->super_p_period[1].minimum = enc_ability->gop[1].minimum;
	enc_ability->super_p_period[1].maximum = enc_ability->gop[1].maximum;

	return RF_ERR_OK;

}

int rf_control_video_capture(
	int request, void *arg)
{
	int ret = RF_ERR_OK;
	struct rf_video_req *req = arg;
	struct rf_encode_attr *enc_attr = NULL;
	struct video_context *context = NULL;
	struct rf_encode_ability *enc_ability = NULL;
	struct rf_isp_attr *isp_attr = NULL;
	struct rf_isp_ability *isp_ability = NULL;
	struct rf_snapshot *snapshot = NULL;
	int index = req->index;

	context = g_video_ctx[index];

	switch (request) {
	case RF_VIDEO_CTRL_PROBE:
		break;
	case RF_VIDEO_CTRL_PAUSE:
		pthread_mutex_lock(&context->mutex);
		context->status = STREAM_STATUS_PAUSE;
		pthread_mutex_unlock(&context->mutex);
		break;
	case RF_VIDEO_CTRL_RESUME:
		pthread_mutex_lock(&context->mutex);
		context->status = STREAM_STATUS_PLAYING;
		pthread_mutex_unlock(&context->mutex);
		break;
	case RF_VIDEO_CTRL_KEY_FRAME:
		get_encode_key_frame(context);
		break;
	case RF_VIDEO_SNAPSHOT:
		snapshot = &req->snapshot;
		snapshot_and_save(snapshot);
		break;
	case RF_VIDEO_GET_ENCODER_ABILITY:
		enc_ability = &req->enc_ability;
		get_encoder_ability(enc_ability);
		break;
	case RF_VIDEO_GET_ENCODER_ATTR:
		enc_attr = &req->enc_attr;
		get_encoder_attr(context, enc_attr);
		break;
	case RF_VIDEO_SET_ENCODER_ATTR:
		enc_attr = &req->enc_attr;
		set_encoder_attr(context, enc_attr);
		update_encoder_attr_to_json(req->index, &g_config[req->index]);
		break;
	case RF_VIDEO_GET_ISP_ABILITY:
		isp_ability = &req->isp_ability;
		get_isp_ability(context, isp_ability);
		break;
	case RF_VIDEO_GET_ISP_ATTR:
		isp_attr = &req->isp_attr;
		get_isp_attr(context, isp_attr);
		break;
	case RF_VIDEO_SET_ISP_ATTR:
		isp_attr = &req->isp_attr;
		set_isp_attr(context, isp_attr);
		update_isp_attr_to_json(req->index, &g_config[req->index]);
		break;
	default:
		ret = RF_ERR_REQUEST_NOT_SUPPORT;
		break;
	}

	return ret;
}

int get_isp_status(void)
{
	int status = RTS_ISP_UNINITIALIZED;
	int i, try_times = 1000;

	for (i = 0; i < try_times; i++) {
		status = rts_av_isp_get_status();
		if (status >= RTS_ISP_RUNNING)
			return 0;

		usleep(10000);
	}

	if (status < 0)
		RTS_ERR("get isp status failed\n");
	else
		RTS_ERR("get isp status abnormal, status is %d", status);

	return -1;
}

int rf_start_video_capture(void)
{
	int ret = RF_ERR_OK;
	int i;
	struct video_context *context = NULL;
	struct video_config *config0 = &g_config[0];
	struct video_config *config1 = &g_config[1];

	ret = get_isp_status();
	if (ret)
		return -1;

	int vin_mode_0 = update_video_config_from_json(0, config0, 1);
	int vin_mode_1 = update_video_config_from_json(1, config1, 1);

	__init_sys_vmem(vin_mode_0, vin_mode_1);

	for (i = 0; i < MAX_STREAM_NUM; i++) {
		context = create_context();
		ret = init_output(context);
		if (ret) {
			RTS_ERR("init video output error\n");
			goto failed;
		}

		ret = create_stream(i, context);
		if (ret) {
			RTS_ERR("creat video stream error\n");
			uninit_output(context);
			goto failed;
		}

		ret = start_stream(context);
		if (ret) {
			RTS_ERR("enable video error\n");
			goto failed;
		}

		pthread_create(&context->tid,
			NULL, capture_thread, (void *)i);
		context->config.priv_status =
			context->config.cur_status = STREAM_STATUS_PLAYING;
		g_video_ctx[i] = context;
	}

	return ret;

failed:
	rf_stop_video_capture();
	return ret;
}

void rf_stop_video_capture(void)
{
	int ret;
	int i;
	struct video_context *context = NULL;

	for (i = 0; i < MAX_STREAM_NUM; i++) {
		context = g_video_ctx[i];

		if (context) {
			stop_capture_thread(context);
			stop_stream(context);
			delete_stream(context);
			uninit_output(context);
			release_context(context);
		}
		g_video_ctx[i] = NULL;
	}

	__release_sys_vmem();
}

