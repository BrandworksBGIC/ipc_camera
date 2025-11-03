/*
 * Realtek Semiconductor Corp.
 * Copyright (C) 2017 Grant Shen <grant_shen@realsil.com.cn>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <glob.h>
#include <rtsavisp.h>

#define SENSOR_PATH "/lib/rtsisp/sensors/libsensor_*.so"
#define IQ_PATH  "/lib/rtsisp/iqs/default.bin"
#define ALGO_DIR "/lib/rtsisp/algos"
#define ALGO_AE_ID RTS_ISP_ALGO_AE_ID0
#define ALGO_AE_PATH ALGO_DIR"/librts_algo_ae.so"
#define ALGO_AWB_ID RTS_ISP_ALGO_AWB_ID0
#define ALGO_AWB_PATH ALGO_DIR"/librts_algo_awb.so"
#define ALGO_AF_ID RTS_ISP_ALGO_AF_ID0
#define ALGO_AF_PATH ALGO_DIR"/librts_algo_af.so"
#define ALGO_OTHER_ID RTS_ISP_ALGO_OTHER_ID0
#define ALGO_OTHER_PATH ALGO_DIR"/librts_algo_other.so"

static void isp_signal_handle(int signo)
{
	rts_av_isp_stop();
}

static int register_algo(enum rts_isp_algo_id id, char *path)
{
	int ret;
	struct rts_isp_algo algo;

	if (!path)
		return -RTS_ISP_EINVAL;

	algo.id = id;
	algo.path = path;

	ret = rts_av_isp_register_algo(&algo);
	if (ret < 0)
		return ret;
	ret = rts_av_isp_bind_algo(ISP0, id);
	if (ret)
		return ret;

	return RTS_ISP_OK;
}

static int register_all_algos(void)
{
	int ret;

	ret = register_algo(ALGO_AE_ID, ALGO_AE_PATH);
	if (ret)
		goto out;
	ret = register_algo(ALGO_AWB_ID, ALGO_AWB_PATH);
	if (ret)
		goto out;
	ret = register_algo(ALGO_AF_ID, ALGO_AF_PATH);
	if (ret)
		goto out;
	ret = register_algo(ALGO_OTHER_ID, ALGO_OTHER_PATH);
out:
	if (ret)
		rts_isp_perror(ret, "register algos fail");
	return ret;
}

static int register_sensor(uint32_t isp_id)
{
	int i;
	int ret;
	int id = -RTS_ISP_EINVAL;
	glob_t globbuf;

	ret = glob(SENSOR_PATH, 0, NULL, &globbuf);
	if (ret)
		return -errno;
	for (i = 0; i < globbuf.gl_pathc; i++) {
		struct rts_isp_sensor sensor;

		sensor.path = globbuf.gl_pathv[i];
		id = rts_av_isp_register_sensor(&sensor);
		if (id < 0)
			break;
		ret = rts_av_isp_check_sensor(isp_id, id);
		if (!ret)
			break;
		rts_av_isp_unregister_sensor(id);
		id = -RTS_ISP_EINVAL;
	}
	globfree(&globbuf);

	return id;
}

static int register_sensor_iq(void)
{
	int ret = RTS_ISP_OK;
	int sensor_id;

	sensor_id = register_sensor(ISP0);
	if (sensor_id < 0) {
		ret = sensor_id;
		goto out;
	}
	ret = rts_av_isp_bind_sensor(ISP0, sensor_id);
	if (ret)
		goto out;
	ret = rts_av_isp_register_iq(ISP0, IQ_PATH);
out:
	if (ret)
		rts_isp_perror(ret, "register sensor iq fail");
	return ret;
}

int main(int argc, char *argv[])
{
	int ret = 0;

	signal(SIGTERM, isp_signal_handle);
	signal(SIGINT, isp_signal_handle);
	signal(SIGPIPE, SIG_IGN);

	ret = rts_av_isp_init();
	if (ret)
		goto exit;

	ret = register_sensor_iq();
	if (ret)
		goto exit;
	ret = register_all_algos();
	if (ret)
		goto exit;

	ret = rts_av_isp_start();
exit:
	if (ret)
		rts_isp_perror(ret, "rts isp fail");
	rts_av_isp_cleanup();

	if (ret)
		printf("Fail\n");
	else
		printf("Success\n");

	return -ret;
}
