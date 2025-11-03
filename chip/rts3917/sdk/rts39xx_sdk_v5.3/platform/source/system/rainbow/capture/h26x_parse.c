#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include <pthread.h>
#include <rtsavisp.h>
#include <errno.h>
#include <string.h>
#include <json-c/json.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "h26x_parse.h"
#include "ringbuffer.h"

#define H264_SPS_TOKEN	7
#define H264_PPS_TOKEN	8

#define NAL_VPS         32
#define NAL_SPS         33
#define NAL_PPS         34

#define FF_INPUT_BUFFER_PADDING_SIZE 16

#define AV_BSWAP16C(x) (((x) << 8 & 0xff00)  | ((x) >> 8 & 0x00ff))
#define AV_BSWAP32C(x) (AV_BSWAP16C(x) << 16 | AV_BSWAP16C((x) >> 16))
union unaligned_32 { uint32_t l; } __attribute__((__packed__)) av_alias;
#define av_const __attribute__((const))
#define av_bswap32 av_bswap32

struct h26x_parse_info {
	unsigned char vps[64];
	unsigned char sps[64];
	unsigned char pps[64];
	int vps_len;
	int sps_len;
	int pps_len;
};

static __attribute__((always_inline)) inline
	av_const uint32_t av_bswap32(uint32_t x)
{
	return AV_BSWAP32C(x);
}

#define AV_RN(s, p) (((const union unaligned_##s *) (p))->l)
#ifndef AV_RB32
#   define AV_RB32(p)    AV_RB(32, p)
#endif
#define AV_RB(s, p)    av_bswap##s(AV_RN##s(p))
#ifndef AV_RN32
#   define AV_RN32(p) AV_RN(32, p)
#endif

static uint8_t h26x_info_buffer[1024];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int get_flag_num;

static unsigned char *next_nalu_start(
	unsigned char *buf_head, unsigned char *buf_tail)
{
	while (buf_tail > buf_head) {
		if (buf_head[-1] > 1)
			buf_head += 3;
		else if (buf_head[-2])
			buf_head += 2;
		else if (buf_head[-3]|(buf_head[-1]-1))
			buf_head++;
		else
			return buf_head;
	}

	return NULL;
}

static int  get_sps_pps(unsigned char *in_buf,
		int in_len, struct h26x_parse_info *info)
{
	int type = 0;
	int ret = -1;
	int sps_len = 0;
	int pps_len = 0;
	unsigned char *sps_start = NULL;
	unsigned char *pps_start = NULL;
	unsigned char *next_nalu = NULL;
	unsigned char *buf_head = NULL;
	unsigned char *buf_tail = NULL;

	type = 0;
	if (in_len >= 4) {
		unsigned int startCode = AV_RB32(in_buf);

		if (startCode == 0x00000001) {
			next_nalu = &in_buf[4];
			type = next_nalu[0] & 0x1F;
		} else
			return ret;
	} else
		return ret;

	if (type == H264_SPS_TOKEN) {
		buf_head = next_nalu + 1;
		buf_tail = in_buf+in_len-1;
		sps_start = next_nalu;
		next_nalu = next_nalu_start(buf_head, buf_tail);
		if (next_nalu)
			type = next_nalu[0]&0x1F;
	} else
		return ret;

	if (type == H264_PPS_TOKEN) {
		sps_len = next_nalu - sps_start - 4;
		pps_start = next_nalu;
		buf_head = next_nalu + 1;
		next_nalu = next_nalu_start(buf_head, buf_tail);
		if (next_nalu) {
			pps_len = next_nalu - pps_start - 4;
			if (sps_len < sizeof(info->sps)
				&& pps_len < sizeof(info->pps)) {
				memcpy(info->sps, sps_start, sps_len);
				info->sps[sps_len] = '\0';
				info->sps_len = sps_len;
				memcpy(info->pps, pps_start, pps_len);
				info->pps[pps_len] = '\0';
				info->pps_len = pps_len;
				ret = 0;
			}
		} else {
			pps_len = buf_tail + 1 - pps_start;
			if (sps_len < sizeof(info->sps)
				&& pps_len < sizeof(info->pps)) {
				memcpy(info->sps, sps_start, sps_len);
				info->sps[sps_len] = '\0';
				info->sps_len = sps_len;

				memcpy(info->pps, pps_start, pps_len);
				info->pps[pps_len] = '\0';
				info->pps_len = pps_len;
				ret = 0;
			}
		}
	}

	return ret;
}

