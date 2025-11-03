/*
 *  Copyright (C) 2019 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <sys/time.h>
#include <libubus.h>
#include <pthread.h>

#include "rf_msg.h"
#include "video.h"
#include "audio.h"
#include "ispctrl.h"
#include "mask.h"
#include "md.h"
#include "osd.h"

static struct ubus_context *g_ubus_ctx;

enum {
	HANDLE_PARAM_REQUEST,
	HANDLE_PARAM_ARG,
	__HANDLE_PARAM_MAX,
};

static const struct blobmsg_policy handle_policy[] = {
	[HANDLE_PARAM_REQUEST] = {.name = "request",
						.type = BLOBMSG_TYPE_INT32},
	[HANDLE_PARAM_ARG] = {.name = "arg", .type = BLOBMSG_TYPE_UNSPEC},
};

static int capture_handle(struct ubus_context *ctx, struct ubus_object *obj,
		struct ubus_request_data *req,
		const char *method, struct blob_attr *msg);

static struct ubus_method capture_methods[] = {
	UBUS_METHOD(CAPTURE_MSGD_HANDLE, capture_handle, handle_policy),
};

static struct ubus_object_type capture_obj_type =
	UBUS_OBJECT_TYPE(CAPTURE_MSGD, capture_methods);

static struct ubus_object msgd_obj = {
	.name = CAPTURE_MSGD,
	.type = &capture_obj_type,
	.methods = capture_methods,
	.n_methods = ARRAY_SIZE(capture_methods),
};

static int capture_handle(struct ubus_context *ctx,
			struct ubus_object *obj,
			struct ubus_request_data *req,
			const char *method, struct blob_attr *msg)
{
	struct blob_attr *tb[__HANDLE_PARAM_MAX];
	uint32_t domain, request;
	struct cam_data *data = NULL;
	void *arg = NULL;
	int arg_len = 0;
	int ret = RF_ERR_OK;
	struct blob_buf b = {0};

	ret = blobmsg_parse(handle_policy, __HANDLE_PARAM_MAX, tb,
			blob_data(msg), blob_len(msg));
	if (ret != 0) {
		ret = UBUS_STATUS_INVALID_ARGUMENT;
		goto out;
	}
	if (!tb[HANDLE_PARAM_REQUEST] || !tb[HANDLE_PARAM_ARG]) {
		ret = UBUS_STATUS_INVALID_ARGUMENT;
		goto out;
	}

	request = blobmsg_get_u32(tb[HANDLE_PARAM_REQUEST]);
	arg = blobmsg_data(tb[HANDLE_PARAM_ARG]);
	arg_len = blobmsg_len(tb[HANDLE_PARAM_ARG]);

	domain = RF_MSG_DOMAIN(request);

	switch (domain) {
	case RF_MSG_DOMAIN_VIDEO:
		ret = rf_control_video_capture(request, arg);
		break;
	case RF_MSG_DOMAIN_AUDIO:
		ret = rf_control_audio_capture(request, arg);
		break;
	case RF_MSG_DOMAIN_ISPCTRL:
		ret = rf_control_ispctrl(request, arg);
		break;
	case RF_MSG_DOMAIN_MD:
		ret = rf_control_md(request, arg);
		break;
	case RF_MSG_DOMAIN_MASK:
		ret = rf_control_mask(request, arg);
		break;
	case RF_MSG_DOMAIN_OSD:
		ret = rf_control_osdi(request, arg);
		break;
	default:
		ret =  RF_ERR_DOMAIN_NOT_SUPPORT;
		break;
	}

out:
	blob_buf_init(&b, 0);
	blobmsg_add_u32(&b, "return_code", ret);
	blobmsg_add_field(&b, BLOBMSG_TYPE_UNSPEC, "data", arg, arg_len);
	ubus_send_reply(ctx, req, b.head);
	free(b.buf);
	return 0;
}

int rf_init_msgd(void)
{
	struct ubus_context *ctx;
	int ret = RF_ERR_OK;

	uloop_init();

	ctx = ubus_connect(NULL);
	if (!ctx) {
		fprintf(stderr, "Failed to connect to ubus\n");
		return RF_ERR_UBUS_CONNECT;
	}

	ubus_add_uloop(ctx);
	ret = ubus_add_object(ctx, (struct ubus_object *)&msgd_obj);
	if (ret) {
		ubus_free(ctx);
		ctx = NULL;
		return ret;
	}
	g_ubus_ctx = ctx;

	return ret;
}

void rf_release_msgd(void)
{
	struct ubus_context *ctx = g_ubus_ctx;

	if (g_ubus_ctx)
		ubus_free(g_ubus_ctx);
	uloop_done();
}

void rf_start_msgd(void)
{
	uloop_run();
}

void rf_stop_msgd(void)
{
	uloop_end();
}


