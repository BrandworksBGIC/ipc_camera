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

#include "rf_error.h"
#include "rf_msg_svr.h"
#include "video.h"
#include "audio.h"
#include "ispctrl.h"
#include "mask.h"
#include "md.h"
#include "osd.h"
#include "mjpeg.h"

static void handle_signal(int sig)
{
	if ((sig == SIGINT)
		|| (sig == SIGTERM))
		rf_stop_msgd();
}

int main(int argc, char **argv)
{
	int ret = RF_ERR_OK;

	rts_av_init();

	signal(SIGINT, handle_signal);
	signal(SIGTERM, handle_signal);

	rts_set_log_ident(argv[0]);
	rts_set_log_mask(RTS_LOG_MASK_SYSLOG);
	rts_set_log_level(1<<RTS_LOG_ERR);

	ret = rf_start_audio_capture();
	if (ret) {
		RTS_ERR("start audio capture error\n");
		goto failed;
	}

	ret = rf_start_video_capture();
	if (ret) {
		RTS_ERR("start video capture error\n");
		goto failed;
	}

	ret = rf_init_mjpeg();
	if (ret) {
		RTS_ERR("init ispctrl error\n");
		goto failed;
	}

	ret = rf_init_ispctrl();
	if (ret) {
		RTS_ERR("init ispctrl error\n");
		goto failed;
	}

	ret = rf_init_mask();
	if (ret) {
		RTS_ERR("init mask error\n");
		goto failed;
	}

	ret = rf_init_md();
	if (ret) {
		RTS_ERR("init md error\n");
		goto failed;
	}

	ret = rf_init_osdi();
	if (ret) {
		RTS_ERR("init osd error\n");
		goto failed;
	}

	ret = rf_init_msgd();
	if (ret) {
		RTS_ERR("init msgd error\n");
		goto failed;
	}

	rf_start_msgd();

failed:
	rf_stop_audio_capture();
	rf_release_osdi();
	rf_stop_video_capture();
	rf_release_ispctrl();
	rf_release_mask();
	rf_release_md();
	rf_release_msgd();
	rts_av_release();

	return ret;
}
