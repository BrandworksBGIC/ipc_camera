// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025 Realtek Semiconductor Corp. All rights reserved.
 *
 * THIS SOFTWARE IS CONFIDENTIAL AND PROPRIETARY TO REALTEK SEMICONDUCTOR
 * CORP. DISCLOSURE, REPRODUCTION, REDISTRIBUTION, IN WHOLE OR IN PART, OF
 * THIS WORK AND ITS DERIVATIVES WITHOUT EXPRESS PERMISSION IS PROHIBITED.
 *
 * REALTEK SEMICONDUCTOR CORP. RESERVES THE RIGHT TO UPDATE, MODIFY, OR
 * DISCONTINUE THIS SOFTWARE AT ANY TIME WITHOUT NOTICE. THIS SOFTWARE IS
 * PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <linux/slab.h>
#include "rts_camera_fps.h"
#include "rts_camera.h"
#include "rts_camera_priv.h"

static void __rtscam_get_sensor_maxfps(struct rtscam_video_fps *fps,
					struct v4l2_fract *n);
static int __rtscam_check_sensor_fps(struct rtscam_video_fps *fps,
					struct v4l2_fract *n);
static void __rtscam_update_sensor_fps_dynamic(
			struct rtscam_video_stream *stream,
			struct v4l2_fract fps, int flag);

static int __calc_gcd(int a, int b)
{
	int tmp;

	while (b) {
		tmp = a % b;
		a = b;
		b = tmp;
	}
	return a;
}

static int __calc_coprime(int *a, int *b,
			int snum, int unum)
{
	int m = *a;
	int n = *b;
	int tmp;

	if (m < 0 || n < 0)
		return 0;
	if (m == 0)
		return n;
	if (n == 0)
		return m;

	tmp = snum * unum / __calc_gcd(snum, unum);

	m = m * (tmp / snum);
	n = n * (tmp / unum);

	tmp = __calc_gcd(m, n);

	*a = m / tmp;
	*b = n / tmp;

	return m;
}

static void __normalize_fract(uint32_t *nume, uint32_t *deno)
{
	int gcd = __calc_gcd(*deno, *nume);

	*deno /= gcd;
	*nume /= gcd;
}

static void __rtscam_init_skip_info(struct rtscam_video_stream *stream)
{
	struct rtscam_soc_skip_info *skip = NULL;

	if (stream->streamid >= stream->fps.sensor_fps->streamnum)
		return;

	skip = &stream->fps.skip_info;

	skip->m = stream->fps.sensor_fps->sensor_fps_actual.denominator;
	skip->n = stream->fps.user_actual.denominator;

	__calc_coprime(&skip->m, &skip->n,
		stream->fps.sensor_fps->sensor_fps_actual.numerator,
		stream->fps.user_actual.numerator);

	if (skip->m > 2 * skip->n) {
		skip->flag = 1;
	} else {
		skip->flag = 0;
		skip->n = skip->m - skip->n;
	}
	skip->count = 0;
	skip->index = 0;
}

int rtscam_skip_frame(struct rtscam_video_stream *stream)
{
	struct rtscam_soc_skip_info *skip_info = NULL;
	int skip = 0;

	if (stream->streamid >= stream->fps.sensor_fps->streamnum)
		return 0;

	skip_info = &stream->fps.skip_info;

	rtsprintk(RTS_TRACE_VIDEO, "%d %d %d %d %d\n",
		  skip_info->m, skip_info->n, skip_info->flag,
		  skip_info->count, skip_info->index);

	if (skip_info->n == 0)
		return skip_info->flag;

	if ((skip_info->n - skip_info->count) * skip_info->m >=
	    (skip_info->m - skip_info->index) * skip_info->n) {
		skip = 1 - skip_info->flag;
		skip_info->count++;
	} else {
		skip = skip_info->flag;
	}

	skip_info->index++;
	if (skip_info->index % skip_info->m == 0) {
		skip_info->count = 0;
		skip_info->index = 0;
	}

	return skip;
}
EXPORT_SYMBOL_GPL(rtscam_skip_frame);

static void __update_user_fps_actual(struct rtscam_video_fps *fps,
				     struct v4l2_fract user_fps)
{
	if (!fps)
		return;

	fps->user_actual.numerator = user_fps.numerator;
	fps->user_actual.denominator = user_fps.denominator;
}

