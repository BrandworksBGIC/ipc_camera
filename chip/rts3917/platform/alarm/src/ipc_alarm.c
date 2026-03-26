/*
 *  Copyright (C) 2019 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <rtsavapi.h>
#include <rtscamkit.h>
#include <rtsvideo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "ipc_alarm.h"
#include "ipc_video.h"
#include "iprt_alarm.h"

#include <rts_nn.h>
#include <rts_nn_log.h>
#include <rts_nn_od.h>
#include <rts_nn_types.h>

struct iou_box {
    int left;
    int top;
    int right;
    int bot;
    int width;
    int height;
};

static struct {
    struct rts_md2_result md_res;
    struct iou_box        md_box;
    struct iou_box        od_box;
    struct rts_md2_ctrl*  pmd2_ctrl;
    s32                   sensor_width;
    s32                   sensor_height;
    s32                   hd_width;
    s32                   hd_height;
    s32                   sd_width;
    s32                   sd_height;
    f32                   nn_width_scale;
    f32                   nn_height_scale;
    f32                   sensor_width_scale;
    f32                   sensor_height_scale;
    f32                   md_width_scale;
    f32                   md_height_scale;
    s32                   vin_id;
    s32                   vin_chn;
    s32                   vin_buf_num;
    s32                   frame_w;
    s32                   frame_h;
    s32                   fps;
    f32                   alarm_sensitive;
    rts_nn_handle         nn_handle;
    f32                   output_width_scale;
    f32                   output_height_scale;
    s32                   alarm_delay;
} g_alarm = {
    .hd_width            = 2560,
    .hd_height           = 1440,
    .sd_width            = 640,
    .sd_height           = 360,
    .nn_width_scale      = 10, // Ratio of main stream resolution to NN stream resolution   HD_WIDTH/256 = 10
    .nn_height_scale     = 9,  // HD_HEIGHT/160 = 9, note coordinates are even numbers
    .sensor_width_scale  = 1.0,
    .sensor_height_scale = 1.0,
    .vin_id              = 10,
    .vin_chn             = -1,
    .vin_buf_num         = 2,
    .frame_w             = 256,
    .frame_h             = 160,
    .fps                 = 3,
    .alarm_sensitive     = 0.2,
};

#define IOU_THRESH 0

#define OSD_BLOCK_INDEX 0
#define LINE_COLOUR 0xc3
#define LINE_WIDTH 8 // Line width, unit: number of pixels

#define __IOU_MAX(x, y) (((x) > (y)) ? (x) : (y))
#define __IOU_MIN(x, y) (((x) < (y)) ? (x) : (y))

static float get_odtiny_md_iou(struct iou_box* r1, struct iou_box* r2)
{
    int x1, x2, y1, y2;
    int U = 0, I = 0;

    x1 = __IOU_MAX(r1->left, r2->left);
    x2 = __IOU_MIN(r1->right, r2->right);

    y1 = __IOU_MAX(r1->top, r2->top);
    y2 = __IOU_MIN(r1->bot, r2->bot);

    if (x1 >= x2 || y1 >= y2)
        return 0.0;

    I = (float)((x2 - x1) * (y2 - y1));
    U = ((r1->right - r1->left) * (r1->bot - r1->top)) + ((r2->right - r2->left) * (r2->bot - r2->top)) - I;

    return 1.f * I / U;
}

static void make_md_box(struct rts_md2_result* res, int md_index, struct iou_box* box)
{
    int min_left   = res->cc_info.cc[md_index].l;
    int min_top    = res->cc_info.cc[md_index].u;
    int max_right  = res->cc_info.cc[md_index].r;
    int max_bottom = res->cc_info.cc[md_index].b;

    box->left   = g_alarm.sensor_width_scale * g_alarm.md_width_scale * min_left;
    box->top    = g_alarm.sensor_height_scale * g_alarm.md_height_scale * min_top;
    box->width  = g_alarm.sensor_width_scale * g_alarm.md_width_scale * (max_right - min_left);
    box->height = g_alarm.sensor_height_scale * g_alarm.md_height_scale * (max_bottom - min_top);
    box->right  = g_alarm.sensor_width_scale * g_alarm.md_width_scale * max_right;
    box->bot    = g_alarm.sensor_height_scale * g_alarm.md_height_scale * max_bottom;
}

static void md_fliter(struct ipc_plat_alarm_result_s* result, struct rts_md2_result* res, f32 score, f32 score_filter)
{
    int   j       = 0;
    float res_iou = 0.0;
    for (j = 0; j < res->cc_info.cc_len; j++) {
        make_md_box(res, j, &g_alarm.md_box);

        res_iou = get_odtiny_md_iou(&g_alarm.md_box, &g_alarm.od_box);
        printf("The IOU of person box and md box is %d %f score %f\n", j, res_iou, score);

        if (res_iou > IOU_THRESH) {

            result->rect[result->alarm_result_num].alarm_type = IPC_PLAT_ALARM_TYPE_MD;

            if (score > score_filter) {
                result->rect[result->alarm_result_num].alarm_type |= IPC_PLAT_ALARM_TYPE_AI_PEOPLE;
            }

            result->rect[result->alarm_result_num].lux = g_alarm.od_box.left;
            result->rect[result->alarm_result_num].luy = g_alarm.od_box.top;
            result->rect[result->alarm_result_num].rdx = g_alarm.od_box.left + g_alarm.od_box.width;
            result->rect[result->alarm_result_num].rdy = g_alarm.od_box.top + g_alarm.od_box.height;

            result->rect[result->alarm_result_num].lux *= g_alarm.output_width_scale;
            result->rect[result->alarm_result_num].luy *= g_alarm.output_height_scale;
            result->rect[result->alarm_result_num].rdx *= g_alarm.output_width_scale;
            result->rect[result->alarm_result_num].rdy *= g_alarm.output_height_scale;

            result->alarm_result_num++;
            break;
        }
    }
}

static void set_rectangle_parameter(struct ipc_plat_alarm_result_s* result, struct rts_nn_od_res* res)
{
    int nr_num = 0;
    int i      = 0;
    s32 ret    = 0;

    nr_num = res->num;

    struct rts_isp_control ctrl         = { 0 };
    f32                    score_filter = 0.7;

    ret = rts_av_get_isp_ctrl(RTS_ISP_CTRL_ID_IR_MODE, &ctrl);
    if (ret) {
        printf("Error, id: RTS_ISP_CTRL_ID_IR_MODE get isp attr fail, ret = %d\n", ret);
    }

    if (1 == ctrl.current_value) {
        score_filter = 0.8;
    }

    for (i = 0; i < nr_num; i++) {
        if (res->bboxes[i].id == 0) {
            g_alarm.od_box.left   = g_alarm.nn_width_scale * res->bboxes[i].x1;
            g_alarm.od_box.top    = g_alarm.nn_height_scale * res->bboxes[i].y1;
            g_alarm.od_box.width  = g_alarm.nn_width_scale * (res->bboxes[i].x2 - res->bboxes[i].x1);
            g_alarm.od_box.height = g_alarm.nn_height_scale * (res->bboxes[i].y2 - res->bboxes[i].y1);
            g_alarm.od_box.right  = g_alarm.nn_width_scale * res->bboxes[i].x2;
            g_alarm.od_box.bot    = g_alarm.nn_height_scale * res->bboxes[i].y2;
            md_fliter(result, &g_alarm.md_res, res->bboxes[i].score, score_filter);

            if (result->alarm_result_num >= sizeof(result->rect) / sizeof(result->rect[0])) {
                goto exit;
            }
        }
    }
exit:
    return;
}

static int __start_stream(void)
{
    struct rts_vin_attr   attr = { 0 };
    struct rts_av_profile pro;
    int                   ret = 0;

    attr.vin_buf_num = g_alarm.vin_buf_num;
    attr.vin_id      = g_alarm.vin_id;
    g_alarm.vin_chn  = rts_av_create_vin_chn(&attr);
    if (g_alarm.vin_chn < 0) {
        ret = -1;
        goto err;
    }

    pro.fmt               = RTS_V_FMT_RGB;
    pro.video.width       = g_alarm.frame_w;
    pro.video.height      = g_alarm.frame_h;
    pro.video.numerator   = 1;
    pro.video.denominator = g_alarm.fps;

    ret = rts_av_set_profile(g_alarm.vin_chn, &pro);
    if (ret < 0)
        goto err;

    return 0;

err:
    if (ret)
        printf("%s\n", rts_strerrno(ret));

    return ret;
}

int ivrt_get_alarm_video_chn()
{
    return g_alarm.vin_chn;
}

static void __set_md2_ctrl(struct rts_md2_ctrl* pctrl, struct rts_md2_attr* attr)
{
    pctrl->train_enable     = 1;
    pctrl->train_frames     = 10;
    pctrl->sensitivity      = 7;
    pctrl->back_thd         = 2;
    pctrl->learn_thd        = 248;
    pctrl->forget_thd       = 10;
    pctrl->scene_change_thd = 0.5 * attr->sample.w * attr->sample.h;

    pctrl->max_ar    = 2;
    pctrl->min_ar    = 0.05;
    pctrl->cc_ratio  = 0.000001;
    pctrl->nr_cc_thd = 100;
}

s32 ipc_plat_alarm_init(IPC_PLAT_ALARM_TYPE* support_alarm_type)
{
    struct rts_md2_attr md_attr;

    // coverity[MIXED_ENUM_TYPE :SUPPRESS]
    *support_alarm_type = IPC_PLAT_ALARM_TYPE_MD | IPC_PLAT_ALARM_TYPE_AI_PEOPLE;

    int ret;

    // coverity[MIXED_ENUM_TYPE :SUPPRESS]
    struct ipc_plat_video_capability cap = { 0 };

    ipc_plat_video_query_capability(&cap);

    g_alarm.hd_width  = cap.res[IPC_VIDEO_CHN_MAIN].width;
    g_alarm.hd_height = cap.res[IPC_VIDEO_CHN_MAIN].height;
    g_alarm.sd_width  = cap.res[IPC_VIDEO_CHN_SUB].width;
    g_alarm.sd_height = cap.res[IPC_VIDEO_CHN_SUB].height;

    __ipc_plat_video_get_resolution(&g_alarm.sensor_width, &g_alarm.sensor_height);

    g_alarm.nn_width_scale  = (float)g_alarm.hd_width / g_alarm.frame_w;
    g_alarm.nn_height_scale = (float)g_alarm.hd_height / g_alarm.frame_h;

    g_alarm.sensor_width_scale  = (float)g_alarm.hd_width / g_alarm.sensor_width;
    g_alarm.sensor_height_scale = (float)g_alarm.hd_height / g_alarm.sensor_height;

    g_alarm.output_width_scale  = (float)g_alarm.sd_width / g_alarm.hd_width;
    g_alarm.output_height_scale = (float)g_alarm.sd_height / g_alarm.hd_height;

    g_alarm.md_width_scale  = (float)g_alarm.sensor_width / 640;
    g_alarm.md_height_scale = (float)g_alarm.sensor_height / 360;

    /* md2参数 */
    md_attr.sample.x       = 0;
    md_attr.sample.y       = 0;
    md_attr.sample.w       = g_alarm.sensor_width / g_alarm.md_width_scale;
    md_attr.sample.h       = g_alarm.sensor_height / g_alarm.md_height_scale;
    md_attr.sample.scale_x = g_alarm.md_width_scale;
    md_attr.sample.scale_y = g_alarm.md_height_scale;

    // coverity[MIXED_ENUM_TYPE :SUPPRESS]
    md_attr.bin_bits = 0;

    // coverity[MIXED_ENUM_TYPE :SUPPRESS]
    md_attr.nr_bins     = 0;
    md_attr.skip_frames = 1;

    ret = rts_av_query_md2(&g_alarm.pmd2_ctrl, &md_attr);
    if (ret) {
        printf("query md2 failed [%d]\n", ret);
        return -1;
    }

    __set_md2_ctrl(g_alarm.pmd2_ctrl, &md_attr);

    // coverity[UNUSED_VALUE :SUPPRESS]
    ret = rts_av_set_md2(g_alarm.pmd2_ctrl);

    struct rts_nn_cfg cfg = { 0 };

    ret = __start_stream();
    if (ret) {
        printf("create video stream failed!\n");
        return -1;
    }

    /* 1. init network: model_name is "odnano" */
    // coverity[DC.STRING_BUFFER :SUPPRESS]
    // coverity[SECURE_CODING :SUPPRESS]
    strcpy(cfg.model_name, "odnano");

    ret = rts_nn_init(&g_alarm.nn_handle, &cfg);
    if (ret) {
        printf("init nn failed: %d\n", ret);
        return -1;
    }

    return 0;
}

