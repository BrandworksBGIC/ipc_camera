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
#include <string.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsamixer.h>
#include <getopt.h>

struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{0, 0, 0, 0}
};

void print_help_info(void)
{
	printf("DESCRIPTION:\n");
	printf("\tset/get the volume of playback/capture\n");
	printf("USAGE:\n");
	printf("\texample_audio_ctrl [OPTION]...[VALUE]...\n");

	printf("EXAMPLE:\n");
	printf("get volume of playback:\n");
	printf("\texample_audio_ctrl pget\n");
	printf("set volume of playback:\n");
	printf("\texample_audio_ctrl pset 100\n");

	printf("get volume of capture:\n");
	printf("\texample_audio_ctrl cget\n");
	printf("set volume of capture:\n");
	printf("\texample_audio_ctrl cset 100\n");

	printf("playback mute:\n");
	printf("\texample_audio_ctrl pmute\n");
	printf("capture mute:\n");
	printf("\texample_audio_ctrl cmute\n");

	printf("playback unmute:\n");
	printf("\texample_audio_ctrl punmute\n");
	printf("capture unmute:\n");
	printf("\texample_audio_ctrl cunmute\n");
}

int main(int argc, char **argv)
{
	int value, ret = 0;
	int c;

	if (argc < 2) {
		printf("please assign command and parameter\n");
		printf("use -h to get help info\n");
		return RTS_OK;
	}

	while ((c = getopt_long(argc, argv,
				":h", longopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			print_help_info();
			return 0;
		}
	}

	if (strcasecmp("pget", argv[1]) == 0) {
		ret = rts_audio_get_playback_volume(&value);
		if (ret) {
			printf("fail to get playback volume\n");
			goto out;
		}
		printf("the current playback volume is %d\n", value);
	} else if (strcasecmp("pset", argv[1]) == 0) {
		value = atoi(argv[2]);
		ret = rts_audio_set_playback_volume(value);
		if (ret) {
			printf("fail to set playback volume\n");
			goto out;
		}
		ret = rts_audio_get_playback_volume(&value);
		if (ret) {
			printf("fail to get playback volume\n");
			goto out;
		}
		printf("the current playback volume is %d\n", value);
	} else if (strcasecmp("cget", argv[1]) == 0) {
		ret = rts_audio_get_capture_volume(&value);
		if (ret) {
			printf("fail to get capture volume\n");
			goto out;
		}
		printf("the current capture volume is %d\n", value);
	} else if (strcasecmp("cset", argv[1]) == 0) {
		value = atoi(argv[2]);
		ret = rts_audio_set_capture_volume(value);
		if (ret) {
			printf("fail to set capture volume\n");
			goto out;
		}
		ret = rts_audio_get_capture_volume(&value);
		if (ret) {
			printf("fail to get capture volume\n");
			goto out;
		}
		printf("the current capture volume is %d\n", value);
	} else if (strcasecmp("pmute", argv[1]) == 0) {
		ret = rts_audio_playback_mute();
		if (ret) {
			printf("fail to set playback mute\n");
			goto out;
		}
		printf("the current playback is mute\n");
	} else if (strcasecmp("punmute", argv[1]) == 0) {
		ret = rts_audio_playback_unmute();
		if (ret) {
			printf("fail to set playback unmute\n");
			goto out;
		}
		printf("the current playback is unmute\n");
	} else if (strcasecmp("cmute", argv[1]) == 0) {
		ret = rts_audio_capture_mute();
		if (ret) {
			printf("fail to set capture mute\n");
			goto out;
		}
		printf("the current capture is mute\n");
	} else if (strcasecmp("cunmute", argv[1]) == 0) {
		ret = rts_audio_capture_unmute();
		if (ret) {
			printf("fail to set capture unmute\n");
			goto out;
		}
		printf("the current capture is unmute\n");
	} else {
		ret = -1;
		printf("error parameter\n");
	}

out:
	if (ret)
		printf("Fail\n");
	else
		printf("Success\n");

	return ret;
}
