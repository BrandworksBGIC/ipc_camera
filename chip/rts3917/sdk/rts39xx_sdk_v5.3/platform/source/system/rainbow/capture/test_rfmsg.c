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

#include "rf_msg.h"

struct rf_video_req video_req;
struct rf_audio_req audio_req;
struct rf_ispctrl_req ispctrl_req;
struct rf_mask_req mask_req;
struct rf_md_req md_req;
struct rf_osd_req osd_req;

static int notify_callback(
	struct rf_notify_st *notify)
{
	printf("get notify event: id %08x time %x\n",
		notify->id, (unsigned int)notify->time);
	return 0;
}

int main(int argc, char **argv)
{
	int ret = RF_ERR_OK;
	int i;
	void *notify = NULL;

	memset(&video_req, 0, sizeof(video_req));
	memset(&audio_req, 0, sizeof(audio_req));
	memset(&ispctrl_req, 0, sizeof(ispctrl_req));
	memset(&mask_req, 0, sizeof(mask_req));
	memset(&md_req, 0, sizeof(md_req));
	memset(&osd_req, 0, sizeof(osd_req));

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

	for (i=0; i<RF_VIDEO_MSG_NUM; i++) {
		ret = rf_send_msg(
			RF_VIDEO_MSG_BASE + i,
			&video_req,
			sizeof(video_req));
		if (ret)
			RTS_ERR("send rf msg %08x ret %d\n",
				RF_VIDEO_MSG_BASE + i, ret);
	}

	for (i=0; i<RF_AUDIO_MSG_NUM; i++) {
		ret = rf_send_msg(
			RF_AUDIO_MSG_BASE + i,
			&audio_req,
			sizeof(audio_req));
		if (ret)
			RTS_ERR("send rf msg %08x ret %d\n",
				RF_AUDIO_MSG_BASE + i, ret);
	}

	for (i=0; i<RF_ISPCTRL_MSG_NUM; i++) {
		ret = rf_send_msg(
			RF_ISPCTRL_MSG_BASE + i,
			&ispctrl_req,
			sizeof(ispctrl_req));
		if (ret)
			RTS_ERR("send rf msg %08x ret %d\n",
				RF_ISPCTRL_MSG_BASE + i, ret);
	}

	for (i=0; i<RF_MASK_MSG_NUM; i++) {
		ret = rf_send_msg(
			RF_MASK_MSG_BASE + i,
			&mask_req,
			sizeof(mask_req));
		if (ret)
			RTS_ERR("send rf msg %08x ret %d\n",
				RF_MASK_MSG_BASE + i, ret);
	}

	for (i=0; i<RF_MD_MSG_NUM; i++) {
		ret = rf_send_msg(
			RF_MD_MSG_BASE + i,
			&md_req,
			sizeof(md_req));
		if (ret)
			RTS_ERR("send rf msg %08x ret %d\n",
				RF_MD_MSG_BASE + i, ret);
	}

	for (i=0; i<RF_OSD_MSG_NUM; i++) {
		ret = rf_send_msg(
			RF_OSD_MSG_BASE + i,
			&osd_req,
			sizeof(osd_req));
		if (ret)
			RTS_ERR("send rf msg %08x ret %d\n",
				RF_OSD_MSG_BASE + i, ret);
	}

	rf_unregister_event_handler(notify);

out:
	rf_disconnect_ubus();

	return ret;
}