s32 ipc_plat_alarm_uninit(void)
{

    RTS_SAFE_RELEASE(g_alarm.pmd2_ctrl, rts_av_release_md2);

    if (NULL != g_alarm.nn_handle)
        rts_nn_release(&g_alarm.nn_handle);

    if (g_alarm.vin_chn >= 0)
        rts_av_destroy_chn(g_alarm.vin_chn);

    return 0;
}

s32 ipc_plat_alarm_ctrl(IPC_PLAT_ALARM_CTRL_CMD cmd, vptr arg)
{

    switch (cmd) {
        case IPC_PLAT_ALARM_CTRL_CMD_START: {
            s32 ret = rts_av_enable_chn(g_alarm.vin_chn);
            if (ret != 0)
                goto err;

            ret = rts_av_start_recv(g_alarm.vin_chn);
            if (ret != 0)
                goto err;
        } break;
        case IPC_PLAT_ALARM_CTRL_CMD_STOP:
            rts_av_stop_recv(g_alarm.vin_chn);
            rts_av_disable_chn(g_alarm.vin_chn);
            break;
        case IPC_PLAT_ALARM_CTRL_CMD_SET_SENSITIVITY:
            g_alarm.alarm_sensitive = *(pf32)arg;
            break;
        case IPC_PLAT_ALARM_CTRL_CMD_NOTICE_IMAGE_CHANGING:
            g_alarm.alarm_delay = 2;
            break;
        default:
            break;
    }
err:
    return 0;
}