static int get_vps_sps_pps(unsigned char *in_buf, int in_len,
				struct h26x_parse_info *info)
{
	int type = 0;
	int ret = -1;
	int sps_len = 0;
	int pps_len = 0;
	int i = 0;
	int len = 0;
	unsigned char *sps_start = NULL;
	unsigned char *pps_start = NULL;
	unsigned char *next_nalu = NULL;
	unsigned char *buf_head = NULL;
	unsigned char *buf_tail = NULL;
	unsigned int startCode = 0;

	buf_tail = in_buf+in_len-1;
	type = 0;
	if (in_len >= 4) {
		startCode = AV_RB32(in_buf);
		if (startCode == 0x00000001) {
			next_nalu = &in_buf[4];
			type = next_nalu[0] & 0x1F;
		} else {
			printf("Err %s, %d, %d\n", __func__, __LINE__, ret);
			return ret;
		}
	} else {
		printf("Err %s, %d, %d\n", __func__, __LINE__, ret);
		return ret;
	}

	type = (next_nalu[0]&0x7E) >> 1;
	if (type == NAL_VPS) {
		buf_head = next_nalu + 2;
		next_nalu = next_nalu_start(buf_head, buf_tail);
		info->vps_len = next_nalu - buf_head - 4;
		memcpy(info->vps, buf_head, info->vps_len);
		info->vps[info->vps_len] = '\0';
		if (next_nalu)
			type = (next_nalu[0]&0x7E) >> 1;
	} else {
		printf("Err %s, %d, %d\n", __func__, __LINE__, ret);
		return ret;
	}

	if (type == NAL_SPS) {
		buf_head = next_nalu + 2;
		next_nalu = next_nalu_start(buf_head, buf_tail);
		info->sps_len = next_nalu - buf_head - 4;
		memcpy(info->sps, buf_head, info->sps_len);
		info->sps[info->sps_len] = '\0';
		if (next_nalu)
			type = (next_nalu[0]&0x7E) >> 1;
	} else {
		printf("Err %s, %d, %d\n", __func__, __LINE__, ret);
		return ret;
	}

	if (type == NAL_PPS) {
		unsigned char *buf_end = NULL;

		buf_head = next_nalu + 2;
		next_nalu = next_nalu_start(buf_head, buf_tail);
		if (next_nalu)
			buf_end = next_nalu - 4;
		else
			buf_end = buf_tail + 1;

		info->pps_len = buf_end - buf_head;
		memcpy(info->pps, buf_head, info->pps_len);
		info->pps[info->pps_len] = '\0';
	}

	return RTS_OK;
}

static void decode_nal(unsigned char *src, int *length)
{
	char dst[64];
	int si = 0;
	int di = 0;

	while (si + 2 < *length) {
		/* remove escapes (very rare 1:2^22) */
		if (src[si + 2] > 3) {
			dst[di++] = src[si++];
			dst[di++] = src[si++];
		} else if (src[si] == 0 && src[si + 1] == 0) {
			if (src[si + 2] == 3) {
				dst[di++]  = 0;
				dst[di++]  = 0;
				si        += 3;
			continue;
			} else
				goto nsc;
		}
		dst[di++] = src[si++];
	}
	while (si < *length)
		dst[di++] = src[si++];

nsc:
	memset(dst + di, 0, FF_INPUT_BUFFER_PADDING_SIZE);
	*length = di;
	memcpy(src, &dst, di);
}