void rtscam_set_user_fps_actual(struct rtscam_video_fps *fps)
{
	if (!fps)
		return;

	__update_user_fps_actual(fps, fps->sensor_fps->sensor_fps_actual);
}
EXPORT_SYMBOL_GPL(rtscam_set_user_fps_actual);

void rtscam_adjust_sensor_fps_dynamic(struct rtscam_video_stream *stream,
		u32 user_numerator, u32 user_denominator)
{
	struct rtscam_video_fps *fps = &stream->fps;
	struct v4l2_fract fps_max;
	struct v4l2_fract fps_set;

	if (!rtscam_is_streaming(stream))
		return;

	if (fps->sensor_fps->flag_max)
		return;

	__rtscam_get_sensor_maxfps(fps, &fps_max);

	if (user_denominator * stream->fps.user_setting.numerator
		== stream->fps.user_setting.denominator
		* user_numerator)
		return;

	if ((user_denominator * stream->fps.user_setting.numerator >
		stream->fps.user_setting.denominator * user_numerator)
		&& (user_denominator * fps_max.numerator <=
		fps_max.denominator * user_numerator))
		return;

	if ((user_denominator * stream->fps.user_setting.numerator <
		stream->fps.user_setting.denominator * user_numerator)
		&& (stream->fps.user_setting.denominator *
		fps_max.numerator <= fps_max.denominator *
		stream->fps.user_setting.numerator))
		return;

	if (user_denominator * fps_max.numerator >
		fps_max.denominator * user_numerator) {
		fps_set.denominator = user_denominator;
		fps_set.numerator = user_numerator;
	} else {
		fps_set.denominator = fps_max.denominator;
		fps_set.numerator = fps_max.numerator;
	}

	if (__rtscam_check_sensor_fps(fps, &fps_set) != 0)
		__rtscam_update_sensor_fps_dynamic(stream, fps_set, 0);
}
EXPORT_SYMBOL_GPL(rtscam_adjust_sensor_fps_dynamic);

void rtscam_set_user_fps(struct rtscam_video_fps *fps,
				u32 user_numerator, u32 user_denominator)
{
	struct rtscam_video_stream *stream;

	if (!fps)
		return;

	stream = container_of(fps, struct rtscam_video_stream, fps);

	__normalize_fract(&user_numerator, &user_denominator);
	rtscam_adjust_sensor_fps_dynamic(stream,
			user_numerator, user_denominator);

	fps->user_setting.numerator = user_numerator;
	fps->user_setting.denominator = user_denominator;

	if (fps->sensor_fps->sensor_fps_actual.numerator
		* user_denominator <=
		fps->sensor_fps->sensor_fps_actual.denominator
		* user_numerator && stream->vin_mode != RTS_VIN_MODE_RING_MEM)
		__update_user_fps_actual(fps,
				fps->user_setting);
	else
		__update_user_fps_actual(fps,
			fps->sensor_fps->sensor_fps_actual);

	__rtscam_init_skip_info(stream);
}
EXPORT_SYMBOL_GPL(rtscam_set_user_fps);

static void __set_sensor_fps(struct rtscam_sensor_fps *sensor_fps,
			struct v4l2_fract fps)
{
	if (!sensor_fps)
		return;

	sensor_fps->sensor_fps_setting.numerator = fps.numerator;
	sensor_fps->sensor_fps_setting.denominator =
					fps.denominator;
}

static void __update_sensor_fps_actual(
			struct rtscam_sensor_fps *sensor_fps,
			struct v4l2_fract fps)
{
	int i;
	struct v4l2_fract  fps_user_setting;
	struct v4l2_fract  fps_user_actual;
	struct rtscam_video_stream *stream;
	struct v4l2_fract *fract;

	if (!sensor_fps)
		return;

	if (sensor_fps->sensor_fps_actual.numerator == fps.numerator
		&& sensor_fps->sensor_fps_actual.denominator ==
		fps.denominator)
		return;

	sensor_fps->sensor_fps_actual.numerator = fps.numerator;
	sensor_fps->sensor_fps_actual.denominator = fps.denominator;

