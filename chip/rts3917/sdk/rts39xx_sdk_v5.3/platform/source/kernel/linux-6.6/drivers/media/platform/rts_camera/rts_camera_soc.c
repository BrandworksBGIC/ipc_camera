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

#define TAG "SOC_CAMERA"
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/sysfs.h>
#include <linux/delay.h>
#include <asm/unaligned.h>
#include <linux/videodev2.h>
#include <media/videobuf2-memops.h>
#include <media/videobuf2-dma-contig.h>
#include <linux/firmware.h>
#include <linux/kfifo.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/of.h>

#include "linux/rts_camera_soc.h"
#include "rts_camera.h"
#include "rts_camera_soc.h"
#include "rts_camera_soc_priv.h"
#include "rts_isp_mem.h"
#include "rts-dma-contig.h"
#include "rts_hw_id.h"
#include "rts_camera_fps.h"
#include "rts_camera_subdev.h"

#define soc_info(...)		rtsprintk(RTS_TRACE_INFO, __VA_ARGS__)
#define soc_err(...)		rtsprintk(RTS_TRACE_ERROR, __VA_ARGS__)

#define RTS_SOC_CAMERA_DRV_NAME		"rts_soc_camera"

#define RTS_SOC_CAM_DEV_NAME		"rtscam"
#define RTS_SOC_CTRL_DEV_NAME		"rtscamctrl"

struct rtscam_soc_icfg {
	int found;
	struct rtscam_region y;
	struct rtscam_region uv;
};

struct rtscam_soc_rgbcfg {
	struct rtscam_region rgb[3];
};

struct rtscam_soc_dev {
	struct device *dev;
	void __iomem *base;

	unsigned long iostart;
	unsigned int iosize;

	int initialized;
	atomic_t init_count;

	const struct vb2_mem_ops *mem_ops;

	struct rtscam_sensor_fps sensor_fps;
	struct rtscam_video_device rvdev;

	struct rtscam_soc_slot_info slot_info[RTSCAM_MAX_STM_COUNT];

	struct rtscam_ge_device *mem_dev;
	struct rtscam_ge_device *cam_dev;
	struct rtscam_ge_device *ctrl_dev;

	struct rtscam_region td_config;
	struct rtscam_soc_icfg icfgs[RTSCAM_YUV_MAX_STRM_NUM];
	unsigned int icfg_count;
	struct rtscam_soc_rgbcfg rgbcfg;

	char name[PLATFORM_NAME_SIZE];
	kernel_ulong_t devtype;
	struct rtscam_mem_info *rtsmem;
	int pause_flag;
	unsigned long drop_frames;
	unsigned long drops[RTSCAM_MAX_STM_COUNT];

	int keep_user_setting;

	struct rtscam_subdev_t *subdev;
	struct rtscam_soc_video_in *video_in;
};

struct rtscam_soc_snr_fmt {
	u16 width;
	u16 height;
	u8 snr_fmt;
};

static struct rtscam_soc_dev *m_rsocdev;

static struct rtscam_video_format_xlate m_rtscam_soc_formats[] = {
	{
		.index = 0,
		.name = "SEMIPLANAR YCBCR 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV12,
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.field = V4L2_FIELD_NONE,
		.bpp = 12,
		.is_yuv = true,
		.rts_code = RTSCAM_FORMAT_TYPE_YUV420_SEMIPLANAR,
	},
	{
		.index = 1,
		.name = "SEMIPLANAR YCBCR 4:2:2",
		.fourcc = V4L2_PIX_FMT_NV16,
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.field = V4L2_FIELD_NONE,
		.bpp = 16,
		.is_yuv = true,
		.rts_code = RTSCAM_FORMAT_TYPE_YUV422_SEMIPLANAR,
	},
	{
		.index = 2,
		.name = "RGB",
		.fourcc = V4L2_PIX_FMT_RGB24,
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.field = V4L2_FIELD_NONE,
		.bpp = 24,
		.is_yuv = false,
		.rts_code = RTSCAM_FORMAT_TYPE_RGB,
	},
	{
		.index = 3,
		.name = "YONLY",
		.fourcc = V4L2_PIX_FMT_Y8I,
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.field = V4L2_FIELD_NONE,
		.bpp = 8,
		.is_yuv = false,
		.rts_code = RTSCAM_FORMAT_TYPE_Y_ONLY,
	},
	{
		.index = 4,
		.name = "INTERLEAVE YUYV 4:2:2",
		.fourcc = V4L2_PIX_FMT_YUYV,
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.field = V4L2_FIELD_NONE,
		.bpp = 16,
		.is_yuv = true,
		.rts_code = RTSCAM_FORMAT_TYPE_YUYV,
	},
	{
		.index = 5,
		.name = "INTERLEAVE YVYU 4:2:2",
		.fourcc = V4L2_PIX_FMT_YVYU,
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.field = V4L2_FIELD_NONE,
		.bpp = 16,
		.is_yuv = true,
		.rts_code = RTSCAM_FORMAT_TYPE_YVYU,
	},
	{
		.index = 6,
		.name = "RGB/YUV 128bit 3pixel",
		.fourcc = V4L2_PIX_FMT_128BIT_3PIXEL,
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.field = V4L2_FIELD_NONE,
		.bpp = 128,
		.is_yuv = false,
		.rts_code = RTSCAM_FORMAT_TYPE_128BIT_3PIXEL,
	},
};

static struct rtscam_video_ctrl_menu exposure_auto_controls[] = {
	{ 1, "Manual Mode" },
	{ 8, "Auto Mode" },
};

static struct rtscam_video_ctrl_info m_rtscam_soc_ctrls[] = {
	{
		.index = 0,
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_BRIGHTNESS_CONTROL,
		.size = 2,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_RANGE |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 1,
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_CONTRAST_CONTROL,
		.size = 2,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_RANGE |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 2,
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_HUE_CONTROL,
		.size = 2,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_RANGE |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 3,
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_SATURATION_CONTROL,
		.size = 2,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_RANGE |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 4,
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_SHARPNESS_CONTROL,
		.size = 2,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_RANGE |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 5,
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_GAMMA_CONTROL,
		.size = 2,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_RANGE |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 6,
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_WHITE_BALANCE_TEMPERATURE_CONTROL,
		.size = 2,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_RANGE |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 7,
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_WHITE_BALANCE_COMPONENT_CONTROL,
		.size = 6,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_RANGE |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 8,
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_BACKLIGHT_COMPENSATION_CONTROL,
		.size = 2,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_RANGE |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 9,
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_GAIN_CONTROL,
		.size = 2,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_RANGE |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 10,
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_POWER_LINE_FREQUENCY_CONTROL,
		.size = 1,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_RANGE |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 11,
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_HUE_AUTO_CONTROL,
		.size = 1,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_CUR |
		RTS_CTRL_FLAG_GET_DEF |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 12,
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL,
		.size = 1,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_RANGE |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 13,
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL,
		.size = 1,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_CUR |
		RTS_CTRL_FLAG_GET_DEF |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 14,
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_DIGITAL_MULTIPLIER_CONTROL,
		.size = 2,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_RANGE |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 15,
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL,
		.size = 2,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_RANGE |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 0,
		.unit = RTSCAM_SOC_CAMERA_TYPE,
		.selector = RTSCAM_SOC_CT_SCANNING_MODE_CONTROL,
		.size = 1,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_CUR |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 1,
		.unit = RTSCAM_SOC_CAMERA_TYPE,
		.selector = RTSCAM_SOC_CT_AE_MODE_CONTROL,
		.size = 1,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_CUR |
		RTS_CTRL_FLAG_GET_DEF |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 2,
		.unit = RTSCAM_SOC_CAMERA_TYPE,
		.selector = RTSCAM_SOC_CT_AE_PRIORITY_CONTROL,
		.size = 1,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_CUR |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 3,
		.unit = RTSCAM_SOC_CAMERA_TYPE,
		.selector = RTSCAM_SOC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL,
		.size = 4,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_RANGE |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 4,
		.unit = RTSCAM_SOC_CAMERA_TYPE,
		.selector = RTSCAM_SOC_CT_EXPOSURE_TIME_RELATIVE_CONTROL,
		.size = 1,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_CUR |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 5,
		.unit = RTSCAM_SOC_CAMERA_TYPE,
		.selector = RTSCAM_SOC_CT_FOCUS_ABSOLUTE_CONTROL,
		.size = 2,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_RANGE |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 6,
		.unit = RTSCAM_SOC_CAMERA_TYPE,
		.selector = RTSCAM_SOC_CT_FOCUS_RELATIVE_CONTROL,
		.size = 2,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_RANGE |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 7,
		.unit = RTSCAM_SOC_CAMERA_TYPE,
		.selector = RTSCAM_SOC_CT_IRIS_ABSOLUTE_CONTROL,
		.size = 2,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_RANGE |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 8,
		.unit = RTSCAM_SOC_CAMERA_TYPE,
		.selector = RTSCAM_SOC_CT_IRIS_RELATIVE_CONTROL,
		.size = 1,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_CUR |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 9,
		.unit = RTSCAM_SOC_CAMERA_TYPE,
		.selector = RTSCAM_SOC_CT_ZOOM_ABSOLUTE_CONTROL,
		.size = 2,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_RANGE |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 10,
		.unit = RTSCAM_SOC_CAMERA_TYPE,
		.selector = RTSCAM_SOC_CT_ZOOM_RELATIVE_CONTROL,
		.size = 3,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_RANGE |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 11,
		.unit = RTSCAM_SOC_CAMERA_TYPE,
		.selector = RTSCAM_SOC_CT_PANTILT_ABSOLUTE_CONTROL,
		.size = 8,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_RANGE |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 12,
		.unit = RTSCAM_SOC_CAMERA_TYPE,
		.selector = RTSCAM_SOC_CT_PANTILT_RELATIVE_CONTROL,
		.size = 4,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_RANGE |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 13,
		.unit = RTSCAM_SOC_CAMERA_TYPE,
		.selector = RTSCAM_SOC_CT_ROLL_ABSOLUTE_CONTROL,
		.size = 2,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_RANGE |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 14,
		.unit = RTSCAM_SOC_CAMERA_TYPE,
		.selector = RTSCAM_SOC_CT_ROLL_RELATIVE_CONTROL,
		.size = 2,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_RANGE |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 30,
		.unit = RTSCAM_SOC_RTK_EXT_CTL_TYPE,
		.selector = RTSCAM_SOC_RTK_EXT_GAIN_CONTROL,
		.size = 2,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_RANGE |
		RTS_CTRL_FLAG_GET_INFO,
	},
	{
		.index = 31,
		.unit = RTSCAM_SOC_RTK_EXT_CTL_TYPE,
		.selector = RTSCAM_SOC_RTK_EXT_SNR_CROP_CTRL,
		.size = 4,
		.flags = RTS_CTRL_FLAG_SET_CUR |
		RTS_CTRL_FLAG_GET_RANGE |
		RTS_CTRL_FLAG_GET_INFO,
	},
};

