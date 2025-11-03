/*
 *  Copyright (C) 2021 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU LESSER General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <sys/inotify.h>
#include <dirent.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <json-c/json.h>

#include "sysconf.h"
#include "entropy.h"
#include "utils.h"


#define FS '%' /* format specifiers */

struct ctrl_element {
	char ctrl;
	union {
		void *p;
		int i;
		double f;
	} data;
};

#ifndef INOTIFY_EVENT_SIZE
#define INOTIFY_EVENT_SIZE (sizeof(struct inotify_event) + NAME_MAX + 16)
#endif

struct metadata_object {
	void *root;
	int fd;
	int dirty;
	int fd_inotify;
	int wd_inotify;
};

static int __get_file_path(const char *pre, const char *domain, char *path, int
		len)
{

	if (domain)
		snprintf(path, len, "%s/%s.json", pre, domain);
	else
		snprintf(path, len, "%s/rts_common.json", pre);

	return 0;
}

static int get_filepath_etc(const char *domain, char *path, int len)
{
	return __get_file_path(CONF_DIR_ETC, domain, path, len);
}

static int get_filepath(const char *domain, char *path, int len)
{
	return __get_file_path(CONF_DIR_RAMFS, domain, path, len);
}

static int get_filepath_jffs2(const char *domain, char *path, int len)
{
	return __get_file_path(CONF_DIR_JFFS2, domain, path, len);
}

static int get_ctrl_element_num(const char *fmt)
{
	const char *pos = fmt;
	int num = 0;

	if (!fmt)
		return 0;

	for (pos = fmt; *pos != '\0'; pos++) {
		if (FS != *pos)
			continue;
		pos++;

		if (FS == *pos)
			continue;
		else if ('\0' == *pos)
			break;
		else
			num++;
	}

	return num;
}

static int get_ctrl_elements(const char *fmt,
		struct ctrl_element *ctrls, int num,
		va_list vl)
{
	const char *pos = fmt;
	struct ctrl_element *ctrl = NULL;

	ctrl = &ctrls[0];
	for (pos = fmt; *pos != '\0'; pos++) {
		if (FS != *pos)
			continue;
		pos++;

		if (FS == *pos)
			continue;
		if ('\0' == *pos)
			break;
		ctrl->ctrl = *pos;
		switch (*pos) {
		case 'i':
		case 'l':
		case 'd':
			ctrl->data.i = va_arg(vl, int);
			break;
		case 'r':
			ctrl->data.p = NULL;
			break;
		case 'f':
			ctrl->data.f = va_arg(vl, double);
			break;
		default:
			ctrl->data.p = va_arg(vl, void *);
		}
		ctrl++;
	}

	return 0;
}

static int verify_scanf_ctrl_elements(struct ctrl_element *ctrls, int num)
{
	int i = 0;

	for (i = 0; i < num; i++) {
		char c = tolower(ctrls[i].ctrl);
		if ('i' == c || 'l' == c || 'k' == c) {
			if (i >= (num - 1))
				return -E_FS_INVALID;
		}

		if ('s' == c || 'a' == c || 'p' == c) {
			if (i <= 0)
				return -E_FS_INVALID;
			char prev = tolower(ctrls[i - 1].ctrl);
			if ('l' != prev)
				return -E_FS_INVALID;
			else
				continue;
		}
	}

	return 0;
}

