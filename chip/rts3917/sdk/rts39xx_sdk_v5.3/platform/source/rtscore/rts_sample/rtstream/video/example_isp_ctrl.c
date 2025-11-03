/*
 *  Copyright (C) 2019 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>

#define MAX_CTRL_CNT 80
struct option longopts[] = {
	{"set-ctrl", required_argument, NULL, 's'},
	{"get-ctrl", required_argument, NULL, 'g'},
	{"help", no_argument, NULL, 'h'},
	{0, 0, 0, 0}
};

struct ctrl_option {
	int cmd_id;
	int ctrl_id;
	int value;
};

struct isp_ctrl_option {
	int ctrl_count;
	struct ctrl_option ctrl_op[MAX_CTRL_CNT];
};

void print_help_info(void)
{
	fprintf(stdout, "DESCRIPTION:\n");
	fprintf(stdout, "\texample for isp_ctrl\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "\texample_isp_ctrl [option]...\n");
	fprintf(stdout, "\t-g, --get-ctrl <ctrlid>\tget ctrl value\n");
	fprintf(stdout, "\t-s, --set-ctrl <ctrlid> <val>\tset ctrl value\n");
	fprintf(stdout, "\t\tfor negative value, -s, --set-ctrl <ctrlid> -- <val>\n");
	fprintf(stdout, "\tavailable ctrlid as follows\n");
	fprintf(stdout, "\t\t\t1: brightness;\n");
	fprintf(stdout, "\t\t\t2: contrast;\n");
	fprintf(stdout, "\t\t\t3: hue;\n");
	fprintf(stdout, "\t\t\t4: saturation;\n");
	fprintf(stdout, "\t\t\t5: sharpness;\n");
	fprintf(stdout, "\t\t\t6: gamma;\n");
	fprintf(stdout, "\t\t\t7: auto white balance;\n");
	fprintf(stdout, "\t\t\t8: white balance temperature;\n");
	fprintf(stdout, "\t\t\t9: gain;\n");
	fprintf(stdout, "\t\t\t10: power liner frequency;\n");
	fprintf(stdout, "\t\t\t11: exposure mode;\n");
	fprintf(stdout, "\t\t\t12: exposure priority;\n");
	fprintf(stdout, "\t\t\t13: exposure time;\n");
	fprintf(stdout, "\t\t\t14: focus mode;\n");
	fprintf(stdout, "\t\t\t15: absolute focus;\n");
	fprintf(stdout, "\t\t\t16: mirror and flip;\n");
	fprintf(stdout, "\t\t\t17: gray mode;\n");
	fprintf(stdout, "\t\t\t18: digital wide dynamic range mode;\n");
	fprintf(stdout, "\t\t\t19: level of wide dynamic range;\n");
	fprintf(stdout, "\t\t\t20: green component of white balance(in component mode);\n");
	fprintf(stdout, "\t\t\t21: red component of white balance(in component mode);\n");
	fprintf(stdout, "\t\t\t22: blue component of white balance(in component mode);\n");
	fprintf(stdout, "\t\t\t23: auto exposure gain;\n");
	fprintf(stdout, "\t\t\t24: three-dimensional noise reduction;\n");
	fprintf(stdout, "\t\t\t25: day or night mode;\n");
	fprintf(stdout, "\t\t\t26: smart infrared ray mode;\n");
	fprintf(stdout, "\t\t\t27: level of manual smart infrared ray;\n");
	fprintf(stdout, "\t\t\t28: lens distortion correction;\n");
	fprintf(stdout, "\t\t\t29: dynamic fps(read only);\n");
	fprintf(stdout, "\t\t\t30: day night statis(read only);\n");
	fprintf(stdout, "\t\t\t31: wide dynamic range mode;\n");
	fprintf(stdout, "\t\t\t32: auto hdr ratio;\n");
	fprintf(stdout, "\t\t\t33: hdr ratio;\n");
	fprintf(stdout, "\t\t\t34: environment brightness(read only);\n");
	fprintf(stdout, "EXAMPLE:\n");
	fprintf(stdout, "\texample_isp_ctrl --set-ctrl 1 10\n");
}

static int get_valid_value(int id, int value, struct rts_isp_control *ctrl)
{
	int tvalue = value;

	if (value < ctrl->minimum)
		tvalue = ctrl->minimum;
	if (value > ctrl->maximum)
		tvalue = ctrl->maximum;
	if ((value - ctrl->minimum) % ctrl->step)
		tvalue = value - (value - ctrl->minimum) % ctrl->step;

	return tvalue;
}

/**
 * we can get the following attributes:
 * brightness, saturation, gamma, contrast, sharpness, mirror, flip, etc
 * all in this pattern
 */