static struct rtscam_video_ctrl_mapping m_rtscam_soc_ctrl_mappings[] = {
	{
		.id = V4L2_CID_BRIGHTNESS,
		.name = "Brightness",
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_BRIGHTNESS_CONTROL,
		.size = 16,
		.offset = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = RTS_CTRL_DATA_TYPE_SIGNED,
	},
	{
		.id = V4L2_CID_CONTRAST,
		.name = "Contrast",
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_CONTRAST_CONTROL,
		.size = 16,
		.offset = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = RTS_CTRL_DATA_TYPE_UNSIGNED,
	},
	{
		.id = V4L2_CID_HUE,
		.name = "Hue",
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_HUE_CONTROL,
		.size = 16,
		.offset = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = RTS_CTRL_DATA_TYPE_SIGNED,
	},
	{
		.id = V4L2_CID_SATURATION,
		.name = "Saturation",
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_SATURATION_CONTROL,
		.size = 16,
		.offset = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = RTS_CTRL_DATA_TYPE_UNSIGNED,
	},
	{
		.id = V4L2_CID_SHARPNESS,
		.name = "Sharpness",
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_SHARPNESS_CONTROL,
		.size = 16,
		.offset = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = RTS_CTRL_DATA_TYPE_UNSIGNED,
	},
	{
		.id = V4L2_CID_GAMMA,
		.name = "Gamma",
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_GAMMA_CONTROL,
		.size  = 16,
		.offset = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = RTS_CTRL_DATA_TYPE_UNSIGNED,
	},
	{
		.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE,
		.name = "White Balance Temperature",
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_WHITE_BALANCE_TEMPERATURE_CONTROL,
		.size = 16,
		.offset = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = RTS_CTRL_DATA_TYPE_UNSIGNED,
	},
	{
		.id = V4L2_CID_RED_BALANCE,
		.name = "Red Balance",
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector =  RTSCAM_SOC_PU_WHITE_BALANCE_COMPONENT_CONTROL,
		.size = 16,
		.offset = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = RTS_CTRL_DATA_TYPE_UNSIGNED,
	},
	{
		.id = RTS_V4L2_CID_GREEN_BALANCE,
		.name = "Green Balance",
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector =  RTSCAM_SOC_PU_WHITE_BALANCE_COMPONENT_CONTROL,
		.size = 16,
		.offset = 16,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = RTS_CTRL_DATA_TYPE_UNSIGNED,
	},
	{
		.id = V4L2_CID_BLUE_BALANCE,
		.name = "Blue Balance",
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector =  RTSCAM_SOC_PU_WHITE_BALANCE_COMPONENT_CONTROL,
		.size = 16,
		.offset = 32,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = RTS_CTRL_DATA_TYPE_UNSIGNED,
	},
	{
		.id = V4L2_CID_BACKLIGHT_COMPENSATION,
		.name = "Backlight Compensation",
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector =  RTSCAM_SOC_PU_BACKLIGHT_COMPENSATION_CONTROL,
		.size = 16,
		.offset = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = RTS_CTRL_DATA_TYPE_UNSIGNED,
	},
	{
		.id = RTS_V4L2_CID_Y_GAIN,
		.name = "y gain",
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_GAIN_CONTROL,
		.size = 16,
		.offset = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = RTS_CTRL_DATA_TYPE_UNSIGNED,
	},
	{
		.id = V4L2_CID_POWER_LINE_FREQUENCY,
		.name = "Power Line Frequency",
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_POWER_LINE_FREQUENCY_CONTROL,
		.size = 2,
		.offset = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = RTS_CTRL_DATA_TYPE_UNSIGNED,
	},
	{
		.id = V4L2_CID_HUE_AUTO,
		.name = "Hue, Automatic",
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_HUE_AUTO_CONTROL,
		.size = 1,
		.offset = 0,
		.v4l2_type = V4L2_CTRL_TYPE_BOOLEAN,
		.data_type = RTS_CTRL_DATA_TYPE_BOOLEAN,
	},
	{
		.id = V4L2_CID_AUTO_WHITE_BALANCE,
		.name = "White Balance, Automatic",
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL,
		.size = 8,
		.offset = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = RTS_CTRL_DATA_TYPE_UNSIGNED,
	},
	{
		.id = RTS_V4L2_CID_AUTO_WHITE_BALANCE_COMPONENT,
		.name = "White Balance Component, Auto",
		.unit = RTSCAM_SOC_ISP_PROCESSING_TYPE,
		.selector = RTSCAM_SOC_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL,
		.size = 1,
		.offset = 0,
		.v4l2_type = V4L2_CTRL_TYPE_BOOLEAN,
		.data_type = RTS_CTRL_DATA_TYPE_BOOLEAN,
	},
	/* Digital Multiplier, not defined */
	/* Digital Multiplier Limit, not defined */
	/* Scanning Mode, not defined */
	{
		.id = V4L2_CID_AUTOGAIN,
		.name = "Gain, Automatic",
		.unit = RTSCAM_SOC_CAMERA_TYPE,
		.selector = RTSCAM_SOC_CT_AE_MODE_CONTROL,
		.size = 4,
		.offset = 0,
		.v4l2_type = V4L2_CTRL_TYPE_MENU,
		.data_type = RTS_CTRL_DATA_TYPE_BITMASK,
		.menu_info = exposure_auto_controls,
		.menu_count = ARRAY_SIZE(exposure_auto_controls),
	},
	{
		.id = V4L2_CID_EXPOSURE_AUTO_PRIORITY,
		.name = "Exposure, Dynamic Framerate",
		.unit = RTSCAM_SOC_CAMERA_TYPE,
		.selector = RTSCAM_SOC_CT_AE_PRIORITY_CONTROL,
		.size = 1,
		.offset = 0,
		.v4l2_type = V4L2_CTRL_TYPE_BOOLEAN,
		.data_type = RTS_CTRL_DATA_TYPE_BOOLEAN,
	},
	{
		.id = V4L2_CID_EXPOSURE_ABSOLUTE,
		.name = "Exposure Time, Absolute",
		.unit = RTSCAM_SOC_CAMERA_TYPE,
		.selector = RTSCAM_SOC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL,
		.size = 32,
		.offset = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = RTS_CTRL_DATA_TYPE_UNSIGNED,
	},
	/* Exposure Time (Relative) */
	{
		.id = V4L2_CID_FOCUS_ABSOLUTE,
		.name = "Focus, Absolute",
		.unit = RTSCAM_SOC_CAMERA_TYPE,
		.selector = RTSCAM_SOC_CT_FOCUS_ABSOLUTE_CONTROL,
		.size = 16,
		.offset = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = RTS_CTRL_DATA_TYPE_UNSIGNED,
	},
	/* Focus (Relative) */
	{
		.id = V4L2_CID_IRIS_ABSOLUTE,
		.name = "Iris, Absolute",
		.unit = RTSCAM_SOC_CAMERA_TYPE,
		.selector = RTSCAM_SOC_CT_IRIS_ABSOLUTE_CONTROL,
		.size = 16,
		.offset = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = RTS_CTRL_DATA_TYPE_UNSIGNED,
	},
	{
		.id = V4L2_CID_IRIS_RELATIVE,
		.name = "Iris, Relative",
		.unit = RTSCAM_SOC_CAMERA_TYPE,
		.selector = RTSCAM_SOC_CT_IRIS_RELATIVE_CONTROL,
		.size = 8,
		.offset = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = RTS_CTRL_DATA_TYPE_SIGNED,
	},
	{
		.id = V4L2_CID_ZOOM_ABSOLUTE,
		.name = "Zoom, Absolute",
		.unit = RTSCAM_SOC_CAMERA_TYPE,
		.selector = RTSCAM_SOC_CT_ZOOM_ABSOLUTE_CONTROL,
		.size = 16,
		.offset = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = RTS_CTRL_DATA_TYPE_UNSIGNED,
	},
	/* Zoom (Relative) */
	{
		.id = V4L2_CID_PAN_ABSOLUTE,
		.name = "Pan, Absolute",
		.unit = RTSCAM_SOC_CAMERA_TYPE,
		.selector = RTSCAM_SOC_CT_PANTILT_ABSOLUTE_CONTROL,
		.size = 32,
		.offset = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = RTS_CTRL_DATA_TYPE_SIGNED,
	},
	{
		.id = V4L2_CID_TILT_ABSOLUTE,
		.name = "Tilt, Absolute",
		.unit = RTSCAM_SOC_CAMERA_TYPE,
		.selector = RTSCAM_SOC_CT_PANTILT_ABSOLUTE_CONTROL,
		.size = 32,
		.offset = 32,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = RTS_CTRL_DATA_TYPE_SIGNED,
	},
	/* PanTilt (Relative) */
	{
		.id = RTS_V4L2_CID_ROLL_ABSOLUTE,
		.name = "Roll, Absolute",
		.unit = RTSCAM_SOC_CAMERA_TYPE,
		.selector = RTSCAM_SOC_CT_ROLL_ABSOLUTE_CONTROL,
		.size = 16,
		.offset = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = RTS_CTRL_DATA_TYPE_SIGNED,
	},
	{
		.id = V4L2_CID_GAIN,
		.name = "Gain",
		.unit = RTSCAM_SOC_RTK_EXT_CTL_TYPE,
		.selector = RTSCAM_SOC_RTK_EXT_GAIN_CONTROL,
		.size = 16,
		.offset = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = RTS_CTRL_DATA_TYPE_UNSIGNED,
	},
	{
		.id = RTS_V4L2_CID_SNR_CROP_X,
		.name = "Sensor horizontal crop",
		.unit = RTSCAM_SOC_RTK_EXT_CTL_TYPE,
		.selector = RTSCAM_SOC_RTK_EXT_SNR_CROP_CTRL,
		.size = 16,
		.offset = 16,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = RTS_CTRL_DATA_TYPE_UNSIGNED,
	},
	{
		.id = RTS_V4L2_CID_SNR_CROP_Y,
		.name = "Sensor vertical crop",
		.unit = RTSCAM_SOC_RTK_EXT_CTL_TYPE,
		.selector = RTSCAM_SOC_RTK_EXT_SNR_CROP_CTRL,
		.size = 16,
		.offset = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = RTS_CTRL_DATA_TYPE_UNSIGNED,
	},
};

int rtscam_soc_attach(struct rtscam_soc_dev *rsocdev, int regist);
int rtscam_soc_detach(struct rtscam_soc_dev *rsocdev, int unregist);