s32 ipc_plat_alarm_recv_result(struct ipc_plat_alarm_result_s* result, s32 timeout_ms)
{
    int                   ret    = 0;
    struct rts_nn_image   img    = { 0 };
    struct rts_nn_od_res* res    = NULL;
    struct rts_av_buffer* buffer = NULL;

    g_alarm.md_res.flags = 0xff; // g_pprc
    g_alarm.md_res.flags |= RTS_MD2_RESULT_FL_ENABLE_MOTION_MAP;
    g_alarm.md_res.motion_cnt = 0;

    memset(result, 0, sizeof(struct ipc_plat_alarm_result_s));

    result->image_width  = g_alarm.sd_width;
    result->image_height = g_alarm.sd_height;

    img.attr.fmt = RTS_NN_RGB_PLANAR;
    img.attr.w   = g_alarm.frame_w;
    img.attr.h   = g_alarm.frame_h;
    img.attr.c   = 3;

    ret = rts_av_recv_block(g_alarm.vin_chn, &buffer, timeout_ms);
    if (ret != 0) {
        return -1;
    }

    if (buffer) {

        ret = rts_av_poll_md2(g_alarm.pmd2_ctrl, 1000 / g_alarm.fps);
        if (ret == RTS_FALSE) {
            printf("poll md2 timeout\n");
            goto try_ai;
        }

        rts_av_get_md2_result(g_alarm.pmd2_ctrl, &g_alarm.md_res);

    try_ai:

        if (g_alarm.md_res.motion_cnt <= 0) {
            rts_av_put_buffer(buffer);
            return 0;
        }

        printf("\nmd cnt %u\n", g_alarm.md_res.cc_info.cc_len);
        img.virt[0] = buffer->vm_addr;
        img.phy[0]  = buffer->phy_addr;

        /* 3. run nn */
        ret = rts_nn_od_run(g_alarm.nn_handle, &img, &res);

        rts_av_put_buffer(buffer);

        if (ret || res == NULL) {
            printf("run network failed, ret: %d\n", ret);
            return -1;
        }

        /* 4. osd_tagging with result */
        if ((res->num > 0) && g_alarm.alarm_delay <= 0) {
            printf("\nai cnt %d\n", res->num);
            set_rectangle_parameter(result, res);
        } else {
            g_alarm.alarm_delay--;
        }
    }

    return 0;
}

s32 ipc_plat_alarm_release_result(struct ipc_plat_alarm_result_s* result)
{
    return 0;
}