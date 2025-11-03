/*
 *  Copyright (C) 2019 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <syslog.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include <rtsaudio.h>
#include <rtscolor.h>

#include <CUnit/Basic.h>

#include "rf_msg.h"

struct rf_video_req video_req_get, video_req_set;
struct rf_audio_req audio_req_get, audio_req_set;
struct rf_ispctrl_req ispctrl_req_get, ispctrl_req_set;
struct rf_mask_req mask_req_get, mask_req_set;
struct rf_md_req md_req_get, md_req_set;
struct rf_osd_req osd_req_get, osd_req_set;

static int notify_callback(
	struct rf_notify_st *notify)
{
	printf("get notify event: id %08x time %x\n",
		notify->id, (unsigned int)notify->time);
	return 0;
}

typedef void (*testcall_t)(void);

struct test_item {
	testcall_t fn;
	const char *name;
	const char *info;
};

void rf_test_audio(void)
{
	int ret = RF_ERR_OK;

	audio_req_set.attr.playback_format = 16;
	audio_req_set.attr.playback_channels = 2;
	audio_req_set.attr.playback_sample_rate = 16000;
	audio_req_set.attr.capture_format = 16;
	audio_req_set.attr.capture_channels = 2;
	audio_req_set.attr.capture_sample_rate = 16000;
	audio_req_set.attr.encode_format = 16;
	audio_req_set.attr.encode_channels = 1;
	audio_req_set.attr.encode_sample_rate = 16000;
	audio_req_set.attr.encode_type = RTS_AUDIO_TYPE_ID_ULAW;
	audio_req_set.attr.encode_bitrate = 48000;

	ret = rf_send_msg(
			RF_AUDIO_SET_ATTR,
			&audio_req_set,
			sizeof(audio_req_set));
	CU_ASSERT(ret == RTS_OK);
	if (ret)
		RTS_ERR("send rf msg %08x ret %d\n",
			RF_AUDIO_SET_ATTR, ret);

	ret = rf_send_msg(
			RF_AUDIO_GET_ATTR,
			&audio_req_get,
			sizeof(audio_req_get));
	CU_ASSERT(ret == RTS_OK);
	if (ret)
		RTS_ERR("send rf msg %08x ret %d\n",
			RF_AUDIO_GET_ATTR, ret);

	CU_ASSERT(audio_req_set.attr.playback_format
			== audio_req_get.attr.playback_format);
	CU_ASSERT(audio_req_set.attr.playback_channels
			== audio_req_get.attr.playback_channels);
	CU_ASSERT(audio_req_set.attr.playback_sample_rate
			== audio_req_get.attr.playback_sample_rate);
	CU_ASSERT(audio_req_set.attr.capture_format
			== audio_req_get.attr.capture_format);
	CU_ASSERT(audio_req_set.attr.capture_channels
			== audio_req_get.attr.capture_channels);
	CU_ASSERT(audio_req_set.attr.capture_sample_rate
			== audio_req_get.attr.capture_sample_rate);
	CU_ASSERT(audio_req_set.attr.encode_format
			== audio_req_get.attr.encode_format);
	CU_ASSERT(audio_req_set.attr.encode_channels
			== audio_req_get.attr.encode_channels);
	CU_ASSERT(audio_req_set.attr.encode_sample_rate
			== audio_req_get.attr.encode_sample_rate);
	CU_ASSERT(audio_req_set.attr.encode_type
			== audio_req_get.attr.encode_type);
	CU_ASSERT(audio_req_set.attr.encode_bitrate
			== audio_req_get.attr.encode_bitrate);

	audio_req_set.volume.capture_volume = 30;
	audio_req_set.volume.play_volume = 30;

	ret = rf_send_msg(
			RF_AUDIO_SET_VOLUME,
			&audio_req_set,
			sizeof(audio_req_set));
	CU_ASSERT(ret == RTS_OK);
	if (ret)
		RTS_ERR("send rf msg %08x ret %d\n",
			RF_AUDIO_SET_VOLUME, ret);

	ret = rf_send_msg(
			RF_AUDIO_GET_VOLUME,
			&audio_req_get,
			sizeof(audio_req_get));
	CU_ASSERT(ret == RTS_OK);
	if (ret)
		RTS_ERR("send rf msg %08x ret %d\n",
			RF_AUDIO_GET_VOLUME, ret);
	CU_ASSERT(audio_req_set.volume.capture_volume
			== audio_req_get.volume.capture_volume);
	CU_ASSERT(audio_req_set.volume.play_volume
			== audio_req_get.volume.play_volume);

	audio_req_set.aec.aecns_enable = 1;
	audio_req_set.aec.aec_en = 1;
	audio_req_set.aec.ns_en = 1;
	audio_req_set.aec.aec_thr = 0;
	audio_req_set.aec.aec_scale = 10;

	ret = rf_send_msg(
			RF_AUDIO_SET_AEC,
			&audio_req_set,
			sizeof(audio_req_set));
	CU_ASSERT(ret == RTS_OK);
	if (ret)
		RTS_ERR("send rf msg %08x ret %d\n",
			RF_AUDIO_SET_AEC, ret);

	ret = rf_send_msg(
			RF_AUDIO_GET_AEC,
			&audio_req_get,
			sizeof(audio_req_get));
	CU_ASSERT(ret == RTS_OK);
	if (ret)
		RTS_ERR("send rf msg %08x ret %d\n",
			RF_AUDIO_GET_AEC, ret);

	CU_ASSERT(audio_req_set.aec.aecns_enable
	== audio_req_get.aec.aecns_enable);
	CU_ASSERT(audio_req_set.aec.aec_en
	== audio_req_get.aec.aec_en);
	CU_ASSERT(audio_req_set.aec.ns_en
	== audio_req_get.aec.ns_en);
	CU_ASSERT(audio_req_set.aec.aec_thr
	== audio_req_get.aec.aec_thr);
	CU_ASSERT(audio_req_set.aec.aec_scale
	== audio_req_get.aec.aec_scale);
}

void rf_test_video(void)
{
	int ret = RF_ERR_OK;

	video_req_set.index = 0;
	video_req_set.isp_attr.height = 720;
	video_req_set.isp_attr.width = 1280;
	video_req_set.isp_attr.framerate = 15;

	ret = rf_send_msg(
			RF_VIDEO_SET_ISP_ATTR,
			&video_req_set,
			sizeof(video_req_set));
	CU_ASSERT(ret == RTS_OK);
	if (ret)
		RTS_ERR("send rf msg %08x ret %d\n",
			RF_VIDEO_SET_ISP_ATTR, ret);

	ret = rf_send_msg(
			RF_VIDEO_GET_ISP_ATTR,
			&video_req_get,
			sizeof(video_req_get));
	CU_ASSERT(ret == RTS_OK);
	if (ret)
		RTS_ERR("send rf msg %08x ret %d\n",
			RF_VIDEO_GET_ISP_ATTR, ret);

	CU_ASSERT(video_req_set.isp_attr.height
			== video_req_get.isp_attr.height);
	CU_ASSERT(video_req_set.isp_attr.width
			== video_req_get.isp_attr.width);
	CU_ASSERT(video_req_set.isp_attr.framerate
			== video_req_get.isp_attr.framerate);

	memset(&video_req_set, 0, sizeof(video_req_set));
	video_req_set.snapshot.number = 3;
	video_req_set.snapshot.interval = 0;

	ret = rf_send_msg(
			RF_VIDEO_SNAPSHOT,
			&video_req_set,
			sizeof(video_req_set));
	CU_ASSERT(ret == RTS_OK);
	if (ret)
		RTS_ERR("send rf msg %08x ret %d\n",
			RF_VIDEO_SNAPSHOT, ret);
}


void rf_test_isp(void)
{
	int ret = RF_ERR_OK;

	ispctrl_req_set.all.power_line_frequency.id
		= ISPCTRL_ID_PWR_FREQUENCY;
	ispctrl_req_set.all.power_line_frequency.current = 1;

	ispctrl_req_set.all.white_balance.id = ISPCTRL_ID_AWB_CTRL;
	ispctrl_req_set.all.white_balance.current = 0;

	ispctrl_req_set.all.white_balance_temperature.id
		= ISPCTRL_ID_WB_TEMPERATURE;
	ispctrl_req_set.all.white_balance_temperature.current
					= 3000;

#if (defined RTS_VER_RTS3903) || (defined RTS_VER_RTS3913)
	ispctrl_req_set.all.mirror.id = ISPCTRL_ID_MIRROR;
	ispctrl_req_set.all.mirror.current = 1;
	ispctrl_req_set.all.mirror.chn = 0;

	ispctrl_req_set.all.flip.id = ISPCTRL_ID_FLIP;
	ispctrl_req_set.all.flip.current = 1;
	ispctrl_req_set.all.flip.chn = 0;
#endif

	ispctrl_req_set.all.dehaze.id = ISPCTRL_ID_DEHAZE;
	ispctrl_req_set.all.dehaze.current = 1;

	ispctrl_req_set.all.ldc.id = ISPCTRL_ID_LDC;
	ispctrl_req_set.all.ldc.current = 1;

	ispctrl_req_set.all.three_dnr.id
			= ISPCTRL_ID_3DNR;
	ispctrl_req_set.all.three_dnr.current = 1;

	ispctrl_req_set.all.gamma.id = ISPCTRL_ID_GAMMA;
	ispctrl_req_set.all.gamma.current = 250;

	ispctrl_req_set.all.brightness.id
			= ISPCTRL_ID_BRIGHTNESS;
	ispctrl_req_set.all.brightness.current = 1;

	ispctrl_req_set.all.contrast.id = ISPCTRL_ID_CONTRAST;
	ispctrl_req_set.all.contrast.current = 50;

	ispctrl_req_set.all.saturation.id = ISPCTRL_ID_SATURATION;
	ispctrl_req_set.all.saturation.current = 50;

	ispctrl_req_set.all.sharpness.id = ISPCTRL_ID_SHARPNESS;
	ispctrl_req_set.all.sharpness.current = 50;

	ispctrl_req_set.all.wdr_mode.id = ISPCTRL_ID_WDR_MODE;
	ispctrl_req_set.all.wdr_mode.current = 1;

	ispctrl_req_set.all.wdr_level.id = ISPCTRL_ID_WDR_LEVEL;
	ispctrl_req_set.all.wdr_level.current = 64;

	ispctrl_req_set.all.ir_mode.id = ISPCTRL_ID_IR_MODE;
	ispctrl_req_set.all.ir_mode.current = 1;

	ispctrl_req_set.all.smart_ir_mode.id
			= ISPCTRL_ID_SMART_IR_MODE;
	ispctrl_req_set.all.smart_ir_mode.current = 1;

	ispctrl_req_set.all.smart_ir_level.id
			= ISPCTRL_ID_SMART_IR_MANUAL_LEVEL;
	ispctrl_req_set.all.smart_ir_level.current = 64;

	ret = rf_send_msg(
			RF_ISPCTRL_SET_ALL,
			&ispctrl_req_set,
			sizeof(ispctrl_req_set));
	CU_ASSERT(ret == RTS_OK);
	if (ret)
		RTS_ERR("send rf msg %08x ret %d\n",
			RF_ISPCTRL_SET_ALL, ret);

	ret = rf_send_msg(
			RF_ISPCTRL_GET_ALL,
			&ispctrl_req_get,
			sizeof(ispctrl_req_get));
	CU_ASSERT(ret == RTS_OK);
	if (ret)
		RTS_ERR("send rf msg %08x ret %d\n",
			RF_ISPCTRL_GET_ALL, ret);

	CU_ASSERT(ispctrl_req_set.all.power_line_frequency.id
			== ispctrl_req_get.all.power_line_frequency.id);
	CU_ASSERT(ispctrl_req_set.all.power_line_frequency.current
			== ispctrl_req_get.all.power_line_frequency.current);

	CU_ASSERT(ispctrl_req_set.all.white_balance_temperature.id
			== ispctrl_req_get.all.white_balance_temperature.id);
	CU_ASSERT(ispctrl_req_set.all.white_balance_temperature.current
		== ispctrl_req_get.all.white_balance_temperature.current);

#if (defined RTS_VER_RTS3903) || (defined RTS_VER_RTS3913)
	CU_ASSERT(ispctrl_req_set.all.mirror.id
			== ispctrl_req_get.all.mirror.id);
	CU_ASSERT(ispctrl_req_set.all.mirror.current
			== ispctrl_req_get.all.mirror.current);
	CU_ASSERT(ispctrl_req_set.all.mirror.chn
			== ispctrl_req_get.all.mirror.chn);

	CU_ASSERT(ispctrl_req_set.all.flip.id
			== ispctrl_req_get.all.flip.id);
	CU_ASSERT(ispctrl_req_set.all.flip.current
			== ispctrl_req_get.all.flip.current);
	CU_ASSERT(ispctrl_req_set.all.mirror.chn
			== ispctrl_req_get.all.mirror.chn);

	CU_ASSERT(ispctrl_req_set.all.dehaze.id
			== ispctrl_req_get.all.dehaze.id);
	CU_ASSERT(ispctrl_req_set.all.dehaze.current
			== ispctrl_req_get.all.dehaze.current);
#endif
	CU_ASSERT(ispctrl_req_set.all.ldc.id
			== ispctrl_req_get.all.ldc.id);
	CU_ASSERT(ispctrl_req_set.all.ldc.current
			== ispctrl_req_get.all.ldc.current);

	CU_ASSERT(ispctrl_req_set.all.three_dnr.id
			== ispctrl_req_get.all.three_dnr.id);
	CU_ASSERT(ispctrl_req_set.all.three_dnr.current
			== ispctrl_req_get.all.three_dnr.current);

	CU_ASSERT(ispctrl_req_set.all.gamma.id
			== ispctrl_req_get.all.gamma.id);
	CU_ASSERT(ispctrl_req_set.all.gamma.current
			== ispctrl_req_get.all.gamma.current);

	CU_ASSERT(ispctrl_req_set.all.brightness.id
			== ispctrl_req_get.all.brightness.id);
	CU_ASSERT(ispctrl_req_set.all.brightness.current
			== ispctrl_req_get.all.brightness.current);

	CU_ASSERT(ispctrl_req_set.all.contrast.id
			== ispctrl_req_get.all.contrast.id);
	CU_ASSERT(ispctrl_req_set.all.contrast.current
			== ispctrl_req_get.all.contrast.current);

	CU_ASSERT(ispctrl_req_set.all.saturation.id
			== ispctrl_req_get.all.saturation.id);
	CU_ASSERT(ispctrl_req_set.all.saturation.current
			== ispctrl_req_get.all.saturation.current);

	CU_ASSERT(ispctrl_req_set.all.sharpness.id
			== ispctrl_req_get.all.sharpness.id);
	CU_ASSERT(ispctrl_req_set.all.sharpness.current
			== ispctrl_req_get.all.sharpness.current);

	CU_ASSERT(ispctrl_req_set.all.wdr_mode.id
			== ispctrl_req_get.all.wdr_mode.id);
	CU_ASSERT(ispctrl_req_set.all.wdr_mode.current
			== ispctrl_req_get.all.wdr_mode.current);

	CU_ASSERT(ispctrl_req_set.all.wdr_level.id
			== ispctrl_req_get.all.wdr_level.id);
	CU_ASSERT(ispctrl_req_set.all.wdr_level.current
			== ispctrl_req_get.all.wdr_level.current);

	CU_ASSERT(ispctrl_req_set.all.ir_mode.id
			== ispctrl_req_get.all.ir_mode.id);
	CU_ASSERT(ispctrl_req_set.all.ir_mode.current
			== ispctrl_req_get.all.ir_mode.current);

	CU_ASSERT(ispctrl_req_set.all.smart_ir_mode.id
			== ispctrl_req_get.all.smart_ir_mode.id);
	CU_ASSERT(ispctrl_req_set.all.smart_ir_mode.current
			== ispctrl_req_get.all.smart_ir_mode.current);

	CU_ASSERT(ispctrl_req_set.all.smart_ir_level.id
			== ispctrl_req_get.all.smart_ir_level.id);
	CU_ASSERT(ispctrl_req_set.all.smart_ir_level.current
			== ispctrl_req_get.all.smart_ir_level.current);

#ifdef RTS_VER_RTS3915
	struct rf_ispctrl_req flip_req_get, flip_req_set;
	struct rf_ispctrl_req mirror_req_get, mirror_req_set;

	mirror_req_set.ctrl.id = ISPCTRL_ID_MIRROR;
	mirror_req_set.ctrl.current = 1;
	mirror_req_set.ctrl.chn = 0;

	mirror_req_get.ctrl.id = ISPCTRL_ID_MIRROR;
	mirror_req_get.ctrl.chn = 0;

	ret = rf_send_msg(
			RF_ISPCTRL_SET_CTRL,
			&mirror_req_set,
			sizeof(mirror_req_set));
	CU_ASSERT(ret == RTS_OK);
	if (ret)
		RTS_ERR("send rf msg %08x ret %d\n",
			RF_ISPCTRL_SET_CTRL, ret);

	ret = rf_send_msg(
			RF_ISPCTRL_GET_CTRL,
			&mirror_req_get,
			sizeof(mirror_req_get));
	CU_ASSERT(ret == RTS_OK);
	if (ret)
		RTS_ERR("send rf msg %08x ret %d\n",
			RF_ISPCTRL_GET_CTRL, ret);

	CU_ASSERT(mirror_req_get.ctrl.id
			== mirror_req_set.ctrl.id);
	CU_ASSERT(mirror_req_get.ctrl.current
			== mirror_req_set.ctrl.current);

	flip_req_set.ctrl.id = ISPCTRL_ID_FLIP;
	flip_req_set.ctrl.current = 1;
	flip_req_set.ctrl.chn = 0;

	flip_req_get.ctrl.id = ISPCTRL_ID_FLIP;
	flip_req_get.ctrl.chn = 0;

	ret = rf_send_msg(
			RF_ISPCTRL_SET_CTRL,
			&flip_req_set,
			sizeof(flip_req_set));
	CU_ASSERT(ret == RTS_OK);
	if (ret)
		RTS_ERR("send rf msg %08x ret %d\n",
			RF_ISPCTRL_SET_CTRL, ret);

	ret = rf_send_msg(
			RF_ISPCTRL_GET_CTRL,
			&flip_req_get,
			sizeof(flip_req_get));
	CU_ASSERT(ret == RTS_OK);
	if (ret)
		RTS_ERR("send rf msg %08x ret %d\n",
			RF_ISPCTRL_GET_CTRL, ret);

	CU_ASSERT(flip_req_get.ctrl.id
			== flip_req_set.ctrl.id);
	CU_ASSERT(flip_req_get.ctrl.current
			== flip_req_set.ctrl.current);

#endif

}

void rf_test_mask(void)
{
	int ret = RF_ERR_OK;

	mask_req_set.attr.rects[0].enable = 1;
	mask_req_set.attr.rects[0].left = 20;
	mask_req_set.attr.rects[0].top = 20;
	mask_req_set.attr.rects[0].right = 60;
	mask_req_set.attr.rects[0].bottom = 60;

	ret = rf_send_msg(
			RF_MASK_SET_ATTR,
			&mask_req_set,
			sizeof(mask_req_set));
	CU_ASSERT(ret == RTS_OK);
	if (ret)
		RTS_ERR("send rf msg %08x ret %d\n",
			RF_MASK_SET_ATTR, ret);

	ret = rf_send_msg(
			RF_MASK_GET_ATTR,
			&mask_req_get,
			sizeof(mask_req_set));
	CU_ASSERT(ret == RTS_OK);
	if (ret)
		RTS_ERR("send rf msg %08x ret %d\n",
			RF_MASK_GET_ATTR, ret);

	CU_ASSERT(mask_req_set.attr.rects[0].enable
			== mask_req_get.attr.rects[0].enable);
	CU_ASSERT(mask_req_set.attr.rects[0].left
			== mask_req_get.attr.rects[0].left);
	CU_ASSERT(mask_req_set.attr.rects[0].top
			== mask_req_get.attr.rects[0].top);
	CU_ASSERT(mask_req_set.attr.rects[0].right
			== mask_req_get.attr.rects[0].right);
	CU_ASSERT(mask_req_set.attr.rects[0].bottom
			== mask_req_get.attr.rects[0].bottom);

	mask_req_set.attr.rects[0].enable = 0;

	mask_req_set.attr.rects[1].enable = 1;
	mask_req_set.attr.rects[1].left = 70;
	mask_req_set.attr.rects[1].top = 70;
	mask_req_set.attr.rects[1].right = 120;
	mask_req_set.attr.rects[1].bottom = 120;

	ret = rf_send_msg(
			RF_MASK_SET_ATTR,
			&mask_req_set,
			sizeof(mask_req_set));
	CU_ASSERT(ret == RTS_OK);
	if (ret)
		RTS_ERR("send rf msg %08x ret %d\n",
			RF_MASK_SET_ATTR, ret);

	ret = rf_send_msg(
			RF_MASK_GET_ATTR,
			&mask_req_get,
			sizeof(mask_req_get));
	CU_ASSERT(ret == RTS_OK);
	if (ret)
		RTS_ERR("send rf msg %08x ret %d\n",
			RF_MASK_GET_ATTR, ret);

	CU_ASSERT(mask_req_set.attr.rects[1].enable
			== mask_req_get.attr.rects[1].enable);
	CU_ASSERT(mask_req_set.attr.rects[1].left
			== mask_req_get.attr.rects[1].left);
	CU_ASSERT(mask_req_set.attr.rects[1].top
			== mask_req_get.attr.rects[1].top);
	CU_ASSERT(mask_req_set.attr.rects[1].right
			== mask_req_get.attr.rects[1].right);
	CU_ASSERT(mask_req_set.attr.rects[1].bottom
			== mask_req_get.attr.rects[1].bottom);

	mask_req_set.attr.rects[0].enable = 0;
	mask_req_set.attr.rects[1].enable = 0;

	mask_req_set.attr.grids[0].enable = 1;
	mask_req_set.attr.grids[0].start_x = 0;
	mask_req_set.attr.grids[0].start_y = 0;
	mask_req_set.attr.grids[0].cell_width = 4;
	mask_req_set.attr.grids[0].cell_height = 3;
	mask_req_set.attr.grids[0].grid_rows = 30;
	mask_req_set.attr.grids[0].grid_columns = 40;
	mask_req_set.attr.grids[0].bitmap.vm_addr = malloc(1200);
	memset(mask_req_set.attr.grids[0].bitmap.vm_addr, 0xf, 1200);

	ret = rf_send_msg(
			RF_MASK_SET_ATTR,
			&mask_req_set,
			sizeof(mask_req_set));
	CU_ASSERT(ret == RTS_OK);
	if (ret)
		RTS_ERR("send rf msg %08x ret %d\n",
			RF_MASK_SET_ATTR, ret);

	ret = rf_send_msg(
			RF_MASK_GET_ATTR,
			&mask_req_get,
			sizeof(mask_req_get));
	CU_ASSERT(ret == RTS_OK);
	if (ret)
		RTS_ERR("send rf msg %08x ret %d\n",
			RF_MASK_GET_ATTR, ret);


	CU_ASSERT(mask_req_set.attr.grids[0].enable
			== mask_req_get.attr.grids[0].enable);
	CU_ASSERT(mask_req_set.attr.grids[0].start_x
			== mask_req_get.attr.grids[0].start_x);
	CU_ASSERT(mask_req_set.attr.grids[0].start_y
			== mask_req_get.attr.grids[0].start_y);
	CU_ASSERT(mask_req_set.attr.grids[0].cell_width
			== mask_req_get.attr.grids[0].cell_width);
	CU_ASSERT(mask_req_set.attr.grids[0].cell_height
			== mask_req_get.attr.grids[0].cell_height);
	CU_ASSERT(mask_req_set.attr.grids[0].grid_rows
			== mask_req_get.attr.grids[0].grid_rows);
	CU_ASSERT(mask_req_set.attr.grids[0].grid_columns
			== mask_req_get.attr.grids[0].grid_columns);

}

void rf_test_osd(void)
{
	int ret = RF_ERR_OK;
	int i = 0;
	struct rf_osd_block *block = &osd_req_set.attr.blocks[i];

	block->enable = 1;
	block->left = 0;
	block->top = 0;
	block->right = 100;
	block->bottom = 50;

	block->picture.pixel_fmt = RTS_OSD2_BLK_FMT_RGBA2222;
	block->picture.length = 0;
	block->picture.pdata = NULL;

	ret = rf_send_msg(
			RF_OSD_SET_ATTR,
			&osd_req_set,
			sizeof(osd_req_set));
	CU_ASSERT(ret == RTS_OK);
	if (ret)
		RTS_ERR("send rf msg %08x ret %d\n",
			RF_OSD_SET_ATTR, ret);

	ret = rf_send_msg(
			RF_OSD_GET_ATTR,
			&osd_req_get,
			sizeof(osd_req_get));
	CU_ASSERT(ret == RTS_OK);
	if (ret)
		RTS_ERR("send rf msg %08x ret %d\n",
			RF_OSD_GET_ATTR, ret);

	CU_ASSERT(osd_req_set.attr.blocks[0].enable
			== osd_req_get.attr.blocks[0].enable);
	CU_ASSERT(osd_req_set.attr.blocks[0].left
			== osd_req_get.attr.blocks[0].left);
	CU_ASSERT(osd_req_set.attr.blocks[0].top
			== osd_req_get.attr.blocks[0].top);
	CU_ASSERT(osd_req_set.attr.blocks[0].right
			== osd_req_get.attr.blocks[0].right);
	CU_ASSERT(osd_req_set.attr.blocks[0].bottom
			== osd_req_get.attr.blocks[0].bottom);
}

#define TEST_ITEM(func) \
	{ \
		.fn = func, \
		.name = #func, \
		.info = NULL \
	}

static struct test_item isp_items[] = {
	TEST_ITEM(rf_test_audio),
	TEST_ITEM(rf_test_video),
	TEST_ITEM(rf_test_isp),
	TEST_ITEM(rf_test_mask),
	TEST_ITEM(rf_test_osd),

	NULL,
};

static int suite_init(void)
{
	rts_av_init();

	return 0;
}

static int  suite_cleanup(void)
{
	rts_av_release();

	return 0;
}

CU_pSuite add_suite(const char *name)
{
	CU_pSuite __suite = NULL;
	struct test_item *e = &isp_items[0];

	__suite = CU_add_suite(name, suite_init, suite_cleanup);

	if (!__suite)
		return NULL;

	while (e != NULL) {
		CU_pTest test = NULL;

		if (!e->fn || !e->name)
			break;

		test = CU_add_test(__suite, e->name, e->fn);
		if (!test)
			break;
		e++;
	}


	return __suite;
}

int main(int argc, char **argv)
{
	int ret = RF_ERR_OK;
	int i;
	void *notify = NULL;

	memset(&video_req_get, 0, sizeof(video_req_get));
	memset(&video_req_set, 0, sizeof(video_req_set));
	memset(&audio_req_get, 0, sizeof(audio_req_get));
	memset(&audio_req_set, 0, sizeof(audio_req_set));
	memset(&ispctrl_req_get, 0, sizeof(ispctrl_req_get));
	memset(&ispctrl_req_set, 0, sizeof(ispctrl_req_set));
	memset(&mask_req_get, 0, sizeof(mask_req_get));
	memset(&mask_req_set, 0, sizeof(mask_req_set));
	memset(&md_req_get, 0, sizeof(md_req_get));
	memset(&md_req_set, 0, sizeof(md_req_set));
	memset(&osd_req_get, 0, sizeof(osd_req_get));
	memset(&osd_req_set, 0, sizeof(osd_req_set));

	rts_set_log_ident(argv[0]);
	rts_set_log_mask(RTS_LOG_MASK_CONS);
	rts_set_log_level(1<<RTS_LOG_ERR);
	ret = rf_connect_ubus();
	if (ret) {
		RTS_ERR("open rfmsg error\n");
		return ret;
	}

	notify = rf_register_event_handler(
		CAPTURE_NOTIFY,
		notify_callback);
	if (!notify) {
		RTS_ERR("register rfmsg notify error\n");
		ret = RF_ERR_UBUS_NOTIFY_REGISTER;
		goto out;
	}

	ret = CU_initialize_registry();
	if (ret != CUE_SUCCESS)
		goto out;
	add_suite("test rainbow");
	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();
	CU_cleanup_registry();

	rf_unregister_event_handler(notify);

out:
	rf_disconnect_ubus();
	return CU_get_error();
}