static int rtscam_soc_on_event(void *master, int id, void *arg)
{
	struct rtscam_soc_dev *rsocdev = master;
	int ret = -EFAULT;

	if (!rsocdev)
		return -EINVAL;

	switch (id) {
	case RTSCAM_EVT_FPS_DYNAMIC_CHANGED:
		ret = rtscam_change_dynamic_fps(&rsocdev->sensor_fps,
						*(struct v4l2_fract *)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int __rtscam_register_subdev(struct rtscam_subdev_t *subdev, int flag)
{
	struct rtscam_soc_dev *rsocdev = m_rsocdev;
	int ret;

	if (!rsocdev || !rsocdev->video_in)
		return -EINVAL;

	if (rsocdev->subdev && flag != 2) {
		soc_err("there is already one subdev\n");
		return -EINVAL;
	}

	if (!subdev)
		return -EINVAL;

	if (!subdev->dev || !subdev->enable ||
	    !subdev->set_stream || !subdev->set_fmt || !subdev->set_fps)
		return -EINVAL;

	if (subdev == rsocdev->subdev && flag != 2)
		return 0;

	if (flag == 2) {
		if (rsocdev->subdev != subdev ||
			rsocdev->video_in->subdev != subdev) {
			soc_err("invalid subdev to refresh\n");
			return -EINVAL;
		}
	} else {
		rsocdev->subdev = subdev;
		rsocdev->video_in->subdev = subdev;
	}

	ret = rtscam_soc_attach(rsocdev, flag);
	if (ret) {
		soc_err("soc camera attach fail, invalid subdev, %d\n", ret);
		rsocdev->subdev = NULL;
		return ret;
	}
	if (flag == 2)
		return 0;

	get_device(subdev->dev);
	subdev->master = rsocdev;
	if (subdev->set_hook)
		subdev->set_hook(subdev, (void *)rsocdev, rtscam_soc_on_event);

	return 0;
}

int rtscam_register_subdev_update(struct rtscam_subdev_t *subdev,
			int force_reset)
{
	struct rtscam_soc_dev *rsocdev = m_rsocdev;
	struct rtscam_video_device *icd = &rsocdev->rvdev;
	int ret = 0;

	mutex_lock(&icd->dev_lock);
	if (force_reset)
		ret = __rtscam_register_subdev(subdev, 0);
	else
		ret = __rtscam_register_subdev(subdev, 2);
	mutex_unlock(&icd->dev_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(rtscam_register_subdev_update);

int rtscam_register_subdev(struct rtscam_subdev_t *subdev)
{
	return __rtscam_register_subdev(subdev, 1);
}
EXPORT_SYMBOL_GPL(rtscam_register_subdev);

int rtscam_register_subdev_ext(struct rtscam_subdev_t *subdev)
{
	return __rtscam_register_subdev(subdev, 0);
}
EXPORT_SYMBOL_GPL(rtscam_register_subdev_ext);

static int __rtscam_unregister_subdev(struct rtscam_subdev_t *subdev, int flag)
{
	struct rtscam_soc_dev *rsocdev;
	int ret;

	if (!subdev || !subdev->master)
		return -EINVAL;
	rsocdev = subdev->master;

	if (rsocdev->subdev != subdev) {
		soc_err("invalid subdev to unregister\n");
		return -EINVAL;
	}

	ret = rtscam_soc_detach(rsocdev, flag);
	if (ret) {
		soc_err("subdev is inuse, unregister it latter\n");
		return ret;
	}
	if (flag == 2)
		return ret;

	if (subdev->set_hook)
		subdev->set_hook(subdev, NULL, NULL);
	put_device(subdev->dev);
	rsocdev->subdev = NULL;
	subdev->master = NULL;

	return 0;
}

int rtscam_unregister_subdev_update(struct rtscam_subdev_t *subdev,
				int force_reset)
{
	struct rtscam_soc_dev *rsocdev = m_rsocdev;
	struct rtscam_video_device *icd = &rsocdev->rvdev;
	int ret = 0;

	mutex_lock(&icd->dev_lock);
	if (force_reset)
		ret = __rtscam_unregister_subdev(subdev, 0);
	else {
		rsocdev->keep_user_setting = 1;
		ret = __rtscam_unregister_subdev(subdev, 2);
		rsocdev->keep_user_setting = 0;
	}
	mutex_unlock(&icd->dev_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(rtscam_unregister_subdev_update);

int rtscam_unregister_subdev(struct rtscam_subdev_t *subdev)
{
	return __rtscam_unregister_subdev(subdev, 1);
}
EXPORT_SYMBOL_GPL(rtscam_unregister_subdev);

int rtscam_unregister_subdev_ext(struct rtscam_subdev_t *subdev)
{
	return __rtscam_unregister_subdev(subdev, 0);
}
EXPORT_SYMBOL_GPL(rtscam_unregister_subdev_ext);

static u32 rts_read_reg(struct rtscam_soc_dev *rsocdev, off_t reg)
{
	WARN_ON(!rsocdev);
	WARN_ON(rsocdev != m_rsocdev);
	WARN_ON(!rsocdev->video_in);

	return rsocdev->video_in->read_reg(rsocdev->video_in, reg);
}

static void rts_write_reg(struct rtscam_soc_dev *rsocdev,
			  u32 value, off_t reg)
{
	WARN_ON(!rsocdev);
	WARN_ON(rsocdev != m_rsocdev);
	WARN_ON(!rsocdev->video_in);

	rsocdev->video_in->write_reg(rsocdev->video_in, value, reg);
}

static u32 rtscam_read_reg(struct rtscam_video_stream *stream, off_t reg)
{
	struct rtscam_soc_dev *rsocdev = stream->icd->priv;

	return rts_read_reg(rsocdev, reg);
}

static void rtscam_write_reg(struct rtscam_video_stream *stream,
			     u32 value, off_t reg)
{
	struct rtscam_soc_dev *rsocdev = stream->icd->priv;

	return rts_write_reg(rsocdev, value, reg);
}

void rtscam_soc_free_memory(struct rtscam_soc_dev *rsocdev,
				   struct rtscam_soc_dma_buffer *buffer)
{
	if (!buffer->initialized)
		return;

	rtscam_mem_free(rsocdev->rtsmem,
			buffer->size, buffer->vaddr, buffer->phy_addr);
	put_device(buffer->dev);
	buffer->initialized = 0;
}

int rtscam_soc_alloc_memory(struct rtscam_soc_dev *rsocdev,
				   struct rtscam_soc_dma_buffer *buffer,
				   int dir, gfp_t gfp, const char *name)
{
	if (!rsocdev || !buffer)
		return -EINVAL;

	if (buffer->initialized)
		return 0;

	if (buffer->size == 0)
		return -EINVAL;

	buffer->vaddr = rtscam_mem_alloc(rsocdev->rtsmem,
			buffer->size, &buffer->phy_addr, 0, dir, name);
	if (!buffer->vaddr) {
		rtsprintk(RTS_TRACE_ERROR, "alloc mem for %s fail\n", name);
		return -ENOMEM;
	}

	buffer->dev = get_device(rsocdev->dev);
	buffer->initialized = 1;

	return 0;
}

int rtscam_soc_add_memory_property(struct rtscam_soc_dev *rsocdev,
			dma_addr_t phyaddr, uint32_t property)
{
	if (!rsocdev)
		return -EINVAL;

	return rtscam_mem_add_property(rsocdev->rtsmem, phyaddr, property);
}

int rtscam_soc_set_fps(struct rtscam_video_stream *stream,
			struct v4l2_fract fps)
{
	struct rtscam_soc_dev *rsocdev =
			container_of(stream->icd, struct rtscam_soc_dev, rvdev);

	if (!rsocdev->subdev || !rsocdev->subdev->set_fps)
		return -EINVAL;

	return rsocdev->subdev->set_fps(rsocdev->subdev, stream->streamid, fps);
}

static int rtscam_soc_start_preview(struct rtscam_video_stream *stream)
{
	struct rtscam_soc_dev *rsocdev =
			container_of(stream->icd, struct rtscam_soc_dev, rvdev);

	if (!rsocdev->subdev || !rsocdev->subdev->set_stream)
		return -EINVAL;

	return rsocdev->subdev->set_stream(rsocdev->subdev,
					   stream->streamid, 1);
}

static int rtscam_soc_stop_preview(struct rtscam_video_stream *stream)
{
	struct rtscam_soc_dev *rsocdev =
			container_of(stream->icd, struct rtscam_soc_dev, rvdev);

	if (!rsocdev->subdev || !rsocdev->subdev->set_stream)
		return -EINVAL;

	return rsocdev->subdev->set_stream(rsocdev->subdev,
					   stream->streamid, 0);
}

#if 0
static int rtscam_soc_pause_video(struct rtscam_video_stream *stream)
{
	struct rtscam_soc_dev *rsocdev =
			container_of(stream->icd, struct rtscam_soc_dev, rvdev);

	if (!rsocdev->subdev || !rsocdev->subdev->pause_stream)
		return -EINVAL;

	return rsocdev->subdev->pause_stream(rsocdev->subdev,
					     stream->streamid, 0);
}

static int rtscam_soc_resume_video(struct rtscam_video_stream *stream)
{
	struct rtscam_soc_dev *rsocdev =
			container_of(stream->icd, struct rtscam_soc_dev, rvdev);

	if (!rsocdev->subdev || !rsocdev->subdev->pause_stream)
		return -EINVAL;

	return rsocdev->subdev->pause_stream(rsocdev->subdev,
					     stream->streamid, 1);
}
#endif

/*g_pix_cnt_thd is to avoid 1280x960 E-PTZ bug*/
u32 g_pix_cnt_thd = 0xffff;
static int rtscam_soc_set_format(struct rtscam_video_stream *stream)
{
	struct rtscam_soc_dev *rsocdev =
			container_of(stream->icd, struct rtscam_soc_dev, rvdev);
	u32 tmp;

	if (!rsocdev->subdev || !rsocdev->subdev->set_fmt)
		return -EINVAL;

	if (stream->streamid && stream->vin_mode == RTS_VIN_MODE_RING_MEM) {
		rtsprintk(RTS_TRACE_ERROR,
				"stream(%dth) not support for vin ringbuf\n",
				stream->streamid);
		return -EINVAL;
	}

	if (!stream->streamid) {
		if (stream->vin_mode == RTS_VIN_MODE_RING_MEM) {
			rts_write_reg(rsocdev, 0x1,
				rsocdev->video_in->reg.ringbuf_mode);
			rts_write_reg(rsocdev, 1,
				rsocdev->video_in->reg.ringbuf_rowend_delay);
		} else {
			rts_write_reg(rsocdev, 0,
				rsocdev->video_in->reg.ringbuf_mode);
		}
	}

	if (!stream->streamid && rsocdev->video_in->reg.yuv_interleave_select) {
		if (stream->rts_code == RTSCAM_FORMAT_TYPE_YUYV
			|| rtscam_soc_is_128bit3pixel(stream->rts_code)
			|| stream->rts_code == RTSCAM_FORMAT_TYPE_YVYU)
			rts_write_reg(rsocdev, 0x01,
				rsocdev->video_in->reg.yuv_interleave_select);
		else
			rts_write_reg(rsocdev, 0,
				rsocdev->video_in->reg.yuv_interleave_select);
	}

	if (rsocdev->video_in->reg.isp_nv12_select) {
		tmp = rts_read_reg(rsocdev,
			rsocdev->video_in->reg.isp_nv12_select);
		if (stream->rts_code == RTSCAM_FORMAT_TYPE_YVYU)
			rts_write_reg(rsocdev, tmp & ~(1 << stream->streamid),
				rsocdev->video_in->reg.isp_nv12_select);
		else
			rts_write_reg(rsocdev, tmp | (1 << stream->streamid),
				rsocdev->video_in->reg.isp_nv12_select);
	}

	g_pix_cnt_thd = stream->user_width * stream->user_height / 16 * 3 / 4;

	return rsocdev->subdev->set_fmt(rsocdev->subdev, stream->streamid,
					stream->rts_code, stream->user_width,
					stream->user_height);
}

static u8 __get_stream_reg_index(struct rtscam_video_stream *stream)
{
	u8 idx;

	idx = stream->streamid;

	if (stream->rts_code == RTSCAM_FORMAT_TYPE_RGB ||
			stream->rts_code == RTSCAM_FORMAT_TYPE_Y_ONLY)
		idx = RTSCAM_RGB_YONLY_STRM_IDX;

	return idx;
}

static struct rtscam_video_stream *__get_stream_from_reg_index(
		struct rtscam_soc_dev *rsocdev, int index)
{
	if (index == RTSCAM_RGB_YONLY_STRM_IDX)
		index = rsocdev->rvdev.streamnum - 1;

	if (index < 0 || index >= rsocdev->rvdev.streamnum)
		return NULL;

	if (rsocdev->rvdev.streamnum == 0)
		return NULL;

	return rsocdev->rvdev.streams + index;
}

u8 rtscam_soc_get_stream_reg_index(struct rtscam_video_stream *stream)
{
	return __get_stream_reg_index(stream);
}

struct rtscam_video_stream *rtscam_soc_get_stream_from_reg_index(
		struct rtscam_soc_dev *rsocdev, int index)
{
	return __get_stream_from_reg_index(rsocdev, index);
}

static void __isp_control(struct rtscam_soc_dev *rsocdev, u8 idx,
				   int enable)
{
	rsocdev->video_in->isp_control(rsocdev->video_in, idx, enable);
}

void rtscam_soc_isp_control(u8 idx, int enable)
{
	struct rtscam_soc_dev *rsocdev = m_rsocdev;
	struct rtscam_video_device *icd = &rsocdev->rvdev;

	mutex_lock(&icd->reg_lock);
	__isp_control(rsocdev, idx, enable);
	mutex_unlock(&icd->reg_lock);

}
EXPORT_SYMBOL_GPL(rtscam_soc_isp_control);

static void __reset_isp_reg(struct rtscam_soc_dev *rsocdev, u8 idx)
{
	rsocdev->video_in->reset_isp_reg(rsocdev->video_in, idx);
}

void rtscam_soc_reset_isp_reg(u8 idx)
{
	struct rtscam_soc_dev *rsocdev = m_rsocdev;
	struct rtscam_video_device *icd = &rsocdev->rvdev;

	mutex_lock(&icd->reg_lock);
	__reset_isp_reg(rsocdev, idx);
	mutex_unlock(&icd->reg_lock);
}
EXPORT_SYMBOL_GPL(rtscam_soc_reset_isp_reg);

int rtscam_get_video_in_buf_status(int streamid, u32 *val)
{
	struct rtscam_soc_dev *rsocdev = m_rsocdev;

	if (!val)
		return -EINVAL;

	if (rtscam_soc_get_devtype() != TYPE_RTS3917) {
		rtsprintk(RTS_TRACE_ERROR, "not support\n");
		return -EINVAL;
	}

	if (streamid < 0 || streamid >= rsocdev->rvdev.streamnum) {
		rtsprintk(RTS_TRACE_ERROR, "streamid is invalid\n");
		return -EINVAL;
	}

	*val = rsocdev->video_in->read_reg(rsocdev->video_in,
			rsocdev->video_in->reg.buf_status_base + 4 * streamid);

	return 0;
}
EXPORT_SYMBOL_GPL(rtscam_get_video_in_buf_status);

int rtscam_set_video_in_buf_status(int streamid, u32 val)
{
	struct rtscam_soc_dev *rsocdev = m_rsocdev;

	if (rtscam_soc_get_devtype() != TYPE_RTS3917) {
		rtsprintk(RTS_TRACE_ERROR, "not support\n");
		return -EINVAL;
	}

	if (streamid < 0 || streamid >= rsocdev->rvdev.streamnum) {
		rtsprintk(RTS_TRACE_ERROR, "streamid is invalid\n");
		return -EINVAL;
	}

	rsocdev->video_in->write_reg(rsocdev->video_in, val,
			rsocdev->video_in->reg.buf_status_base + 4 * streamid);

	return 0;
}
EXPORT_SYMBOL_GPL(rtscam_set_video_in_buf_status);

static void rtscam_soc_reset_stream(struct rtscam_video_stream *stream)
{
	int idx;
	struct rtscam_soc_dev *rsocdev = stream->icd->priv;

	idx = __get_stream_reg_index(stream);

	__reset_isp_reg(rsocdev, idx);
}

void rtscam_soc_dec_drops(struct rtscam_video_stream *stream)
{
	struct rtscam_soc_dev *rsocdev = stream->icd->priv;

	rsocdev->drops[stream->streamid]--;
}

int rtscam_soc_get_drops(struct rtscam_video_stream *stream)
{
	struct rtscam_soc_dev *rsocdev = stream->icd->priv;

	return rsocdev->drops[stream->streamid];
}

void rtscam_soc_inc_overflow(struct rtscam_soc_dev *rsocdev, int i)
{
	struct rtscam_video_stream *stream = NULL;

	stream = __get_stream_from_reg_index(rsocdev, i);
	if (stream)
		stream->overflow_count++;
}

void rtscam_soc_inc_abnormals(struct rtscam_soc_dev *rsocdev, int i)
{
	struct rtscam_video_stream *stream = NULL;

	stream = __get_stream_from_reg_index(rsocdev, i);
	if (stream)
		stream->abnormal_count++;
}

void rtscam_soc_inc_frames(struct rtscam_video_stream *stream)
{
	if (stream)
		stream->frame_count++;
}

void rtscam_soc_inc_skips(struct rtscam_video_stream *stream)
{
	if (stream)
		stream->skip_count++;
}

void rtscam_soc_inc_errors(struct rtscam_video_stream *stream)
{
	if (stream)
		stream->error_count++;
}

int rtscam_soc_is_yuv(u8 rts_code)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(m_rtscam_soc_formats); i++) {
		if (m_rtscam_soc_formats[i].rts_code == rts_code)
			return m_rtscam_soc_formats[i].is_yuv;
	}

	return false;
}

int rtscam_soc_is_rgb(u8 rts_code)
{
	if (rts_code == RTSCAM_FORMAT_TYPE_RGB)
		return true;

	return false;
}

int rtscam_soc_is_yonly(u8 rts_code)
{
	if (rts_code == RTSCAM_FORMAT_TYPE_Y_ONLY)
		return true;

	return false;
}

int rtscam_soc_is_128bit3pixel(u8 rts_code)
{
	if (rts_code == RTSCAM_FORMAT_TYPE_128BIT_3PIXEL)
		return true;

	return false;
}

static irqreturn_t rtscam_soc_irq(int irq, void *data)
{
	struct rtscam_soc_dev *rsocdev = data;

	if (!rsocdev->subdev)
		return IRQ_NONE;

	return rsocdev->video_in->process_irq(rsocdev->video_in);
}

static void rtscam_soc_enable_interrupt(struct rtscam_soc_dev *rsocdev,
					int enable)
{
	return rsocdev->video_in->enable_interrupt(rsocdev->video_in, enable);
}

static void rtscam_soc_control_reset(struct rtscam_soc_dev *rsocdev)
{
	if (!rsocdev || !rsocdev->video_in ||
		!rsocdev->video_in->reset_video)
		return;

	reset_control_reset(rsocdev->video_in->reset_video);
	udelay(1);
}

static int rtscam_soc_enable_mcu(struct rtscam_soc_dev *rsocdev,
				 int enable)
{
	WARN_ON(!rsocdev);
	WARN_ON(!rsocdev->subdev);

	rtscam_soc_control_reset(rsocdev);

	if (enable) {
		rsocdev->subdev->enable(rsocdev->subdev, 1);
		rtscam_soc_enable_interrupt(rsocdev, 1);
	} else {
		rtscam_soc_enable_interrupt(rsocdev, 0);
		rsocdev->subdev->enable(rsocdev->subdev, 0);
	}

	return 0;
}

static int rtscam_soc_config_hw_slot_num(struct rtscam_video_stream *stream,
					 int count)
{
	u32 reg;
	u32 value;
	u8 idx = __get_stream_reg_index(stream);
	struct rtscam_soc_dev *rsocdev = stream->icd->priv;

	if (count < 0 || count >= 8)
		return -EINVAL;

	if (idx == RTSCAM_RGB_YONLY_STRM_IDX) {
		idx = 0;
		reg = rsocdev->video_in->reg.rgb_frame_buf_cnt;
	} else {
		reg = rsocdev->video_in->reg.yuv_frame_buf_cnt;
	}
	if (reg == 0)
		return -EINVAL;

	value = rtscam_read_reg(stream, reg);

	clear_bit(4 * idx, (void *)&value);
	clear_bit(4 * idx + 1, (void *)&value);
	clear_bit(4 * idx + 2, (void *)&value);
	clear_bit(4 * idx + 3, (void *)&value);

	value |= (count << (4 * idx));

	rtscam_write_reg(stream, value, reg);

	return 0;
}

static int rtscam_soc_config_multi_read(struct rtscam_soc_dev *rsocdev)
{
	u32 reg = 0;

	if (!rsocdev || !rsocdev->video_in)
		return -EINVAL;

	reg = rsocdev->video_in->reg.multi_read;
	if (!reg)
		return 0;

	rts_write_reg(rsocdev, 1, reg);

	return 0;
}

static void rtscam_soc_config_rgb_buffer(struct rtscam_soc_dev *rsocdev)
{
	struct rtscam_soc_rgbcfg *rgbcfg;
	u32 reg, addr, size, value;
	int i;

	if (!rsocdev || !rsocdev->video_in)
		return;

	if (!rsocdev->video_in->support_rgb)
		return;

	rgbcfg = &rsocdev->rgbcfg;
	for (i = 0; i < 3; i++) {
		reg = rsocdev->video_in->reg.rgb_axibuf_base +
			i * rsocdev->video_in->reg.rgb_axibuf_interval;
		addr = rgbcfg->rgb[i].base;
		size = rgbcfg->rgb[i].size;
		value = addr + (size << 16);
		rts_write_reg(rsocdev, value, reg);
	}
}

static void rtscam_soc_config_isp_td_buffer(struct rtscam_soc_dev *rsocdev)
{
	u32 start = 0;
	u32 size = 0;
	u32 reg = 0;

	if (!rsocdev || !rsocdev->video_in)
		return;

	reg = rsocdev->video_in->reg.td_buf_cfg;
	if (!reg)
		return;

	start = rsocdev->td_config.base;
	size = rsocdev->td_config.size;
	rts_write_reg(rsocdev, ((start << 3) + (size << 19)), reg);
}

static int rtscam_soc_config_isp_buffer(struct rtscam_soc_dev *rsocdev)
{
	u8 idx = 0;
	struct rtscam_video_stream *stream;
	struct rtscam_soc_icfg *icfg;
	int i;
	u32 reg;
	u32 value;
	u32 base_bit;
	u32 size_bit;

	if (!rsocdev)
		return -EINVAL;

	for (i = 0; i < rsocdev->rvdev.streamnum; i++) {
		stream = rsocdev->rvdev.streams + i;
		icfg = &rsocdev->icfgs[i];
		idx = __get_stream_reg_index(stream);

		/*yuv: y start&size*/
		reg = rsocdev->video_in->reg.yuv_axibuf_base + 8 * idx;
		base_bit = rsocdev->video_in->reg.yuv_base_val_bit;
		size_bit = rsocdev->video_in->reg.yuv_size_val_bit;
		value = (icfg->y.base << base_bit) + (icfg->y.size << size_bit);
		rts_write_reg(rsocdev, value, reg);

		/*yuv: uv start&size*/
		reg = reg + 4;
		value = (icfg->uv.base << base_bit) +
				(icfg->uv.size << size_bit);
		rts_write_reg(rsocdev, value, reg);
	}

	rtscam_soc_config_isp_td_buffer(rsocdev);

	return 0;
}

static int rtscam_soc_config_isp_buffer(struct rtscam_soc_dev *rsocdev);

static int rtscam_soc_start_clock(struct rtscam_video_device *icd)
{
	return 0;
}

static int rtscam_soc_stop_clock(struct rtscam_video_device *icd)
{
	return 0;
}

static struct rtscam_subdev_unit_desc *__get_unit_desc(
		struct rtscam_soc_dev *rsocdev, int *size)
{
	if (!rsocdev || !rsocdev->subdev || !size)
		return NULL;

	*size = ARRAY_SIZE(rsocdev->subdev->desc.entities);

	return rsocdev->subdev->desc.entities;
}

static int rtscam_soc_get_ctrl(struct rtscam_video_device *icd,
			struct rtscam_video_ctrl **ctrl, int index)
{
	struct rtscam_soc_dev *rsocdev = icd->priv;
	int ret = -EINVAL;
	int i;
	int count = 0;
	struct rtscam_subdev_unit_desc *entity;
	struct rtscam_subdev_unit_desc *entities;
	int size = 0;

	if (index < 0)
		return -EINVAL;

	entities = __get_unit_desc(rsocdev, &size);
	for (i = 0; i < size; i++) {
		unsigned int idx = index - count;

		entity = entities + i;
		if (!entity->ncontrols)
			continue;

		if (idx < entity->ncontrols) {
			*ctrl = entity->controls + idx;
			ret = 0;
			break;
		}
		count += entity->ncontrols;
	}

	return ret;
}

static int rtscam_soc_put_ctrl(struct rtscam_video_device *icd,
			struct rtscam_video_ctrl *ctrl)
{
	return 0;
}

static int rtscam_soc_query_ctrl(struct rtscam_video_device *icd,
			  u8 unit, u8 query, u8 selector, u8 length, u8 *data)
{
	int ret;
	struct rtscam_soc_dev *rsocdev = icd->priv;
	struct rtscam_soc_cmd_stru rcmd = {
		.cmdcode = (unit << 8) | query,
		.index = selector,
		.length = length,
		.param = 0,
		.addr = 0,
		.buf = data
	};

	if (!rsocdev || !rsocdev->subdev ||
		!rsocdev->subdev->query_ctrl)
		return -EINVAL;

	ret = rsocdev->subdev->query_ctrl(rsocdev->subdev, &rcmd);
	rtsprintk((ret ? RTS_TRACE_ERROR : RTS_TRACE_CTRL),
		  "%s : %d 0x%02x 0x%02x, %d, ret = %d\n",
		  __func__, unit, query, selector, length, ret);

	return ret;
}

static int rtscam_soc_init_capture_buffers(struct rtscam_video_stream *stream)
{
	struct rtscam_soc_dev *rsocdev = stream->icd->priv;
	struct rtscam_soc_slot_info *info;
	int i;

	info = &rsocdev->slot_info[stream->streamid];
	info->slot_index = 0;
	for (i = 0; i < RTSCAM_SOC_HW_SLOT_NUM; i++)
		info->slots[i] = NULL;

	if (stream->vb2_vidp.num_buffers < RTSCAM_SOC_HW_SLOT_NUM)
		info->slot_num = stream->vb2_vidp.num_buffers;
	else
		info->slot_num = RTSCAM_SOC_HW_SLOT_NUM;

	rtscam_soc_config_hw_slot_num(stream, info->slot_num);

	return 0;
}

static int rtscam_soc_submit_buffer(struct rtscam_video_stream *stream,
			     struct rtscam_video_buffer *rbuf)
{
	struct rtscam_soc_dev *rsocdev = stream->icd->priv;
	struct rtscam_soc_slot_info *info;
	int i;
	int nr;
	dma_addr_t y_phy_addr;
	dma_addr_t uv_phy_addr;
	int ret;

	info = &rsocdev->slot_info[stream->streamid];

	for (i = 0; i < info->slot_num; i++) {
		if (info->slots[i] == rbuf) {
			rtsprintk(RTS_TRACE_BUF, "buf in slot already\n");
			return -EINVAL;
		}
	}

	for (i = 0; i < info->slot_num; i++) {
		nr = (i + info->slot_index) % info->slot_num;
		if (info->slots[nr] == NULL) {
			info->slot_index = (nr + 1) % info->slot_num;
			break;
		}
	}

	if (i == info->slot_num) {
		rtsprintk(RTS_TRACE_BUF, "there is no free slot now\n");
		return -EINVAL;
	}

	ret = rtscam_get_phy_addr(&rbuf->buf.vb2_buf,
				&y_phy_addr, &uv_phy_addr);
	if (ret) {
		rtsprintk(RTS_TRACE_BUF, "get buffer phy addr fail\n");
		return ret;
	}

	info->slots[nr] = rbuf;

	return rsocdev->video_in->submit_buffer(rsocdev->video_in,
			stream, y_phy_addr, uv_phy_addr, nr);
}

static void __calc_stream_delta(struct rtscam_video_stream *stream)
{
	stream->delta = 3;
}

static void __clear_stream_delta(struct rtscam_video_stream *stream)
{
	stream->delta = 0;
}

static int rtscam_soc_init_stream_delay(struct rtscam_soc_dev *rsocdev)
{
	int i;
	unsigned long delay = 0;

	for (i = 0; i < rsocdev->rvdev.streamnum; i++) {
		struct rtscam_video_stream *stream = rsocdev->rvdev.streams + i;

		stream->delay = delay;

		if (stream->delay > 30)
			stream->delay = 30;

		delay += stream->delta;
	}

	return 0;
}

static void __fill_stream_slots(struct rtscam_video_stream *stream)
{
	struct rtscam_soc_dev *rsocdev = stream->icd->priv;
	struct rtscam_soc_slot_info *info;
	int i;

	spin_lock_irq(&stream->lock);
	info = &rsocdev->slot_info[stream->streamid];

	for (i = 0; i < info->slot_num; i++) {
		if (info->slots[i])
			continue;

		rtscam_submit_buffer(stream, NULL);
	}
	spin_unlock_irq(&stream->lock);
}

static void __clear_stream_slots(struct rtscam_video_stream *stream)
{
	struct rtscam_soc_dev *rsocdev = stream->icd->priv;
	struct rtscam_soc_slot_info *info;
	int i;

	spin_lock_irq(&stream->lock);
	info = &rsocdev->slot_info[stream->streamid];

	for (i = 0; i < info->slot_num; i++) {
		struct rtscam_video_buffer *buffer = info->slots[i];

		rsocdev->video_in->submit_buffer(
				rsocdev->video_in, stream, 0, 0, i);

		if (!buffer)
			continue;

		info->slots[i] = NULL;
		rtscam_push_back_ready_buffer(stream, buffer);
	}
	info->slot_index = 0;
	spin_unlock_irq(&stream->lock);
}

static int __pre_streamon(struct rtscam_soc_dev *rsocdev)
{
	return 0;
}

static int __post_streamon(struct rtscam_soc_dev *rsocdev)
{
	return 0;
}

static int __pre_streamoff(struct rtscam_soc_dev *rsocdev)
{
	return 0;
}

static int __post_streamoff(struct rtscam_soc_dev *rsocdev)
{
	return 0;
}

static void rtscam_soc_set_frame_len(struct rtscam_video_stream *stream)
{
	u32 len, len_rgb, len_y, len_uv;
	u32 rts_code = stream->rts_code;
	struct rtscam_soc_dev *rsocdev = stream->icd->priv;

	if (!rsocdev->video_in->reg.rgb_frame_len_r ||
			!rsocdev->video_in->reg.rgb_frame_len_g ||
			!rsocdev->video_in->reg.rgb_frame_len_b)
		return;

	if (!rsocdev->video_in->reg.y_frame_len_base ||
			!rsocdev->video_in->reg.uv_frame_len_base ||
			!rsocdev->video_in->reg.yuv_frame_len_interval)
		return;

	len = stream->user_width * stream->user_height;

	len_rgb = (len % 16) ? (len + 16) : len;
	if (rts_code == RTSCAM_FORMAT_TYPE_Y_ONLY) {
		rts_write_reg(rsocdev, len_rgb,
			rsocdev->video_in->reg.rgb_frame_len_r);
		return;
	} else if (rts_code == RTSCAM_FORMAT_TYPE_RGB) {
		rts_write_reg(rsocdev, len_rgb,
			rsocdev->video_in->reg.rgb_frame_len_r);
		rts_write_reg(rsocdev, len_rgb,
			rsocdev->video_in->reg.rgb_frame_len_g);
		rts_write_reg(rsocdev, len_rgb,
			rsocdev->video_in->reg.rgb_frame_len_b);
		return;
	}

	switch (rts_code) {
	case RTSCAM_FORMAT_TYPE_YUYV:
	case RTSCAM_FORMAT_TYPE_YVYU:
		len_y = len << 1;
		len_uv = 0;
		break;
	case RTSCAM_FORMAT_TYPE_YUV420_SEMIPLANAR:
		len_y = len;
		len_uv = len >> 1;
		break;
	case RTSCAM_FORMAT_TYPE_YUV422_SEMIPLANAR:
		len_y = len;
		len_uv = len;
		break;
	case RTSCAM_FORMAT_TYPE_128BIT_3PIXEL:
		len_y = (len + 2) / 3 * 128 / 8;
		len_uv = 0;
		break;
	default:
		rtsprintk(RTS_TRACE_ERROR, "set frame len fail, invalid fmt\n");
		return;
	}

	len_y = (len_y % 16) ? (len_y + 16) : len_y;
	len_uv = (len_uv % 16) ? (len_uv + 16) : len_uv;

	rts_write_reg(rsocdev, len_y,
			rsocdev->video_in->reg.y_frame_len_base +
			stream->streamid *
			rsocdev->video_in->reg.yuv_frame_len_interval);
	rts_write_reg(rsocdev, len_uv,
			rsocdev->video_in->reg.uv_frame_len_base +
			stream->streamid *
			rsocdev->video_in->reg.yuv_frame_len_interval);
}

static void __reset_streaminfo(struct rtscam_video_stream *stream)
{
	spin_lock_irq(&stream->lock);

	stream->overflow_count = 0;
	stream->frame_count = 0;
	stream->skip_count = 0;
	stream->error_count = 0;
	stream->abnormal_count = 0;

	spin_unlock_irq(&stream->lock);
}

static int __rtscam_soc_s_stream(struct rtscam_video_stream *stream,
				int enable)
{
	int ret = 0;
	u8 idx;
	struct rtscam_soc_dev *rsocdev = stream->icd->priv;

	rtsprintk(RTS_TRACE_VIDEO, "set stream %s\n", enable ? "on" : "off");

	if (!stream || !stream->icd || !stream->icd->priv)
		return -EINVAL;

	idx = __get_stream_reg_index(stream);

	if (enable) {
		stream->sequence = 0;
		__reset_streaminfo(stream);
		rsocdev->drops[stream->streamid] = rsocdev->drop_frames;

		rtscam_soc_set_format(stream);

		rtscam_soc_set_frame_len(stream);

		rtscam_soc_reset_stream(stream);

		if (stream->vin_mode != RTS_VIN_MODE_NO_MEM) {
			__fill_stream_slots(stream);
			__isp_control(rsocdev, idx, 1);
		}

		__pre_streamon(rsocdev);
		ret = rtscam_soc_start_preview(stream);
		if (ret)
			__isp_control(rsocdev, idx, 0);

		__post_streamon(rsocdev);
		__calc_stream_delta(stream);

		if (rsocdev->video_in->get_streamon_time)
			rsocdev->video_in->get_streamon_time();
	} else {
		if (rsocdev->video_in->delay_stream_off)
			rsocdev->video_in->delay_stream_off();

		__pre_streamoff(rsocdev);
		rtscam_soc_stop_preview(stream);
		__post_streamoff(rsocdev);
		mdelay(10);

		__isp_control(rsocdev, idx, 0);

		/*
		 *mdelay(10);
		 */

		__clear_stream_delta(stream);
		__clear_stream_slots(stream);
	}

	rtscam_soc_init_stream_delay(rsocdev);

	return ret;
}

static int rtscam_soc_s_stream(struct rtscam_video_stream *stream, int enable)
{
	int ret = 0;

	if (!stream || !stream->icd || !stream->icd->priv)
		return -EINVAL;

	if (stream->fps.sensor_fps->flag_max) {
		rtscam_exec_sensor_fps_setting(stream, enable);

		return __rtscam_soc_s_stream(stream, enable);
	}

	if (enable) {
		rtscam_adjust_sensor_fps(stream, enable);
		ret = __rtscam_soc_s_stream(stream, enable);
	} else {
		ret = __rtscam_soc_s_stream(stream, enable);
		rtscam_adjust_sensor_fps(stream, enable);
	}

	return ret;
}

static void __pause_stream(struct rtscam_soc_dev *rsocdev, int resume)
{
	unsigned i;
	struct rtscam_video_device *icd = &rsocdev->rvdev;

	if (!resume && rsocdev->pause_flag)
		return;
	if (resume && !rsocdev->pause_flag)
		return;

	for (i = 0; i < icd->streamnum; i++) {
		struct rtscam_video_stream *stream = icd->streams + i;
		if (rtscam_is_streaming(stream))
			__rtscam_soc_s_stream(stream, resume);
	}

	if (resume)
		rsocdev->pause_flag = 0;
	else
		rsocdev->pause_flag = 1;
}

static int __set_ive_ctrl(struct rtscam_soc_dev *rsocdev,
				struct rtscam_soc_ive_ctrl *ctrl)
{
	if (!rsocdev || !ctrl || !rsocdev->subdev->set_ive_ctrl)
		return -EINVAL;

	return rsocdev->subdev->set_ive_ctrl(rsocdev->subdev, ctrl);
}

static int __get_ive_ctrl(struct rtscam_soc_dev *rsocdev,
				struct rtscam_soc_ive_ctrl *ctrl)
{
	if (!rsocdev || !ctrl || !rsocdev->subdev->get_ive_ctrl)
		return -EINVAL;

	return rsocdev->subdev->get_ive_ctrl(rsocdev->subdev, ctrl);
}

static long rtscam_soc_do_exec_vcmd(struct file *filp, unsigned int cmd,
				    void *arg)
{
	struct rtscam_video_stream *stream =
			(struct rtscam_video_stream *)filp;
	struct rtscam_soc_dev *rsocdev = stream->icd->priv;
	long ret = -EFAULT;

	switch (cmd) {
	case RTSOCIOC_G_HWOFFSET:
		*(unsigned long *)arg = rsocdev->iostart;
		ret = 0;
		break;
	case RTSOCIOC_G_HWIOSIZE:
		*(unsigned int *)arg = rsocdev->iosize;
		ret = 0;
		break;
	case RTSOCIOC_G_STREAMID:
		*(int *)arg = stream->streamid;
		ret = 0;
		break;
	case RTSOCIOC_PAUSE:
		__pause_stream(rsocdev, 0);
		ret = 0;
		break;
	case RTSOCIOC_RESUME:
		__pause_stream(rsocdev, 1);
		ret = 0;
		break;
	case RTSOCIOC_S_IVE_CTRL:
		ret = __set_ive_ctrl(rsocdev, arg);
		break;
	case RTSOCIOC_G_IVE_CTRL:
		ret = __get_ive_ctrl(rsocdev, arg);
		break;
	default:
		rtsprintk(RTS_TRACE_ERROR,
			  "Unknown rtscam soc cmd: 0x%x, '%c' 0x%x\n",
			  cmd,
			  _IOC_TYPE(cmd),
			  _IOC_NR(cmd));
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int rtscam_soc_exec_vcmd(struct rtscam_video_stream *stream,
			 struct rtscam_vcmd *pcmd)
{
	return rtscam_usercopy((struct file *)stream, pcmd->cmdcode,
			       (unsigned long)pcmd->arg,
			       rtscam_soc_do_exec_vcmd);
}
static int rtscam_soc_set_selection(struct rtscam_video_stream *stream,
			struct v4l2_selection *selection)
{
	struct rtscam_soc_dev *rsocdev = stream->icd->priv;
	struct rtscam_subdev_crop_info crop;

	if (!rsocdev || !rsocdev->subdev)
		return -EINVAL;

	if (!rsocdev->subdev->set_crop)
		return -EPERM;

	crop.mode = selection->type;
	crop.start_x = selection->r.left;
	crop.start_y = selection->r.top;
	crop.width = selection->r.width;
	crop.height = selection->r.height;

	return rsocdev->subdev->set_crop(
		rsocdev->subdev, stream->streamid, &crop);
}

static int rtscam_soc_get_selection(struct rtscam_video_stream *stream,
			struct v4l2_selection *selection)
{
	struct rtscam_soc_dev *rsocdev = stream->icd->priv;
	struct rtscam_subdev_crop_info crop;
	int ret;

	if (!rsocdev || !rsocdev->subdev)
		return -EINVAL;

	if (!rsocdev->subdev->get_crop)
		return -EPERM;

	ret = rsocdev->subdev->get_crop(rsocdev->subdev,
			stream->streamid, &crop);
	if (ret)
		return ret;

	selection->r.left = crop.start_x;
	selection->r.top = crop.start_y;
	selection->r.width = crop.width;
	selection->r.height = crop.height;
	return 0;
}

static struct rtscam_video_ops rtscam_soc_ops = {
	.owner			= THIS_MODULE,
	.start_clock		= rtscam_soc_start_clock,
	.stop_clock		= rtscam_soc_stop_clock,
	.get_ctrl		= rtscam_soc_get_ctrl,
	.put_ctrl		= rtscam_soc_put_ctrl,
	.query_ctrl		= rtscam_soc_query_ctrl,
	.init_capture_buffers	= rtscam_soc_init_capture_buffers,
	.submit_buffer		= rtscam_soc_submit_buffer,
	.s_stream		= rtscam_soc_s_stream,
	.set_selection		= rtscam_soc_set_selection,
	.get_selection		= rtscam_soc_get_selection,
	.exec_command		= rtscam_soc_exec_vcmd,
};

static void rtscam_soc_ctrl_init_ctrl(struct rtscam_soc_dev *rsocdev,
				      struct rtscam_video_ctrl *ctrl)
{
	struct rtscam_video_ctrl_info *info;
	struct rtscam_video_ctrl_mapping *mapping;
	int i;

	for (i = 0; i < ARRAY_SIZE(m_rtscam_soc_ctrls); i++) {
		info = m_rtscam_soc_ctrls + i;
		if (ctrl->unit == info->unit && ctrl->index == info->index) {
			rtscam_ctrl_add_info(ctrl, info);
			break;
		}
	}

	if (!ctrl->initialized)
		return;

	for (i = 0; i < ARRAY_SIZE(m_rtscam_soc_ctrl_mappings); i++) {
		mapping = m_rtscam_soc_ctrl_mappings + i;
		if (ctrl->unit == mapping->unit &&
		    ctrl->info.selector == mapping->selector)
			rtscam_ctrl_add_mapping(ctrl, mapping);
	}
}

static int rtscam_soc_ctrl_init_device(struct rtscam_soc_dev *rsocdev)
{
	unsigned int ncontrols;
	int i;
	struct rtscam_subdev_unit_desc *entity;
	struct rtscam_subdev_unit_desc *entities;
	int ret = 0;
	int size = 0;

	entities = __get_unit_desc(rsocdev, &size);
	for (i = 0; i < size; i++) {
		struct rtscam_video_ctrl *ctrl;
		int j;

		entity = &entities[i];

		ncontrols = memweight(entity->bmcontrols, entity->controlsize);
		entity->controls = kcalloc(ncontrols,
					   sizeof(struct rtscam_video_ctrl),
					   GFP_KERNEL);
		if (NULL == entity->controls) {
			ret = -ENOMEM;
			goto error;
		}
		entity->ncontrols = ncontrols;

		/* Initialize all supported controls */
		ctrl = entity->controls;
		for (j = 0; j < entity->controlsize * 8; j++) {
			if (rts_test_bit(entity->bmcontrols, j) == 0)
				continue;
			ctrl->index = j;
			ctrl->unit = entity->type;
			rtscam_soc_ctrl_init_ctrl(rsocdev, ctrl);
			ctrl++;
		}
	}

	return 0;

error:
	for (i = 0; i < size; i++) {
		entity = &entities[i];
		if (entity->controls) {
			kfree(entity->controls);
			entity->controls = NULL;
		}
		entity->ncontrols = 0;
	}
	return ret;
}

static struct rtscam_ge_device *__create_device(
		struct rtscam_soc_dev *rsocdev, const char *name,
		struct rtscam_ge_file_operations *fops)
{
	struct rtscam_ge_device *gdev;
	int ret;

	gdev = rtscam_ge_device_alloc();
	if (!gdev)
		return NULL;

	strlcpy(gdev->name, name, sizeof(gdev->name));
	gdev->parent = get_device(rsocdev->dev);
	gdev->release = rtscam_ge_device_release;
	gdev->fops = fops;

	rtscam_ge_set_drvdata(gdev, rsocdev);
	ret = rtscam_ge_register_device(gdev);
	if (ret) {
		rtscam_ge_device_release(gdev);
		return NULL;
	}

	return gdev;
}

static void __remove_device(struct rtscam_ge_device *gdev)
{
	if (!gdev)
		return;

	put_device(gdev->parent);
	rtscam_ge_unregister_device(gdev);
}

static int rtscam_soc_cam_open(struct file *filp)
{
	return 0;
}

static int rtscam_soc_cam_close(struct file *filp)
{
	return 0;
}

kernel_ulong_t rtscam_soc_get_devtype(void)
{
	struct rtscam_soc_dev *rsocdev = m_rsocdev;

	if (!rsocdev)
		return -EINVAL;

	return (rsocdev->devtype & ~TYPE_FPGA);
}

static long rtscam_soc_cam_do_ioctl(struct file *filp, unsigned int cmd,
				    void *arg)
{
	struct rtscam_soc_dev *rsocdev;
	struct rtscam_video_device *icd;
	int ret = 0;

	if (NULL == m_rsocdev)
		return -EINVAL;

	rsocdev = m_rsocdev;
	icd = &rsocdev->rvdev;

	mutex_lock(&icd->dev_lock);
	switch (cmd) {
	case RTSOCIOC_CAMERA_DETACH:
		ret = rtscam_unregister_subdev(rsocdev->subdev);
		break;
	case RTSOCIOC_CAMERA_ATTACH:
		ret = rtscam_register_subdev(rsocdev->subdev);
		break;
	default:
		rtsprintk(RTS_TRACE_ERROR,
			  "Unknown[rtscam] ioctl 0x%08x, type = '%c' nr = 0x%x\n",
			  cmd, _IOC_TYPE(cmd), _IOC_NR(cmd));
		ret = -ENOTTY;
		break;
	}
	mutex_unlock(&icd->dev_lock);

	return ret;
}

static long rtscam_soc_cam_ioctl(struct file *filp, unsigned int cmd,
				 unsigned long arg)
{
	return rtscam_usercopy(filp, cmd, arg, rtscam_soc_cam_do_ioctl);
}

static struct rtscam_ge_file_operations rtscam_soc_cam_fops = {
	.owner		= THIS_MODULE,
	.open		= rtscam_soc_cam_open,
	.release	= rtscam_soc_cam_close,
	.ioctl		= rtscam_soc_cam_ioctl,
};

static int rtscam_soc_create_cam_dev(struct rtscam_soc_dev *rsocdev)
{
	if (rsocdev->cam_dev)
		return 0;

	rsocdev->cam_dev = __create_device(rsocdev, RTS_SOC_CAM_DEV_NAME,
					   &rtscam_soc_cam_fops);
	if (!rsocdev->cam_dev)
		return -EINVAL;

	return 0;
}

static void rtscam_soc_remove_cam_dev(struct rtscam_soc_dev *rsocdev)
{
	__remove_device(rsocdev->cam_dev);
	rsocdev->cam_dev = NULL;
}

static int rtscam_soc_ctrl_open(struct file *filp)
{
	struct rtscam_ge_device *gdev = rtscam_devdata(filp);
	struct rtscam_soc_dev *rsocdev = rtscam_ge_get_drvdata(gdev);
	struct rtscam_video_device *icd = &rsocdev->rvdev;
	int ret = 0;

	if (!rsocdev->initialized)
		return -EINVAL;

	if (mutex_lock_interruptible(&icd->dev_lock))
		return -ERESTARTSYS;

	if (atomic_inc_return(&icd->use_count) == 1) {
		ret = rtscam_soc_start_clock(icd);
		if (ret < 0) {
			rtsprintk(RTS_TRACE_ERROR,
				  "couldn't activate the mcu:%d\n",
				  ret);
			atomic_dec(&icd->use_count);
			mutex_unlock(&icd->dev_lock);
			return ret;
		}
	}

	mutex_unlock(&icd->dev_lock);

	filp->private_data = icd;

	return 0;
}

static int rtscam_soc_ctrl_close(struct file *filp)
{
	struct rtscam_video_device *icd = filp->private_data;

	mutex_lock(&icd->dev_lock);
	if (atomic_dec_return(&icd->use_count) == 0)
		rtscam_soc_stop_clock(icd);
	mutex_unlock(&icd->dev_lock);

	filp->private_data = NULL;

	return 0;
}

static long rtscam_soc_ctrl_do_ioctl(struct file *filp, unsigned int cmd,
				     void *arg)
{
	struct rtscam_video_device *icd = filp->private_data;

	return rtscam_video_do_ctrl_ioctl(icd, cmd, arg);
}

static long rtscam_soc_ctrl_ioctl(struct file *filp, unsigned int cmd,
				  unsigned long arg)
{
	return rtscam_usercopy(filp, cmd, arg, rtscam_soc_ctrl_do_ioctl);
}

static struct rtscam_ge_file_operations rtscam_soc_ctrl_fops = {
	.owner		= THIS_MODULE,
	.open		= rtscam_soc_ctrl_open,
	.release	= rtscam_soc_ctrl_close,
	.ioctl		= rtscam_soc_ctrl_ioctl,
};

static int rtscam_soc_create_ctrl_dev(struct rtscam_soc_dev *rsocdev)
{
	if (rsocdev->ctrl_dev)
		return 0;

	rsocdev->ctrl_dev = __create_device(rsocdev,
					    RTS_SOC_CTRL_DEV_NAME,
					    &rtscam_soc_ctrl_fops);
	if (!rsocdev->ctrl_dev)
		return -EINVAL;

	return 0;

}

static void rtscam_soc_remove_ctrl_dev(struct rtscam_soc_dev *rsocdev)
{
	__remove_device(rsocdev->ctrl_dev);
	rsocdev->ctrl_dev = NULL;
}

static int rtscam_soc_init_video_stream(struct rtscam_soc_dev *rsocdev,
					struct rtscam_video_stream *stream,
					struct rtscam_subdev_strm_desc *desc)
{
	struct rtscam_video_format_xlate *fmt = NULL;
	int i;
	struct rtscam_frame_size max;
	struct rtscam_frame_size min = {RTSCAM_SOC_MIN_W, RTSCAM_SOC_MIN_H};
	struct rtscam_frame_size step;
	int ret;

	step.width = rsocdev->video_in->width_step;
	step.height = rsocdev->video_in->height_step;

	for (i = 0; i < ARRAY_SIZE(m_rtscam_soc_formats); i++) {
		fmt = &m_rtscam_soc_formats[i];
		if (!(fmt->rts_code & desc->format_bitmap))
			continue;
		ret = rtscam_register_format(stream, fmt);
		if (ret)
			goto error;

		max.width = desc->width;
		max.height = desc->height;
		ret = rtscam_register_frame_stepwise(stream, fmt->fourcc,
						     &max, &min, &step);
		if (ret)
			goto error;

		ret = rtscam_register_frmival(stream, fmt->fourcc,
						       &max);
		if (ret)
			goto error;
	}
	return 0;
error:
	rtscam_clr_format(stream, 0);
	return ret;
}

static int rtscam_register_fps_info(struct rtscam_soc_dev *rsocdev,
				    struct rtscam_video_stream *streams,
				    int streamnum)
{
	struct rtscam_sensor_fps *sensor_fps;
	struct rtscam_video_stream *stream;
	int i;
	struct rtscam_subdev_fps *subdev_fps;
	struct rtscam_soc_fps_descriptor *desc;

	if (!rsocdev || !rsocdev->subdev ||
		!rsocdev->video_in)
		return -EINVAL;

	subdev_fps = &rsocdev->subdev->desc.fps;
	sensor_fps = &rsocdev->sensor_fps;
	desc = &sensor_fps->desc;

	if (subdev_fps->type == RTSCAM_SUBDEV_FPS_CONTINUOUS) {
		desc->stepwise.max = subdev_fps->stepwise.max;
		desc->stepwise.min = subdev_fps->stepwise.min;
		desc->stepwise.step = subdev_fps->stepwise.step;
		desc->type = RTSCAM_SIZE_STEPWISE;
	} else {
		for (i = 0; i < RTSCAM_MAX_FPS_COUNT; i++) {
			u32 deno =
			rsocdev->subdev->desc.fps.discrete.fps[i].denominator;
			if (!deno)
				break;
		}

		if (!i)
			return -EINVAL;

		desc->discrete.length = i;
		desc->discrete.fps = kcalloc(i,
			sizeof(*desc->discrete.fps), GFP_KERNEL);
		if (!desc->discrete.fps)
			return -ENOMEM;

		for (i = 0; i < desc->discrete.length; i++)
			memcpy(&desc->discrete.fps[i],
				&rsocdev->subdev->desc.fps.discrete.fps[i],
				sizeof(desc->discrete.fps[i]));
		desc->type = RTSCAM_SIZE_DISCRETE;
	}

	sensor_fps = &rsocdev->sensor_fps;
	sensor_fps->set_stream = __rtscam_soc_s_stream;
	sensor_fps->set_fps = rsocdev->video_in->set_fps;
	sensor_fps->set_fps_dynamic = rsocdev->video_in->set_fps_dynamic;

	rtscam_init_sensor_fps(&rsocdev->sensor_fps, 0);

	sensor_fps->streamnum = streamnum;
	sensor_fps->streaming_count = &rsocdev->rvdev.streaming_count;
	sensor_fps->streams = streams;

	for (i = 0; i < streamnum; i++) {
		stream = streams + i;
		stream->fps.sensor_fps = sensor_fps;
	}
	return 0;
}

static int rtscam_soc_release_video_stream(struct rtscam_soc_dev *rsocdev,
					   struct rtscam_video_stream *stream)
{
	rtscam_clr_format(stream, rsocdev->keep_user_setting);
	return 0;
}

static int __get_valid_streamnum(struct rtscam_subdev_t *subdev)
{
	int i = 0;

	WARN_ON(!subdev);

	for (i = 0; i < ARRAY_SIZE(subdev->desc.strms); i++) {
		int j = 0;
		int found = 0;
		u32 bitmap;

		bitmap = subdev->desc.strms[i].format_bitmap;
		if (!bitmap)
			return i;

		for (j = 0; j < ARRAY_SIZE(m_rtscam_soc_formats); j++) {
			struct rtscam_video_format_xlate *fmt;

			fmt = &m_rtscam_soc_formats[j];
			if (fmt->rts_code & bitmap) {
				found = 1;
				break;
			}
		}
		if (!found)
			break;
	}

	return i;
}

static struct rtscam_soc_slot_info *__get_soc_skip_info(
			struct rtscam_soc_dev *rsocdev, int streamid)
{
	return &rsocdev->slot_info[streamid];
}

struct rtscam_soc_slot_info *rtscam_soc_get_skip_info(
			struct rtscam_soc_dev *rsocdev, int streamid)
{
	return __get_soc_skip_info(rsocdev, streamid);
}

static int rtscam_soc_release_video_device(struct rtscam_soc_dev *rsocdev)
{
	struct rtscam_video_device *icd = &rsocdev->rvdev;
	int i;

	for (i = 0; i < icd->streamnum; i++) {
		struct rtscam_video_stream *stream = icd->streams + i;

		rtscam_soc_release_video_stream(rsocdev, stream);
	}
	icd->streamnum = 0;

	icd->ops = NULL;
	icd->drv_name = NULL;
	icd->dev_name = NULL;
	icd->dev = NULL;
	icd->mem_ops = NULL;

	icd->priv = NULL;

	icd->initialized = 0;

	if (rsocdev->sensor_fps.desc.type == RTSCAM_SIZE_DISCRETE) {
		kfree(rsocdev->sensor_fps.desc.discrete.fps);
		rsocdev->sensor_fps.desc.discrete.fps = NULL;
	}

	rtscam_release_sensor_fps(&rsocdev->sensor_fps);
	return 0;
}

static int rtscam_soc_init_video_device(struct rtscam_soc_dev *rsocdev)
{
	struct rtscam_video_device *icd = &rsocdev->rvdev;
	int ret = 0;
	int i;
	int streamnum;
	struct rtscam_video_stream *streams = icd->streams;

	streamnum = __get_valid_streamnum(rsocdev->subdev);
	if (!streamnum) {
		soc_err("invalid stream num\n");
		return -EINVAL;
	}

	if (rsocdev->subdev->desc.dev_desc.streamnum &&
		rsocdev->subdev->desc.dev_desc.streamnum != streamnum) {
		soc_err("stream num %d is not same as subdev%d\n", streamnum,
			rsocdev->subdev->desc.dev_desc.streamnum);
		return -EINVAL;
	}

	if (streamnum > RTSCAM_VIDEO_SUPPORT_MAX_STREAM_NUM) {
		soc_err("stream num(%d) is so big, max support %d\n", streamnum,
				RTSCAM_VIDEO_SUPPORT_MAX_STREAM_NUM);
		return -EINVAL;
	}

	for (i = 0; i < streamnum; i++) {
		struct rtscam_video_stream *stream = streams + i;

		mutex_init(&stream->stream_lock);
		mutex_init(&stream->queue_lock);
		INIT_LIST_HEAD(&stream->capture);
		spin_lock_init(&stream->lock);
	}

	ret = rtscam_register_fps_info(rsocdev, streams, streamnum);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR, "register rts fps info fail\n");
		rtscam_release_sensor_fps(&rsocdev->sensor_fps);
		return ret;
	}

	for (i = 0; i < streamnum; i++) {
		struct rtscam_video_stream *stream = streams + i;
		struct rtscam_subdev_strm_desc *desc =
					&rsocdev->subdev->desc.strms[i];

		stream->streamid = i;
		stream->vin_mode = RTS_VIN_MODE_NORMAL;
		ret = rtscam_soc_init_video_stream(rsocdev, stream, desc);
		if (ret) {
			rtsprintk(RTS_TRACE_ERROR,
				  "init stream<%d> fail\n", i);
			goto error;
		}

		if (rtscam_soc_is_rgb(stream->rts_code)) {
			stream->video_nr = RTSCAM_SOC_VIDEO_RGB_NR_B;
			stream->fov_status = 1;
		} else {
			stream->video_nr = RTSCAM_SOC_VIDEO_YUV_NR_B;
			stream->fov_status = 0;
		}
	}

	icd->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	icd->streamnum = streamnum;

	icd->ops = &rtscam_soc_ops;
	icd->drv_name = RTS_SOC_CAMERA_DRV_NAME;
	icd->dev_name = rsocdev->name;
	icd->dev = rsocdev->dev;
	icd->mem_ops = rsocdev->mem_ops;

	icd->priv = rsocdev;

	icd->initialized = 1;

	return 0;
error:
	rtscam_soc_release_video_device(rsocdev);
	return ret;
}

static int rtscam_soc_release(struct rtscam_soc_dev *rsocdev)
{
	int i;
	int size = 0;
	struct rtscam_subdev_unit_desc *entities;

	if (!rsocdev)
		return -EINVAL;

	rtscam_soc_release_video_device(rsocdev);

	if (rsocdev->video_in->clear_isp_head)
		rsocdev->video_in->clear_isp_head(rsocdev->video_in);

	entities = __get_unit_desc(rsocdev, &size);
	for (i = 0; i < size; i++) {
		struct rtscam_subdev_unit_desc *entity;
		int j;

		entity = &entities[i];
		for (j = 0; j < entity->ncontrols; j++) {
			struct rtscam_video_ctrl *ctrl = entity->controls + i;
			if (!ctrl->initialized)
				continue;
			rtscam_ctrl_clr_mapping(ctrl);
			rtscam_ctrl_clr_info(ctrl);
		}

		kfree(entity->controls);
	}

	return 0;
}

static int rtscam_soc_init(struct rtscam_soc_dev *rsocdev)
{
	int ret = 0;

	if (!rsocdev || !rsocdev->video_in)
		return -EINVAL;

	ret = rtscam_soc_ctrl_init_device(rsocdev);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR, "init device ctrl fail\n");
		goto error;
	}

	if (rsocdev->video_in->config_isp_head) {
		ret = rsocdev->video_in->config_isp_head(rsocdev->video_in);
		if (ret) {
			rtsprintk(RTS_TRACE_ERROR,
				"config isp header fail\n");
			goto error;
		}
	}

	ret = rtscam_soc_init_video_device(rsocdev);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR, "init rts video device fail\n");
		goto error;
	}

	ret = rtscam_soc_config_isp_buffer(rsocdev);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR, "config isp buffer fail\n");
		goto error;
	}

	rtscam_soc_config_rgb_buffer(rsocdev);
	rtscam_soc_config_multi_read(rsocdev);
	return 0;