	for (i = 0; i < sensor_fps->streamnum; i++) {
		stream = sensor_fps->streams + i;

		fract = &stream->fps.user_setting;
		fps_user_setting.denominator = fract->denominator;
		fps_user_setting.numerator = fract->numerator;
		fract = &stream->fps.user_actual;
		fps_user_actual.denominator = fract->denominator;
		fps_user_actual.numerator = fract->numerator;

		if (fps_user_setting.denominator *
			sensor_fps->sensor_fps_actual.numerator >
			sensor_fps->sensor_fps_actual.denominator
			* fps_user_setting.numerator
			|| stream->vin_mode == RTS_VIN_MODE_RING_MEM) {
			__update_user_fps_actual(&stream->fps,
				sensor_fps->sensor_fps_actual);
		} else if (fps_user_setting.denominator *
			fps_user_actual.numerator !=
			fps_user_setting.numerator *
			fps_user_actual.denominator)
			__update_user_fps_actual(&stream->fps,
				stream->fps.user_setting);

		__rtscam_init_skip_info(stream);
	}
}

static void __rtscam_get_sensor_maxfps(struct rtscam_video_fps *fps,
					struct v4l2_fract *n)
{
	int i;
	struct v4l2_fract n_max = {1, 0};
	struct rtscam_video_stream *stream;
	struct rtscam_video_stream *stream_tmp;

	stream = container_of(fps, struct rtscam_video_stream, fps);
	for (i = 0; i < fps->sensor_fps->streamnum; i++) {
		stream_tmp = fps->sensor_fps->streams + i;
		if (stream_tmp->streamid == stream->streamid)
			continue;

		if (rtscam_is_streaming(stream_tmp)) {
			if (stream_tmp->fps.user_setting.denominator *
				n_max.numerator > n_max.denominator *
				stream_tmp->fps.user_setting.numerator) {
				n_max.numerator =
					stream_tmp->fps.user_setting.numerator;
				n_max.denominator =
				stream_tmp->fps.user_setting.denominator;
			}
		}
	}
	n->numerator = n_max.numerator;
	n->denominator = n_max.denominator;
}

static int  __check_fps_valid(struct v4l2_fract *fps,
	struct v4l2_fract step, struct v4l2_fract min, struct v4l2_fract max)
{
	int m = fps->numerator;
	int n = step.numerator;
	int deno = fps->denominator;
	int tmp;
	int min_com_multi;

	if (m <= 0 || n <= 0)
		return 0;

	if (deno < 0 || step.denominator < 0)
		return 0;

	if (!deno || !step.denominator)
		goto exit;

	min_com_multi = m * n / __calc_gcd(m, n);

	m = deno * (min_com_multi / m);
	n = step.denominator * (min_com_multi / n);

	if (m / n * n == m)
		goto exit;

	deno = m / n * n + n;
	tmp = __calc_gcd(deno, min_com_multi);

	fps->denominator = deno / tmp;
	fps->numerator = min_com_multi / tmp;
exit:
	if (fps->denominator * min.numerator <
		fps->numerator * min.denominator)
		memcpy(fps, &min, sizeof(struct v4l2_fract));
	else if (fps->denominator * max.numerator >
		fps->numerator * max.denominator)
		memcpy(fps, &max, sizeof(struct v4l2_fract));

	return 1;
}

static int __rtscam_check_sensor_fps(struct rtscam_video_fps *fps,
				struct v4l2_fract *n)
{
	int i;
	int flag = 0;
	struct rtscam_sensor_fps *sensor_fps;
	struct v4l2_fract m;
	struct v4l2_fract fps_tmp = {1, 0};
	u32 numerator;
	u32 denominator;

	sensor_fps = fps->sensor_fps;
	m.denominator = sensor_fps->sensor_fps_setting.denominator;
	m.numerator = sensor_fps->sensor_fps_setting.numerator;

	if (*fps->sensor_fps->streaming_count == 0) {
		m.denominator = 0;
		m.numerator = 1;
	}