static int  get_sps_pps_info(unsigned int h264_chnno,
			struct h26x_parse_info *info)
{
	int ret = -1;
	struct rts_h264_info h264_info;
	int i;

	ret = rts_av_get_h264_mediainfo(h264_chnno, &h264_info);
	if (RTS_IS_ERR(ret)) {
		printf("rts_av_get_h264_mediainfo failed,ret=%d\n", ret);
		return ret;
	}
	ret = get_sps_pps((unsigned char *)h264_info.sps_pps,
				h264_info.sps_pps_len, info);
	if (RTS_IS_ERR(ret)) {
		printf("get_sps_pps failed\n");
		return ret;
	}
	decode_nal(info->sps, &info->sps_len);
	decode_nal(info->pps, &info->pps_len);

	return ret;
}

static int  get_vps_sps_pps_info(int h265_chnno,
				struct h26x_parse_info *info)
{
	int ret = -1;
	int i;
	struct rts_h265_info h265_info;

	ret = rts_av_get_h265_mediainfo(h265_chnno, &h265_info);
	if (RTS_IS_ERR(ret)) {
		RTS_ERR("rts_av_get_h265_mediainfo failed");
		return ret;
	}

	/* split vps sps pps from flat info */
	ret = get_vps_sps_pps((unsigned char *)h265_info.vps_sps_pps,
			h265_info.vps_sps_pps_len, info);
	if (RTS_IS_ERR(ret))
		return ret;

	/* get string vps sps pps */
	decode_nal(info->vps, &info->vps_len);
	decode_nal(info->sps, &info->sps_len);
	decode_nal(info->pps, &info->pps_len);

	return ret;
}

#define H26X_INFO_SHARE_FILE  "/var/tmp/h25x_parse_info.shm"
#define ENCODE_TYPE_H264      0
#define ENCODE_TYPE_H265      1
void write_h26x_info_to_ringbuf(int max_video_num)
{
	int ret = -1;
	void *handle = NULL;
	packet_t v_pack;

	if (get_flag_num < max_video_num)
		return;
	handle = rts_ringbuffer_init(H26X_INFO_SHARE_FILE, 2 * 1024);
	if (!handle) {
		RTS_ERR("video ring buffer init failed\n");
		return;
	}
	v_pack.vm_addr = &h26x_info_buffer[0];
	v_pack.length = 1024;
	ret = rts_ringbuffer_write_packet(handle, &v_pack);
	if (ret != RTS_OK)
		RTS_ERR("write ring buffer fail\n");
}

int save_sps_pps_info(int video_index, unsigned int h264_chnno)
{
	int ret = -1;
	void *buf = NULL;
	void *buf_offset = NULL;
	struct h26x_parse_info info = {0};

	ret = get_sps_pps_info(h264_chnno, &info);
	if (ret != 0) {
		RTS_ERR("get_sps_pps_info failed\n");
		return -errno;
	}

	buf = &h26x_info_buffer[0];
	buf_offset = buf + video_index * 512 + ENCODE_TYPE_H264 * 256;
	memcpy(buf_offset, &info, sizeof(struct h26x_parse_info));
	pthread_mutex_lock(&mutex);
	get_flag_num++;
	pthread_mutex_unlock(&mutex);
}

int save_vps_sps_pps_info(int video_index, unsigned int h265_chnno)
{
	int ret = -1;
	void *buf = NULL;
	void *buf_offset = NULL;
	struct h26x_parse_info info = {0};

	ret = get_vps_sps_pps_info(h265_chnno, &info);
	if (ret != 0) {
		RTS_ERR("get_vps_sps_pps_info failed\n");
		return -1;
	}

	buf = &h26x_info_buffer[0];
	buf_offset = buf + video_index * 512 + ENCODE_TYPE_H265 * 256;
	memcpy(buf_offset, &info, sizeof(struct h26x_parse_info));
	pthread_mutex_lock(&mutex);
	get_flag_num++;
	pthread_mutex_unlock(&mutex);
}