error:
	rtscam_soc_release(rsocdev);
	return ret;
}

static ssize_t show_streaminfo(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct rtscam_video_device *icd = to_rtscam_video_device(dev);
	struct rtscam_video_stream *stream = NULL;
	struct rtscam_video_buffer *buffer;
	int num = 0;
	int i;
	int j;

	num += scnprintf(buf + num, PAGE_SIZE - num,
			 "stream id\t");
	num += scnprintf(buf + num, PAGE_SIZE - num,
			 "frame count\t");
	num += scnprintf(buf + num, PAGE_SIZE - num,
			 "skip count\t");
	num += scnprintf(buf + num, PAGE_SIZE - num,
			 "overflow count\t");
	num += scnprintf(buf + num, PAGE_SIZE - num,
			 "error count\n");
	for (i = 0; i < icd->streamnum; i++) {
		stream = icd->streams + i;

		if (stream->vin_mode == RTS_VIN_MODE_RING_MEM) {
			num += scnprintf(buf + num, PAGE_SIZE - num,
				 "%-10d\t%-10s\t%-10s\t%-10s\t[%s%10s]\n",
				 i, "-", "-", "-", "-", "-");
			continue;
		}

		num += scnprintf(buf + num, PAGE_SIZE - num,
				 "%-10d\t%-10ld\t%-10ld\t%-10ld\t[%ld%10ld]\t",
				 i, stream->frame_count, stream->skip_count,
				 stream->overflow_count, stream->error_count,
				 stream->abnormal_count);

		for (j = 0; j < stream->vb2_vidp.num_buffers; j++) {
			buffer = rtscam_get_video_buffer(stream, j);
			if (!buffer)
				continue;

			num += scnprintf(buf + num, PAGE_SIZE - num,
					 "  [%d]:%d",
					 j, buffer->state);
		}
		num += scnprintf(buf + num, PAGE_SIZE - num, "\n");
	}

	return num;
}