static int test_get_ctrl(struct ctrl_option *ctrl_op)
{
	struct rts_isp_control ctrl;
	int ret;

	ret = rts_av_get_isp_ctrl(ctrl_op->ctrl_id, &ctrl);
	if (ret) {
		RTS_ERR("get isp attr fail, ret = %d\n", ret);
		return ret;
	}
	RTS_INFO("get isp ctrl: %s\n", ctrl.name);
	RTS_INFO("min = %d, max = %d, step = %d, def = %d, cur = %d\n",
		 ctrl.minimum, ctrl.maximum,
		 ctrl.step, ctrl.default_value, ctrl.current_value);

	return RTS_OK;
}

/**
 * we can set the following attributes:
 * brightness, saturation, gamma, contrast, sharpness, mirror, flip, etc
 * all in this pattern
 */
static int test_set_ctrl(struct ctrl_option *ctrl_op)
{
	struct rts_isp_control ctrl;
	int ret;
	int id = ctrl_op->ctrl_id;

	ret = rts_av_get_isp_ctrl(id, &ctrl);
	if (ret) {
		RTS_ERR("get isp attr fail, ret = %d\n", ret);
		return ret;
	}
	ctrl_op->value = get_valid_value(id, ctrl_op->value, &ctrl);

	RTS_INFO("set isp ctrl: %s\n", ctrl.name);
	RTS_INFO("before: min = %d, max = %d, step = %d, def = %d, cur = %d\n",
		 ctrl.minimum, ctrl.maximum, ctrl.step,
		 ctrl.default_value, ctrl.current_value);

	ctrl.current_value = ctrl_op->value;
	ret = rts_av_set_isp_ctrl(id, &ctrl);
	if (ret) {
		RTS_ERR("set isp attr fail, ret = %d\n", ret);
		return ret;
	}

	/**
	 * get check whether the new value is set or not,
	 * no need in actual use
	 */
	ret = rts_av_get_isp_ctrl(id, &ctrl);
	if (ret) {
		RTS_ERR("get isp attr fail, ret = %d\n", ret);
		return ret;
	}
	RTS_INFO("after: min = %d, max = %d, step = %d, def = %d, cur = %d\n",
		 ctrl.minimum, ctrl.maximum, ctrl.step,
		 ctrl.default_value, ctrl.current_value);

	return RTS_OK;
}

static int test_isp_ctrl(struct isp_ctrl_option *ctrlopt)
{
	int i;
	int ret = RTS_OK;

	for (i = 0; i < ctrlopt->ctrl_count; i++) {
		if (ctrlopt->ctrl_op[i].cmd_id == 'g')
			ret = test_get_ctrl(&ctrlopt->ctrl_op[i]);
		else if (ctrlopt->ctrl_op[i].cmd_id == 's')
			ret = test_set_ctrl(&ctrlopt->ctrl_op[i]);
		if (ret)
			break;
	}

	return ret;
}

static int cvt_to_isp_ctrl_id(int id)
{
	int isp_ctrl_id = -1;

	switch (id) {
	case 1:
		isp_ctrl_id = RTS_ISP_CTRL_ID_BRIGHTNESS;
		break;
	case 2:
		isp_ctrl_id = RTS_ISP_CTRL_ID_CONTRAST;
		break;
	case 3:
		isp_ctrl_id = RTS_ISP_CTRL_ID_HUE;
		break;
	case 4:
		isp_ctrl_id = RTS_ISP_CTRL_ID_SATURATION;
		break;
	case 5:
		isp_ctrl_id = RTS_ISP_CTRL_ID_SHARPNESS;
		break;
	case 6:
		isp_ctrl_id = RTS_ISP_CTRL_ID_GAMMA;
		break;
	case 7:
		isp_ctrl_id = RTS_ISP_CTRL_ID_AWB_CTRL;
		break;
	case 8:
		isp_ctrl_id = RTS_ISP_CTRL_ID_WB_TEMPERATURE;
		break;
	case 9:
		isp_ctrl_id = RTS_ISP_CTRL_ID_GAIN;
		break;
	case 10:
		isp_ctrl_id = RTS_ISP_CTRL_ID_PWR_FREQUENCY;
		break;
	case 11:
		isp_ctrl_id = RTS_ISP_CTRL_ID_EXPOSURE_MODE;
		break;
	case 12:
		isp_ctrl_id = RTS_ISP_CTRL_ID_EXPOSURE_PRIORITY;
		break;
	case 13:
		isp_ctrl_id = RTS_ISP_CTRL_ID_EXPOSURE_TIME;
		break;
	case 14:
		isp_ctrl_id = RTS_ISP_CTRL_ID_AF;
		break;
	case 15:
		isp_ctrl_id = RTS_ISP_CTRL_ID_FOCUS;
		break;
	case 16:
		isp_ctrl_id = RTS_ISP_CTRL_ID_MIRROR_FLIP;
		break;
	case 17:
		isp_ctrl_id = RTS_ISP_CTRL_ID_GRAY_MODE;
		break;
	case 18:
		isp_ctrl_id = RTS_ISP_CTRL_ID_WDR_MODE;
		break;
	case 19:
		isp_ctrl_id = RTS_ISP_CTRL_ID_WDR_LEVEL;
		break;
	case 20:
		isp_ctrl_id = RTS_ISP_CTRL_ID_GREEN_BALANCE;
		break;
	case 21:
		isp_ctrl_id = RTS_ISP_CTRL_ID_RED_BALANCE;
		break;
	case 22:
		isp_ctrl_id = RTS_ISP_CTRL_ID_BLUE_BALANCE;
		break;
	case 23:
		isp_ctrl_id = RTS_ISP_CTRL_ID_AE_GAIN;
		break;
	case 24:
		isp_ctrl_id = RTS_ISP_CTRL_ID_3DNR;
		break;
	case 25:
		isp_ctrl_id = RTS_ISP_CTRL_ID_IR_MODE;
		break;
	case 26:
		isp_ctrl_id = RTS_ISP_CTRL_ID_SMART_IR_MODE;
		break;
	case 27:
		isp_ctrl_id = RTS_ISP_CTRL_ID_SMART_IR_MANUAL_LEVEL;
		break;
	case 28:
		isp_ctrl_id = RTS_ISP_CTRL_ID_LDC;
		break;
	case 29:
		isp_ctrl_id = RTS_ISP_CTRL_ID_DYNAMIC_FPS;
		break;
	case 30:
		isp_ctrl_id = RTS_ISP_CTRL_ID_DAYNIGHT_STATIS;
		break;
	case 31:
		isp_ctrl_id = RTS_ISP_CTRL_ID_HDR_MODE;
		break;
	case 32:
		isp_ctrl_id = RTS_ISP_CTRL_ID_AUTO_HDR_RATIO;
		break;
	case 33:
		isp_ctrl_id = RTS_ISP_CTRL_ID_HDR_RATIO;
		break;
	case 34:
		isp_ctrl_id = RTS_ISP_CTRL_ID_BV;
		break;
	default:
		isp_ctrl_id = 0;
		break;
	}

	return isp_ctrl_id;
}