	if (sensor_fps->desc.type == RTSCAM_SIZE_STEPWISE ||
		sensor_fps->desc.type == RTSCAM_SIZE_CONTINUOUS) {
		flag = __check_fps_valid(n,
			sensor_fps->desc.stepwise.step,
			sensor_fps->desc.stepwise.min,
			sensor_fps->desc.stepwise.max
			);
	} else {
		for (i = 0; i < sensor_fps->desc.discrete.length; i++) {
			if (!sensor_fps->desc.discrete.fps[i].numerator)
				break;

			denominator =
				sensor_fps->desc.discrete.fps[i].denominator;
			numerator = sensor_fps->desc.discrete.fps[i].numerator;

			if (((m.denominator * n->numerator <
				n->denominator * m.numerator) && (denominator
				* n->numerator >= numerator * n->denominator))
				|| ((m.denominator * numerator > denominator *
				m.numerator) && (denominator * n->numerator
				>= n->denominator * numerator))) {
				m.denominator = denominator;
				m.numerator = numerator;
				memcpy(&fps_tmp, &m, sizeof(fps_tmp));
				flag = 1;
			}
		}
		if (flag) {
			n->denominator = fps_tmp.denominator;
			n->numerator = fps_tmp.numerator;
		}
	}

	return flag;
}

static int __rtscam_update_sensor_fps(struct rtscam_video_fps *fps,
					struct v4l2_fract val, int flag)
{
	int i;
	int ret;
	struct rtscam_video_stream *stream_tmp;
	struct rtscam_video_stream *stream = container_of(
				fps, struct rtscam_video_stream, fps);
	struct rtscam_sensor_fps *sensor_fps = fps->sensor_fps;

	if (val.denominator > RTSCAM_SOC_MAX_FPS * val.numerator) {
		rtsprintk(RTS_TRACE_ERROR,
			  "%s:fps is too large\n", __func__);
		return -EINVAL;
	}

	__normalize_fract(&val.numerator, &val.denominator);
	for (i = 0; i < sensor_fps->streamnum; i++) {
		stream_tmp = sensor_fps->streams + i;

		if (stream_tmp->streamid == stream->streamid && flag)
			continue;

		if (!rtscam_is_streaming(stream_tmp))
			continue;

		sensor_fps->set_stream(stream_tmp, 0);
	}
	ret = sensor_fps->set_fps(stream, val);
	if (ret == 0) {
		__set_sensor_fps(sensor_fps, val);
		__update_sensor_fps_actual(sensor_fps, val);
	} else {
		rtsprintk(RTS_TRACE_ERROR, "%s:set fps fail\n", __func__);
	}

	for (i = 0; i < sensor_fps->streamnum; i++) {
		stream_tmp = sensor_fps->streams + i;

		if (stream_tmp->streamid == stream->streamid && flag)
			continue;

		if (!rtscam_is_streaming(stream_tmp))
			continue;

		sensor_fps->set_stream(stream_tmp, 1);
	}

	return ret;
}

int rtscam_update_sensor_fps(struct rtscam_video_stream *stream,
			struct v4l2_fract fps, int flag)
{
	struct rtscam_video_fps *stream_fps = &stream->fps;

	if (stream_fps->sensor_fps->flag_max)
		return 0;

	return __rtscam_update_sensor_fps(stream_fps, fps, flag);
}
EXPORT_SYMBOL_GPL(rtscam_update_sensor_fps);

static void __rtscam_update_sensor_fps_dynamic(
			struct rtscam_video_stream *stream,
			struct v4l2_fract fps, int flag)
{
	struct rtscam_video_fps *stream_fps = &stream->fps;
	struct rtscam_sensor_fps *sensor_fps = stream_fps->sensor_fps;
	int ret;

	if (fps.denominator > RTSCAM_SOC_MAX_FPS * fps.numerator) {
		rtsprintk(RTS_TRACE_ERROR,
				"%s:frmival is too large\n", __func__);
		return;
	}

	if (sensor_fps->dynamic_fps.denominator != 0 &&
			sensor_fps->dynamic_fps.numerator != 0)
		return;

	__normalize_fract(&fps.numerator, &fps.denominator);
	ret = sensor_fps->set_fps_dynamic(stream, fps, flag);
	if (ret == 0) {
		__set_sensor_fps(sensor_fps, fps);
		__update_sensor_fps_actual(sensor_fps, fps);
		rtsprintk(RTS_TRACE_FPS, "dynamic set fps as %d/%d ok\n",
					fps.denominator, fps.numerator);
	} else {
		rtsprintk(RTS_TRACE_ERROR, "dynamic set sensor fps fail\n");
	}
}