static ssize_t clr_streaminfo(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct rtscam_video_device *icd = to_rtscam_video_device(dev);
	struct rtscam_video_stream *stream = NULL;
	int i;

	for (i = 0; i < icd->streamnum; i++) {
		stream = icd->streams + i;
		__reset_streaminfo(stream);
	}

	return count;
}
static DEVICE_ATTR(streaminfo, 0664, show_streaminfo, clr_streaminfo);

static ssize_t show_meminfo(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct rtscam_video_device *icd = to_rtscam_video_device(dev);
	struct rtscam_soc_dev *rsocdev =
			container_of(icd, struct rtscam_soc_dev, rvdev);
	int num = 0;

	num += scnprintf(buf + num, PAGE_SIZE - num, "total : %12ld\n",
			 rtscam_mem_get_total_size(rsocdev->rtsmem));
	num += scnprintf(buf + num, PAGE_SIZE - num, "used  : %12ld\n",
			 rtscam_mem_get_used_size(rsocdev->rtsmem));
	num += scnprintf(buf + num, PAGE_SIZE - num, "left  : %12ld\n",
			 rtscam_mem_get_left_size(rsocdev->rtsmem));

	return num;
}
static DEVICE_ATTR(meminfo, 0444, show_meminfo, NULL);

static ssize_t show_delayinfo(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct rtscam_video_device *icd = to_rtscam_video_device(dev);
	int num = 0;
	int i;

	for (i = 0; i < icd->streamnum; i++) {
		struct rtscam_video_stream *stream = icd->streams + i;
		num += scnprintf(buf + num, PAGE_SIZE - num,
				"%lu\n", stream->delay);
	}

	return num;
}
static DEVICE_ATTR(delay, S_IRUGO, show_delayinfo, NULL);

