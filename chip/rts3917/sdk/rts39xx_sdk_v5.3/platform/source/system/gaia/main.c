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
#include <json-c/json.h>
#include <rtsavapi.h>
#include <rts_log.h>
#include <getopt.h>

char g_path[64] = "/etc/gaia.json";

enum {
	MAX_AES			= 4,
	MAX_AWBS		= 4,
	MAX_AFS			= 4,
	MAX_OTHERS		= 4,

	MAX_PATH_LEN		= RTS_ISP_PATH_LEN,
};

struct ga_sensor_t {
	char path[MAX_PATH_LEN];
};

struct ga_iq_t {
	char path[MAX_PATH_LEN];
};

struct ga_algo_t {
	char path[MAX_PATH_LEN];
	char bind[MAX_PATH_LEN];
};

struct ga_algos_t {
	int ae_num;
	struct ga_algo_t ae[MAX_AES];
	int awb_num;
	struct ga_algo_t awb[MAX_AWBS];
	int af_num;
	struct ga_algo_t af[MAX_AFS];
	int other_num;
	struct ga_algo_t other[MAX_OTHERS];
};

struct ga_conf_t {
	char path[MAX_PATH_LEN];
	int32_t log_behavor;
	struct ga_sensor_t sensor;
	struct ga_iq_t iq;
	struct ga_algos_t algos;
};

static struct ga_conf_t s_conf;

struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"conf_path", optional_argument, NULL, 'c'},
	{0, 0, 0, 0}
};