void rtscam_adjust_sensor_fps(struct rtscam_video_stream *stream, int enable)
{
	struct v4l2_fract fps_max;
	struct v4l2_fract fps_cur;
	struct v4l2_fract fps_set;
	struct rtscam_video_fps *fps = &stream->fps;
	struct rtscam_sensor_fps *sensor_fps;
	int ret = 0;

	sensor_fps = fps->sensor_fps;
	if (stream->streamid >= sensor_fps->streamnum)
		return;

	fps_cur.denominator = fps->user_setting.denominator;
	fps_cur.numerator = fps->user_setting.numerator;
	__rtscam_get_sensor_maxfps(fps, &fps_max);

	if (fps_cur.denominator * fps_max.numerator >
		fps_cur.numerator * fps_max.denominator) {
		if (enable) {
			fps_set.denominator = fps_cur.denominator;
			fps_set.numerator = fps_cur.numerator;
		} else {
			fps_set.denominator = fps_max.denominator;
			fps_set.numerator = fps_max.numerator;
		}
	} else {
		if (sensor_fps->sensor_fps_setting.denominator !=
			sensor_fps->sensor_fps_actual.denominator ||
			sensor_fps->sensor_fps_setting.numerator !=
			sensor_fps->sensor_fps_actual.numerator) {
			fps_set.denominator =
				sensor_fps->sensor_fps_setting.denominator;
			fps_set.numerator =
				sensor_fps->sensor_fps_setting.numerator;
		}
		goto exit;
	}

	if (!enable && rtscam_is_streaming(stream) &&
	    *fps->sensor_fps->streaming_count == 1)
		goto exit;

	ret = __rtscam_check_sensor_fps(fps, &fps_set);
exit:
	if (ret != 0)
		__rtscam_update_sensor_fps_dynamic(stream, fps_set, 1);

	if (enable)
		__rtscam_init_skip_info(stream);
}
EXPORT_SYMBOL_GPL(rtscam_adjust_sensor_fps);

void rtscam_exec_sensor_fps_setting(
		struct rtscam_video_stream *stream, int enable)
{
	struct rtscam_sensor_fps *sensor_fps = stream->fps.sensor_fps;

	if (!stream->icd->streaming_count) {
		sensor_fps->set_fps(stream,
				sensor_fps->sensor_fps_setting);
		__update_sensor_fps_actual(sensor_fps,
				sensor_fps->sensor_fps_setting);
	}

	if (enable)
		__rtscam_init_skip_info(stream);
}
EXPORT_SYMBOL_GPL(rtscam_exec_sensor_fps_setting);

void rtscam_enable_snr_fps_max(struct rtscam_sensor_fps *sensor_fps)
{
	struct v4l2_fract fps;
	int i;

	if (!sensor_fps)
		return;

	if (sensor_fps->flag_max)
		return;

	if (*sensor_fps->streaming_count) {
		rtsprintk(RTS_TRACE_INFO, "please stop streaming first\n");
		return;
	}

	if (sensor_fps->desc.type == RTSCAM_SIZE_DISCRETE) {
		fps.denominator = sensor_fps->desc.discrete.fps[0].denominator;
		fps.numerator = sensor_fps->desc.discrete.fps[0].numerator;
		for (i = 0; i < sensor_fps->desc.discrete.length; i++) {
			if (!sensor_fps->desc.discrete.fps[i].denominator)
				break;
			if (fps.denominator *
				sensor_fps->desc.discrete.fps[i].numerator
				< sensor_fps->desc.discrete.fps[i].denominator
				* fps.numerator) {
				fps.denominator =
				sensor_fps->desc.discrete.fps[i].denominator;
				fps.numerator =
				sensor_fps->desc.discrete.fps[i].numerator;
			}
		}
	} else {
		fps.denominator = sensor_fps->desc.stepwise.max.denominator;
		fps.numerator = sensor_fps->desc.stepwise.max.numerator;
	}
	if (!fps.denominator) {
		rtsprintk(RTS_TRACE_ERROR, "there is no max fps\n");
		return;
	}

	__normalize_fract(&fps.numerator, &fps.denominator);
	__set_sensor_fps(sensor_fps, fps);
	sensor_fps->flag_max = 1;
}
EXPORT_SYMBOL_GPL(rtscam_enable_snr_fps_max);