static int rts_conf_get_array_form_json(struct json_object *array,
		char *buf, int size, uint32_t type)
{
	struct array_list *al = NULL;
	struct json_object **obj = NULL;
	int ret = -E_INVALID_OBJ_TYPE;

	if(json_type_array != json_object_get_type(array))
		return -E_INVALID_OBJ_TYPE;

	al = json_object_get_array(array);
	if (!al)
		return -E_INVALID_OBJ_TYPE;

	obj = (struct json_object **) al->array;

	switch (type) {
	case json_type_double:
	{
		double *pos = (double *) buf;
		int length = size / sizeof(*pos);

		length = (length < al->length) ? length : al->length;
		pos = (double*) buf;
		for(int i = 0; i < length; i++)
			*pos++ = json_object_get_double(*obj++);

		ret = length * sizeof(*pos);
		break;
	}
	case json_type_int:
	{
		int *pos = (int *) buf;
		int length = size / sizeof(*pos);

		length = (length < al->length) ? length : al->length;
		pos = (int *) buf;
		for(int i = 0; i < length; i++)
			*pos++ = json_object_get_int(*obj++);

		ret = length * sizeof(*pos);
		break;
	}
	default:
		ret = -E_INVALID_OBJ_TYPE;
		break;
	}

	return ret;
}

static int rts_conf_get_value_from_json(struct ctrl_element *ctrls, int num,
		struct json_object *root)
{
	struct json_object *prev = root;
	int ret = 0;
	int i = 0;
	int length = 0;

	for (i = 0; i < num; i++) {
		char ctrl = tolower(ctrls[i].ctrl);
		struct json_object *o;
		int get = 0;

		switch (ctrl) {
		case 'k':
		{
			char *key = ctrls[i].data.p;
			get = json_object_object_get_ex(prev, key, &o);
			if (!get) {
				ret = -E_KEY_NOT_FOUND;
				goto out;
			}
			prev = o;
			break;
		}
		case 'i':
		{
			int idx = ctrls[i].data.i;
			o = json_object_array_get_idx(prev, idx);
			if (!o) {
				ret = -E_FS_INVALID;
				goto out;
			}
			prev = o;
			break;
		}
		case 'z':
		{
			int size = json_object_array_length(prev);

			*((int *) (ctrls[i].data.p)) = size;
			break;
		}
		case 'l':
		{
			length = ctrls[i].data.i;
			if (length <= 0) {
				ret = -EINVAL;
				goto out;
			}
			break;
		}
		case 's':
		{
			char *buf = ctrls[i].data.p;
			const char *value = NULL;

			value = json_object_get_string(prev);
			snprintf(buf, length, "%s", value);
			break;
		}
		case 'd':
		{
			int value = json_object_get_int(prev);
			*((int32_t *) (ctrls[i].data.p)) = value;

			break;
		}
		case 'f':
		{
			double value = (double) json_object_get_double(prev);
			*((double*) (ctrls[i].data.p)) = value;

			break;
		}
		case 'a':
		{
			ret = rts_conf_get_array_form_json(prev,
				ctrls[i].data.p, length, json_type_int);
			break;
		}
		case 'p':
		{
			ret = rts_conf_get_array_form_json(prev,
				ctrls[i].data.p, length, json_type_double);
			break;
		}
		default:
			ret = -E_FS_INVALID;
			goto out;
		}

	}

out:
	return ret;

}

static int create_ctrl_elements(const char *fmt, struct ctrl_element **__ctrls,
		va_list vl)
{
	struct ctrl_element *ctrls = NULL;
	int num = get_ctrl_element_num(fmt);

	*__ctrls = NULL;

	ctrls = calloc(num, sizeof(*ctrls));
	if (!ctrls)
		return -ENOMEM;

	get_ctrl_elements(fmt, ctrls, num, vl);

	*__ctrls = ctrls;
	return num;
}

static int rts_conf_vscanf(char *domain, const char *fmt, va_list vl)
{
	struct json_object *root = NULL;
	char filepath[128] = {0};
	struct ctrl_element *ctrls = NULL;
	int fd = 0;
	int num = 0;
	int ret = 0;

	get_filepath(domain, filepath, sizeof(filepath));

	fd = open(filepath, O_RDONLY);
	if (fd < 0)
		return -errno;

	ret = sc_lock_file(fd, F_RDLCK, 1);
	if (ret < 0) {
		close(fd);
		return -errno;
	}
	root = json_object_from_file(filepath);
	if (!root) {
		ret = -EIO;
		goto out;
	}

	num = create_ctrl_elements(fmt, &ctrls, vl);
	if (!ctrls) {
		ret = num;
		goto out;
	}


	ret = verify_scanf_ctrl_elements(ctrls, num);
	if (ret)
		goto out;

	ret = rts_conf_get_value_from_json(ctrls, num, root);

out:
	if (ctrls)
		free(ctrls);

	if (root)
		json_object_put(root);

	sc_unlock_file(fd);
	close(fd);

	return ret;
}

