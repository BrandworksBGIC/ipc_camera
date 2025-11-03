/*
 *  Copyright (C) 2019 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsaudio.h>
#include <rtsamixer.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <libubus.h>
#include <rts_queue.h>
#include <json-c/json.h>

#include "rf_msg.h"
#include "audio.h"
#include "ringbuffer.h"

struct audio_config {
	struct rf_audio_node node;
	struct rf_audio_attr attr;
	struct rf_audio_volume volume;
	struct rf_audio_aec aec;
};

static struct audio_config g_config = {
	.node = {
		.dev_node = "hw:0,1",
	},
	.attr = {
		.playback_sample_rate = 8000,
		.playback_channels = 2,
		.playback_format = 16,
		.capture_sample_rate = 16000,
		.capture_channels = 2,
		.capture_format = 16,
		.encode_sample_rate = 8000,
		.encode_channels = 1,
		.encode_format = 16,
		.encode_type = RTS_AUDIO_TYPE_ID_ULAW,
		.encode_bitrate = 48000,
	},
	.volume = {
		.capture_volume = 50,
		.play_volume = 50,
	},
	.aec = {
		.aecns_enable = 1,
		.aec_en = 1,
		.ns_en = 1,
		.aec_thr = 0,
		.aec_scale = 0,
	}
};

static int update_audio_config_from_json(
	struct audio_config *conf)
{
	int i, ret = RF_ERR_OK;
	int val[11] = {};
	char *data = NULL;
	char *path = "/etc/conf/rainbow.json";
	struct json_object *root_obj = NULL;
	struct json_object *audio_obj = NULL;
	struct json_object *dev_node_obj = NULL;
	struct json_object *audio_attr_obj = NULL;
	struct json_object *audio_volume_obj = NULL;
	struct json_object *audio_aec_obj = NULL;

	root_obj = json_object_from_file(path);
	if (!root_obj)
		ret = -1;

	ret = json_object_object_get_ex(
		root_obj, "audio_config", &audio_obj);
	if (!ret)
		return ret;

	ret = json_object_object_get_ex(audio_obj, "dev_node", &dev_node_obj);
	if (ret) {
		data = (char *)json_object_get_string(dev_node_obj);
		if (data && strlen(data) > 0)
			strcpy(conf->node.dev_node, data);
	}

	ret = json_object_object_get_ex(
			audio_obj, "audio_attr", &audio_attr_obj);
	if (ret) {
		json_object_object_foreach(audio_attr_obj, attr_key, attr_val) {
		val[i] = json_object_get_int(attr_val);
		i = i + 1;
		}
		conf->attr.playback_sample_rate = val[0];
		conf->attr.playback_channels = val[1];
		conf->attr.playback_format = val[2];
		conf->attr.capture_sample_rate = val[3];
		conf->attr.capture_channels = val[4];
		conf->attr.capture_format = val[5];
		conf->attr.encode_sample_rate = val[6];
		conf->attr.encode_channels = val[7];
		conf->attr.encode_format = val[8];
		conf->attr.encode_type = val[9];
		conf->attr.encode_bitrate = val[10];
		i = 0;
	}

	ret = json_object_object_get_ex(
			audio_obj, "audio_volume", &audio_volume_obj);
	if (ret) {
		json_object_object_foreach(
			audio_volume_obj, volume_key, volume_val) {
		val[i] = json_object_get_int(volume_val);
		i = i + 1;
		}
		conf->volume.capture_volume = val[0];
		conf->volume.play_volume = val[1];
		i = 0;
	}

	ret = json_object_object_get_ex(audio_obj, "audio_aec", &audio_aec_obj);
	if (ret) {
		json_object_object_foreach(audio_aec_obj, aec_key, aec_val) {
			val[i] = json_object_get_int(aec_val);
			i = i + 1;
		}
		conf->aec.aecns_enable = val[0];
		conf->aec.aec_en = val[1];
		conf->aec.ns_en = val[2];
		conf->aec.aec_thr = val[3];
		conf->aec.aec_scale = val[4];
		i = 0;
	}

	if (root_obj)
		json_object_put(root_obj);

	return ret;
}

static struct audio_context *g_audio_ctx;

static void update_decode_profile(
	struct audio_context *context,
	struct payload_header *header)
{
	struct rts_av_profile profile;

	if (header->type == TWOWAY_AUDIO_PCM &&
		context->bitfmt == header->bitfmt &&
		context->channels == header->channels &&
		context->samplerate == header->samplerate)
		return;

	rts_av_get_profile(context->decode_ch, &profile);

	switch (header->type) {
	case TWOWAY_AUDIO_PCM:
		profile.fmt = RTS_A_FMT_AUDIO;
		profile.audio.bitfmt = header->bitfmt;
		profile.audio.channels = header->channels;
		profile.audio.samplerate = (uint16_t)header->samplerate;
		break;
	case TWOWAY_AUDIO_ULAW:
		profile.fmt = RTS_A_FMT_ULAW;
		break;
	case TWOWAY_AUDIO_ALAW:
		profile.fmt = RTS_A_FMT_ALAW;
		break;
	}

	rts_av_set_profile(context->decode_ch, &profile);
}

static void delete_twoway_stream(
	struct audio_context *context)
{
	if (!context)
		return;

	rts_av_stop_send(context->decode_ch);

	if (context->decode_ch >= 0) {
		rts_av_disable_chn(context->decode_ch);
		rts_av_destroy_chn(context->decode_ch);
		context->decode_ch = -1;
	}

	if (context->d2m_resample_ch >= 0) {
		rts_av_disable_chn(context->d2m_resample_ch);
		rts_av_destroy_chn(context->d2m_resample_ch);
		context->d2m_resample_ch = -1;
	}
}

void recycle_buffer(void *master,
	struct rts_av_buffer *buffer)
{
	struct audio_context *context = master;

	if (context->quit)
		return;

	buffer->bytesused = 0;
	buffer->timestamp = 0;
	rts_queue_push_back(context->idles, rts_av_get_buffer(buffer));
}

static int create_twoway_stream(
	struct audio_context *context)
{
	int i, ret = RF_ERR_OK;
	struct rts_av_profile profile;

	pthread_mutex_lock(&context->mutex);

	context->need_config = 0;

	context->idles = rts_queue_init();
	if (!context->idles)
		ret = -1;

	context->decode_ch =
		rts_av_create_audio_decode_chn();
	if (context->decode_ch < 0) {
		RTS_ERR("creat audio decode channel error\n");
		ret = -1;
		goto failed;
	}

	rts_av_get_profile(context->decode_ch, &profile);
	profile.fmt = RTS_A_FMT_AUDIO;
	profile.audio.bitfmt = 16;
	profile.audio.channels = 1;
	profile.audio.samplerate = 44100;
	ret = rts_av_set_profile(context->decode_ch, &profile);
	if (RTS_IS_ERR(ret)) {
		RTS_ERR("set decode fail, ret = %d\n", ret);
		goto failed;
	}

	context->d2m_resample_ch =
		rts_av_create_audio_resample_chn(context->cfg_p.rate,
				context->cfg_p.format, context->cfg_p.channels);
	if (context->d2m_resample_ch < 0) {
		RTS_ERR("creat audio d2m_resample channel error\n");
		ret = -1;
		goto failed;
	}

	pthread_mutex_unlock(&context->mutex);

	return ret;

failed:
	pthread_mutex_unlock(&context->mutex);

	if (context->decode_ch >= 0) {
		rts_av_destroy_chn(context->decode_ch);
		context->decode_ch = -1;
	}
	if (context->d2m_resample_ch >= 0) {
		rts_av_destroy_chn(context->d2m_resample_ch);
		context->d2m_resample_ch = -1;
	}

	return ret;
}

static int send_twoway_data(
	struct audio_context *context,
	void *data, uint32_t len)
{
	int ret = RF_ERR_OK;
	struct rts_av_buffer *buffer = NULL;
	struct payload_header *header = (struct payload_header *)data;
	int header_size = sizeof(struct payload_header);

	if (header->length + header_size != len)
		return -1;

	update_decode_profile(context, header);

	while (rts_queue_empty(context->idles))
		usleep(1000);

	buffer = rts_queue_pop(context->idles);
	buffer->vm_addr = header->data;
	buffer->bytesused = header->length;
	buffer->timestamp = header->timestamp;

	rts_av_send(context->decode_ch, buffer);

	RTS_SAFE_RELEASE(buffer, rts_av_put_buffer);

	return ret;
}

static void stop_audio_stream(
	struct audio_context *context)
{
	context->quit = 1;
	if (context->tid)
		pthread_join(context->tid, NULL);
	if (context->tw_tid)
		pthread_join(context->tw_tid, NULL);
	context->tid = 0;
	context->tw_tid = 0;
}

static int start_audio_stream(
	struct audio_context *context)
{
	int i, ret = RF_ERR_OK;
	struct rts_audio_capture_vqe c_vqe;
	struct audio_config *config = &g_config;

	ret = rts_av_bind(context->decode_ch,
		context->d2m_resample_ch);
	ret = rts_av_bind(context->d2m_resample_ch,
		context->mixer_ch);
	ret = rts_av_set_audio_mixer_droppable(
		context->mixer_ch, context->d2m_resample_ch, 0);
	ret = rts_av_bind(context->mixer_ch,
		context->playback_ch);
	ret = rts_av_bind(context->capture_ch,
		context->a2e_resample_ch);
	ret = rts_av_bind(context->a2e_resample_ch,
		context->encode_ch);
	if (ret) {
		RTS_ERR("bind error\n");
		return RF_ERR_CHN_BIND;
	}

	if (context->decode_ch >= 0) {
		ret = rts_av_enable_chn(context->decode_ch);
		if (ret) {
			RTS_ERR("enable audio decode ch error\n");
			return RF_ERR_CHN_ENABLE;
		}
	}

	if (context->d2m_resample_ch >= 0) {
		ret = rts_av_enable_chn(context->d2m_resample_ch);
		if (ret) {
			RTS_ERR("enable audio d2m_resample ch error\n");
			return RF_ERR_CHN_ENABLE;
		}
	}

	if (context->mixer_ch >= 0) {
		ret = rts_av_enable_chn(context->mixer_ch);
		if (ret) {
			RTS_ERR("enable audio mixer ch error\n");
			return RF_ERR_CHN_ENABLE;
		}
	}

	if (context->playback_ch >= 0) {
		ret = rts_av_enable_chn(context->playback_ch);
		if (ret) {
			RTS_ERR("enable audio playback ch error\n");
			return RF_ERR_CHN_ENABLE;
		}
	}

	if (context->capture_ch >= 0) {
		ret = rts_av_enable_chn(context->capture_ch);
		if (ret) {
			RTS_ERR("enable audio capture ch error\n");
			return RF_ERR_CHN_ENABLE;
		}
	}

	if (context->a2e_resample_ch >= 0) {
		ret = rts_av_enable_chn(context->a2e_resample_ch);
		if (ret) {
			RTS_ERR("enable audio a2e ch error\n");
			return RF_ERR_CHN_ENABLE;
		}
	}

	if (context->encode_ch >= 0) {
		ret = rts_av_enable_chn(context->encode_ch);
		if (ret) {
			RTS_ERR("enable audio encode ch error\n");
			return RF_ERR_CHN_ENABLE;
		}
	}

	rts_av_get_audio_capture_vqe(context->capture_ch, &c_vqe);
	c_vqe.aecns_enable = g_config.aec.aecns_enable;
	c_vqe.aecns_attr.aec_enable = g_config.aec.aec_en;
	c_vqe.aecns_attr.aec_scale = g_config.aec.aec_scale;
	c_vqe.aecns_attr.aec_thr = g_config.aec.aec_thr;
	c_vqe.aecns_attr.ns_enable = g_config.aec.ns_en;
	rts_av_set_audio_capture_vqe(context->capture_ch, &c_vqe);

	rts_av_start_send(context->decode_ch);
	rts_av_start_recv(context->encode_ch);

	for (i = 0; i < RTS_ARRAY_SIZE(context->buffers); i++) {
		struct rts_av_buffer *buffer = NULL;

		context->buffers[i] = rts_av_new_buffer(TWOWAY_BUFF);
		if (!context->buffers[i])
			RTS_ERR("new buffer error\n");

		buffer = rts_av_get_buffer(context->buffers[i]);
		rts_av_set_buffer_callback(buffer, context, recycle_buffer);
		RTS_SAFE_RELEASE(buffer, rts_av_put_buffer);
	}

	return ret;
}

static void delete_audio_stream(
	struct audio_context *context)
{
	if (!context)
		return;

	rts_av_stop_recv(context->encode_ch);

	if (context->mixer_ch >= 0) {
		rts_av_disable_chn(context->mixer_ch);
		rts_av_destroy_chn(context->mixer_ch);
		context->mixer_ch = -1;
	}

	if (context->playback_ch >= 0) {
		rts_av_disable_chn(context->playback_ch);
		rts_av_destroy_chn(context->playback_ch);
		context->playback_ch = -1;
	}

	if (context->capture_ch >= 0) {
		rts_av_disable_chn(context->capture_ch);
		rts_av_destroy_chn(context->capture_ch);
		context->capture_ch = -1;
	}

	if (context->a2e_resample_ch >= 0) {
		rts_av_disable_chn(context->a2e_resample_ch);
		rts_av_destroy_chn(context->a2e_resample_ch);
		context->a2e_resample_ch = -1;
	}

	if (context->encode_ch >= 0) {
		rts_av_disable_chn(context->encode_ch);
		rts_av_destroy_chn(context->encode_ch);
		context->encode_ch = -1;
	}
}

static int create_audio_stream(
	struct audio_context *context)
{
	int res;
	int ret = RF_ERR_OK;
	struct rts_av_profile profile;
	struct audio_config *config = &g_config;

	pthread_mutex_lock(&context->mutex);

	res = update_audio_config_from_json(config);
	if (res == -1)
		RTS_ERR("update with default parameter\n");

	context->need_config = 0;

	snprintf(context->cfg_p.dev_node,
		sizeof(context->cfg_p.dev_node), g_config.node.dev_node);
	context->cfg_p.rate = g_config.attr.playback_sample_rate;
	context->cfg_p.format = g_config.attr.playback_format;
	context->cfg_p.channels = g_config.attr.playback_channels;
	context->cfg_p.period_frames = 0;

	context->mixer_ch =
		rts_av_create_audio_mixer_chn();
	if (context->mixer_ch < 0) {
		RTS_ERR("creat audio mixer channel error\n");
		ret = -1;
		goto failed;
	}
	context->playback_ch =
		rts_av_create_audio_playback_chn(&context->cfg_p);
	if (context->playback_ch < 0) {
		RTS_ERR("creat audio playback channel error\n");
		ret = -1;
		goto failed;
	}

	snprintf(context->cfg_c.dev_node,
		sizeof(context->cfg_c.dev_node), g_config.node.dev_node);
	context->cfg_c.rate = g_config.attr.capture_sample_rate;
	context->cfg_c.format = g_config.attr.capture_format;
	context->cfg_c.channels = g_config.attr.capture_channels;
	context->cfg_c.period_frames = 0;

	context->capture_ch =
		rts_av_create_audio_capture_chn(&context->cfg_c);
	if (context->capture_ch < 0) {
		RTS_ERR("creat audio capture channel error\n");
		ret = -1;
		goto failed;
	}
	context->a2e_resample_ch =
		rts_av_create_audio_resample_chn(
			g_config.attr.encode_sample_rate,
			g_config.attr.encode_format,
			g_config.attr.encode_channels);
	if (context->a2e_resample_ch < 0) {
		RTS_ERR("creat audio a2e_resample channel error\n");
		ret = -1;
		goto failed;
	}
	context->encode_ch =
		rts_av_create_audio_encode_chn(
			g_config.attr.encode_type,
			g_config.attr.encode_bitrate);
	if (context->encode_ch < 0) {
		RTS_ERR("creat audio encode channel error\n");
		ret = -1;
		goto failed;
	}

	pthread_mutex_unlock(&context->mutex);

	return ret;

failed:
	pthread_mutex_unlock(&context->mutex);

	if (context->mixer_ch >= 0) {
		rts_av_destroy_chn(context->mixer_ch);
		context->mixer_ch = -1;
	}
	if (context->playback_ch >= 0) {
		rts_av_destroy_chn(context->playback_ch);
		context->playback_ch = -1;
	}
	if (context->capture_ch >= 0) {
		rts_av_destroy_chn(context->capture_ch);
		context->capture_ch = -1;
	}
	if (context->a2e_resample_ch >= 0) {
		rts_av_destroy_chn(context->a2e_resample_ch);
		context->a2e_resample_ch = -1;
	}
	if (context->encode_ch >= 0) {
		rts_av_destroy_chn(context->encode_ch);
		context->encode_ch = -1;
	}

	return ret;
}

static void uninit_output(
	struct audio_context *context)
{

}

static int init_output(
	struct audio_context *context)
{
	int ret = RF_ERR_OK;


	return ret;
}

static int v_packet_set(packet_t *v_pack,
	struct rts_av_buffer *v_buff)
{
	static long long index;
	RTS_ASSERT(v_pack);
	RTS_ASSERT(v_buff);

	v_pack->vm_addr = v_buff->vm_addr;
	v_pack->length = v_buff->bytesused;
	v_pack->timestamp = v_buff->timestamp;
	v_pack->flags = v_buff->flags;
	v_pack->type = v_buff->type;
	v_pack->index = index++;

	return RF_ERR_OK;
}

static int put_buf_to_rb(
	struct rts_av_buffer *vbuffer,
	void *v_handle, packet_t *v_pack)
{
	int ret;
	struct rts_av_buffer *vbuf = vbuffer;
	void *handle = v_handle;
	packet_t *pack = v_pack;

	ret = v_packet_set(pack, vbuf);
	ret = rts_ringbuffer_write_packet(handle, pack);
	if (ret != RTS_OK) {
		RTS_ERR("write ring buffer fail\n");
		return RF_ERR_RB_WRITE;
	}

	return RF_ERR_OK;
}

static int rf_convert_audio_fmt(int fmt)
{
	int audio_fmt = RB_A_FMT_AUDIO;

	switch (fmt) {
	case RTS_AUDIO_TYPE_ID_ULAW:
		return RB_A_FMT_ULAW;

	case RTS_AUDIO_TYPE_ID_ALAW:
		return RB_A_FMT_ALAW;

	case RTS_AUDIO_TYPE_ID_G726:
		return RB_A_FMT_G726;
#if 0
	case RTS_AUDIO_TYPE_ID_MP3:
		return RB_A_FMT_MP3;

	case RTS_AUDIO_TYPE_ID_AMRNB:
		return RB_A_FMT_AMRNB;

	case RTS_AUDIO_TYPE_ID_AAC:
		return RB_A_FMT_AAC;

	case RTS_AUDIO_TYPE_ID_OPUS:
		return RB_A_FMT_OPUS;
#endif
	default:
		return audio_fmt;
	}
}

static int set_ringbuffer_fmt(
	struct audio_context *context)
{
	int ret = RF_ERR_OK;
	stream_format format;
	void *handle = context->ringbuf;

	format.fmt = rf_convert_audio_fmt(g_config.attr.encode_type);
	format.audio.samplerate = g_config.attr.encode_sample_rate;
	format.audio.bitfmt = g_config.attr.encode_format / 2;
	format.audio.channels = g_config.attr.encode_channels;

	ret = rts_ringbuffer_set_stream_format(handle, &format);
	if (ret != RF_ERR_OK)
		return RF_ERR_RB_SET_FMT;

	return ret;
}

#define RINGBUFFER_AUDIO_FILENAME_FORMAT "/var/tmp/capture_audio_profile.shm"

static void *capture_thread(void *args)
{
	struct audio_context *context =
		(struct audio_context *)args;

	prctl(PR_SET_NAME, __func__);

	packet_t v_pack;

	context->ringbuf = rts_ringbuffer_init
		(RINGBUFFER_AUDIO_FILENAME_FORMAT, AUDIO_SHM_BUFF);
	if (!context->ringbuf) {
		RTS_ERR("audio ring buffer init failed\n");
		goto err;
	}

	set_ringbuffer_fmt(context);

	while (!context->quit) {
		struct rts_av_buffer *buffer = NULL;
		if (context->need_config) {
			delete_twoway_stream(context);
			delete_audio_stream(context);
			create_audio_stream(context);
			create_twoway_stream(context);
			set_ringbuffer_fmt(context);
			start_audio_stream(context);
		}
		usleep(1000);

		if (rts_av_recv_block(context->encode_ch, &buffer, 100))
			continue;

		if (buffer) {
			put_buf_to_rb(buffer, context->ringbuf, &v_pack);
			rts_av_put_buffer(buffer);
			buffer = NULL;
		}
	}
err:
	rts_ringbuffer_release(context->ringbuf);

	return NULL;
}

static void *twoway_thread(void *args)
{
	struct audio_context *context =
		(struct audio_context *)args;

	int sfd, len;
	int ret = RF_ERR_OK;
	uint8_t buf[BUF_LEN];
	unsigned int addr_length;
	struct sockaddr_in addr;

	prctl(PR_SET_NAME, __func__);

	sfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sfd == -1) {
		perror("socket");
		ret = -1;
	}

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(AUDIO_LISTEN_PORT);
	addr.sin_addr.s_addr = INADDR_ANY;
	addr_length = sizeof(addr);

	ret = bind(sfd, (struct sockaddr *)&addr, sizeof(addr));
	if (ret == -1)
		perror("bind");

	while (!context->quit) {
		len = recvfrom(sfd, buf, BUF_LEN, MSG_DONTWAIT, (struct sockaddr *) &addr, &addr_length);

		if (len == 0) {
			perror("socket connect");
		} else if (len < 0) {
			if (EAGAIN == errno || EWOULDBLOCK == errno) {
				usleep(1);
			} else {
				perror("recv");
				break;
			}
		} else if (len > 0) {
			ret = send_twoway_data(context, buf, len);
		}
	}
err:
	delete_twoway_stream(context);

	return NULL;
}

static struct audio_context *create_context(void)
{
	struct audio_context *context = NULL;

	context = calloc(1, sizeof(struct audio_context));
	pthread_mutex_init(&context->mutex, NULL);

	context->decode_ch = -1;
	context->d2m_resample_ch = -1;
	context->mixer_ch = -1;
	context->playback_ch = -1;
	context->capture_ch = -1;
	context->a2e_resample_ch = -1;
	context->encode_ch = -1;

	return context;
}

static void release_context(
	struct audio_context *context)
{
	int i = 0;

	if (context) {
		for (i = 0; i < RTS_ARRAY_SIZE(context->buffers); i++)
			rts_av_set_buffer_callback(context->buffers[i], NULL, NULL);

		if (context->idles)
			rts_queue_clear(context->idles,
				(cleanup_item_func)rts_av_put_buffer);

		for (i = 0; i < RTS_ARRAY_SIZE(context->buffers); i++)
			RTS_SAFE_RELEASE(context->buffers[i], rts_av_delete_buffer);

		pthread_mutex_destroy(&context->mutex);
		free(context);
	}
}

static int audio_set_volume(
	struct audio_config *volume_config)
{
	int ret = RF_ERR_OK;
	struct rf_audio_volume volume;

	volume.capture_volume = volume_config->volume.capture_volume;
	volume.play_volume = volume_config->volume.play_volume;

	ret = rts_audio_set_capture_volume(volume.capture_volume);
	if (ret != RF_ERR_OK) {
		RTS_ERR("set capture volume wrong\n");
		return RF_ERR_SET_CAPTURE_VOLUME;
	}

	ret = rts_audio_set_playback_volume(volume.play_volume);
	if (ret != RF_ERR_OK) {
		RTS_ERR("set play volume wrong\n");
		return RF_ERR_SET_PLAY_VOLUME;
	}

	return RF_ERR_OK;
}

static int audio_get_volume(
	struct audio_config *volume_config)
{
	int ret = RF_ERR_OK;
	struct rf_audio_volume volume;

	ret = rts_audio_get_playback_volume(&volume.play_volume);
	if (ret != RF_ERR_OK) {
		RTS_ERR("get play volume wrong\n");
		return RF_ERR_GET_PLAY_VOLUME;
	}

	ret = rts_audio_get_capture_volume(&volume.capture_volume);
	if (ret != RF_ERR_OK) {
		RTS_ERR("get capture volume wrong\n");
		return RF_ERR_GET_CAPTURE_VOLUME;
	}

	volume_config->volume.capture_volume = volume.capture_volume;
	volume_config->volume.play_volume = volume.play_volume;

	return RF_ERR_OK;
}

int rf_control_audio_capture(
	int request, void *arg)
{
	struct audio_context *context = g_audio_ctx;
	int ret = RF_ERR_OK;
	struct rf_audio_req *req = arg;
	struct rf_audio_attr *attr = NULL;
	struct rf_audio_volume *volume = NULL;
	struct rf_audio_aec *aec = NULL;

	switch (request) {
	case RF_AUDIO_GET_ATTR:
		attr = &req->attr;
		pthread_mutex_lock(&context->mutex);
		*attr = g_config.attr;
		pthread_mutex_unlock(&context->mutex);
		break;
	case RF_AUDIO_SET_ATTR:
		attr = &req->attr;
		pthread_mutex_lock(&context->mutex);
		g_config.attr = *attr;
		strcpy(g_config.node.dev_node, "hw:0,1");
		context->need_config = 1;
		pthread_mutex_unlock(&context->mutex);

		break;
	case RF_AUDIO_GET_VOLUME:
		volume = &req->volume;
		audio_get_volume(&g_config);
		pthread_mutex_lock(&context->mutex);
		*volume = g_config.volume;
		pthread_mutex_unlock(&context->mutex);
		break;
	case RF_AUDIO_SET_VOLUME:
		volume = &req->volume;
		pthread_mutex_lock(&context->mutex);
		g_config.volume = *volume;
		pthread_mutex_unlock(&context->mutex);
		audio_set_volume(&g_config);
		break;
	case RF_AUDIO_GET_AEC:
		aec = &req->aec;
		pthread_mutex_lock(&context->mutex);
		*aec = g_config.aec;
		pthread_mutex_unlock(&context->mutex);
		break;
	case RF_AUDIO_SET_AEC:
		aec = &req->aec;
		pthread_mutex_lock(&context->mutex);
		g_config.aec = *aec;
		context->need_config = 1;
		pthread_mutex_unlock(&context->mutex);
		break;
	default:
		ret = RF_ERR_REQUEST_NOT_SUPPORT;
		break;
	}

	return ret;
}

int rf_start_audio_capture(void)
{
	int ret = RF_ERR_OK;
	struct audio_context *context = NULL;

	context = create_context();

	ret = init_output(context);
	if (ret) {
		RTS_ERR("init audio output error\n");
		goto failed;
	}

	ret = create_audio_stream(context);
	if (ret) {
		RTS_ERR("create audio stream error\n");
		uninit_output(context);
		goto failed;
	}

	ret = create_twoway_stream(context);
	if (ret) {
		RTS_ERR("create twoway stream error\n");
		goto failed;
	}

	ret = start_audio_stream(context);
	if (ret) {
		RTS_ERR("enable audio error\n");
		goto failed;
	}

	pthread_create(&context->tid,
		NULL, capture_thread, context);

	pthread_create(&context->tw_tid,
		NULL, twoway_thread, context);

	g_audio_ctx = context;

	return ret;

failed:
	rf_stop_audio_capture();

	return ret;
}

void rf_stop_audio_capture(void)
{
	struct audio_context *context =
		g_audio_ctx;

	if (context) {
		stop_audio_stream(context);
		delete_twoway_stream(context);
		delete_audio_stream(context);
		uninit_output(context);
		release_context(context);
	}
	g_audio_ctx = NULL;
}