static ssize_t show_num_buffers(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rtscam_video_device *icd = to_rtscam_video_device(dev);
	struct rtscam_video_stream *stream = NULL;
	int num = 0;
	int i;

	for (i = 0; i < icd->streamnum; i++) {
		stream = icd->streams + i;
		num += scnprintf(buf + num, PAGE_SIZE - num,
				 "%u\n",
				 stream->vb2_vidp.num_buffers);
	}

	return num;
}
static DEVICE_ATTR(num_buffers, 0444, show_num_buffers, NULL);

static ssize_t get_drop_frames(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct rtscam_video_device *icd = to_rtscam_video_device(dev);
	struct rtscam_soc_dev *rsocdev =
			container_of(icd, struct rtscam_soc_dev, rvdev);
	int num = 0;

	num = scnprintf(buf, PAGE_SIZE, "%lu\n", rsocdev->drop_frames);

	return num;
}

static ssize_t set_drop_frames(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct rtscam_video_device *icd = to_rtscam_video_device(dev);
	struct rtscam_soc_dev *rsocdev =
			container_of(icd, struct rtscam_soc_dev, rvdev);
	unsigned long drop_frames = 0;
	int ret;

	ret = kstrtoul(buf, 0, &drop_frames);
	if (ret)
		return ret;

	if (drop_frames > RTSCAM_SOC_DROP_FRAMES_THRESHOLD)
		drop_frames = RTSCAM_SOC_DROP_FRAMES_THRESHOLD;

	rsocdev->drop_frames = drop_frames;

	return count;
}
static DEVICE_ATTR(drop_frames, 0664, get_drop_frames, set_drop_frames);