/* rts_conf_scanf - scan param from json file
 *
 * @domain: domain name of config file
 * @fmt: specifiers the number of varable params and param type
 *
 * specifications		type			desc
 *	%k			const char*	the key string
 *	%i			int		array index
 *	%z			int*		get array size
 *	%l			int		size of the next param
 *	%s			char*		scan string
 *	%d			int*		scan integer
 *	%a			int*		scan integer array
 *	%f			double*		scan double
 *	%p			double*		scan double array
 *
 */
int rts_conf_scanf(char *domain, const char *fmt, ...)
{
	int ret = 0;
	va_list vl;

	va_start(vl, fmt);

	ret = rts_conf_vscanf(domain, fmt, vl);
	va_end(vl);

	return ret;
}

static int rts_conf_put_value_to_json(struct ctrl_element *ctrls, int num,
		struct json_object *root)
{
	struct json_object *prev = root;
	struct json_object *curr = root;
	const char *key = NULL;
	char prev_ctrl = '\0';
	int ret = 0;
	int index = 0;
	int i = 0;

	for (i = 0; i < num; i++) {
		char ctrl = tolower(ctrls[i].ctrl);
		struct json_object *o;
		int get = 0;

		switch (ctrl) {
		case 'k':
		{
			const char *k = ctrls[i].data.p;

			if (!curr) {
				ret = -E_FS_INVALID;
				goto out;
			}

			get = json_object_object_get_ex(curr, k, &o);
			if (!get) {
				char next_ctrl = tolower(ctrls[i + 1].ctrl);
				if ('i' == next_ctrl)
					o = json_object_new_array();
				else
					o = json_object_new_object();
				json_object_object_add(curr, k, o);
			}
			prev = curr;
			curr = o;
			key = k;
			break;
		}

		case 'j':
		{
			struct json_object *obj = NULL;

			obj = json_tokener_parse(ctrls[i].data.p);
			if (!obj) {
				ret = -E_FS_INVALID;
				goto out;
			}

			if ('k' == prev_ctrl) {
				if (!key) {
					ret = -E_FS_INVALID;
					goto out;
				}

				json_object_object_add(prev, key, obj);
			} else if ('i' == prev_ctrl) {
				json_object_array_put_idx(prev, index, obj);
			} else {
				ret = -E_FS_INVALID;
				goto out;
			}
			break;
		}

		case 'i':
		{
			int idx = ctrls[i].data.i;
			o = json_object_array_get_idx(curr, idx);
			if (!o) {
				o = json_object_new_object();
				json_object_array_put_idx(curr, idx, o);
			}

			prev = curr;
			curr = o;
			index = idx;
			break;
		}

		case 's':
		{
			char *buf = ctrls[i].data.p;

			if ('k' == prev_ctrl) {
				if (!key) {
					ret = -E_FS_INVALID;
					goto out;
				}

				json_object_object_add(prev, key,
						json_object_new_string(buf));
			} else if ('i' == prev_ctrl) {
				json_object_array_put_idx(prev, index,
						json_object_new_string(buf));
			} else {
				ret = -E_FS_INVALID;
				goto out;
			}
			break;
		}

		case 'd':
		{
			int value = ctrls[i].data.i;

			if ('k' == prev_ctrl) {
				json_object_object_add(prev, key,
					json_object_new_int(value));
			} else if ('i' == prev_ctrl) {
				json_object_array_put_idx(prev, index,
					json_object_new_int(value));
			} else {
				ret = -E_FS_INVALID;
				goto out;
			}

			break;
		}

		case 'f':
		{
			double value = ctrls[i].data.f;

			if ('k' == prev_ctrl) {
				json_object_object_add(prev, key,
					json_object_new_double(value));
			} else if ('i' == prev_ctrl) {
				json_object_array_put_idx(prev, index,
					json_object_new_double(value));
			} else {
				ret = -E_FS_INVALID;
				goto out;
			}

			break;
		}

		case 'r':
		{
			if ('k' == prev_ctrl) {
				json_object_object_del(prev, key);
			} else if ('i' == prev_ctrl) {
				json_object_array_put_idx(prev, index, NULL);
			} else {
				if (!key) {
					ret = -E_FS_INVALID;
					goto out;
				}
			}

			break;
		}

		default:
			ret = -E_FS_INVALID;
			goto out;
		}

		prev_ctrl = ctrl;
	}

out:
	return ret;

}

