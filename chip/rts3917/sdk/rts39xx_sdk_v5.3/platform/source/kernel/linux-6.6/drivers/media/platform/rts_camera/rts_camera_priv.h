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

#ifndef _RTS_CAMERA_PRIV_H
#define _RTS_CAMERA_PRIV_H

#include "rts_camera.h"

#define RTSCAM_MAX_BUFFER_NUM			16

#define rtscam_call_video_op(icd, op, args...)		\
	(((icd)->ops->op) ? ((icd)->ops->op(args)) : -EINVAL)

u64 rtscam_get_timestamp(void);


int rtscam_video_init_ctrl(struct rtscam_video_device *icd);
int rtscam_video_release_ctrl(struct rtscam_video_device *icd);

int rtscam_query_v4l2_ctrl(struct rtscam_video_device *icd,
			   struct v4l2_queryctrl *v4l2_ctrl);
int rtscam_get_ctrl(struct rtscam_video_device *icd,
		    struct v4l2_control *ctrl);
int rtscam_set_ctrl(struct rtscam_video_device *icd,
		    struct v4l2_control *ctrl);
int rtscam_get_ext_ctrls(struct rtscam_video_device *icd,
			 struct v4l2_ext_controls *ctrls);
int rtscam_set_ext_ctrls(struct rtscam_video_device *icd,
			 struct v4l2_ext_controls *ctrls);
int rtscam_try_ext_ctrls(struct rtscam_video_device *icd,
			 struct v4l2_ext_controls *ctrls);

struct rtscam_video_format *find_format_by_fourcc(
		struct rtscam_video_stream *stream, u32 fourcc);

struct rtscam_video_frmival *rtscam_get_video_frmival(
		struct rtscam_video_stream *stream, u32 fourcc,
		u32 width, u32 height);
int rtscam_clr_frmival(struct rtscam_video_frmival *frmival);

int rtscam_check_stream_format(struct rtscam_video_stream *stream);
int rtscam_check_user_format(struct rtscam_video_stream *stream,
			     u32 user_format, u32 user_width, u32 user_height,
			     u32 user_numerator, u32 user_denominator);
int rtscam_try_user_format(struct rtscam_video_stream *stream,
			   u32 user_format, u32 user_width, u32 user_height);
int rtscam_set_user_format(struct rtscam_video_stream *stream,
			   u32 user_format, u32 user_width, u32 user_height);
int rtscam_set_user_frmival(struct rtscam_video_stream *stream,
			    u32 user_numerator, u32 user_denominator);
int rtscam_get_max_frame_size(struct rtscam_video_format *format,
			      struct rtscam_frame_size *max);

#define v4l2pixfmtstr(x)	(x) & 0xff, ((x) >> 8) & 0xff, ((x) >> 16) & 0xff, ((x) >> 24) & 0xff

#endif
