/*
 *  Copyright (C) 2019 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <libubus.h>
#include <rtscamkit.h>
#include <rtsvideo.h>
#include <rtsavapi.h>
#include <rts_errno.h>
#include <json-c/json.h>

#include "rf_msg.h"
#include "ispctrl.h"
#include "video.h"

enum {
	RF_ISP_AWB_TEMPERATURE = 0,
	RF_ISP_AWB_AUTO = 1,
	RF_ISP_AWB_COMPONENT = 2,
};

enum {
	RF_ISP_AE_AUTO = 0,
	RF_ISP_AE_MANUAL = 1,
};

int fd;

struct rf_ispctrl_all isp_ctrl_context = {
	.power_line_frequency = {
			.id = ISPCTRL_ID_PWR_FREQUENCY,
			.current = 1,
	},
	.white_balance = {
			.id = ISPCTRL_ID_AWB_CTRL,
			.current = 1,
	},
	.white_balance_temperature = {
			.id = ISPCTRL_ID_WB_TEMPERATURE,
			.current = 2800,
	},
	.mirror = {
			.id = ISPCTRL_ID_MIRROR,
			.current = 0,
			.chn = 0,
	},
	.flip = {
			.id = ISPCTRL_ID_FLIP,
			.current = 0,
			.chn = 0,
	},
	.dehaze = {
			.id = ISPCTRL_ID_DEHAZE,
			.current = 0,
	},
	.ldc = {
			.id = ISPCTRL_ID_LDC,
			.current = 0,
	},
	.three_dnr = {
			.id = ISPCTRL_ID_3DNR,
			.current = 0,
	},
	.gamma = {
			.id = ISPCTRL_ID_GAMMA,
			.current = 300,
	},
	.brightness = {
			.id = ISPCTRL_ID_BRIGHTNESS,
			.current = 0,
	},
	.contrast = {
			.id = ISPCTRL_ID_CONTRAST,
			.current = 50,
	},
	.saturation = {
			.id = ISPCTRL_ID_SATURATION,
			.current = 50,
	},
	.sharpness = {
			.id = ISPCTRL_ID_SHARPNESS,
			.current = 50,
	},
	.wdr_mode = {
			.id = ISPCTRL_ID_WDR_MODE,
			.current = 0,
	},
	.wdr_level = {
			.id = ISPCTRL_ID_WDR_LEVEL,
			.current = 64,
	},
	.ir_mode = {
			.id = ISPCTRL_ID_IR_MODE,
			.current = 0,
	},
	.smart_ir_mode = {
			.id = ISPCTRL_ID_SMART_IR_MODE,
			.current = 0,
	},
	.smart_ir_level = {
			.id = ISPCTRL_ID_SMART_IR_MANUAL_LEVEL,
			.current = 64,
	},
};

struct rts_isp_control mf_ctrl;

static int get_valid_value(struct rts_isp_control *ctrl)
{
	int value = ctrl->current_value;

	if (value < ctrl->minimum)
		value = ctrl->minimum;
	if (value > ctrl->maximum)
		value = ctrl->maximum;
	if ((value - ctrl->minimum) % ctrl->step)
		value = value - (value - ctrl->minimum) % ctrl->step;

	return value;
}

static int convert_ID(uint32_t RF_ID)
{
	int RTS_ID;

	switch (RF_ID) {

	case ISPCTRL_ID_BRIGHTNESS:
		RTS_ID = RTS_ISP_CTRL_ID_BRIGHTNESS;
		return RTS_ID;

	case ISPCTRL_ID_CONTRAST:
		RTS_ID = RTS_ISP_CTRL_ID_CONTRAST;
		return RTS_ID;

	case ISPCTRL_ID_SATURATION:
		RTS_ID = RTS_ISP_CTRL_ID_SATURATION;
		return RTS_ID;

	case ISPCTRL_ID_SHARPNESS:
		RTS_ID = RTS_ISP_CTRL_ID_SHARPNESS;
		return RTS_ID;

	case ISPCTRL_ID_GAMMA:
		RTS_ID = RTS_ISP_CTRL_ID_GAMMA;
		return RTS_ID;

	case ISPCTRL_ID_AWB_CTRL:
		RTS_ID = RTS_ISP_CTRL_ID_AWB_CTRL;
		return RTS_ID;

	case ISPCTRL_ID_WB_TEMPERATURE:
		RTS_ID = RTS_ISP_CTRL_ID_WB_TEMPERATURE;
		return RTS_ID;

	case ISPCTRL_ID_PWR_FREQUENCY:
		RTS_ID = RTS_ISP_CTRL_ID_PWR_FREQUENCY;
		return RTS_ID;

	case ISPCTRL_ID_FLIP:
#ifdef RTS_VER_RTS3917
		RTS_ID = RTS_ISP_CTRL_ID_MIRROR_FLIP;
#else
		RTS_ID = RTS_ISP_CTRL_ID_FLIP;
#endif
		return RTS_ID;

	case ISPCTRL_ID_MIRROR:
#ifdef RTS_VER_RTS3917
		RTS_ID = RTS_ISP_CTRL_ID_MIRROR_FLIP;
#else
		RTS_ID = RTS_ISP_CTRL_ID_MIRROR;
#endif
		return RTS_ID;

	case ISPCTRL_ID_ROTATE:
		RTS_ID = RTS_ISP_CTRL_ID_ROTATE;
		return RTS_ID;

	case ISPCTRL_ID_WDR_MODE:
		RTS_ID = RTS_ISP_CTRL_ID_WDR_MODE;
		return RTS_ID;

	case ISPCTRL_ID_WDR_LEVEL:
		RTS_ID = RTS_ISP_CTRL_ID_WDR_LEVEL;
		return RTS_ID;

	case ISPCTRL_ID_3DNR:
		RTS_ID = RTS_ISP_CTRL_ID_3DNR;
		return RTS_ID;

	case ISPCTRL_ID_DEHAZE:
		RTS_ID = RTS_ISP_CTRL_ID_DEHAZE;
		return RTS_ID;

	case ISPCTRL_ID_IR_MODE:
		RTS_ID = RTS_ISP_CTRL_ID_IR_MODE;
		return RTS_ID;

	case ISPCTRL_ID_SMART_IR_MODE:
		RTS_ID = RTS_ISP_CTRL_ID_SMART_IR_MODE;
		return RTS_ID;

	case ISPCTRL_ID_SMART_IR_MANUAL_LEVEL:
		RTS_ID = RTS_ISP_CTRL_ID_SMART_IR_MANUAL_LEVEL;
		return RTS_ID;

	case ISPCTRL_ID_LDC:
		RTS_ID = RTS_ISP_CTRL_ID_LDC;
		return RTS_ID;

	default:
		return RF_ERR_OK;
	}
}

static int get_isp_ctrl_brightness(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_BRIGHTNESS;

	RTS_ASSERT(isp_ctrl);

	ret = rts_av_get_isp_ctrl(id,  &vctrl);

	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	strcpy(isp_ctrl->name, vctrl.name);
	isp_ctrl->id = ISPCTRL_ID_BRIGHTNESS;
	isp_ctrl->type = vctrl.type;
	isp_ctrl->minimum = vctrl.minimum;
	isp_ctrl->maximum = vctrl.maximum;
	isp_ctrl->step = vctrl.step;
	isp_ctrl->default_val = vctrl.default_value;
	isp_ctrl->current = vctrl.current_value;
	isp_ctrl->flags = vctrl.flags;

	return RF_ERR_OK;
}

static int set_isp_ctrl_brightness(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_BRIGHTNESS;

	RTS_ASSERT(isp_ctrl);

	if (isp_ctrl->id != ISPCTRL_ID_BRIGHTNESS)
		return RF_ERR_PARAM;

	ret = rts_av_get_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	vctrl.current_value = ctrl->current;

	vctrl.current_value = get_valid_value(&vctrl);

	ret = rts_av_set_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_SET_ISP_CTL;
	}

	return RF_ERR_OK;
}

static int get_isp_ctrl_contrast(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_CONTRAST;

	RTS_ASSERT(isp_ctrl);

	ret = rts_av_get_isp_ctrl(id,  &vctrl);

	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	strcpy(isp_ctrl->name, vctrl.name);
	isp_ctrl->id = ISPCTRL_ID_CONTRAST;
	isp_ctrl->type = vctrl.type;
	isp_ctrl->minimum = vctrl.minimum;
	isp_ctrl->maximum = vctrl.maximum;
	isp_ctrl->step = vctrl.step;
	isp_ctrl->default_val = vctrl.default_value;
	isp_ctrl->current = vctrl.current_value;
	isp_ctrl->flags = vctrl.flags;

	return RF_ERR_OK;
}

static int set_isp_ctrl_contrast(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_CONTRAST;

	RTS_ASSERT(isp_ctrl);

	if (isp_ctrl->id != ISPCTRL_ID_CONTRAST)
		return RF_ERR_PARAM;

	ret = rts_av_get_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	vctrl.current_value = ctrl->current;

	vctrl.current_value = get_valid_value(&vctrl);

	ret = rts_av_set_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_SET_ISP_CTL;
	}

	return RF_ERR_OK;
}

static int get_isp_ctrl_saturation(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_SATURATION;

	RTS_ASSERT(isp_ctrl);

	ret = rts_av_get_isp_ctrl(id,  &vctrl);

	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	strcpy(isp_ctrl->name, vctrl.name);
	isp_ctrl->id = ISPCTRL_ID_SATURATION;
	isp_ctrl->type = vctrl.type;
	isp_ctrl->minimum = vctrl.minimum;
	isp_ctrl->maximum = vctrl.maximum;
	isp_ctrl->step = vctrl.step;
	isp_ctrl->default_val = vctrl.default_value;
	isp_ctrl->current = vctrl.current_value;
	isp_ctrl->flags = vctrl.flags;

	return RF_ERR_OK;
}

static int set_isp_ctrl_saturation(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_SATURATION;

	RTS_ASSERT(isp_ctrl);

	if (isp_ctrl->id != ISPCTRL_ID_SATURATION)
		return RF_ERR_PARAM;

	ret = rts_av_get_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	vctrl.current_value = ctrl->current;

	vctrl.current_value = get_valid_value(&vctrl);

	ret = rts_av_set_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_SET_ISP_CTL;
	}

	return RF_ERR_OK;
}

static int get_isp_ctrl_sharpness(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_SHARPNESS;

	RTS_ASSERT(isp_ctrl);

	ret = rts_av_get_isp_ctrl(id,  &vctrl);

	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	strcpy(isp_ctrl->name, vctrl.name);
	isp_ctrl->id = ISPCTRL_ID_SHARPNESS;
	isp_ctrl->type = vctrl.type;
	isp_ctrl->minimum = vctrl.minimum;
	isp_ctrl->maximum = vctrl.maximum;
	isp_ctrl->step = vctrl.step;
	isp_ctrl->default_val = vctrl.default_value;
	isp_ctrl->current = vctrl.current_value;
	isp_ctrl->flags = vctrl.flags;

	return RF_ERR_OK;
}

static int set_isp_ctrl_sharpness(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_SHARPNESS;

	RTS_ASSERT(isp_ctrl);

	if (isp_ctrl->id != ISPCTRL_ID_SHARPNESS)
		return RF_ERR_PARAM;

	ret = rts_av_get_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	vctrl.current_value = ctrl->current;

	vctrl.current_value = get_valid_value(&vctrl);

	ret = rts_av_set_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_SET_ISP_CTL;
	}

	return RF_ERR_OK;
}

static int get_isp_ctrl_gamma(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_GAMMA;

	RTS_ASSERT(isp_ctrl);

	ret = rts_av_get_isp_ctrl(id,  &vctrl);

	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	strcpy(isp_ctrl->name, vctrl.name);
	isp_ctrl->id = ISPCTRL_ID_GAMMA;
	isp_ctrl->type = vctrl.type;
	isp_ctrl->minimum = vctrl.minimum;
	isp_ctrl->maximum = vctrl.maximum;
	isp_ctrl->step = vctrl.step;
	isp_ctrl->default_val = vctrl.default_value;
	isp_ctrl->current = vctrl.current_value;
	isp_ctrl->flags = vctrl.flags;

	return RF_ERR_OK;
}

static int set_isp_ctrl_gamma(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_GAMMA;

	RTS_ASSERT(isp_ctrl);

	if (isp_ctrl->id != ISPCTRL_ID_GAMMA)
		return RF_ERR_PARAM;

	ret = rts_av_get_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}
	vctrl.current_value = ctrl->current;

	vctrl.current_value = get_valid_value(&vctrl);
	ret = rts_av_set_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_SET_ISP_CTL;
	}

	return RF_ERR_OK;
}

static int get_isp_ctrl_awb_ctrl(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_AWB_CTRL;

	RTS_ASSERT(isp_ctrl);

	ret = rts_av_get_isp_ctrl(id,  &vctrl);

	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	strcpy(isp_ctrl->name, vctrl.name);
	isp_ctrl->id = ISPCTRL_ID_AWB_CTRL;
	isp_ctrl->type = vctrl.type;
	isp_ctrl->minimum = vctrl.minimum;
	isp_ctrl->maximum = vctrl.maximum;
	isp_ctrl->step = vctrl.step;
	isp_ctrl->default_val = vctrl.default_value;
	isp_ctrl->current = vctrl.current_value;
	isp_ctrl->flags = vctrl.flags;

	return RF_ERR_OK;
}

static int set_isp_ctrl_awb_ctrl(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_AWB_CTRL;

	RTS_ASSERT(isp_ctrl);

	if (isp_ctrl->id != ISPCTRL_ID_AWB_CTRL)
		return RF_ERR_PARAM;

	ret = rts_av_get_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	vctrl.current_value = ctrl->current;

	vctrl.current_value = get_valid_value(&vctrl);
	ret = rts_av_set_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_SET_ISP_CTL;
	}

	return RF_ERR_OK;
}

static int get_isp_ctrl_wb_temperature(struct rf_ispctrl_ctrl *ctrl)
{
int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_WB_TEMPERATURE;

	RTS_ASSERT(isp_ctrl);

	ret = rts_av_get_isp_ctrl(id,  &vctrl);

	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	strcpy(isp_ctrl->name, vctrl.name);
	isp_ctrl->id = ISPCTRL_ID_WB_TEMPERATURE;
	isp_ctrl->type = vctrl.type;
	isp_ctrl->minimum = vctrl.minimum;
	isp_ctrl->maximum = vctrl.maximum;
	isp_ctrl->step = vctrl.step;
	isp_ctrl->default_val = vctrl.default_value;
	isp_ctrl->current = vctrl.current_value;
	isp_ctrl->flags = vctrl.flags;

	return RF_ERR_OK;
}

static int set_isp_ctrl_wb_temperature(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_WB_TEMPERATURE;

	RTS_ASSERT(isp_ctrl);

	if (isp_ctrl->id != ISPCTRL_ID_WB_TEMPERATURE)
		return RF_ERR_PARAM;

	vctrl.current_value = RF_ISP_AWB_TEMPERATURE;
	ret = rts_av_set_isp_ctrl(RTS_ISP_CTRL_ID_AWB_CTRL,  &vctrl);
	if (ret) {
		RTS_ERR("set awb mode fail, ret = %d\n", ret);
		return RF_ERR_SET_ISP_CTL;
	}

	ret = rts_av_get_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	vctrl.current_value = ctrl->current;

	vctrl.current_value = get_valid_value(&vctrl);

	ret = rts_av_set_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_SET_ISP_CTL;
	}

	return RF_ERR_OK;
}

static int get_isp_ctrl_pwl_frequency(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_PWR_FREQUENCY;

	RTS_ASSERT(isp_ctrl);

	ret = rts_av_get_isp_ctrl(id,  &vctrl);

	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	strcpy(isp_ctrl->name, vctrl.name);
	isp_ctrl->id = ISPCTRL_ID_PWR_FREQUENCY;
	isp_ctrl->type = vctrl.type;
	isp_ctrl->minimum = vctrl.minimum;
	isp_ctrl->maximum = vctrl.maximum;
	isp_ctrl->step = vctrl.step;
	isp_ctrl->default_val = vctrl.default_value;
	isp_ctrl->current = vctrl.current_value;
	isp_ctrl->flags = vctrl.flags;

	return RF_ERR_OK;
}

static int set_isp_ctrl_pwl_frequency(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_PWR_FREQUENCY;

	RTS_ASSERT(isp_ctrl);

	if (isp_ctrl->id != ISPCTRL_ID_PWR_FREQUENCY)
		return RF_ERR_PARAM;

	ret = rts_av_get_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	vctrl.current_value = ctrl->current;

	vctrl.current_value = get_valid_value(&vctrl);

	ret = rts_av_set_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_SET_ISP_CTL;
	}

	return RF_ERR_OK;
}

static int get_3917_isp_ctrl_mirror_flip(struct rf_ispctrl_ctrl *isp_ctrl, struct rts_isp_control *ctrl)
{
	int ret = 0;
	struct rts_isp_control *vctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_MIRROR_FLIP;

	ret = rts_av_get_isp_ctrl(id,  vctrl);
	if (ret) {
		RTS_ERR("rts_av_get_isp_ctrl failed ret = %d\n", ret);
		return RF_ERR_GET_ISP_CTL;
	}

	strcpy(isp_ctrl->name, vctrl->name);
	isp_ctrl->type = vctrl->type;
	isp_ctrl->minimum = vctrl->minimum;
	isp_ctrl->maximum = vctrl->maximum;
	isp_ctrl->step = vctrl->step;
	isp_ctrl->default_val = vctrl->default_value;
	isp_ctrl->flags = vctrl->flags;

	if (isp_ctrl->id == ISPCTRL_ID_FLIP) {
		if (vctrl->current_value == RTS_ISP_CTRL_MIRROR_FLIP_DEFAULT)
			isp_ctrl->current = 0;
		else if (vctrl->current_value == RTS_ISP_CTRL_MIRROR_FLIP_M)
			isp_ctrl->current = 0;
		else if (vctrl->current_value == RTS_ISP_CTRL_MIRROR_FLIP_F)
			isp_ctrl->current = 1;
		else
			isp_ctrl->current = 1;
	}
	if (isp_ctrl->id == ISPCTRL_ID_MIRROR) {
		if (vctrl->current_value == RTS_ISP_CTRL_MIRROR_FLIP_DEFAULT)
			isp_ctrl->current = 0;
		else if (vctrl->current_value == RTS_ISP_CTRL_MIRROR_FLIP_M)
			isp_ctrl->current = 1;
		else if (vctrl->current_value == RTS_ISP_CTRL_MIRROR_FLIP_F)
			isp_ctrl->current = 0;
		else
			isp_ctrl->current = 1;
	}

	return ret;
}

static int set_3917_isp_ctrl_mirror_flip(struct rf_ispctrl_ctrl *isp_ctrl)
{
	int ret = 0;
	struct rts_isp_control *vctrl = &mf_ctrl;

	if (isp_ctrl->id == ISPCTRL_ID_FLIP) {
		if (isp_ctrl->current) {
			if (vctrl->current_value == RTS_ISP_CTRL_MIRROR_FLIP_DEFAULT)
				vctrl->current_value = RTS_ISP_CTRL_MIRROR_FLIP_F;
			else if (vctrl->current_value == RTS_ISP_CTRL_MIRROR_FLIP_M)
				vctrl->current_value = RTS_ISP_CTRL_MIRROR_FLIP_MF;
		} else {
			if (vctrl->current_value == RTS_ISP_CTRL_MIRROR_FLIP_F)
				vctrl->current_value = RTS_ISP_CTRL_MIRROR_FLIP_DEFAULT;
			else if (vctrl->current_value == RTS_ISP_CTRL_MIRROR_FLIP_MF)
				vctrl->current_value = RTS_ISP_CTRL_MIRROR_FLIP_M;
		}
	}
	if (isp_ctrl->id == ISPCTRL_ID_MIRROR) {
		if (isp_ctrl->current) {
			if (vctrl->current_value == RTS_ISP_CTRL_MIRROR_FLIP_DEFAULT)
				vctrl->current_value = RTS_ISP_CTRL_MIRROR_FLIP_M;
			else if (vctrl->current_value == RTS_ISP_CTRL_MIRROR_FLIP_F)
				vctrl->current_value = RTS_ISP_CTRL_MIRROR_FLIP_MF;
		} else {
			if (vctrl->current_value == RTS_ISP_CTRL_MIRROR_FLIP_M)
				vctrl->current_value = RTS_ISP_CTRL_MIRROR_FLIP_DEFAULT;
			else if (vctrl->current_value == RTS_ISP_CTRL_MIRROR_FLIP_MF)
				vctrl->current_value = RTS_ISP_CTRL_MIRROR_FLIP_F;
		}
	}

	return ret;
}

static int get_isp_ctrl_flip(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_FLIP;

	RTS_ASSERT(isp_ctrl);

#if (defined RTS_VER_RTS3903) || (defined RTS_VER_RTS3913)
	ret = rts_av_get_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	strcpy(isp_ctrl->name, vctrl.name);
	isp_ctrl->id = ISPCTRL_ID_FLIP;
	isp_ctrl->type = vctrl.type;
	isp_ctrl->minimum = vctrl.minimum;
	isp_ctrl->maximum = vctrl.maximum;
	isp_ctrl->step = vctrl.step;
	isp_ctrl->default_val = vctrl.default_value;
	isp_ctrl->current = vctrl.current_value;
	isp_ctrl->flags = vctrl.flags;
#endif
#ifdef RTS_VER_RTS3915
	int chn, flip;

	chn = g_video_ctx[isp_ctrl->chn]->h265_ch;
	flip = rts_av_get_mirror(chn);

	if (flip == RTS_AV_MIRROR_VER)
		flip = 1;
	else if (flip == RTS_AV_MIRROR_NO)
		flip = 0;
	else
		flip = -1;

	strcpy(isp_ctrl->name, vctrl.name);
	isp_ctrl->id = ISPCTRL_ID_FLIP;
	isp_ctrl->type = 0;
	isp_ctrl->minimum = 0;
	isp_ctrl->maximum = 1;
	isp_ctrl->step = 1;
	isp_ctrl->default_val = 0;
	isp_ctrl->current = flip;
	isp_ctrl->flags = 0;
#endif
#ifdef RTS_VER_RTS3917
	ret = get_3917_isp_ctrl_mirror_flip(isp_ctrl, &vctrl);
	if (ret) {
		RTS_ERR("get_3917_isp_ctrl_mirror_flip failed ret = %d\n", ret);
		return RF_ERR_GET_ISP_CTL;
	}
#endif

	return RF_ERR_OK;
}

static int set_isp_ctrl_flip(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_FLIP;

	RTS_ASSERT(isp_ctrl);

#if (defined RTS_VER_RTS3903) || (defined RTS_VER_RTS3913)
	if (isp_ctrl->id != ISPCTRL_ID_FLIP)
		return RF_ERR_PARAM;

	ret = rts_av_get_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	vctrl.current_value = ctrl->current;
	vctrl.current_value = get_valid_value(&vctrl);

	ret = rts_av_set_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_SET_ISP_CTL;
	}
#endif
#ifdef RTS_VER_RTS3915

	int chn;

	chn = g_video_ctx[isp_ctrl->chn]->h265_ch;
	rts_av_disable_chn(chn);
	if (ctrl->current == 1)
		ret = rts_av_set_mirror(chn, RTS_AV_MIRROR_VER);
	else if (ctrl->current == 0)
		ret = rts_av_set_mirror(chn, RTS_AV_MIRROR_NO);
	else
		ret = RF_ERR_PARAM;

	rts_av_enable_chn(chn);

	if (ret != RF_ERR_OK)
		return RF_ERR_SET_ISP_CTL;
#endif
#ifdef RTS_VER_RTS3917
	ret = set_3917_isp_ctrl_mirror_flip(isp_ctrl);
	if (ret) {
		RTS_ERR("set_3917_isp_ctrl_mirror_flip failed ret = %d\n", ret);
		return RF_ERR_GET_ISP_CTL;
	}
#endif

	return RF_ERR_OK;
}

static int get_isp_ctrl_mirror(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_MIRROR;

	RTS_ASSERT(isp_ctrl);

#if (defined RTS_VER_RTS3903) || (defined RTS_VER_RTS3913)
	ret = rts_av_get_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	strcpy(isp_ctrl->name, vctrl.name);
	isp_ctrl->id = ISPCTRL_ID_MIRROR;
	isp_ctrl->type = vctrl.type;
	isp_ctrl->minimum = vctrl.minimum;
	isp_ctrl->maximum = vctrl.maximum;
	isp_ctrl->step = vctrl.step;
	isp_ctrl->default_val = vctrl.default_value;
	isp_ctrl->current = vctrl.current_value;
	isp_ctrl->flags = vctrl.flags;
#endif

#if RTS_VER_RTS3915
	int chn, mirror;

	chn = g_video_ctx[isp_ctrl->chn]->h265_ch;
	mirror = rts_av_get_mirror(chn);

	if (mirror == RTS_AV_MIRROR_HOR)
		mirror = 1;
	else if (mirror == RTS_AV_MIRROR_NO)
		mirror = 0;
	else
		mirror = -1;

	strcpy(isp_ctrl->name, vctrl.name);
	isp_ctrl->id = ISPCTRL_ID_MIRROR;
	isp_ctrl->type = 0;
	isp_ctrl->minimum = 0;
	isp_ctrl->maximum = 1;
	isp_ctrl->step = 1;
	isp_ctrl->default_val = 0;
	isp_ctrl->current = mirror;
	isp_ctrl->flags = 0;
#endif

#if RTS_VER_RTS3917
	ret = get_3917_isp_ctrl_mirror_flip(isp_ctrl, &vctrl);
	if (ret) {
		RTS_ERR("get_3917_isp_ctrl_mirror_flip failed ret = %d\n", ret);
		return RF_ERR_GET_ISP_CTL;
	}
#endif

	return RF_ERR_OK;
}

static int set_isp_ctrl_mirror(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_MIRROR;

	RTS_ASSERT(isp_ctrl);

#if (defined RTS_VER_RTS3903) || (defined RTS_VER_RTS3913)
	if (isp_ctrl->id != ISPCTRL_ID_MIRROR)
		return RF_ERR_PARAM;

	ret = rts_av_get_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	vctrl.current_value = ctrl->current;
	vctrl.current_value = get_valid_value(&vctrl);

	ret = rts_av_set_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_SET_ISP_CTL;
	}
#endif
#ifdef RTS_VER_RTS3915
	int chn;

	chn = g_video_ctx[isp_ctrl->chn]->h265_ch;
	rts_av_disable_chn(chn);

	if (isp_ctrl->current == 1)
		ret = rts_av_set_mirror(chn, RTS_AV_MIRROR_HOR);
	else if (isp_ctrl->current == 0)
		ret = rts_av_set_mirror(chn, RTS_AV_MIRROR_NO);
	else
		ret = RF_ERR_PARAM;

	rts_av_enable_chn(chn);

	if (ret != RF_ERR_OK)
		return RF_ERR_SET_ISP_CTL;
#endif
#ifdef RTS_VER_RTS3917
	ret = set_3917_isp_ctrl_mirror_flip(isp_ctrl);
	if (ret) {
		RTS_ERR("set_3917_isp_ctrl_mirror_flip failed ret = %d\n", ret);
		return RF_ERR_GET_ISP_CTL;
	}
#endif

	return RF_ERR_OK;
}

static int get_isp_ctrl_wdr_mode(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_WDR_MODE;

	RTS_ASSERT(isp_ctrl);

	ret = rts_av_get_isp_ctrl(id,  &vctrl);

	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	strcpy(isp_ctrl->name, vctrl.name);
	isp_ctrl->id = ISPCTRL_ID_WDR_MODE;
	isp_ctrl->type = vctrl.type;
	isp_ctrl->minimum = vctrl.minimum;
	isp_ctrl->maximum = vctrl.maximum;
	isp_ctrl->step = vctrl.step;
	isp_ctrl->default_val = vctrl.default_value;
	isp_ctrl->current = vctrl.current_value;
	isp_ctrl->flags = vctrl.flags;

	return RF_ERR_OK;
}

static int set_isp_ctrl_wdr_mode(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_WDR_MODE;

	RTS_ASSERT(isp_ctrl);

	if (isp_ctrl->id != ISPCTRL_ID_WDR_MODE)
		return RF_ERR_PARAM;

	ret = rts_av_get_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	vctrl.current_value = ctrl->current;

	vctrl.current_value = get_valid_value(&vctrl);

	ret = rts_av_set_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_SET_ISP_CTL;
	}

	return RF_ERR_OK;
}

static int get_isp_ctrl_wdr_level(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_WDR_LEVEL;

	RTS_ASSERT(isp_ctrl);

	ret = rts_av_get_isp_ctrl(id,  &vctrl);

	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	strcpy(isp_ctrl->name, vctrl.name);
	isp_ctrl->id = ISPCTRL_ID_WDR_LEVEL;
	isp_ctrl->type = vctrl.type;
	isp_ctrl->minimum = vctrl.minimum;
	isp_ctrl->maximum = vctrl.maximum;
	isp_ctrl->step = vctrl.step;
	isp_ctrl->default_val = vctrl.default_value;
	isp_ctrl->current = vctrl.current_value;
	isp_ctrl->flags = vctrl.flags;

	return RF_ERR_OK;
}

static int set_isp_ctrl_wdr_level(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_WDR_LEVEL;

	RTS_ASSERT(isp_ctrl);

	if (isp_ctrl->id != ISPCTRL_ID_WDR_LEVEL)
		return RF_ERR_PARAM;

	ret = rts_av_get_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	vctrl.current_value = ctrl->current;

	vctrl.current_value = get_valid_value(&vctrl);

	ret = rts_av_set_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_SET_ISP_CTL;
	}

	return RF_ERR_OK;
}

static int get_isp_ctrl_3dnr(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_3DNR;

	RTS_ASSERT(isp_ctrl);

	ret = rts_av_get_isp_ctrl(id,  &vctrl);

	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	strcpy(isp_ctrl->name, vctrl.name);
	isp_ctrl->id = ISPCTRL_ID_3DNR;
	isp_ctrl->type = vctrl.type;
	isp_ctrl->minimum = vctrl.minimum;
	isp_ctrl->maximum = vctrl.maximum;
	isp_ctrl->step = vctrl.step;
	isp_ctrl->default_val = vctrl.default_value;
	isp_ctrl->current = vctrl.current_value;
	isp_ctrl->flags = vctrl.flags;

	return RF_ERR_OK;
}

static int set_isp_ctrl_3dnr(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_3DNR;

	RTS_ASSERT(isp_ctrl);

	if (isp_ctrl->id != ISPCTRL_ID_3DNR)
		return RF_ERR_PARAM;

	ret = rts_av_get_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	vctrl.current_value = ctrl->current;

	vctrl.current_value = get_valid_value(&vctrl);

	ret = rts_av_set_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_SET_ISP_CTL;
	}

	return RF_ERR_OK;
}

static int get_isp_ctrl_dehaze(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_DEHAZE;

	RTS_ASSERT(isp_ctrl);

	ret = rts_av_get_isp_ctrl(id,  &vctrl);

	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	strcpy(isp_ctrl->name, vctrl.name);
	isp_ctrl->id = ISPCTRL_ID_DEHAZE;
	isp_ctrl->type = vctrl.type;
	isp_ctrl->minimum = vctrl.minimum;
	isp_ctrl->maximum = vctrl.maximum;
	isp_ctrl->step = vctrl.step;
	isp_ctrl->default_val = vctrl.default_value;
	isp_ctrl->current = vctrl.current_value;
	isp_ctrl->flags = vctrl.flags;

	return RF_ERR_OK;
}

static int set_isp_ctrl_dehaze(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_DEHAZE;

	RTS_ASSERT(isp_ctrl);

	if (isp_ctrl->id != ISPCTRL_ID_DEHAZE)
		return RF_ERR_PARAM;

	ret = rts_av_get_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	vctrl.current_value = ctrl->current;

	vctrl.current_value = get_valid_value(&vctrl);

	ret = rts_av_set_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_SET_ISP_CTL;
	}

	return RF_ERR_OK;
}

static int get_isp_ctrl_ir_mode(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_IR_MODE;

	RTS_ASSERT(isp_ctrl);

	ret = rts_av_get_isp_ctrl(id,  &vctrl);

	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	strcpy(isp_ctrl->name, vctrl.name);
	isp_ctrl->id = ISPCTRL_ID_IR_MODE;
	isp_ctrl->type = vctrl.type;
	isp_ctrl->minimum = vctrl.minimum;
	isp_ctrl->maximum = vctrl.maximum;
	isp_ctrl->step = vctrl.step;
	isp_ctrl->default_val = vctrl.default_value;
	isp_ctrl->current = vctrl.current_value;
	isp_ctrl->flags = vctrl.flags;

	return RF_ERR_OK;
}

static int set_isp_ctrl_ir_mode(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_IR_MODE;

	RTS_ASSERT(isp_ctrl);

	if (isp_ctrl->id != ISPCTRL_ID_IR_MODE)
		return RF_ERR_PARAM;

	ret = rts_av_get_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	vctrl.current_value = ctrl->current;

	vctrl.current_value = get_valid_value(&vctrl);

	ret = rts_av_set_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_SET_ISP_CTL;
	}

	return RF_ERR_OK;
}

static int get_isp_ctrl_smart_ir_mode(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_SMART_IR_MODE;

	RTS_ASSERT(isp_ctrl);

	ret = rts_av_get_isp_ctrl(id,  &vctrl);

	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	strcpy(isp_ctrl->name, vctrl.name);
	isp_ctrl->id = ISPCTRL_ID_SMART_IR_MODE;
	isp_ctrl->type = vctrl.type;
	isp_ctrl->minimum = vctrl.minimum;
	isp_ctrl->maximum = vctrl.maximum;
	isp_ctrl->step = vctrl.step;
	isp_ctrl->default_val = vctrl.default_value;
	isp_ctrl->current = vctrl.current_value;
	isp_ctrl->flags = vctrl.flags;

	return RF_ERR_OK;
}

static int set_isp_ctrl_smart_ir_mode(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_SMART_IR_MODE;

	RTS_ASSERT(isp_ctrl);

	if (isp_ctrl->id != ISPCTRL_ID_SMART_IR_MODE)
		return RF_ERR_PARAM;

	ret = rts_av_get_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	vctrl.current_value = ctrl->current;

	vctrl.current_value = get_valid_value(&vctrl);

	ret = rts_av_set_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_SET_ISP_CTL;
	}

	return RF_ERR_OK;
}

static int get_isp_ctrl_smart_ir_level(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_SMART_IR_MANUAL_LEVEL;

	RTS_ASSERT(isp_ctrl);

	ret = rts_av_get_isp_ctrl(id,  &vctrl);

	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	strcpy(isp_ctrl->name, vctrl.name);
	isp_ctrl->id = ISPCTRL_ID_SMART_IR_MANUAL_LEVEL;
	isp_ctrl->type = vctrl.type;
	isp_ctrl->minimum = vctrl.minimum;
	isp_ctrl->maximum = vctrl.maximum;
	isp_ctrl->step = vctrl.step;
	isp_ctrl->default_val = vctrl.default_value;
	isp_ctrl->current = vctrl.current_value;
	isp_ctrl->flags = vctrl.flags;

	return RF_ERR_OK;
}

static int set_isp_ctrl_smart_ir_level(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_SMART_IR_MANUAL_LEVEL;

	RTS_ASSERT(isp_ctrl);

	if (isp_ctrl->id != ISPCTRL_ID_SMART_IR_MANUAL_LEVEL)
		return RF_ERR_PARAM;

	ret = rts_av_get_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	vctrl.current_value = ctrl->current;

	vctrl.current_value = get_valid_value(&vctrl);

	ret = rts_av_set_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_SET_ISP_CTL;
	}

	return RF_ERR_OK;
}

static int get_isp_ctrl_ldc(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_LDC;

	RTS_ASSERT(isp_ctrl);

	ret = rts_av_get_isp_ctrl(id,  &vctrl);

	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	strcpy(isp_ctrl->name, vctrl.name);
	isp_ctrl->id = ISPCTRL_ID_LDC;
	isp_ctrl->type = vctrl.type;
	isp_ctrl->minimum = vctrl.minimum;
	isp_ctrl->maximum = vctrl.maximum;
	isp_ctrl->step = vctrl.step;
	isp_ctrl->default_val = vctrl.default_value;
	isp_ctrl->current = vctrl.current_value;
	isp_ctrl->flags = vctrl.flags;

	return RF_ERR_OK;
}

static int set_isp_ctrl_ldc(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_LDC;

	RTS_ASSERT(isp_ctrl);

	if (isp_ctrl->id != ISPCTRL_ID_LDC)
		return RF_ERR_PARAM;

	ret = rts_av_get_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_GET_ISP_CTL;
	}

	vctrl.current_value = ctrl->current;

	vctrl.current_value = get_valid_value(&vctrl);

	ret = rts_av_set_isp_ctrl(id,  &vctrl);
	if (ret != RF_ERR_OK) {
		return RF_ERR_SET_ISP_CTL;
	}

	return RF_ERR_OK;
}

static int get_mf_ispctl(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	uint32_t id;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;

	RTS_ASSERT(isp_ctrl);

#ifdef RTS_VER_RTS3903
	id = convert_ID(isp_ctrl->id);
	if (id == RF_ERR_OK)
		return RF_ERR_SET_ISP_CTL;

	ret = rts_av_get_isp_ctrl(id, &vctrl);
	if (ret != RF_ERR_OK)
		return RF_ERR_GET_ISP_CTL;

	strcpy(isp_ctrl->name, vctrl.name);
	isp_ctrl->type = vctrl.type;
	isp_ctrl->minimum = vctrl.minimum;
	isp_ctrl->maximum = vctrl.maximum;
	isp_ctrl->step = vctrl.step;
	isp_ctrl->default_val = vctrl.default_value;
	isp_ctrl->current = vctrl.current_value;
	isp_ctrl->flags = vctrl.flags;

	return RF_ERR_OK;
#endif
#if (defined RTS_VER_RTS3915) || (defined RTS_VER_RTS3917)
	if (isp_ctrl->id == ISPCTRL_ID_MIRROR)
		ret = get_isp_ctrl_mirror(ctrl);
		if (ret)
			RTS_ERR("get_isp_ctrl_mirror failed ret = %d\n", ret);
	if (isp_ctrl->id == ISPCTRL_ID_FLIP)
		ret = get_isp_ctrl_flip(ctrl);
		if (ret)
			RTS_ERR("get_isp_ctrl_flip failed ret = %d\n", ret);
	if (ret != RF_ERR_OK)
		return RF_ERR_SET_ISP_CTL;

	return RF_ERR_OK;
#endif

}
#ifdef RTS_VER_RTS3917
static int set_mf_ispctl(struct rf_ispctrl_ctrl *fctrl, struct rf_ispctrl_ctrl *mctrl)
{
	int ret = 0;
	struct rts_isp_control *vctrl =  &mf_ctrl;
	uint32_t id = RTS_ISP_CTRL_ID_MIRROR_FLIP;
	ret = rts_av_get_isp_ctrl(id, vctrl);
	if (ret) {
		RTS_ERR("rts_av_get_isp_ctrl failed ret = %d\n", ret);
		return RF_ERR_GET_ISP_CTL;
	}

	if (fctrl->id == ISPCTRL_ID_FLIP) {
		ret = set_isp_ctrl_flip(fctrl);
		if (ret)
			RTS_ERR("set_isp_ctrl_flip failed ret = %d\n", ret);
	}
	if (mctrl->id == ISPCTRL_ID_MIRROR) {
		ret = set_isp_ctrl_mirror(mctrl);
		if (ret)
			RTS_ERR("set_isp_ctrl_mirror ret = %d\n", ret);
	}

	RTS_INFO("rts_av_set_isp_ctrl mirror_flip value = %d\n", vctrl->current_value);

	ret = rts_av_set_isp_ctrl(id,  vctrl);
	if (ret != RF_ERR_OK) {
		RTS_ERR("rts_av_set_isp_ctrl ret = %d\n", ret);
		return RF_ERR_SET_ISP_CTL;
	}

	return RF_ERR_OK;
}
#else
static int set_mf_ispctl(struct rf_ispctrl_ctrl *ctrl)
{
	int ret;
	uint32_t id;
	struct rts_isp_control vctrl;
	struct rf_ispctrl_ctrl *isp_ctrl = ctrl;

	RTS_ASSERT(isp_ctrl);

#ifdef RTS_VER_RTS3903
	id = convert_ID(isp_ctrl->id);
	if (id == RF_ERR_OK)
		return RF_ERR_SET_ISP_CTL;

	ret = rts_av_set_isp_ctrl(id,  &vctrl);

	if (ret != RF_ERR_OK)
		return RF_ERR_SET_ISP_CTL;

	return RF_ERR_OK;
#endif
#ifdef RTS_VER_RTS3915
	if (isp_ctrl->id == ISPCTRL_ID_MIRROR)
		ret = set_isp_ctrl_mirror(ctrl);
	if (isp_ctrl->id == ISPCTRL_ID_FLIP)
		ret = set_isp_ctrl_flip(ctrl);

	if (ret != RF_ERR_OK)
		return RF_ERR_SET_ISP_CTL;

	return RF_ERR_OK;
#endif
}
#endif

static int get_ispctl_all(struct rf_ispctrl_all *attr)
{
	int ret = RF_ERR_OK;
	struct rf_ispctrl_all *isp_attr = attr;

	ret = get_isp_ctrl_brightness(&isp_attr->brightness);
	ret = get_isp_ctrl_contrast(&isp_attr->contrast);
	ret = get_isp_ctrl_saturation(&isp_attr->saturation);
	ret = get_isp_ctrl_sharpness(&isp_attr->sharpness);
	ret = get_isp_ctrl_gamma(&isp_attr->gamma);
	ret = get_isp_ctrl_awb_ctrl(&isp_attr->white_balance);
	ret = get_isp_ctrl_wb_temperature(&isp_attr->white_balance_temperature);
	ret = get_isp_ctrl_pwl_frequency(&isp_attr->power_line_frequency);
	ret = get_isp_ctrl_flip(&isp_attr->flip);
	ret = get_isp_ctrl_mirror(&isp_attr->mirror);
	ret = get_isp_ctrl_wdr_mode(&isp_attr->wdr_mode);
	ret = get_isp_ctrl_wdr_level(&isp_attr->wdr_level);
	ret = get_isp_ctrl_3dnr(&isp_attr->three_dnr);
	ret = get_isp_ctrl_dehaze(&isp_attr->dehaze);
	ret = get_isp_ctrl_ir_mode(&isp_attr->ir_mode);
	ret = get_isp_ctrl_smart_ir_mode(&isp_attr->smart_ir_mode);
	ret = get_isp_ctrl_smart_ir_level(&isp_attr->smart_ir_level);
	ret = get_isp_ctrl_ldc(&isp_attr->ldc);

	return ret;
}

static int set_ispctl_all(struct rf_ispctrl_all *attr)
{
	int ret = RF_ERR_OK;
	struct rf_ispctrl_all *isp_attr = attr;

	ret = set_isp_ctrl_brightness(&isp_attr->brightness);
	ret = set_isp_ctrl_contrast(&isp_attr->contrast);
	ret = set_isp_ctrl_saturation(&isp_attr->saturation);
	ret = set_isp_ctrl_sharpness(&isp_attr->sharpness);
	ret = set_isp_ctrl_gamma(&isp_attr->gamma);
	ret = set_isp_ctrl_awb_ctrl(&isp_attr->white_balance);
	if (!isp_attr->white_balance.current)
		ret = set_isp_ctrl_wb_temperature(
				&isp_attr->white_balance_temperature);
	ret = set_isp_ctrl_pwl_frequency(&isp_attr->power_line_frequency);
#ifdef RTS_VER_RTS3917
	ret = set_mf_ispctl(&isp_attr->flip, &isp_attr->mirror);
#else
	ret = set_isp_ctrl_flip(&isp_attr->flip);
	ret = set_isp_ctrl_mirror(&isp_attr->mirror);
#endif
	ret = set_isp_ctrl_wdr_mode(&isp_attr->wdr_mode);
	ret = set_isp_ctrl_wdr_level(&isp_attr->wdr_level);
	ret = set_isp_ctrl_3dnr(&isp_attr->three_dnr);
	ret = set_isp_ctrl_dehaze(&isp_attr->dehaze);
	ret = set_isp_ctrl_ir_mode(&isp_attr->ir_mode);
	ret = set_isp_ctrl_smart_ir_mode(&isp_attr->smart_ir_mode);
	ret = set_isp_ctrl_smart_ir_level(&isp_attr->smart_ir_level);
	ret = set_isp_ctrl_ldc(&isp_attr->ldc);

	return ret;
}

int rf_control_ispctrl(
	int request, void *arg)
{
	int ret = RF_ERR_OK;
	struct rf_ispctrl_req *req = arg;
	struct rf_ispctrl_ctrl *ctrl = NULL;
	struct rf_ispctrl_all *attr = NULL;

	switch (request) {
	case RF_ISPCTRL_GET_ALL:
		attr = &req->all;
		get_ispctl_all(attr);
		break;

	case RF_ISPCTRL_SET_ALL:
		attr = &req->all;
		set_ispctl_all(attr);
		break;

	case RF_ISPCTRL_GET_CTRL:
		ctrl = &req->ctrl;
		get_mf_ispctl(ctrl);
		break;

	case RF_ISPCTRL_SET_CTRL:
		ctrl = &req->ctrl;
#ifdef RTS_VER_RTS3917
		set_mf_ispctl(ctrl, ctrl);
#else
		set_mf_ispctl(ctrl);
#endif
		break;

	default:
		ret = RF_ERR_REQUEST_NOT_SUPPORT;
		break;
	}

	return ret;
}

static int update_rainbow_ispctrl_main(
	struct rf_ispctrl_all *isp_ctrl_context)
{
	int ret, i = 0;
	int val[20] = {};
	char *path = "/etc/conf/rainbow.json";
	struct json_object *root_obj = NULL;
	struct json_object *ispctrl_obj = NULL;

	root_obj = json_object_from_file(path);
	if (!root_obj)
		ret = -1;

	ret = json_object_object_get_ex(root_obj, "ispctrl", &ispctrl_obj);

	if (ret) {
		json_object_object_foreach(ispctrl_obj, json_key, json_val) {
			val[i] = json_object_get_int(json_val);
			i++;
		}

		isp_ctrl_context->brightness.current = val[0];
		isp_ctrl_context->contrast.current = val[1];
		isp_ctrl_context->saturation.current = val[2];
		isp_ctrl_context->sharpness.current = val[3];
		isp_ctrl_context->gamma.current = val[4];
		isp_ctrl_context->white_balance.current = val[5];
		isp_ctrl_context->white_balance_temperature.current = val[6];
		isp_ctrl_context->power_line_frequency.current = val[7];
		isp_ctrl_context->flip.current = val[8];
		isp_ctrl_context->mirror.current = val[9];
		isp_ctrl_context->wdr_mode.current = val[10];
		isp_ctrl_context->wdr_level.current = val[11];
		isp_ctrl_context->three_dnr.current = val[12];
		isp_ctrl_context->dehaze.current = val[13];
		isp_ctrl_context->ir_mode.current = val[14];
		isp_ctrl_context->smart_ir_mode.current = val[15];
		isp_ctrl_context->smart_ir_level.current = val[16];
		isp_ctrl_context->ldc.current = val[17];
		i = 0;
	}

	if (root_obj)
		json_object_put(root_obj);

	return ret;
}

int rf_init_ispctrl(void)
{
	int res;
	int ret = RF_ERR_OK;

	res = update_rainbow_ispctrl_main(&isp_ctrl_context);
	if (res == -1)
		printf("update with default parameter\n");

	ret = set_ispctl_all(&isp_ctrl_context);

	return ret;
}

int rf_release_ispctrl(void)
{
	int ret = RF_ERR_OK;

	return ret;
}
