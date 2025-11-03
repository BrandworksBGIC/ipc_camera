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
#include <rtscamkit.h>

#include "rf_msg.h"

enum {
	RESPONSE_CODE,
	RESPONSE_DATA,
	__RESPONSE_MAX,
};

static const struct blobmsg_policy response_policy[__RESPONSE_MAX] = {
	[RESPONSE_CODE] = { .name = "return_code", .type = BLOBMSG_TYPE_INT32 },
	[RESPONSE_DATA] = { .name = "data", .type = BLOBMSG_TYPE_UNSPEC },
};

struct ret_value {
	int code;
	void *data;
};

struct message_context {
	struct ubus_context *ctx;
	uint32_t id;
	pthread_mutex_t mutex;
};

static struct message_context g_msg_ctx;

static void msg_cb(
		struct ubus_request *request,
		int type,
		struct blob_attr *msg)
{
	struct ret_value *ret_cb = (struct ret_value *)request->priv;
	struct blob_attr *tb[__RESPONSE_MAX];

	blobmsg_parse(response_policy, __RESPONSE_MAX, tb, blob_data(msg),
			blob_len(msg));

	if (!tb[RESPONSE_CODE] || !tb[RESPONSE_DATA]) {
		RTS_ERR("parse ubus response error\n");
		ret_cb->code = -EINVAL;
		return;
	}

	ret_cb->code = blobmsg_get_u32(tb[RESPONSE_CODE]);
	memcpy(ret_cb->data, blobmsg_data(tb[RESPONSE_DATA]),
			blobmsg_len(tb[RESPONSE_DATA]));
}

int rf_connect_ubus(void)
{
	struct ubus_context *ctx;
	int ret = RF_ERR_OK;
	uint32_t id;

	uloop_init();
	ctx = ubus_connect(NULL);
	if (!ctx) {
		RTS_ERR("Failed to connect to ubus\n");
		ret = -EINVAL;
		goto failed;
	}
	ret = ubus_lookup_id(ctx, CAPTURE_MSGD, &id);
	if (ret) {
		RTS_ERR("Failed to look up %s ubus object\n",
			CAPTURE_MSGD);
		goto failed;
	}

	g_msg_ctx.ctx = ctx;
	g_msg_ctx.id = id;
	pthread_mutex_init(&g_msg_ctx.mutex, NULL);

	return ret;

failed:
	if (ctx)
		ubus_free(ctx);
	return ret;
}

void rf_disconnect_ubus(void)
{
	struct ubus_context *ctx = g_msg_ctx.ctx;

	if (ctx)
		ubus_free(ctx);
	pthread_mutex_destroy(&g_msg_ctx.mutex);
	uloop_done();
}

int rf_send_msg(
		RF_MSG_REQUEST request,
		void *arg,
		int len)
{
	struct blob_buf b = {0};
	int ret = RF_ERR_OK;
	struct ret_value ret_cb;
	struct ubus_context *ctx = g_msg_ctx.ctx;
	uint32_t id = g_msg_ctx.id;
	pthread_mutex_t *mutex = &g_msg_ctx.mutex;

	blob_buf_init(&b, 0);
	blobmsg_add_u32(&b, "request", request);
	blobmsg_add_field(&b, BLOBMSG_TYPE_UNSPEC, "arg", arg, len);

	ret_cb.code = 0;
	ret_cb.data = arg;

	pthread_mutex_lock(mutex);
	ret = ubus_invoke(ctx, id, CAPTURE_MSGD_HANDLE, b.head,
			msg_cb, &ret_cb, 3000);
	pthread_mutex_unlock(mutex);
	if (!ret)
		ret = ret_cb.code;
	else if (ret > 0)
		ret = RF_ERR_UBUS_INVOKE;

	free(b.buf);

	return ret;
}

enum {
	NOTIFY_EVENT_ID = 0,
	NOTIFY_EVENT_TIME,
	__NOTIFY_EVENT_MAX
};

static const struct blobmsg_policy event_notify_policy[] = {
	[NOTIFY_EVENT_ID] = {
		.name = "event id", .type = BLOBMSG_TYPE_INT32 },
	[NOTIFY_EVENT_TIME] = {
		.name = "event time", .type = BLOBMSG_TYPE_INT32 },
};

struct event_handler {
	struct ubus_event_handler listener;
	notify_cb_t cb;
};

int rf_send_event(enum rf_msg_notify_type event_id)
{
	struct blob_buf b;
	int ret;
	struct ubus_context *ctx = g_msg_ctx.ctx;
	pthread_mutex_t *mutex = &g_msg_ctx.mutex;

	memset(&b, 0, sizeof(b));

	blob_buf_init(&b, 0);

	blobmsg_add_u32(&b,
		event_notify_policy[NOTIFY_EVENT_ID].name,
		event_id);
	blobmsg_add_u32(&b,
		event_notify_policy[NOTIFY_EVENT_TIME].name,
		time(NULL));
	pthread_mutex_lock(mutex);
	ret = ubus_send_event(ctx, CAPTURE_NOTIFY, b.head);
	pthread_mutex_unlock(mutex);

	blob_buf_free(&b);

	return ret;
}

static void event_callback(struct ubus_context *ctx,
	struct ubus_event_handler *ev,
	const char *type, struct blob_attr *msg)
{
	struct blob_attr *tb[__NOTIFY_EVENT_MAX];
	struct rf_notify_st event;
	struct event_handler *handler = NULL;

	if (!msg)
		return;

	blobmsg_parse(event_notify_policy,
		__NOTIFY_EVENT_MAX,
		tb, blob_data(msg), blob_len(msg));

	if (!tb[NOTIFY_EVENT_ID] ||
		!tb[NOTIFY_EVENT_TIME]) {
		return;
	}

	event.id = blobmsg_get_u32(tb[NOTIFY_EVENT_ID]);
	event.time = blobmsg_get_u32(tb[NOTIFY_EVENT_TIME]);

	handler = container_of(ev, struct event_handler, listener);
	if (handler)
		handler->cb(&event);
}

void *rf_register_event_handler(
	char *event_notify,
	notify_cb_t cb)
{
	struct ubus_context *ctx = g_msg_ctx.ctx;
	struct event_handler *handler = NULL;
	int ret = 0;

	handler = malloc(sizeof(struct event_handler));
	if (!handler)
		return NULL;

	memset(handler, 0, sizeof(*handler));
	handler->listener.cb = event_callback;
	handler->cb = cb;
	ret = ubus_register_event_handler(ctx,
		&handler->listener,
		event_notify);
	if (ret) {
		ubus_unregister_event_handler(
			ctx,
			&handler->listener);
		free(handler);
		fprintf(stderr, "register event failed.\n");
		return NULL;
	}

	return handler;
}

void rf_unregister_event_handler(void *handler)
{
	struct ubus_context *ctx = g_msg_ctx.ctx;
	struct event_handler *hdr = handler;

	ubus_unregister_event_handler(ctx,
		&hdr->listener);
	free(handler);
}

