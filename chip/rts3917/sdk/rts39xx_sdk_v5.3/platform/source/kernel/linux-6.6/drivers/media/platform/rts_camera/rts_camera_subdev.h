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

#ifndef _RTS_CAMERA_SUBDEV_H
#define _RTS_CAMERA_SUBDEV_H

#include "rts_camera_config.h"
#include "linux/rts_camera_soc.h"

struct rtscam_subdev_dev_desc {
	u8 length;
	u8 type;
	u16 hwversion;
	u16 fwversion;
	u8 streamnum;
	u8 frmivalnum;
};

struct rtscam_subdev_unit_desc {
	u8 length;
	u8 type;
	u8 controlsize;
	u8 bmcontrols[32];

	unsigned int ncontrols;
	struct rtscam_video_ctrl *controls;
};

enum {
	RTSCAM_EVT_FPS_DYNAMIC_CHANGED = 0,
	RTSCAM_EVT_RESERVED
};

struct rtscam_subdev_strm_desc {
	u32 format_bitmap;
	u32 width;
	u32 height;
};

enum {
	RTSCAM_SUBDEV_FPS_DISCRETE = 0,
	RTSCAM_SUBDEV_FPS_CONTINUOUS
};

struct rtscam_subdev_fps {
	u8 type;
	union {
		struct {
			struct v4l2_fract fps[RTSCAM_MAX_FPS_COUNT];
		} discrete;
		struct {
			struct v4l2_fract max;
			struct v4l2_fract min;
			struct v4l2_fract step;
		} stepwise;
	};
};


struct rtscam_subdev_desc {
	struct rtscam_subdev_dev_desc dev_desc;
	struct rtscam_subdev_unit_desc entities[3];
	struct rtscam_subdev_strm_desc strms[RTSCAM_MAX_STM_COUNT];
	struct rtscam_subdev_fps fps;
};

struct rtscam_subdev_crop_info {
	int mode;
	__u32 start_x;
	__u32 start_y;
	__u32 width;
	__u32 height;
};

struct rtscam_subdev_t {
	struct device *dev;
	struct rtscam_subdev_desc desc;

	int (*enable)(struct rtscam_subdev_t *sub, int enable);
	int (*set_stream)(struct rtscam_subdev_t *sub,
			  int streamid, int enable);
	int (*pause_stream)(struct rtscam_subdev_t *sub,
			    int streamid, int resume);
	int (*set_fmt)(struct rtscam_subdev_t *sub, int streamid,
		       u32 fmt, u32 w, u32 h);
	int (*get_fmt)(struct rtscam_subdev_t *sub, int streamid,
		       u32 *fmt, u32 *w, u32 *h);
	int (*set_crop)(struct rtscam_subdev_t *sub, int streamid,
			struct rtscam_subdev_crop_info *crop);
	int (*get_crop)(struct rtscam_subdev_t *sub, int streamid,
			struct rtscam_subdev_crop_info *crop);
	int (*set_fps)(struct rtscam_subdev_t *sub, int streamid,
		struct v4l2_fract fps);
	int (*get_fps)(struct rtscam_subdev_t *sub, int streamid,
		struct v4l2_fract *fps);
	u32 (*read_reg)(struct rtscam_subdev_t *sub, off_t reg);
	void (*write_reg)(struct rtscam_subdev_t *sub, u32 value, off_t reg);
	int (*set_hook)(struct rtscam_subdev_t *sub, void *master,
			int (*hook)(void *master, int id, void *arg));
	int (*query_ctrl)(struct rtscam_subdev_t *sub, void *cmd);
	int (*exec_cmd)(struct rtscam_subdev_t *sub, void *cmd);

	int (*set_ive_ctrl)(struct rtscam_subdev_t *sub,
			    struct rtscam_soc_ive_ctrl *ctrl);
	int (*get_ive_ctrl)(struct rtscam_subdev_t *sub,
			    struct rtscam_soc_ive_ctrl *ctrl);
	void *master;
};

int rtscam_register_subdev(struct rtscam_subdev_t *subdev);
int rtscam_unregister_subdev(struct rtscam_subdev_t *subdev);

int rtscam_register_subdev_ext(struct rtscam_subdev_t *subdev);
int rtscam_unregister_subdev_ext(struct rtscam_subdev_t *subdev);

void rtscam_soc_isp_control(u8 idx, int enable);
void rtscam_soc_reset_isp_reg(u8 idx);

int rtscam_unregister_subdev_update(struct rtscam_subdev_t *subdev,
	int force_reset);
int rtscam_register_subdev_update(struct rtscam_subdev_t *subdev,
	int force_reset);

#endif
