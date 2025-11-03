/*
 *  Copyright (C) 2021 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <glob.h>
#include <syslog.h>
#include <rtsavisp.h>

static int register_algo(enum rts_isp_algo_id id,
	const char *path, int bind)
{
	int ret;
	struct rts_isp_algo algo;

	if (!path)
		return -RTS_ISP_EINVAL;

	algo.id = id;
	algo.path = path;

	ret = rts_av_isp_register_algo(&algo);
	if (ret < 0) {
		rts_isp_perror(ret,
		"register algo(id:%d path:%s) fail",
		id, path);
		return ret;
	}
	if (bind) {
		ret = rts_av_isp_bind_algo(ISP0, id);
		if (ret) {
			rts_isp_perror(ret, "bind algo(id:%d, path:%s) fail",
				       id, path);
			return ret;
		}
	}

	return RTS_ISP_OK;
}

static int register_all_algos(void)
{
	int ret;

	ret = register_algo(RTS_ISP_ALGO_AE_ID0,
			    "/usr/lib/rtsisp/algos/librts_algo_ae.so",
			    RTS_ISP_TRUE);
	if (ret)
		return ret;
	ret = register_algo(RTS_ISP_ALGO_AWB_ID0,
			    "/usr/lib/rtsisp/algos/librts_algo_awb.so",
			    RTS_ISP_TRUE);
	if (ret)
		return ret;
	ret = register_algo(RTS_ISP_ALGO_AF_ID0,
			    "/usr/lib/rtsisp/algos/librts_algo_af.so",
			    RTS_ISP_TRUE);
	if (ret)
		return ret;

	return register_algo(RTS_ISP_ALGO_OTHER_ID0,
			    "/usr/lib/rtsisp/algos/librts_algo_other.so",
			    RTS_ISP_TRUE);
}

static int register_sensor_specified(const char *path)
{
	struct rts_isp_sensor sensor;

	if (!path)
		return RTS_ISP_EINVAL;

	sensor.path = path;
	return rts_av_isp_register_sensor(&sensor);
}

static int register_iq_specified(const char *path)
{
	if (!path)
		return -RTS_ISP_EINVAL;
	return rts_av_isp_register_iq(ISP0, path);
}

static int register_sensor_iq(void)
{
	int ret;
	int sensor_id;
	char snr_path[128] = {0};
	char default_iq_path[] = "/usr/lib/rtsisp/iqs/default.bin";
	char specific_iq_path[128] = {0};
	char *iq_path = default_iq_path;

	snprintf(snr_path, 128, "/usr/lib/rtsisp/sensors/libsensor_%s.so",
			SENSOR_SUPPORT);
	snprintf(specific_iq_path, 128, "/usr/lib/rtsisp/iqs/%s.bin",
			SENSOR_SUPPORT);

	sensor_id = register_sensor_specified(snr_path);
	if (sensor_id < 0) {
		ret = sensor_id;
		rts_isp_perror(ret, "register sensor(%s) fail", snr_path);
		goto out;
	}
	ret = rts_av_isp_bind_sensor(ISP0, sensor_id);
	if (ret) {
		rts_isp_perror(ret, "bind sensor(%s) fail", snr_path);
		goto out;
	}

	if (!access(specific_iq_path, F_OK))
		iq_path = specific_iq_path;

	ret = register_iq_specified(iq_path);
	if (ret) {
		rts_isp_perror(ret, "register iq(%s) fail", iq_path);
		goto out;
	}

out:
	return ret;
}

static void signal_handle(int signo)
{
	rts_av_isp_stop();
}

int main(int argc, char **argv)
{
	int ret = 0;

	signal(SIGTERM, signal_handle);
	signal(SIGINT, signal_handle);
	signal(SIGPIPE, SIG_IGN);

	openlog(argv[0], LOG_PID, LOG_USER);

	ret = rts_av_isp_init();
	if (ret) {
		rts_isp_perror(ret, "init isp fail");
		goto out;
	}

	ret = register_sensor_iq();
	if (ret) {
		rts_isp_perror(ret, "register sensor iq fail");
		goto out;
	}

	ret = register_all_algos();
	if (ret) {
		rts_isp_perror(ret, "register all algos fail");
		goto out;
	}

	ret = rts_av_isp_start();
	if (ret)
		rts_isp_perror(ret, "start isp fail");

out:
	rts_av_isp_cleanup();
	return ret;
}