static ssize_t show_fps(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct rtscam_video_device *icd = to_rtscam_video_device(dev);
	struct rtscam_video_stream *stream = NULL;
	struct rtscam_soc_dev *rsocdev = icd->priv;
	int i;
	int num = 0;

	num += scnprintf(buf + num, PAGE_SIZE - num, "\t\tsetting\tactual:\n");

	num += scnprintf(buf + num, PAGE_SIZE - num,
		"senor:\t\t%d/%d\t%d/%d\n",
		rsocdev->sensor_fps.sensor_fps_setting.denominator,
		rsocdev->sensor_fps.sensor_fps_setting.numerator,
		rsocdev->sensor_fps.sensor_fps_actual.denominator,
		rsocdev->sensor_fps.sensor_fps_actual.numerator);

	num += scnprintf(buf + num, PAGE_SIZE - num, "user:\n");

	for (i = 0; i < icd->streamnum; i++) {
		stream = icd->streams + i;
		num += scnprintf(buf + num, PAGE_SIZE - num,
				 "    stream[%d] :\t%d/%d\t%d/%d\n",
				 i, stream->fps.user_setting.denominator,
				 stream->fps.user_setting.numerator,
				 stream->fps.user_actual.denominator,
				 stream->fps.user_actual.numerator);
	}

	return num;
}
static DEVICE_ATTR(fps, 0444, show_fps, NULL);

static ssize_t show_hz(struct device *dev,
		       struct device_attribute *attr, char *buf)
{
	int num = 0;

	num = scnprintf(buf, PAGE_SIZE, "%d\n", HZ);

	return num;
}
static DEVICE_ATTR(hz, 0444, show_hz, NULL);

static ssize_t rtscam_soc_get_snrfps_max(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct rtscam_video_device *icd = to_rtscam_video_device(dev);
	struct rtscam_soc_dev *rsocdev =
			container_of(icd, struct rtscam_soc_dev, rvdev);
	int num = 0;

	num += scnprintf(buf, PAGE_SIZE, "%d\n", rsocdev->sensor_fps.flag_max);

	return num;
}

static ssize_t rtscam_soc_set_snrfps_max(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct rtscam_video_device *icd = to_rtscam_video_device(dev);
	struct rtscam_soc_dev *rsocdev =
			container_of(icd, struct rtscam_soc_dev, rvdev);
	int val = 0;

	if (kstrtos32(buf, 0, &val))
		return -EINVAL;

	if (val == 1)
		rtscam_enable_snr_fps_max(&rsocdev->sensor_fps);
	else if (val == 0)
		rtscam_disable_snr_fps_max(&rsocdev->sensor_fps);

	return count;
}
static DEVICE_ATTR(sensorfps_max, 0664,
	    rtscam_soc_get_snrfps_max, rtscam_soc_set_snrfps_max);

int rtscam_soc_attach(struct rtscam_soc_dev *rsocdev, int regist)
{
	int ret = 0;
	struct rtscam_video_device *icd = NULL;

	if (!rsocdev)
		return -EINVAL;

	if (rsocdev->initialized && regist != 2)
		return 0;

	if (regist == 2 && !rsocdev->initialized) {
		soc_err("soc not initialized, cann't attach");
		return -EINVAL;
	}

	if (!rsocdev->subdev) {
		soc_err("there's not any subdev registered, cann't attach\n");
		return -EINVAL;
	}

	if (regist == 2) {
		if (rsocdev->rvdev.streaming_count) {
			soc_err("stop stream before attach cam\n");
			return -EBUSY;
		}
		ret = rtscam_soc_init(rsocdev);
		if (ret)
			rtsprintk(RTS_TRACE_ERROR,
				"init rts soc camera fail\n");
		return ret;
	}
	if (atomic_inc_return(&rsocdev->init_count) != 1) {
		atomic_dec(&rsocdev->init_count);
		return -EBUSY;
	}

	icd = &rsocdev->rvdev;

	ret = rtscam_soc_enable_mcu(rsocdev, 1);
	if (ret)
		goto eenable;

	ret = rtscam_soc_init(rsocdev);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR, "init rts soc camera fail\n");
		goto einit;
	}

	if (regist) {
		ret = rtscam_video_register_device(icd);
		if (ret) {
			rtsprintk(RTS_TRACE_ERROR,
				"register rts video device fail\n");
			goto ereg;
		}
	}

	device_create_file(rsocdev->dev, &dev_attr_streaminfo);
	device_create_file(rsocdev->dev, &dev_attr_meminfo);
	device_create_file(rsocdev->dev, &dev_attr_delay);
	device_create_file(rsocdev->dev, &dev_attr_num_buffers);
	device_create_file(rsocdev->dev, &dev_attr_drop_frames);
	device_create_file(rsocdev->dev, &dev_attr_fps);
	device_create_file(rsocdev->dev, &dev_attr_sensorfps_max);
	device_create_file(rsocdev->dev, &dev_attr_hz);

	rsocdev->initialized = 1;

	rtsprintk(RTS_TRACE_DEBUG, "%s initialized\n",
		  rsocdev->name);

	return 0;
ereg:
	rtscam_soc_release(rsocdev);
einit:
	rtscam_soc_enable_mcu(rsocdev, 0);
eenable:
	if (ret)
		atomic_dec(&rsocdev->init_count);
	return ret;
}

int rtscam_soc_detach(struct rtscam_soc_dev *rsocdev, int unregist)
{
	struct rtscam_video_device *icd = NULL;

	if (!rsocdev)
		return -EINVAL;

	if (!rsocdev->initialized)
		return 0;

	if (unregist == 2) {
		if (rsocdev->rvdev.streaming_count) {
			soc_err("stop stream before detach cam\n");
			return -EBUSY;
		}
		rtscam_soc_release(rsocdev);
		return 0;
	}

	if (atomic_read(&rsocdev->rvdev.use_count)) {
		soc_err("close device before detach camera\n");
		return -EBUSY;
	}

	if (atomic_dec_return(&rsocdev->init_count) != 0) {
		atomic_inc(&rsocdev->init_count);
		return -EBUSY;
	}

	device_remove_file(rsocdev->dev, &dev_attr_streaminfo);
	device_remove_file(rsocdev->dev, &dev_attr_meminfo);
	device_remove_file(rsocdev->dev, &dev_attr_delay);
	device_remove_file(rsocdev->dev, &dev_attr_num_buffers);
	device_remove_file(rsocdev->dev, &dev_attr_drop_frames);
	device_remove_file(rsocdev->dev, &dev_attr_fps);
	device_remove_file(rsocdev->dev, &dev_attr_sensorfps_max);
	device_remove_file(rsocdev->dev, &dev_attr_hz);

	icd = &rsocdev->rvdev;

	if (unregist)
		rtscam_video_unregister_device(icd);

	rtscam_soc_release(rsocdev);

	rtscam_soc_enable_mcu(rsocdev, 0);
	rsocdev->initialized = 0;

	return 0;
}