void rtscam_disable_snr_fps_max(struct rtscam_sensor_fps *sensor_fps)
{
	if (!sensor_fps)
		return;

	if (!sensor_fps->flag_max)
		return;

	if (*sensor_fps->streaming_count) {
		rtsprintk(RTS_TRACE_INFO, "please stop streaming first\n");
		return;
	}

	sensor_fps->flag_max = 0;
}
EXPORT_SYMBOL_GPL(rtscam_disable_snr_fps_max);

int rtscam_change_dynamic_fps(struct rtscam_sensor_fps *sensor_fps,
			struct v4l2_fract fps)
{
	if (!sensor_fps)
		return -EINVAL;

	if (fps.denominator <= 0 || fps.numerator <= 0) {
		rtsprintk(RTS_TRACE_ERROR, "invalid dynamic fps : %d/%d\n",
				fps.denominator, fps.numerator);
		return -EINVAL;
	}

	__normalize_fract(&fps.numerator, &fps.denominator);
	__update_sensor_fps_actual(sensor_fps, fps);

	if (sensor_fps->sensor_fps_setting.numerator * fps.denominator
		!= sensor_fps->sensor_fps_setting.denominator * fps.numerator)
		sensor_fps->dynamic_fps = fps;
	else {
		sensor_fps->dynamic_fps.denominator = 0;
		sensor_fps->dynamic_fps.numerator = 1;
	}

	rtsprintk(RTS_TRACE_FPS, "dynamic fps change to: %d/%d\n",
		sensor_fps->sensor_fps_actual.denominator,
		sensor_fps->sensor_fps_actual.numerator);

	return 0;
}
EXPORT_SYMBOL_GPL(rtscam_change_dynamic_fps);

struct rtscam_video_frmival *rtscam_get_video_frmival(
		struct rtscam_video_stream *stream, u32 fourcc,
		u32 width, u32 height)
{
	struct rtscam_video_format *format;
	struct rtscam_video_frmival *frmival = NULL;

	format = find_format_by_fourcc(stream, fourcc);

	if (format == NULL) {
		rtsprintk(RTS_TRACE_ERROR,
			  "please register format(%c%c%c%c) first\n",
			  v4l2pixfmtstr(fourcc));
		return NULL;
	}

	if (format->frame_type == RTSCAM_SIZE_STEPWISE ||
	    format->frame_type == RTSCAM_SIZE_CONTINUOUS) {
		if (width < format->stepwise.min.width)
			return NULL;
		if (width > format->stepwise.max.width)
			return NULL;
		if ((width - format->stepwise.min.width) %
		    format->stepwise.step.width != 0)
			return NULL;
		if (height < format->stepwise.min.height)
			return NULL;
		if (height > format->stepwise.max.height)
			return NULL;
		if ((height - format->stepwise.min.height) %
		    format->stepwise.step.height != 0)
			return NULL;
		frmival = &format->stepwise.frmival;
	} else if (format->frame_type == RTSCAM_SIZE_DISCRETE) {
		struct rtscam_video_frame *frame = find_frame(format,
							      width, height);
		if (frame == NULL)
			return NULL;
		frmival = &frame->frmival;
	}

	return frmival;
}

int rtscam_register_frmival(struct rtscam_video_stream *stream,
				     __u32 fourcc,
				     struct rtscam_frame_size *size)
{
	struct rtscam_video_frmival *frmival = NULL;
	struct v4l2_fract fps = stream->fps.sensor_fps->sensor_fps_setting;
	u8 type = stream->fps.sensor_fps->desc.type;

	if (fps.denominator > fps.numerator * RTSCAM_SOC_MAX_FPS) {
		fps.denominator = RTSCAM_SOC_MAX_FPS;
		fps.numerator = 1;
	}

	frmival = rtscam_get_video_frmival(stream, fourcc,
			size->width, size->height);
	if (frmival == NULL)
		return -EINVAL;

	if (frmival->initialized && frmival->frmival_type != type)
		return -EINVAL;

	if (stream->user_format == fourcc &&
	    stream->user_width == size->width &&
	    stream->user_height == size->height)
		rtscam_set_user_fps(&stream->fps,
			fps.numerator, fps.denominator);

	if (type == RTSCAM_SIZE_DISCRETE)
		frmival->discrete.frmivals =
			stream->fps.sensor_fps->discrete.frmivals;
	else
		memcpy(&frmival->stepwise,
			&stream->fps.sensor_fps->stepwise,
			sizeof(frmival->stepwise));