static int verify_printf_ctrl_elements(struct ctrl_element *ctrls, int num)
{
	char prev = '\0';
	int i = 0;

	for (i = 0; i < num; i++) {
		char c = tolower(ctrls[i].ctrl);
		switch (c) {
		case 'i':
		{
			if ('k' != prev)
				return -E_FS_INVALID;
			if (i <= 0 || i >= (num - 1))
				return -E_FS_INVALID;
			break;
		}
		case 'd':
		case 'f':
		case 's':
		case 'j':
		{
			if (i != (num - 1))
				return -E_FS_INVALID;

			if ('k' != prev && 'i' != prev)
				return -E_FS_INVALID;
			break;
		}
		case 'r':
		{
			if (i <= 0 || i < (num - 1))
				return -E_FS_INVALID;
			if ('k' != prev && 'i' != prev)
				return -E_FS_INVALID;
			break;

		}
		case 'k':
			if (i >= (num - 1))
				return -E_FS_INVALID;
			break;
		default:
			return -E_FS_INVALID;
		}

		prev = c;
	}

	return 0;
}

static int rts_conf_vprintf(char *domain, const char *fmt, va_list vl)
{
	struct json_object *root = NULL;
	char filepath[128] = {0};
	struct ctrl_element *ctrls = NULL;
	int fd = 0;
	int num = 0;
	int ret = 0;

	get_filepath(domain, filepath, sizeof(filepath));
	fd = open(filepath, O_WRONLY | O_CREAT, 0644);
	if (fd < 0)
		return -errno;

	ret = sc_lock_file(fd, F_WRLCK, 1);
	if (ret < 0) {
		close(fd);
		return -errno;
	}

	root = json_object_from_file(filepath);
	if (!root) {
		struct stat st;

		stat(filepath, &st);
		if (st.st_size == 0) {
			root = json_object_new_object();
		} else {
			ret = -EIO;
			goto out;
		}
	}

	num = create_ctrl_elements(fmt, &ctrls, vl);
	if (!ctrls) {
		ret = num;
		goto out;
	}

	ret = verify_printf_ctrl_elements(ctrls, num);
	if (ret)
		goto out;

	ret = rts_conf_put_value_to_json(ctrls, num, root);
	if (ret)
		goto out;

	json_object_to_file_ext(filepath, root, JSON_C_TO_STRING_PRETTY);

out:
	if (ctrls)
		free(ctrls);

	if (root)
		json_object_put(root);

	sc_unlock_file(fd);
	close(fd);

	return ret;
}

/* rts_conf_printf - print param to json file
 *
 * @domain: domain name of config file
 * @fmt: specifiers the number of varable params and param type
 *
 * specifications		type			desc
 *	%k			const char*	the key string
 *	%i			int		array index
 *	%s			char*		print string
 *	%d			int*		print integer
 *	%f			float*		print float
 *	%r			N/A		delete param
 *	%j			const char *	the json string
 *
 */
