/* SPDX-License-Identifier: GPL-2.0-only */
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

#ifndef _RTS_CAMERA_FPS_H
#define _RTS_CAMERA_FPS_H

#include <linux/workqueue.h>
#include <linux/videodev2.h>
#include "rts_camera_config.h"

#define RTSCAM_SOC_MAX_FPS			300
#define RTSCAM_SOC_STEP_FPS			1
#define RTSCAM_SOC_FPS_CVRT_NUMERATOR		1000

struct rtscam_soc_skip_info {
	int m;
	int n;
	int flag;
	int count;
	int index;
};

struct rtscam_soc_fps_descriptor {
	u8 type;
	union {
		struct {
			struct v4l2_fract *fps;
			u16 length;
		} discrete;
		struct {
			struct v4l2_fract max;
			struct v4l2_fract min;
			struct v4l2_fract step;
		} stepwise;
	};
};

struct rtscam_sensor_fps {
	struct v4l2_fract sensor_fps_setting;
	struct v4l2_fract sensor_fps_actual;
	struct v4l2_fract dynamic_fps; //0: normal status; >0: status of dynamic fps reduction

	union {
		struct {
			struct rtscam_frame_frmival *frmivals;
		} discrete;
		struct {
			struct v4l2_fract max;
			struct v4l2_fract min;
			struct v4l2_fract step;
		} stepwise;
	};
	struct rtscam_soc_fps_descriptor desc;

	u8 streamnum;
	int *streaming_count;
	struct rtscam_video_stream *streams;

	int flag_max;

	int (*set_fps)(struct rtscam_video_stream *stream,
			struct v4l2_fract fps);
	int (*set_fps_dynamic)(struct rtscam_video_stream *stream,
			struct v4l2_fract fps, int flag);
	int (*set_stream)(struct rtscam_video_stream *stream, int enable);
};

struct rtscam_video_fps {
	struct v4l2_fract user_setting;
	struct v4l2_fract user_actual;

	struct rtscam_soc_skip_info skip_info;

	struct rtscam_sensor_fps *sensor_fps;
};

void rtscam_set_user_fps(struct rtscam_video_fps *fps,
				u32 user_numerator, u32 user_denominator);
void rtscam_set_user_fps_actual(struct rtscam_video_fps *fps);
int rtscam_update_sensor_fps(struct rtscam_video_stream *stream,
				struct v4l2_fract fps, int flag);
void rtscam_adjust_sensor_fps(
			struct rtscam_video_stream *stream, int enable);
void rtscam_adjust_sensor_fps_dynamic(struct rtscam_video_stream *stream,
			u32 user_numerator, u32 user_denominator);
void rtscam_exec_sensor_fps_setting(
			struct rtscam_video_stream *stream, int enable);
void rtscam_enable_snr_fps_max(struct rtscam_sensor_fps *sensor_fps);
void rtscam_disable_snr_fps_max(struct rtscam_sensor_fps *sensor_fps);

int rtscam_change_dynamic_fps(struct rtscam_sensor_fps *sensor_fps,
			struct v4l2_fract fps);
int rtscam_skip_frame(struct rtscam_video_stream *stream);

int rtscam_init_sensor_fps(struct rtscam_sensor_fps *sensor_fps, int flag_max);
int rtscam_release_sensor_fps(struct rtscam_sensor_fps *sensor_fps);

struct rtscam_frame_size;
int rtscam_register_frmival(struct rtscam_video_stream *stream,
		__u32 fourcc, struct rtscam_frame_size *size);

#endif