int main(int argc, char *argv[])
{
	int c;
	int ret = 0;
	int id = 0;
	int ctrl_id = 0;
	struct isp_ctrl_option ctrlopt = {0};

	if (argc < 2) {
		printf("too few parameter\n");
		printf("use -h to get help info\n");
		return -1;
	}

	while ((c = getopt_long(argc, argv, ":hs:g:",
				longopts, NULL)) != -1) {
		switch (c) {
		case 's':
			if (ctrlopt.ctrl_count >= MAX_CTRL_CNT) {
				printf("ctrl count(%d) > max(%d)\n",
					ctrlopt.ctrl_count, MAX_CTRL_CNT);
				return -1;
			}
			ctrlopt.ctrl_op[ctrlopt.ctrl_count].cmd_id = 's';
			id = (int)strtol(optarg, NULL, 0);
			ctrl_id = cvt_to_isp_ctrl_id(id);
			if (ctrl_id <= 0) {
				printf("invalid ctrl id(%d)\n", id);
				return -1;
			}
			ctrlopt.ctrl_op[ctrlopt.ctrl_count].ctrl_id = ctrl_id;
			if (strncmp(argv[optind], "--", 2) == 0 &&
				    optind + 2 <= argc)
				ctrlopt.ctrl_op[ctrlopt.ctrl_count].value =
					(int)strtol(argv[optind + 1], NULL, 0);
			else
				ctrlopt.ctrl_op[ctrlopt.ctrl_count].value =
					(int)strtol(argv[optind], NULL, 0);
			ctrlopt.ctrl_count++;
			break;
		case 'g':
			if (ctrlopt.ctrl_count >= MAX_CTRL_CNT) {
				printf("ctrl count(%d) > max(%d)\n",
					ctrlopt.ctrl_count, MAX_CTRL_CNT);
				return -1;
			}
			ctrlopt.ctrl_op[ctrlopt.ctrl_count].cmd_id = 'g';
			id = (int)strtol(optarg, NULL, 0);
			ctrl_id = cvt_to_isp_ctrl_id(id);
			if (ctrl_id <= 0) {
				printf("invalid ctrl id(%d)\n", id);
				return -1;
			}
			ctrlopt.ctrl_op[ctrlopt.ctrl_count].ctrl_id = ctrl_id;
			ctrlopt.ctrl_count++;
			break;
		case 'h':
			print_help_info();
			return 0;
		case ':':
			printf("required argument : -%c\n", optopt);
			return -1;
		case '?':
			printf("invalid param: -%c\n", optopt);
			return -1;
		}
	}

	rts_set_log_mask(RTS_LOG_MASK_CONS);

	if (!ctrlopt.ctrl_op[0].cmd_id || ctrlopt.ctrl_op[0].ctrl_id <= 0) {
		RTS_INFO("please assign ctrl id\n");
		return -1;
	}

	ret = test_isp_ctrl(&ctrlopt);
	if (ret)
		printf("Fail\n");
	else
		printf("Success\n");

	return ret;
}