	frmival->initialized = 1;
	frmival->frmival_type = type;

	return 0;
}
EXPORT_SYMBOL_GPL(rtscam_register_frmival);

int rtscam_clr_frmival(struct rtscam_video_frmival *frmival)
{
	if (!frmival->initialized)
		return 0;

	if (frmival->frmival_type == RTSCAM_SIZE_DISCRETE)
		frmival->discrete.frmivals = NULL;
	else
		memset(&frmival->stepwise, 0,
			sizeof(frmival->stepwise));

	frmival->initialized = 0;

	return 0;
}

static int __rtscam_register_frmival_discrete(
		struct rtscam_sensor_fps *sensor_fps, struct v4l2_fract *ival)
{
	struct rtscam_frame_frmival *p;
	struct rtscam_frame_frmival *fival;

	p = sensor_fps->discrete.frmivals;
	while (p) {
		if (p->frmival.numerator == ival->numerator &&
		    p->frmival.denominator == ival->denominator)
			return -EEXIST;
		p = p->next;
	}
	fival = kzalloc(sizeof(*fival), GFP_KERNEL);
	if (!fival)
		return -ENOMEM;

	fival->frmival.numerator = ival->numerator;
	fival->frmival.denominator = ival->denominator;
	fival->next = NULL;

	if (sensor_fps->discrete.frmivals == NULL) {
		sensor_fps->discrete.frmivals = fival;
	} else {
		p = sensor_fps->discrete.frmivals;
		while (p->next)
			p = p->next;
		p->next = fival;
	}

	return 0;
}

int rtscam_release_sensor_fps(struct rtscam_sensor_fps *sensor_fps)
{
	struct rtscam_frame_frmival *p;
	struct rtscam_frame_frmival *next;

	if (!sensor_fps)
		return -EINVAL;

	if (sensor_fps->desc.type != RTSCAM_SIZE_DISCRETE)
		return 0;

	p = sensor_fps->discrete.frmivals;
	while (p) {
		next = p->next;
		kfree(p);
		p = next;
	}
	sensor_fps->discrete.frmivals = NULL;
	return 0;
}
EXPORT_SYMBOL_GPL(rtscam_release_sensor_fps);

int rtscam_init_sensor_fps(struct rtscam_sensor_fps *sensor_fps, int flag_max)
{
	int i;
	int ret;
	struct v4l2_fract fps = {1, 0};
	struct rtscam_soc_fps_descriptor *desc = &sensor_fps->desc;

	if (!sensor_fps || (!desc->discrete.fps &&
		desc->type == RTSCAM_SIZE_DISCRETE))
		return -EINVAL;

	sensor_fps->flag_max = flag_max;
	if (desc->type == RTSCAM_SIZE_DISCRETE) {
		for (i = 0; i < desc->discrete.length; i++) {
			if (!desc->discrete.fps[i].denominator)
				break;

			if (fps.denominator * desc->discrete.fps[i].numerator <
				desc->discrete.fps[i].denominator *
				fps.numerator) {
				fps.denominator =
					desc->discrete.fps[i].denominator;
				fps.numerator =
					desc->discrete.fps[i].numerator;
			}
		}
	} else {
		fps.denominator = desc->stepwise.max.denominator;
		fps.numerator = desc->stepwise.max.numerator;
	}

	if (fps.denominator > RTSCAM_SOC_MAX_FPS * fps.numerator) {
		fps.denominator = RTSCAM_SOC_MAX_FPS;
		fps.numerator = 1;
	}
	__normalize_fract(&fps.numerator, &fps.denominator);
	__set_sensor_fps(sensor_fps, fps);

	if (desc->type == RTSCAM_SIZE_DISCRETE) {
		for (i = 0; i < desc->discrete.length; i++) {
			if (!desc->discrete.fps[i].denominator)
				break;
			ret = __rtscam_register_frmival_discrete(
				sensor_fps, &desc->discrete.fps[i]);
			if (ret)
				goto error;
		}
	} else
		memcpy(&sensor_fps->stepwise, &desc->stepwise,
				sizeof(sensor_fps->stepwise));

	return 0;

error:
	rtscam_release_sensor_fps(sensor_fps);
	return ret;
}
EXPORT_SYMBOL_GPL(rtscam_init_sensor_fps);