void print_help(void)
{
	fprintf(stdout, "DESCRIPTION:\n");
	fprintf(stdout, "\tan example for gaia\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "\t-h, help info\n");
	fprintf(stdout, "\t-c, conf_path, default /etc/gaia.json\n");
	fprintf(stdout, "EXAMPLE:\n");
	fprintf(stdout, "\tgaia -c tmp/gaia.json\n");
}

static int parse_arg(int argc, char **argv)
{
	int cs;

	while ((cs = getopt_long(argc, argv,
			"h::c:", longopts, NULL)) != -1) {
		switch (cs) {
		case 'h':
			print_help();
			return 1;
		case 'c':
			strcpy(g_path, optarg);
			break;
		}
	}
	return 0;
}

static int ga_get_conf(struct ga_conf_t *conf)
{
	struct json_object *root_obj = NULL;
	struct json_object *path_obj = NULL;
	struct json_object *log_obj = NULL;
	struct json_object *sensor_obj = NULL;
	struct json_object *iq_obj = NULL;
	struct json_object *algos_obj = NULL;
	int ret;
	int i;

	root_obj = json_object_from_file(g_path);
	if (!root_obj)
		return -EINVAL;

	ret = json_object_object_get_ex(root_obj, "path", &path_obj);
	if (ret)
		strcpy(conf->path, json_object_get_string(path_obj));
	else
		strcpy(conf->path, "/usr/lib/rtsisp");
	conf->log_behavor = RTS_ISP_LOG2LOG;
	ret = json_object_object_get_ex(root_obj, "log", &log_obj);
	if (ret) {
		const char *log_str;

		log_str = json_object_get_string(log_obj);
		if (!strcmp(log_str, "syslog"))
			conf->log_behavor = RTS_ISP_LOG2LOG;
		else if (!strcmp(log_str, "console"))
			conf->log_behavor = RTS_ISP_LOG2CONS;
		else if (!strcmp(log_str, "file"))
			conf->log_behavor = RTS_ISP_LOG2FILE;
		else if (!strcmp(log_str, "kmsg"))
			conf->log_behavor = RTS_ISP_LOG2KMSG;
	}
	ret = json_object_object_get_ex(root_obj, "sensor", &sensor_obj);
	if (!ret)
		goto out;
	strcpy(conf->sensor.path, json_object_get_string(sensor_obj));

	ret = json_object_object_get_ex(root_obj, "iq", &iq_obj);
	if (!ret)
		goto out;
	strcpy(conf->iq.path, json_object_get_string(iq_obj));

	struct json_object *ae_algos_obj = NULL;

	ret = json_object_object_get_ex(root_obj, "algos", &algos_obj);
	if (!ret)
		goto out;

	ret = json_object_object_get_ex(algos_obj, "ae_algos", &ae_algos_obj);
	if (!ret)
		goto out;

	conf->algos.ae_num = json_object_array_length(ae_algos_obj);
	conf->algos.ae_num = conf->algos.ae_num < MAX_AES ?
		conf->algos.ae_num : MAX_AES;
	for (i = 0; i < conf->algos.ae_num; i++) {
		struct json_object *ae_obj = NULL;
		struct json_object *temp_obj = NULL;

		ae_obj = json_object_array_get_idx(ae_algos_obj, i);
		json_object_object_get_ex(ae_obj, "path", &temp_obj);
		strcpy(conf->algos.ae[i].path,
		       json_object_get_string(temp_obj));

		json_object_object_get_ex(ae_obj, "bind", &temp_obj);
		strcpy(conf->algos.ae[i].bind,
			json_object_get_string(temp_obj));
	}

	struct json_object *af_algos_obj = NULL;

	ret = json_object_object_get_ex(algos_obj,
		"af_algos", &af_algos_obj);
	if (!ret)
		goto out;

	conf->algos.af_num = json_object_array_length(af_algos_obj);
	conf->algos.af_num = conf->algos.af_num < MAX_AFS ?
		conf->algos.af_num : MAX_AFS;
	for (i = 0; i < conf->algos.af_num; i++) {
		struct json_object *af_obj = NULL;
		struct json_object *temp_obj = NULL;

		af_obj = json_object_array_get_idx(af_algos_obj, i);
		json_object_object_get_ex(af_obj, "path", &temp_obj);
		strcpy(conf->algos.af[i].path,
		       json_object_get_string(temp_obj));

		json_object_object_get_ex(af_obj, "bind", &temp_obj);
		strcpy(conf->algos.af[i].bind,
		       json_object_get_string(temp_obj));
	}

	struct json_object *awb_algos_obj = NULL;

	ret = json_object_object_get_ex(algos_obj,
		"awb_algos", &awb_algos_obj);
	if (!ret)
		goto out;

	conf->algos.awb_num = json_object_array_length(awb_algos_obj);
	conf->algos.awb_num = conf->algos.awb_num < MAX_AWBS ?
		conf->algos.awb_num : MAX_AWBS;
	for (i = 0; i < conf->algos.awb_num; i++) {
		struct json_object *awb_obj = NULL;
		struct json_object *temp_obj = NULL;

		awb_obj = json_object_array_get_idx(awb_algos_obj, i);
		json_object_object_get_ex(awb_obj, "path", &temp_obj);
		strcpy(conf->algos.awb[i].path,
		       json_object_get_string(temp_obj));

		json_object_object_get_ex(awb_obj, "bind", &temp_obj);
		strcpy(conf->algos.awb[i].bind,
		       json_object_get_string(temp_obj));
	}

	struct json_object *other_algos_obj = NULL;

	ret = json_object_object_get_ex(algos_obj,
		"other_algos", &other_algos_obj);
	if (!ret)
		goto out;

	conf->algos.other_num = json_object_array_length(other_algos_obj);
	conf->algos.other_num = conf->algos.other_num < MAX_OTHERS ?
		conf->algos.other_num : MAX_OTHERS;
	for (i = 0; i < conf->algos.other_num; i++) {
		struct json_object *other_obj = NULL;
		struct json_object *temp_obj = NULL;

		other_obj = json_object_array_get_idx(other_algos_obj, i);
		json_object_object_get_ex(other_obj, "path", &temp_obj);
		strcpy(conf->algos.other[i].path,
		       json_object_get_string(temp_obj));

		json_object_object_get_ex(other_obj, "bind", &temp_obj);
		strcpy(conf->algos.other[i].bind,
		       json_object_get_string(temp_obj));
	}

out:
	if (root_obj)
		json_object_put(root_obj);

	return ret > 0 ? 0 : -EINVAL;
}

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
	int id;
	int i;
	struct ga_conf_t *conf = &s_conf;

	id = RTS_ISP_ALGO_AE_ID0;
	for (i = 0; i < conf->algos.ae_num; i++) {
		char *path = conf->algos.ae[i].path;
		char *bind = conf->algos.ae[i].bind;
		int bind_enable = 0;

		if (bind && !strcmp(bind, "enable"))
			bind_enable = 1;

		ret = register_algo(id++,
			path, bind_enable);
		if (ret)
			goto out;
	}

	id = RTS_ISP_ALGO_AF_ID0;
	for (i = 0; i < conf->algos.af_num; i++) {
		char *path = conf->algos.af[i].path;
		char *bind = conf->algos.af[i].bind;
		int bind_enable = 0;

		if (bind && !strcmp(bind, "enable"))
			bind_enable = 1;

		ret = register_algo(id++,
			path, bind_enable);
		if (ret)
			goto out;
	}

	id = RTS_ISP_ALGO_AWB_ID0;
	for (i = 0; i < conf->algos.awb_num; i++) {
		char *path = conf->algos.awb[i].path;
		char *bind = conf->algos.awb[i].bind;
		int bind_enable = 0;

		if (bind && !strcmp(bind, "enable"))
			bind_enable = 1;

		ret = register_algo(id++,
			path, bind_enable);
		if (ret)
			goto out;
	}

	id = RTS_ISP_ALGO_OTHER_ID0;
	for (i = 0; i < conf->algos.other_num; i++) {
		char *path = conf->algos.other[i].path;
		char *bind = conf->algos.other[i].bind;
		int bind_enable = 0;

		if (bind && !strcmp(bind, "enable"))
			bind_enable = 1;

		ret = register_algo(id++,
			path, bind_enable);
		if (ret)
			goto out;
	}

out:
	return ret;
}