int rts_conf_printf(char *domain, const char *fmt, ...)
{
	int ret = 0;
	va_list vl;

	va_start(vl, fmt);

	ret = rts_conf_vprintf(domain, fmt, vl);
	va_end(vl);

	return ret;
}

int rts_conf_printf_vl(char *domain, const char *fmt, va_list vl)
{
	return rts_conf_vprintf(domain, fmt, vl);
}

int rts_conf_reset(char *domain)
{
	int ret = 0;
	struct stat st;
	char dst[NAME_MAX] = {0};
	char src[NAME_MAX] = {0};

	get_filepath_jffs2(domain, dst, sizeof(dst));
	if (!strcmp(domain, CFG_DOMAIN_ONVIF))
		return stat(dst, &st) ? 0 : unlink(dst);

	get_filepath_etc(domain, src, sizeof(src));

	ret = sync_file(dst, src, 1);
	if (ret < 0)
		return ret;
	/*
	 * The inotify mechanism cannot catch the file change via sendfile().
	 * so we copied conf file twice : to /var/conf/ and /usr/conf/
	 */
	get_filepath(domain, dst, sizeof(dst));
	return sync_file(dst, src, 1);
}

static int rts_conf_vscanf_ex(void *metadata, const char *fmt, va_list vl)
{
	struct ctrl_element *ctrls = NULL;
	int num = 0;
	int ret = 0;

	num = create_ctrl_elements(fmt, &ctrls, vl);
	if (!ctrls) {
		ret = num;
		goto out;
	}

	ret = verify_scanf_ctrl_elements(ctrls, num);
	if (ret)
		goto out;

	void *root = ((struct metadata_object *)metadata)->root;

	ret = rts_conf_get_value_from_json(ctrls, num,
			(struct json_object *)root);

out:
	if (ctrls)
		free(ctrls);

	return ret;
}

int rts_conf_scanf_ex(void *metadata, const char *fmt, ...)
{
	if (metadata == NULL)
		return -1;

	int ret = 0;
	va_list vl;

	va_start(vl, fmt);

	ret = rts_conf_vscanf_ex(metadata, fmt, vl);
	va_end(vl);

	return ret;
}

static int __init_inotify(struct metadata_object *metadata, const char *fn)
{
	metadata->wd_inotify = -1;

	metadata->fd_inotify = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
	if (metadata->fd_inotify < 0)
		return -EIO;

	metadata->wd_inotify  = inotify_add_watch(metadata->fd_inotify,
						fn, IN_MODIFY | IN_DELETE);
	if (metadata->wd_inotify < 0)
		goto fail;

	return 0;
fail:
	if (metadata->fd_inotify >= 0)
		close(metadata->fd_inotify);
	if (metadata->wd_inotify >= 0)
		close(metadata->wd_inotify);

	return -EIO;
}

static int __open_file_with_lock(const char *fn, int flag)
{
	int fd = -1;
	int ret = -1;
	int o_flag = 0;
	int l_flag = 0;


	if (flag == SHARED_ACCESS) {
		o_flag = O_RDONLY;
		l_flag = F_RDLCK;
	} else {
		o_flag = O_WRONLY;
		l_flag = F_WRLCK;
	}

	fd = open(fn, o_flag);
	if (fd < 0)
		return -EIO;

	ret = sc_lock_file(fd, l_flag, 1);
	if (ret < 0) {
		close(fd);
		return ret;
	}

	return fd;
}