static void rtscam_custom_release_dev(struct rtscam_soc_dev *rsocdev)
{
	rsocdev->video_in->release(rsocdev->video_in);
	rsocdev->video_in = NULL;
}

static int rtscam_custom_init_dev(struct rtscam_soc_dev *rsocdev,
		struct resource *res)
{
	int ret;

	ret = rtscam_socdev_init_videoin(&rsocdev->video_in,
			rsocdev->dev, res);
	if (ret)
		return ret;

	rsocdev->video_in->priv = rsocdev;

	return 0;
}

static int rtscam_soc_init_devtype(struct rtscam_soc_dev *rsocdev,
			struct device_node *np, struct resource *res)
{
	int ret;

	if (!np)
		return -EINVAL;

	if (of_device_is_compatible(np, "realtek,rts3903-videoin")) {
		strncpy(rsocdev->name, "rts3903", PLATFORM_NAME_SIZE);
		rsocdev->devtype = TYPE_RTS3903;
	} else if (of_device_is_compatible(np, "realtek,rts3915-videoin")) {
		strncpy(rsocdev->name, "rts3915", PLATFORM_NAME_SIZE);
		rsocdev->devtype = TYPE_RTS3915;
	} else if (of_device_is_compatible(np, "realtek,rts3917-videoin")) {
		strncpy(rsocdev->name, "rts3917", PLATFORM_NAME_SIZE);
		rsocdev->devtype = TYPE_RTS3917;
	} else {
		rtsprintk(RTS_TRACE_ERROR, "unknown device type\n");
		return -EINVAL;
	}

	if (of_find_property(np, "is-fpga", NULL))
		rsocdev->devtype |= TYPE_FPGA;

	ret = rtscam_custom_init_dev(rsocdev, res);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR,
				"init soc dev failed, type = %d\n",
				(u32)rsocdev->devtype);
		return ret;
	}

	rtsprintk(RTS_TRACE_DEBUG, "%s(0x%lx)\n",
		  rsocdev->name, rsocdev->devtype);

	return 0;
}

static int rtscam_soc_parse_region(struct rtscam_region *region,
				   struct device_node *np)
{
	int ret;

	ret = of_property_read_u32_index(np, "reg", 0, &region->base);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR, "fail to get reg address\n");
		return ret;
	}
	ret = of_property_read_u32_index(np, "reg", 1, &region->size);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR, "fail to get reg size\n");
		return ret;
	}

	return 0;
}

static int rtscam_soc_parse_region_by_name(struct rtscam_region *region,
					   struct device_node *parent,
					   const char *name)
{
	struct device_node *np;
	int ret;

	if (!region || !parent || !name)
		return -EINVAL;

	np = of_find_node_by_name(parent, name);
	if (!np) {
		rtsprintk(RTS_TRACE_ERROR, "cann't find %s node\n", name);
		return -EINVAL;
	}
	ret = rtscam_soc_parse_region(region, np);
	of_node_put(np);
	np = NULL;
	if (ret)
		rtsprintk(RTS_TRACE_ERROR, "fail to get %s\n", name);

	return ret;

}

static int rtscam_soc_parse_icfg(struct rtscam_soc_icfg *icfg,
				 struct device_node *np)
{
	int ret;

	ret = rtscam_soc_parse_region_by_name(&icfg->y, np, "y");
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR, "fail to get y config\n");
		return ret;
	}
	ret = rtscam_soc_parse_region_by_name(&icfg->uv, np, "uv");
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR, "fail to get uv config\n");
		return ret;
	}

	icfg->found = 1;

	return 0;
}

static int rtscam_soc_parse_icfgs(struct rtscam_soc_dev *rsocdev,
				  struct device_node *np)
{
	int ret;
	int i;

	for (i = 0; i < ARRAY_SIZE(rsocdev->icfgs); i++) {
		struct device_node *node;

		node = of_parse_phandle(np, "isp-buffer-config", i);
		if (!node) {
			rtsprintk(RTS_TRACE_ERROR,
				  "there is no config for isp %d\n", i);
			break;
		}
		ret = rtscam_soc_parse_icfg(&rsocdev->icfgs[i], node);
		if (ret) {
			rtsprintk(RTS_TRACE_ERROR,
				  "fail to parse icfg[%d]\n", i);
			of_node_put(node);
			break;
		}
		of_node_put(node);
	}
	rsocdev->icfg_count = i;

	for (i = 0; i < rsocdev->icfg_count; i++) {
		struct rtscam_soc_icfg *icfg = &rsocdev->icfgs[i];

		if (!icfg->found)
			continue;
		rtsprintk(RTS_TRACE_DEBUG,
			  "icfg:[%d] y<0x%04x, 0x%04x> uv<0x%04x, 0x%04x>\n", i,
			  icfg->y.base, icfg->y.size,
			  icfg->uv.base, icfg->uv.size);
	}

	if (rsocdev->icfg_count == 0) {
		rtsprintk(RTS_TRACE_ERROR,
			  "there isn't any isp buffer config\n");
		return -EINVAL;
	}

	return 0;
}

static int rtscam_soc_parse_rgbcfg(struct rtscam_soc_dev *rsocdev,
					   struct device_node *np)
{
	struct rtscam_soc_rgbcfg *rgbcfg;
	struct device_node *node;
	int ret;
	int i;

	if (!rsocdev || !np || !rsocdev->video_in)
		return -EINVAL;

	if (!rsocdev->video_in->support_rgb)
		return 0;

	rgbcfg = &rsocdev->rgbcfg;
	for (i = 0; i < 3; i++) {
		node = of_parse_phandle(np, "rgb-buffer-config", i);
		if (!node) {
			rtsprintk(RTS_TRACE_ERROR,
				  "there is no config for rgbcfg[%d]\n", i);
			return -EINVAL;
		}

		ret = rtscam_soc_parse_region(&rgbcfg->rgb[i], node);
		if (ret) {
			rtsprintk(RTS_TRACE_ERROR,
				"fail to get rgbcfg[%d]\n", i);
			return ret;
		}

		of_node_put(node);
		rtsprintk(RTS_TRACE_DEBUG, "rgbcfg[%d]: <0x%x 0x%x>\n",
			i, rgbcfg->rgb[i].base, rgbcfg->rgb[i].size);
	}

	return 0;
}

static int rtscam_soc_parse_td_config(struct rtscam_soc_dev *rsocdev,
				      struct device_node *np)
{
	struct device_node *node;
	int ret;

	if (!rsocdev || !np || !rsocdev->video_in)
		return -EINVAL;

	if (!rsocdev->video_in->reg.td_buf_cfg)
		return 0;

	node = of_parse_phandle(np, "td-config", 0);
	if (!node) {
		rtsprintk(RTS_TRACE_ERROR, "there is no td-config node\n");
		return -EINVAL;
	}

	ret = rtscam_soc_parse_region(&rsocdev->td_config, node);
	if (ret)
		rtsprintk(RTS_TRACE_ERROR, "fail to get td config\n");
	else
		rtsprintk(RTS_TRACE_DEBUG, "isp td buffer config: <0x%x 0x%x>\n",
			  rsocdev->td_config.base, rsocdev->td_config.size);

	of_node_put(node);
	node = NULL;

	return ret;
}

static int rtscam_soc_parse_dt_buf_info(struct rtscam_soc_dev *rsocdev,
				    struct device_node *np)
{
	int ret;

	if (!np)
		return -EINVAL;

	ret = rtscam_soc_parse_icfgs(rsocdev, np);
	if (ret)
		return ret;

	ret = rtscam_soc_parse_rgbcfg(rsocdev, np);
	if (ret)
		return ret;

	ret = rtscam_soc_parse_td_config(rsocdev, np);
	if (ret)
		return ret;

	return 0;
}

static int rtscam_soc_init_clk(struct rtscam_soc_dev *rsocdev)
{
	return 0;
}

static int rtscam_soc_suspend(struct platform_device *pdev, pm_message_t state)
{
	int i;
	struct rtscam_video_device *icd = &m_rsocdev->rvdev;
	struct rtscam_video_stream *stream = NULL;

	for (i = 0; i < icd->streamnum; i++) {
		stream = icd->streams + i;

		if (!rtscam_is_streaming(stream))
			continue;

		__rtscam_soc_s_stream(stream, 0);
	}

	return 0;
}

static int rtscam_soc_resume(struct platform_device *pdev)
{
	int i;
	struct rtscam_video_device *icd = &m_rsocdev->rvdev;
	struct rtscam_video_stream *stream = NULL;

	for (i = 0; i < icd->streamnum; i++) {
		stream = icd->streams + i;

		if (!rtscam_is_streaming(stream))
			continue;

		__rtscam_soc_s_stream(stream, 1);
	}
	return 0;
}

int rtscam_soc_probe(struct platform_device *pdev)
{
	struct rtscam_soc_dev *rsocdev;
	struct resource *res;
	int irq;
	int err = 0;

	rtsprintk(RTS_TRACE_INFO, "%s\n", __func__);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res)
		rtsprintk(RTS_TRACE_DEBUG, "res : <0x%x 0x%x>\n",
			res->start, resource_size(res));

	/*irq = irq_of_parse_and_map(pdev->dev.of_node, 0);*/
	irq = platform_get_irq(pdev, 0);
	rtsprintk(RTS_TRACE_DEBUG, "irq : <%d>\n", irq);

	if (irq < 0) {
		rtsprintk(RTS_TRACE_ERROR, "Missing platform resource data\n");
		err = -ENODEV;
		goto out;
	}

	rsocdev = devm_kzalloc(&pdev->dev, sizeof(*rsocdev), GFP_KERNEL);
	if (NULL == rsocdev) {
		rtsprintk(RTS_TRACE_ERROR,
			  "Couldn't allocate rts camera soc object\n");
		err = -ENOMEM;
		goto out;
	}

	rsocdev->dev = get_device(&pdev->dev);
	m_rsocdev = rsocdev;

	atomic_set(&rsocdev->init_count, 0);

	mutex_init(&rsocdev->rvdev.dev_lock);
	mutex_init(&rsocdev->rvdev.ctrl_lock);
	mutex_init(&rsocdev->rvdev.reg_lock);

	err = rtscam_soc_init_clk(rsocdev);
	if (err)
		goto out;

	err = rtscam_soc_init_devtype(rsocdev, pdev->dev.of_node, res);
	if (err)
		goto out;

	err = rtscam_soc_parse_dt_buf_info(rsocdev, pdev->dev.of_node);
	if (err)
		goto out;

	rsocdev->rtsmem = rts_get_mem_info();
	if (!rsocdev->rtsmem) {
		rtsprintk(RTS_TRACE_ERROR, "Couldn't get mem info\n");
		err = -EINVAL;
		goto out;
	}

	rsocdev->iostart = res->start;
	rsocdev->iosize = resource_size(res);

	err = devm_request_irq(rsocdev->dev, irq, rtscam_soc_irq,
			       IRQF_SHARED, RTS_SOC_CAMERA_DRV_NAME, rsocdev);
	if (err) {
		rtsprintk(RTS_TRACE_ERROR, "request irq failed\n");
		goto out;
	}

	rsocdev->mem_ops = &rts_dma_contig_memops;

	if (rsocdev->video_in->sysmem)
		reset_control_deassert(rsocdev->video_in->sysmem);

	rtscam_soc_create_cam_dev(rsocdev);
	rtscam_soc_create_ctrl_dev(rsocdev);

	return 0;
out:
	if (rsocdev->rtsmem) {
		rts_put_mem_info(rsocdev->rtsmem);
		rsocdev->rtsmem = NULL;
	}
	if (rsocdev->dev) {
		put_device(rsocdev->dev);
		rsocdev->dev = NULL;
	}
	m_rsocdev = NULL;
	return err;
}

static int rtscam_soc_remove(struct platform_device *pdev)
{
	struct rtscam_soc_dev *rsocdev = m_rsocdev;

	rtscam_soc_remove_ctrl_dev(rsocdev);
	rtscam_soc_remove_cam_dev(rsocdev);

	rtscam_soc_detach(rsocdev, 1);

	if (rsocdev->video_in->sysmem)
		reset_control_assert(rsocdev->video_in->sysmem);

	rtscam_custom_release_dev(rsocdev);

	rts_put_mem_info(rsocdev->rtsmem);
	put_device(rsocdev->dev);
	rsocdev->dev = NULL;
	m_rsocdev = NULL;

	return 0;
}

static const struct of_device_id rtscam_soc_dt_ids[] = {
	{ .compatible = "realtek,rts3903-videoin", },
	{ .compatible = "realtek,rts3915-videoin", },
	{ .compatible = "realtek,rts3917-videoin", },
	{ /* sentinel */ },
};

static struct platform_driver rtscam_soc_driver = {
	.driver		= {
		.name	= RTS_SOC_CAMERA_DRV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(rtscam_soc_dt_ids),
	},
	.suspend	= rtscam_soc_suspend,
	.resume		= rtscam_soc_resume,
	.probe		= rtscam_soc_probe,
	.remove		= rtscam_soc_remove,
};

module_platform_driver(rtscam_soc_driver);

MODULE_DESCRIPTION("Realsil Soc Camera device driver");
MODULE_AUTHOR("Ming Qian <ming_qian@realsil.com.cn>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.1.1");
MODULE_ALIAS("platform:" RTS_SOC_CAMERA_DRV_NAME);