static const char *get_sensor_path(const char *path, const char *name)
{
	int ret;
	static char snr_path[MAX_PATH_LEN];

	if (name[0] == '/')
		return name;
	ret = snprintf(snr_path, sizeof(snr_path), "%s/sensors/libsensor_%s.so",
		 path, name);
	if (ret < 0 || ret >= sizeof(snr_path))
		return NULL;
	return snr_path;
}

static int register_sensor_specified(const char *path, const char *name)
{
	struct rts_isp_sensor sensor;

	if (!path)
		return RTS_ISP_EINVAL;

	sensor.path = get_sensor_path(path, name);
	return rts_av_isp_register_sensor(&sensor);
}

static int register_sensor_auto(const char *path)
{
	int i;
	int ret;
	int id = -RTS_ISP_EINVAL;
	glob_t globbuf;
	char snr_path[MAX_PATH_LEN];

	ret = snprintf(snr_path, sizeof(snr_path), "%s/sensors/libsensor_*.so",
		       path);
	if (ret < 0 || ret >= sizeof(snr_path))
		return -RTS_ISP_EINVAL;
	ret = glob(snr_path, 0, NULL, &globbuf);
	if (ret)
		return -errno;
	for (i = 0; i < globbuf.gl_pathc; i++) {
		struct rts_isp_sensor sensor;

		sensor.path = globbuf.gl_pathv[i];
		id = rts_av_isp_register_sensor(&sensor);
		if (id < 0)
			break;
		ret = rts_av_isp_check_sensor(ISP0, id);
		if (!ret)
			break;
		rts_av_isp_unregister_sensor(id);
		id = -RTS_ISP_EINVAL;
	}
	globfree(&globbuf);

	return id;
}

static const char *get_iq_path(const char *path, const char *name)
{
	int ret;
	static char iq_path[MAX_PATH_LEN];

	if (name[0] == '/')
		return name;
	ret = snprintf(iq_path, sizeof(iq_path), "%s/iqs/%s.bin", path, name);
	if (ret < 0 || ret >= sizeof(iq_path))
		return NULL;
	return iq_path;
}

static int register_iq_specified(const char *path, const char *name)
{
	if (!path || !name)
		return -RTS_ISP_EINVAL;
	return rts_av_isp_register_iq(ISP0, get_iq_path(path, name));
}

static const char *get_sensor_name(int sensor_id)
{
	int ret;
	void *begin;
	void *end;
	size_t len;
	struct rts_isp_sensor sensor;
	static char name[RTS_ISP_NAME_LEN];

	ret = rts_av_isp_get_sensor(sensor_id, &sensor);
	if (ret)
		return NULL;
	begin = strstr(sensor.path, "libsensor_");
	if (!begin)
		return NULL;
	begin += strlen("libsensor_");
	end = strstr(begin, ".so");
	if (!end)
		return NULL;
	len = end - begin;
	memcpy(name, begin, len);
	*(name + len) = '\0';

	return name;
}

static int register_iq_auto(const char *path, int sensor_id)
{
	int ret;
	const char *name;

	name = get_sensor_name(sensor_id);
	if (!name)
		return -RTS_ISP_EINVAL;
	ret = rts_av_isp_register_iq(ISP0, get_iq_path(path, name));
	if (!ret)
		return RTS_ISP_OK;
	return rts_av_isp_register_iq(ISP0, get_iq_path(path, "default"));
}

static int register_sensor_iq(void)
{
	int ret;
	int sensor_id;
	struct ga_conf_t *conf = &s_conf;

	if (strcmp(conf->sensor.path, "auto"))
		sensor_id = register_sensor_specified(conf->path,
						      conf->sensor.path);
	else
		sensor_id = register_sensor_auto(conf->path);
	if (sensor_id < 0) {
		ret = sensor_id;
		rts_isp_perror(ret, "register sensor(%s) fail",
			       conf->sensor.path);
		goto out;
	}
	ret = rts_av_isp_bind_sensor(ISP0, sensor_id);
	if (ret) {
		rts_isp_perror(ret, "bind sensor(%s) fail", conf->sensor.path);
		goto out;
	}

	if (strcmp(conf->iq.path, "auto"))
		ret = register_iq_specified(conf->path, conf->iq.path);
	else
		ret = register_iq_auto(conf->path, sensor_id);
	if (ret) {
		rts_isp_perror(ret, "register iq(%s) fail", conf->iq.path);
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

	if (parse_arg(argc, argv))
		return 0;

	ret = ga_get_conf(&s_conf);
	if (ret) {
		rts_isp_perror(ret, "get conf fail");
		goto out;
	}

	if (s_conf.log_behavor == RTS_ISP_LOG2LOG)
		openlog(argv[0], LOG_PID, LOG_USER);
	rts_av_isp_open_log(s_conf.log_behavor);

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