void *rts_conf_get_metadata(const char *domain, int flag)
{
	struct metadata_object *metadata = NULL;
	struct json_object *root = NULL;
	char filepath[128] = {0};
	int fd = 0;
	int ret = 0;


	metadata = (struct metadata_object *)malloc(
			sizeof(struct metadata_object));
	if (!metadata)
		return NULL;

	get_filepath(domain, filepath, sizeof(filepath));
	/* lock the file before read */
	fd = __open_file_with_lock(filepath, flag);
	if (fd < 0) {
		ret = -EIO;
		goto fail;
	}

	root = json_object_from_file(filepath);
	if (!root) {
		struct stat st;

		stat(filepath, &st);
		if (st.st_size == 0) {
			root = json_object_new_object();
		} else {
			ret = -EIO;
			goto fail;
		}
	}

	/* unlock the file if SHARED_ACCESS */
	if (flag == SHARED_ACCESS) {
		sc_unlock_file(fd);
		close(fd);
		fd = -1;
	}

	ret = __init_inotify(metadata, filepath);
	if (ret)
		goto fail;

	metadata->fd = fd;
	metadata->root = (void *)root;
	metadata->dirty = 0;

	return metadata;

fail:
	if (fd >= 0) {
		sc_unlock_file(fd);
		close(fd);
	}

	if (root)
		json_object_put(root);

	if (metadata)
		free(metadata);

	return NULL;
}

static int rts_conf_vprintf_ex(void *metadata, const char *fmt, va_list vl)
{
	struct ctrl_element *ctrls = NULL;
	int num = 0;
	int ret = 0;

	num = create_ctrl_elements(fmt, &ctrls, vl);
	if (!ctrls) {
		ret = num;
		goto out;
	}

	ret = verify_printf_ctrl_elements(ctrls, num);
	if (ret)
		goto out;

	void *root = ((struct metadata_object *)metadata)->root;

	ret = rts_conf_put_value_to_json(ctrls, num,
			(struct json_object *)root);
	if (ret)
		goto out;

out:
	if (ctrls)
		free(ctrls);

	return ret;
}

int rts_conf_printf_ex(void *metadata, const char *fmt, ...)
{
	struct metadata_object *__m = metadata;

	if (metadata == NULL)
		return -1;

	int ret = 0;
	va_list vl;

	va_start(vl, fmt);

	ret = rts_conf_vprintf_ex(metadata, fmt, vl);
	va_end(vl);

	if (!ret)
		__m->dirty = 1;

	return ret;
}

int rts_conf_put_metadata(const char *domain, void *metadata)
{
	struct metadata_object *__m = metadata;

	if (metadata == NULL)
		return -1;

	if (0 == __m->dirty) {
		return 0;
	} else {

		char filepath[128] = {0};

		get_filepath(domain, filepath, sizeof(filepath));
		json_object_to_file_ext(filepath,
				__m->root,
				JSON_C_TO_STRING_PRETTY);

		return 0;
	}
}

int rts_conf_check_storage_change(void *_metadata)
{

	char buf[INOTIFY_EVENT_SIZE] = {0};
	int len = 0;
	char *pos = buf;
	char *end = NULL;
	struct metadata_object *metadata = (struct metadata_object *) _metadata;

	len = read(metadata->fd_inotify, buf, INOTIFY_EVENT_SIZE);
	end = &buf[0] + len;

	while (pos < end) {
		struct inotify_event *event = (struct inotify_event *) pos;

		if (event->mask & IN_MODIFY)
			return SYSCONF_FILE_MODIFY;
		else if (event->mask & IN_DELETE)
			return SYSCONF_FILE_DELETE;

		pos += sizeof(struct inotify_event) + event->len;
	}

	return 0;
}

void rts_conf_free_metadata(void *metadata)
{
	if (metadata) {
		struct metadata_object *pmetadata = NULL;

		pmetadata = (struct metadata_object *)metadata;
		if (pmetadata->fd >= 0) {
			sc_unlock_file(pmetadata->fd);
			close(pmetadata->fd);
		}

		if (pmetadata->fd_inotify >= 0) {
			if (pmetadata->wd_inotify >= 0)
				inotify_rm_watch(pmetadata->fd_inotify,
						pmetadata->wd_inotify);

			close(pmetadata->fd_inotify);
		}

		json_object_put((struct json_object *)(pmetadata->root));
		free(pmetadata);
	}
}
