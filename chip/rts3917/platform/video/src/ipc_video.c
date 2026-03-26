#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "ipc_osd.h"
#include "ipc_video.h"
#include "iprt_alarm.h"

#include <rts_isp_sensor.h>
#include <rtsavapi.h>
#include <rtsavisp.h>
#include <rtscamkit.h>
#include <rtsvideo.h>

#include "ipc_core.h"

#define IPC_VIDEO_PRINT(fmt, ...) printf("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)

#define IPRT_ALGO_AE_ID_D RTS_ISP_ALGO_AE_ID0
#define IPRT_ALGO_AWB_ID_D RTS_ISP_ALGO_AWB_ID0
#define IPRT_ALGO_AF_ID_D RTS_ISP_ALGO_AF_ID0
#define IPRT_ALGO_OTHER_ID_D RTS_ISP_ALGO_OTHER_ID0

#define IPRT_IQ_PATH_D "/app/rt/rtsisp/iqs"
#define IPRT_IQ_PATH_TEST_D "/mnt/sdcard/default.bin"
#define IPRT_SENSOR_PATH_D "/app/rt/rtsisp/sensors/libsensor_*.so"
#define IPRT_SENSOR_PATH_TEST_D "/tmp/libsensor_*.so"

#define IPRT_ALGO_DIR_D "/app/rt/rtsisp/algos"
#define IPRT_ALGO_AE_PATH_D IPRT_ALGO_DIR_D "/librts_algo_ae.so"
#define IPRT_ALGO_AWB_PATH_D IPRT_ALGO_DIR_D "/librts_algo_awb.so"
#define IPRT_ALGO_AF_PATH_D IPRT_ALGO_DIR_D "/librts_algo_af.so"
#define IPRT_ALGO_OTHER_PATH_D IPRT_ALGO_DIR_D "/librts_algo_other.so"

#define IPRT_ISP_WDR_LEVEL_D 50

    typedef enum {
        IPRT_ENCODER_TYPE_H264,
        IPRT_ENCODER_TYPE_H265,
        IPRT_ENCODER_TYPE_ALL
    } IPRT_ENCODER_TYPED_E;

typedef enum {
    IPRT_VIDEO_CHN_MAIN = IPC_VIDEO_CHN_MAIN,
    IPRT_VIDEO_CHN_SUB  = IPC_VIDEO_CHN_SUB,
    IPRT_VIDEO_CHN_JPEG = IPC_VIDEO_CHN_JPEG,
    IPRT_VIDEO_CHN_NUM
} IPRT_VIDEO_CHN_TYPE_E;

typedef struct video_sensor_info {
    s32 sensor_width;
    s32 sensor_height;
    s32 main_width;
    s32 main_height;
    s32 sub_width;
    s32 sub_height;
    v8 sensor_name[64];
} IPRT_SENSOR_INFO_S, *P_IPRT_SENSOR_INFO_S;

typedef struct video_encoder_attr {
    s32 qp;
    s32 min_qp;
    s32 max_qp;
    s32 intra_min_qp;
    s32 intra_max_qp;
    s32 max_pic_size;
    s32 intra_qp_offset;
    s32 bitrate;
    s32 min_bitrate;
    s32 max_bitrate;
    s32 gop;
    s32 level;
    s32 tier;
    s32 rotation;
    s32 mirror;
    union {
        s32 h264_ch;
        s32 h265_ch;
    };

} IPRT_VIDEO_ENCODER_ATTR_S, *P_IPRT_VIDEO_ENCODER_ATTR_S;

typedef struct video_context {
    s32 encode_h26x_type;
    s32 encode_type;
    s32 isp_id;
    s32 isp_buf_num;
    s32 isp_mode; /* RTS_AV_VIN_MODE, specific value assignment will be parameter checked by relteck, and if erroneous,
                     will be forcibly modified; see _gvrt_create_stream for details */
    s32 isp_ch;
    s32 mjpeg_ch;
    s32 width;
    s32 height;
    s32 framerate;
    struct rts_osdi_attr* osd_attr;
    IPRT_VIDEO_ENCODER_ATTR_S video_encoder[IPRT_ENCODER_TYPE_ALL];
} IPRT_VIDEO_CONTEXT_S, *P_IPRT_VIDEO_CONTEXT_S;

typedef struct video_attr {
    s32 init_ok;
    s32 jpeg_res_chn;
    struct rts_isp_awb_ctrl* isp_awb;
    struct rts_isp_ae_ctrl* isp_ae;
    IPRT_VIDEO_CONTEXT_S video_context[IPRT_VIDEO_CHN_NUM];
    s32 sensor_width;
    s32 sensor_height;
    v8 sensor_name[64];
    u64 image_change_time;
} IPRT_VIDEO_ATTR_S, *P_IPRT_VIDEO_ATTR_S;

static pthread_mutex_t _gvrt_video_ctrl_mutex   = PTHREAD_MUTEX_INITIALIZER;
static u8              _g_yuv_buffer[640 * 368 * 3 / 2] = { 0 };
static volatile u8     _g_in_process_yuv        = 0;

static IPRT_VIDEO_ATTR_S _gvrt_video_attr = {
    .video_context = {
        [IPRT_VIDEO_CHN_MAIN] = {
            .mjpeg_ch = -1,
            .isp_ch = -1,
            .video_encoder = {
                [IPRT_ENCODER_TYPE_H264] = {
                    .h264_ch = -1,
                },
                [IPRT_ENCODER_TYPE_H265] = {
                    .h265_ch = -1,
                }
            }
        },
        [IPRT_VIDEO_CHN_SUB] = {
            .mjpeg_ch = -1,
            .isp_ch = -1,
            .video_encoder = {
                [IPRT_ENCODER_TYPE_H264] = {
                    .h264_ch = -1,
                },
                [IPRT_ENCODER_TYPE_H265] = {
                    .h265_ch = -1,
                }
            }
        },
        [IPRT_VIDEO_CHN_JPEG] = {
            .mjpeg_ch = -1,
            .isp_ch = -1,
            .video_encoder = {
                [IPRT_ENCODER_TYPE_H264] = {
                    .h264_ch = -1,
                },
                [IPRT_ENCODER_TYPE_H265] = {
                    .h265_ch = -1,
                }
            }
        }
    }
};

#ifdef __SCALER_ENABLE__
static void __scale_up_100w(ps32 w, ps32 h)
{
    switch (*w) {
        case 1280:
            *w = 1920;
            *h = 1080;
            break;
        case 1920:
            *w = 2304;
            *h = 1296;
            break;
        case 2304:
            *w = 2560;
            *h = 1440;
            break;
        case 2560:
            *w = 2816;
            *h = 1584;
            break;
        case 2816:
            *w = 3840;
            *h = 2160;
            break;
        default:
            break;
    }
}
#endif

static s32 _gvrt_get_sensor_info(P_IPRT_SENSOR_INFO_S sensor_info, pv8 sensor_dir)
{
    s32 ret          = 0;
    s32 len          = 0;
    v8 sensor_so[64] = { 0 };
    len              = strlen(sensor_dir);

    /* Check the suffix (libsensor_xxx.so) */
    if (strncmp(sensor_dir + (len - 3), ".so", 3)) {
        IPC_VIDEO_PRINT("Error, sensor so [%s] suffix error\n", sensor_dir);
        return IPC_FAILED;
    }

    for (s32 p = 0; p < len; p++) {
        if (sensor_dir[p] == '_') {
            memcpy(sensor_so, sensor_dir + (p + 1), (len - 1) - p - 3);
            ret = 1;
            break;
        }
    }

    /* format error (xxx_xxx.so) */
    if (!ret) {
        IPC_VIDEO_PRINT("Error, sensor so [%s] format error\n", sensor_dir);
        return IPC_FAILED;
    }

    IPRT_SENSOR_INFO_S sensor_table[] = {
        { .sensor_name   = "jxk347p_mipi",
          .sensor_width  = 2816,
          .sensor_height = 1584,
          .main_width    = 3840,
          .main_height   = 2160,
          .sub_width     = 640,
          .sub_height    = 360 },
        { .sensor_name   = "jxk306p_mipi",
          .sensor_width  = 2560,
          .sensor_height = 1440,
          .main_width    = 2816,
          .main_height   = 1584,
          .sub_width     = 640,
          .sub_height    = 360 },
        { .sensor_name   = "jxq03p_mipi",
          .sensor_width  = 2304,
          .sensor_height = 1296,
          .main_width    = 2304,
          .main_height   = 1296,
          .sub_width     = 640,
          .sub_height    = 360 },
        { .sensor_name   = "jxk06",
          .sensor_width  = 2560,
          .sensor_height = 1440,
          .main_width    = 2560,
          .main_height   = 1440,
          .sub_width     = 640,
          .sub_height    = 360 },
        { .sensor_name   = "jxf38p_mipi",
          .sensor_width  = 1920,
          .sensor_height = 1080,
          .main_width    = 1920,
          .main_height   = 1080,
          .sub_width     = 640,
          .sub_height    = 360 },
        { .sensor_name   = "sc3235",
          .sensor_width  = 2304,
          .sensor_height = 1296,
          .main_width    = 2304,
          .main_height   = 1296,
          .sub_width     = 640,
          .sub_height    = 360 },
        { .sensor_name   = "sc401ai",
          .sensor_width  = 2560,
          .sensor_height = 1440,
          .main_width    = 2560,
          .main_height   = 1440,
          .sub_width     = 640,
          .sub_height    = 360 },
        { .sensor_name   = "sc2336_mipi",
          .sensor_width  = 1920,
          .sensor_height = 1080,
          .main_width    = 1920,
          .main_height   = 1080,
          .sub_width     = 640,
          .sub_height    = 360 },
        { .sensor_name   = "sc2336p_mipi",
          .sensor_width  = 1920,
          .sensor_height = 1080,
          .main_width    = 1920,
          .main_height   = 1080,
          .sub_width     = 640,
          .sub_height    = 360 },
        { .sensor_name   = "sc3338_mipi",
          .sensor_width  = 2304,
          .sensor_height = 1296,
          .main_width    = 2304,
          .main_height   = 1296,
          .sub_width     = 640,
          .sub_height    = 360 },
        { .sensor_name   = "sc5336",
          .sensor_width  = 2816,
          .sensor_height = 1584,
          .main_width    = 2816,
          .main_height   = 1584,
          .sub_width     = 640,
          .sub_height    = 360 },
        { .sensor_name   = "sc1b5ak_mipi",
          .sensor_width  = 1280,
          .sensor_height = 720,
          .main_width    = 1280,
          .main_height   = 720,
          .sub_width     = 640,
          .sub_height    = 360 }
    };

    // IPC_VIDEO_PRINT("=== sensor so [%s] ===\n", sensor_so);
    for (s32 i = 0; i < sizeof(sensor_table) / sizeof(sensor_table[0]); i++) {
        // IPC_VIDEO_PRINT("sensor so: %s, sensor_table: %s\n", sensor_so, sensor_table[i].sensor_name);
        if (strncmp(sensor_so, sensor_table[i].sensor_name, strlen(sensor_table[i].sensor_name)) == 0) {
            memcpy(sensor_info, &sensor_table[i], sizeof(IPRT_SENSOR_INFO_S));

#ifdef __SCALER_ENABLE__
            if (sensor_info->main_width == sensor_info->sensor_width) {
                __scale_up_100w(&sensor_info->main_width, &sensor_info->main_height);
            }
#endif
            IPC_VIDEO_PRINT("match sensor [%s], resolution [%d x %d]\n", sensor_info->sensor_name,
                           sensor_info->main_width, sensor_info->main_height);
            return IPC_SUCCESS;
        }
    }

    return IPC_FAILED;
}

static int get_valid_value(int id, int value, struct rts_isp_control* ctrl)
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

static int __gvrt_isp_set_attr(uint32_t id, int value)
{
    struct rts_isp_control ctrl;
    int ret;

    ret = rts_av_get_isp_ctrl(id, &ctrl);
    if (ret) {
        IPC_VIDEO_PRINT("id: %d get isp attr fail, ret = %d\n", id, ret);
        return ret;
    }
    value = get_valid_value(id, value, &ctrl);

    IPC_VIDEO_PRINT("before settting [%s] min = %d, max = %d, step = %d, default = %d, cur = %d, to set vaule = %d\n",
                   ctrl.name, ctrl.minimum, ctrl.maximum, ctrl.step, ctrl.default_value, ctrl.current_value, value);

    ctrl.current_value = value;
    ret                = rts_av_set_isp_ctrl(id, &ctrl);
    if (ret) {
        IPC_VIDEO_PRINT("id: %d set isp attr fail, ret = %d\n", id, ret);
        return ret;
    }

    /**
     * get check whether the new value is set or not,
     * no need in actual use
     */
    ret = rts_av_get_isp_ctrl(id, &ctrl);
    if (ret) {
        IPC_VIDEO_PRINT("get isp attr fail, ret = %d\n", ret);
        return ret;
    }

    IPC_VIDEO_PRINT("after settting [%s] default = %d, cur = %d\n", ctrl.name, ctrl.default_value, ctrl.current_value);

    return RTS_OK;
}

static int __gvrt_isp_get_attr(uint32_t id)
{
    struct rts_isp_control ctrl;
    int ret;

    ret = rts_av_get_isp_ctrl(id, &ctrl);
    if (ret) {
        IPC_VIDEO_PRINT("id: %d get isp attr fail, ret = %d\n", id, ret);
        return ret;
    }
    // IPC_VIDEO_PRINT("after setting : id:%d val:%d\n", id, ctrl.current_value);
    IPC_VIDEO_PRINT("%s min = %d, max = %d, step = %d, default = %d, cur = %d\n", ctrl.name, ctrl.minimum, ctrl.maximum,
                   ctrl.step, ctrl.default_value, ctrl.current_value);

    return ctrl.current_value;
}

static void print_ae_ctrl(struct rts_isp_ae_ctrl* ae, struct ipc_plat_isp_exp_status* status)
{

    // printf("iso : %d\n", ae->_manual.exposure_time * ae->_manual.total_gain);
    if (strstr(_gvrt_video_attr.sensor_name, "SC2300")) {
        status->ev = (ae->_manual.exposure_time / 1000) * (ae->_manual.gain.isp_digital);
    } else {
        status->ev = (ae->_manual.exposure_time / 1000) * (ae->_manual.gain.analog + ae->_manual.gain.isp_digital);
    }
    // printf("%s:%d:%d:%d:%d\n", __func__, ae->_manual.total_gain, ae->_manual.gain.digital, ae->_manual.gain.analog,
    // ae->_manual.gain.isp_digital);
}

static void print_awb_ctrl(struct rts_isp_awb_ctrl* awb, struct ipc_plat_isp_exp_status* status)
{

    uint16_t* pdata;
    int i;
    int rsum = 0;
    int gsum = 0;
    int bsum = 0;
    int ret  = 0;

    pdata = awb->statis.r_means;
    for (i = 0; i < awb->window_num; i++) {
        rsum += *pdata;
        pdata++;
    }
    // printf("r_means sum[%d]\n", rsum);

    pdata = awb->statis.g_means;
    for (i = 0; i < awb->window_num; i++) {
        gsum += *pdata;
        pdata++;
    }

    // printf("g_means sum[%d]\n", gsum);

    pdata = awb->statis.b_means;
    for (i = 0; i < awb->window_num; i++) {
        bsum += *pdata;
        pdata++;
    }

    // printf("b_means sum[%d]\n", bsum);

    status->wb_statis_b_g_diff = 0;
    status->wb_statis_r_g_diff = 0;

    ret = rsum - gsum;
    if (ret > 0) {
        status->wb_statis_r_g_diff = ret;
    }

    ret = bsum - gsum;
    if (ret > 0) {
        status->wb_statis_b_g_diff = ret;
    }
}

static s32 _gvrt_set_sensor_fps(s32 chn, u8 fps)
{
    s32 ret;
    P_IPRT_VIDEO_CONTEXT_S p_video_context = &_gvrt_video_attr.video_context[chn];

#if 1
    if (p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].h264_ch >= 0) {
        struct rts_h264_ctrl* h264_ctrl = NULL;
        ret = rts_av_query_h264_ctrl(p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].h264_ch, &h264_ctrl);
        if (ret) {
            IPC_VIDEO_PRINT("Error, query h264 ctrl fail, ret = %d\n", ret);
            return ret;
        }

        ret = rts_av_get_h264_ctrl(h264_ctrl);
        if (ret) {
            IPC_VIDEO_PRINT("set h264 ctrl fail, ret = %d\n", ret);
            return IPC_FAILED;
        }

        h264_ctrl->gop = fps * 3;
        ret            = rts_av_set_h264_ctrl(h264_ctrl);
        if (ret) {
            IPC_VIDEO_PRINT("Error, set h264 ctrl fail, ret = %d\n", ret);
            return IPC_FAILED;
        }

        ret = rts_av_get_h264_ctrl(h264_ctrl);
        if (ret) {
            IPC_VIDEO_PRINT("set h264 ctrl fail, ret = %d\n", ret);
            return IPC_FAILED;
        }

        IPC_VIDEO_PRINT("chn: %d, gop=%d\n", chn, h264_ctrl->gop);

        RTS_SAFE_RELEASE(h264_ctrl, rts_av_release_h264_ctrl);
    }

    if (p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].h265_ch >= 0) {
        struct rts_h265_ctrl* h265_ctrl = NULL;
        ret = rts_av_query_h265_ctrl(p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].h265_ch, &h265_ctrl);
        if (ret) {
            IPC_VIDEO_PRINT("Error, query h265 ctrl fail, ret = %d\n", ret);
            return ret;
        }

        ret = rts_av_get_h265_ctrl(h265_ctrl);
        if (ret) {
            IPC_VIDEO_PRINT("set h265 ctrl fail, ret = %d\n", ret);
            return IPC_FAILED;
        }

        h265_ctrl->gop = fps * 3;
        ret            = rts_av_set_h265_ctrl(h265_ctrl);
        if (ret) {
            IPC_VIDEO_PRINT("Error, set h265 ctrl fail, ret = %d\n", ret);
            return IPC_FAILED;
        }

        ret = rts_av_get_h265_ctrl(h265_ctrl);
        if (ret) {
            IPC_VIDEO_PRINT("set h265 ctrl fail, ret = %d\n", ret);
            return IPC_FAILED;
        }

        IPC_VIDEO_PRINT("[change gop chn:%d]gop=%d\n", chn, h265_ctrl->gop);

        RTS_SAFE_RELEASE(h265_ctrl, rts_av_release_h265_ctrl);
    }
#endif

#if 1
    u8 tmpfps;
    struct rts_av_profile profile;

    if (p_video_context->isp_ch < 0) {
        IPC_VIDEO_PRINT("Error, isp chn [%d] is not init\n", chn);
        return IPC_FAILED;
    }

    ret = rts_av_get_profile(p_video_context->isp_ch, &profile);
    if (ret) {
        IPC_VIDEO_PRINT("Error, get profile fail, ret = %d\n", ret);
        return IPC_FAILED;
    }

    tmpfps = profile.video.denominator / profile.video.numerator;

    profile.video.numerator   = 1;
    profile.video.denominator = fps;
    ret                       = rts_av_set_profile(p_video_context->isp_ch, &profile);
    if (ret) {
        IPC_VIDEO_PRINT("Error, set profile fail, ret = %d\n", ret);
        return IPC_FAILED;
    }

    /**
     * get check whether the new value is set or not,
     * no need in actual use
     */
    ret = rts_av_get_profile(p_video_context->isp_ch, &profile);
    if (ret) {
        IPC_VIDEO_PRINT("Error, get profile fail, ret = %d\n", ret);
        return IPC_FAILED;
    }
    IPC_VIDEO_PRINT("[change fps chn:%d]%d->%d\n", chn, tmpfps, profile.video.denominator / profile.video.numerator);
#endif

    return IPC_SUCCESS;
}

static s32 ___gvrt_register_sensor(uint32_t isp_id, P_IPRT_SENSOR_INFO_S sensor_info)
{
    s32 i;
    s32 ret;
    s32 id = -RTS_ISP_EINVAL;
    glob_t globbuf = { 0 };

    // IPC_VIDEO_PRINT("=== ___gvrt_register_sensor ===\n");

    system("cp /mnt/sdcard/rts_sn_so/libsensor_*.so /tmp/");
    ret = glob(IPRT_SENSOR_PATH_TEST_D, 0, NULL, &globbuf);
    if (0 == ret) {
        IPC_VIDEO_PRINT("Warning !!! Test so exist !!!\n");
    } else {
        ret = glob(IPRT_SENSOR_PATH_D, 0, NULL, &globbuf);
        if (ret)
            return -errno;
    }

    for (i = 0; i < globbuf.gl_pathc; i++) {
        struct rts_isp_sensor sensor;

        // IPC_VIDEO_PRINT("gl_pathv: %s\n", globbuf.gl_pathv[i]);
        sensor.path = globbuf.gl_pathv[i];
        id          = rts_av_isp_register_sensor(&sensor);
        if (id < 0)
            break;

        ret = rts_av_isp_check_sensor(isp_id, id);
        if (!ret) {
            ret = _gvrt_get_sensor_info(sensor_info, globbuf.gl_pathv[i]);
            if (IPC_SUCCESS != ret) {
                id = -RTS_ISP_EINVAL;
            }
            break;
        }
        rts_av_isp_unregister_sensor(id);
        id = -RTS_ISP_EINVAL;
    }

    globfree(&globbuf);

    return id;
}

static s32 ___gvrt_register_algo(enum rts_isp_algo_id id, char* path)
{
    s32 ret;
    struct rts_isp_algo algo;

    if (!path)
        return -RTS_ISP_EINVAL;

    algo.id   = id;
    algo.path = path;

    ret = rts_av_isp_register_algo(&algo);
    if (ret < 0)
        return ret;
    ret = rts_av_isp_bind_algo(ISP0, id);
    if (ret)
        return ret;

    return RTS_ISP_OK;
}

static void* ___gvrt_thread_isp_start(void* arg)
{
    s32 ret = 0;

    ret = rts_av_isp_start();
    if (ret) {
        IPC_VIDEO_PRINT("Error, rts av isp start failed, ret: %d\n", ret);
    }

    return NULL;
}

static int __gvrt_register_sensor_iq(P_IPRT_SENSOR_INFO_S sensor_info)
{
    s32 ret = RTS_ISP_OK;
    s32 sensor_id;
    v8 iq_bin_path[128] = { 0 };

    // IPC_VIDEO_PRINT("=== __gvrt_register_sensor_iq ===\n");

    sensor_id = ___gvrt_register_sensor(ISP0, sensor_info);
    if (sensor_id < 0) {
        ret = sensor_id;
        IPC_VIDEO_PRINT("Error, register sensor failed, ret: %d\n", ret);
        goto register_sensor_iq_err;
    }

    ret = rts_av_isp_bind_sensor(ISP0, sensor_id);
    if (ret) {
        IPC_VIDEO_PRINT("Error, rts av isp bind sensor failed, ret: %d\n", ret);
        goto register_sensor_iq_err;
    }

    if (0 == access(IPRT_IQ_PATH_TEST_D, F_OK)) {
        snprintf(iq_bin_path, sizeof(iq_bin_path), "%s", IPRT_IQ_PATH_TEST_D);
        IPC_VIDEO_PRINT("Warning!!! Test iq bin exist, using test iq bin: %s\n", iq_bin_path);
    } else {
        snprintf(iq_bin_path, sizeof(iq_bin_path), "%s/%s.bin", IPRT_IQ_PATH_D, sensor_info->sensor_name);
        IPC_VIDEO_PRINT("iq bin path: %s\n", iq_bin_path);
        if (access(iq_bin_path, F_OK)) {
            snprintf(iq_bin_path, sizeof(iq_bin_path), "%s/default.bin", IPRT_IQ_PATH_D);
            IPC_VIDEO_PRINT("iq bin path not exist, set default: %s\n", iq_bin_path);
        }
    }

    ret = rts_av_isp_register_iq(ISP0, iq_bin_path);
    if (ret) {
        IPC_VIDEO_PRINT("Error, rts av isp register iq failed, ret: %d\n", ret);
        goto register_sensor_iq_err;
    }

register_sensor_iq_err:
    if (ret)
        rts_isp_perror(ret, "register sensor iq fail");

    return ret;
}

static s32 __gvrt_register_all_algos(void)
{
    s32 ret;

    ret = ___gvrt_register_algo(IPRT_ALGO_AE_ID_D, IPRT_ALGO_AE_PATH_D);
    if (ret) {
        IPC_VIDEO_PRINT("Error, register algo AE failed, ret: %d\n", ret);
        goto register_algos_err;
    }
    ret = ___gvrt_register_algo(IPRT_ALGO_AWB_ID_D, IPRT_ALGO_AWB_PATH_D);
    if (ret) {
        IPC_VIDEO_PRINT("Error, register algo AWB failed, ret: %d\n", ret);
        goto register_algos_err;
    }
    ret = ___gvrt_register_algo(IPRT_ALGO_AF_ID_D, IPRT_ALGO_AF_PATH_D);
    if (ret) {
        IPC_VIDEO_PRINT("Error, register algo AF failed, ret: %d\n", ret);
        goto register_algos_err;
    }

    ret = ___gvrt_register_algo(IPRT_ALGO_OTHER_ID_D, IPRT_ALGO_OTHER_PATH_D);
    if (ret) {
        IPC_VIDEO_PRINT("Error, register algo OTHER failed, ret: %d\n", ret);
        goto register_algos_err;
    }

register_algos_err:
    if (ret)
        rts_isp_perror(ret, "register algos fail");
    return ret;
}

static s32 _gvrt_isp_init(P_IPRT_SENSOR_INFO_S sensor_info)
{
    s32 ret = 0;

    // IPC_VIDEO_PRINT("=== gvtr isp init ===\n");
    ret = rts_av_isp_init();
    if (ret) {
        IPC_VIDEO_PRINT("Error, rts isp init failed, ret: %d\n", ret);
        goto isp_init_err;
    }

    ret = __gvrt_register_sensor_iq(sensor_info);
    if (ret) {
        IPC_VIDEO_PRINT("Error, register sensor iq failed, ret: %d\n", ret);
        goto isp_init_err;
    }

    ret = __gvrt_register_all_algos();
    if (ret) {
        IPC_VIDEO_PRINT("Error, register all algos failed, ret: %d\n", ret);
        goto isp_init_err;
    }

    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 128 * 1024);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    // coverity[UNUSED_VALUE :SUPPRESS]
    ret = pthread_create(&tid, &attr, ___gvrt_thread_isp_start, NULL);
    pthread_attr_destroy(&attr);

    do {
        ret = rts_av_isp_get_status();
        usleep(100000);
    } while (ret != RTS_ISP_RUNNING);

    return IPC_SUCCESS;

isp_init_err:
    if (ret)
        rts_isp_perror(ret, "rts isp fail");

    rts_av_isp_stop();
    rts_av_isp_cleanup();

    return IPC_FAILED;
}

static void _gvrt_isp_uninit(void)
{
    rts_av_isp_stop();
    rts_av_isp_cleanup();
}

static s32 _g_frame_cnt[4];
static s32 _g_frame_size[4];
static void* ___gvrt_thread_frame_rate(void* arg)
{
    time_t cur_time_ts  = 0;
    time_t last_time_ts = 0;

    while (_gvrt_video_attr.init_ok) {
        cur_time_ts = time(NULL);

        if (cur_time_ts - last_time_ts >= 5) {
            IPC_VIDEO_PRINT("chn[%d], fps[%d], size[%dkB/s]\n", IPC_VIDEO_CHN_MAIN, _g_frame_cnt[IPC_VIDEO_CHN_MAIN] / 5,
                           ((_g_frame_size[IPC_VIDEO_CHN_MAIN]) / 1024) / 5);
            IPC_VIDEO_PRINT("chn[%d], fps[%d], size[%dkB/s]\n", IPC_VIDEO_CHN_SUB, _g_frame_cnt[IPC_VIDEO_CHN_SUB] / 5,
                           ((_g_frame_size[IPC_VIDEO_CHN_SUB]) / 1024) / 5);
            IPC_VIDEO_PRINT("chn[%d], fps[%d], size[%dkB/s]\n\n", IPC_VIDEO_CHN_YUV, _g_frame_cnt[IPC_VIDEO_CHN_YUV] / 5,
                           ((_g_frame_size[IPC_VIDEO_CHN_YUV]) / 1024) / 5);

            last_time_ts = cur_time_ts;
            memset(_g_frame_cnt, 0, sizeof(_g_frame_cnt));
            memset(_g_frame_size, 0, sizeof(_g_frame_size));
        }

        usleep(10000);
    }

    return NULL;
}

static void _gvrt_frame_rate_statistics(void)
{
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 128 * 1024);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, ___gvrt_thread_frame_rate, NULL);
    pthread_attr_destroy(&attr);
}

static s32 _gvrt_init_sys_vmem(int vin_mode_0, int vin_mode_1)
{
    s32 ret                     = 0;
    struct rts_sys_vmem_cfg cfg = { 0 };
    s32 share                   = 0;
    s32 status                  = 0;

    status = rts_av_sys_vmem_status();

    if (status == RTS_SYS_VMEM_STATUS_ON)
        goto out;

    /* 1-channel */
    {
        cfg.stream[0].enable = 1;
        cfg.stream[0].fmt    = RTS_V_FMT_YUV420SEMIPLANAR;
        cfg.stream[0].width  = 0;
        cfg.stream[0].height = 0;

        /* vin */
        cfg.stream[0].module[0].type = RTS_AV_ID_VIN;
        cfg.stream[0].module[0].cnt  = 1;
        cfg.stream[0].module[0].mode = vin_mode_0;

        /* h26x */
        cfg.stream[0].module[1].type
            = (_gvrt_video_attr.video_context[IPRT_VIDEO_CHN_MAIN].encode_type & IPC_VIDEO_ENC_TYPE_H264)
                  ? RTS_AV_ID_H264
                  : RTS_AV_ID_H265;
        cfg.stream[0].module[1].cnt           = 1;
        cfg.stream[0].module[1].outbuf.setted = 1;
        cfg.stream[0].module[1].outbuf.shared = share;
        cfg.stream[0].module[1].outbuf.num    = 1;
        cfg.stream[0].module[1].outbuf.size   = 0; // default size

        /* mjpeg */
        if (_gvrt_video_attr.video_context[IPRT_VIDEO_CHN_MAIN].encode_type & IPC_VIDEO_ENC_TYPE_JPEG) {
            cfg.stream[0].module[2].type          = RTS_AV_ID_MJPGENC;
            cfg.stream[0].module[2].cnt           = 1;
            cfg.stream[0].module[2].outbuf.setted = 1;
            cfg.stream[0].module[2].outbuf.shared = share;
            cfg.stream[0].module[2].outbuf.num    = 1;
            cfg.stream[0].module[2].outbuf.size   = 0; // default size
        }
    }

    /* 2-channel */
    {
        cfg.stream[1].enable = 1;
        cfg.stream[1].fmt    = RTS_V_FMT_YUV420SEMIPLANAR;
        cfg.stream[1].width  = 0;
        cfg.stream[1].height = 0;

        /* vin */
        cfg.stream[1].module[0].type = RTS_AV_ID_VIN;
        cfg.stream[1].module[0].cnt  = 1;
        cfg.stream[1].module[0].mode = vin_mode_1;

        /* h26x */
        cfg.stream[1].module[1].type
            = (_gvrt_video_attr.video_context[IPRT_VIDEO_CHN_SUB].encode_type & IPC_VIDEO_ENC_TYPE_H264)
                  ? RTS_AV_ID_H264
                  : RTS_AV_ID_H265;
        cfg.stream[1].module[1].cnt           = 1;
        cfg.stream[1].module[1].outbuf.setted = 1;
        cfg.stream[1].module[1].outbuf.shared = 0;
        cfg.stream[1].module[1].outbuf.num    = 1;
        cfg.stream[1].module[1].outbuf.size   = 0; // default size

        /* mjpeg */
        if (_gvrt_video_attr.video_context[IPRT_VIDEO_CHN_SUB].encode_type & IPC_VIDEO_ENC_TYPE_JPEG) {
            cfg.stream[1].module[2].type          = RTS_AV_ID_MJPGENC;
            cfg.stream[1].module[2].cnt           = 1;
            cfg.stream[1].module[2].outbuf.setted = 1;
            cfg.stream[1].module[2].outbuf.shared = share;
            cfg.stream[1].module[2].outbuf.num    = 1;
            cfg.stream[1].module[2].outbuf.size   = 0; // default size
        }
    }


    /* 4-channel */
    {
        cfg.stream[3].enable = 1;
        cfg.stream[3].fmt    = RTS_V_FMT_RGB;
        cfg.stream[3].width  = 0;
        cfg.stream[3].height = 0;

        /* vin */
        cfg.stream[3].module[0].type = RTS_AV_ID_VIN;
        cfg.stream[3].module[0].cnt  = 1;
        cfg.stream[3].module[0].mode = vin_mode_1;
    }

    ret = rts_av_sys_vmem_set_conf(&cfg);
    if (ret) {
        RTS_ERR("failed to set sysmem cfg, ret:%d\n", ret);
        return ret;
    }

    ret = rts_av_sys_vmem_init();
    if (ret) {
        RTS_ERR("failed to init sysmem cfg, ret:%d\n", ret);
        return ret;
    }

out:
    return ret;
}

static void _gvrt_release_sys_vmem(void)
{
    s32 status = 0;

    status = rts_av_sys_vmem_status();

    if (status == RTS_SYS_VMEM_STATUS_OFF)
        return;

    rts_av_sys_vmem_release();
}

static s32 __gvrt_check_isp_attr_cfg(struct rts_vin_attr* p_isp_attr)
{
    if (p_isp_attr->vin_id != 0 && p_isp_attr->vin_mode == RTS_AV_VIN_RING_MODE) {
        RTS_ERR("vin ring mode only work in vin_id 0\n");
        return RTS_RETURN(RTS_E_INVALID_ARG);
    }
    if (p_isp_attr->vin_id > 1 && p_isp_attr->vin_mode == RTS_AV_VIN_DIRECT_MODE) {
        RTS_ERR("vin direct mode only work in vin_id 0/1\n");
        return RTS_RETURN(RTS_E_INVALID_ARG);
    }
    if (p_isp_attr->vin_id < 0 && p_isp_attr->vin_mode > 2) {
        RTS_ERR("vin mode range [0~2]\n");
        return RTS_RETURN(RTS_E_INVALID_ARG);
    }

    return RTS_OK;
}

static s32 __gvrt_create_mjpeg_encoder(P_IPRT_VIDEO_CONTEXT_S p_video_context, s32 chn)
{
    s32 ret                         = 0;
    struct rts_jpgenc_attr jpg_attr = { 0 };

    jpg_attr.stream_mode      = RTS_AV_JPG_TRIGGER;
    jpg_attr.rotation         = RTS_AV_ROTATION_0;
    p_video_context->mjpeg_ch = rts_av_create_mjpeg_chn(&jpg_attr);
    if (p_video_context->mjpeg_ch < 0) {
        printf("[%s:%d] creat mjpeg channel error, chn: %d\n", __func__, __LINE__, chn);
        ret = IPC_FAILED;
        goto mjpeg_encoder_err;
    }

    ret = rts_av_bind(p_video_context->isp_ch, p_video_context->mjpeg_ch);
    if (ret) {
        printf("[%s:%d] bind mjpeg to isp error, chn: %d\n", __func__, __LINE__, chn);
        ret = IPC_FAILED;
        goto mjpeg_encoder_err;
    }

    return IPC_SUCCESS;

mjpeg_encoder_err:
    return ret;
}

static s32 __gvrt_create_h264_encoder(P_IPRT_VIDEO_CONTEXT_S p_video_context, s32 chn)
{
    s32 ret                        = 0;
    struct rts_h264_attr h264_attr = { 0 };
    struct rts_h264_ctrl* h264_ctl = NULL;

    h264_attr.level    = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].level;
    h264_attr.rotation = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].rotation;
    h264_attr.mirror   = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].mirror;

    p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].h264_ch = rts_av_create_h264_chn(&h264_attr);
    if (p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].h264_ch < 0) {
        printf("[%s:%d] creat h264 channel error, chn: %d\n", __func__, __LINE__, chn);
        ret = IPC_FAILED;
        goto h264_encoder_err;
    }

    ret = rts_av_bind(p_video_context->isp_ch, p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].h264_ch);
    if (ret) {
        printf("[%s:%d] bind h264 to isp error, chn: %d\n", __func__, __LINE__, chn);
        ret = IPC_FAILED;
        goto h264_encoder_err;
    }

    rts_av_query_h264_ctrl(p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].h264_ch, &h264_ctl);
    // coverity[CHECKED_RETURN :SUPPRESS]
    rts_av_get_h264_ctrl(h264_ctl);

    h264_ctl->forced_idr_header_enable = 1;
    h264_ctl->bitrate_mode             = RTS_BITRATE_MODE_C_VBR;
    h264_ctl->qp                       = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].qp;
    h264_ctl->bitrate                  = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].bitrate;
    h264_ctl->gop                      = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].gop;
    h264_ctl->min_bitrate              = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].min_bitrate;
    h264_ctl->max_bitrate              = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].max_bitrate;
    h264_ctl->qp                       = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].qp;
    h264_ctl->min_qp                   = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].min_qp;
    h264_ctl->max_qp                   = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].max_qp;
    h264_ctl->intra_min_qp             = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].intra_min_qp;
    h264_ctl->intra_max_qp             = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].intra_max_qp;
    h264_ctl->intra_qp_delta           = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].intra_qp_offset;
    /* For Realtek 264, this parameter is qp_delta; for 265, this parameter is qp_offset */
    h264_ctl->max_pic_size = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].max_pic_size;

    ret = rts_av_set_h264_ctrl(h264_ctl);
    if (ret) {
        rts_av_release_h264_ctrl(h264_ctl);
        printf("[%s:%d] Failed to set h264 ctrl, ret: %d, chn: %d\n", __func__, __LINE__, ret, chn);
        ret = IPC_FAILED;
        goto h264_encoder_err;
    }
    rts_av_release_h264_ctrl(h264_ctl);

    return IPC_SUCCESS;

h264_encoder_err:
    return ret;
}

static s32 __gvrt_create_h265_encoder(P_IPRT_VIDEO_CONTEXT_S p_video_context, s32 chn)
{
    s32 ret                        = 0;
    struct rts_h265_attr h265_attr = { 0 };
    struct rts_h265_ctrl* h265_ctl = NULL;

    h265_attr.level    = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].level;
    h265_attr.tier     = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].tier;
    h265_attr.rotation = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].rotation;
    h265_attr.mirror   = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].mirror;

    printf("[%s] === h265 attr level: %d, tier: %d, rotation: %d, mirror: %d ===\n", __func__, h265_attr.level,
           h265_attr.tier, h265_attr.rotation, h265_attr.mirror);

    p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].h265_ch = rts_av_create_h265_chn(&h265_attr);
    if (p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].h265_ch < 0) {
        printf("[%s:%d] creat h265 channel error, ret:%d, chn: %d\n", __func__, __LINE__,
               p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].h265_ch, chn);
        ret = IPC_FAILED;
        goto h265_encoder_err;
    }

    ret = rts_av_bind(p_video_context->isp_ch, p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].h265_ch);
    if (ret) {
        printf("[%s:%d] bind h265 to isp error, ret: %d, chn: %d\n", __func__, __LINE__, ret, chn);
        ret = IPC_FAILED;
        goto h265_encoder_err;
    }

    rts_av_query_h265_ctrl(p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].h265_ch, &h265_ctl);

    // coverity[CHECKED_RETURN :SUPPRESS]
    rts_av_get_h265_ctrl(h265_ctl);

    h265_ctl->forced_idr_header_enable = 1;
    h265_ctl->bitrate_mode             = RTS_BITRATE_MODE_C_VBR;
    h265_ctl->gop                      = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].gop;
    h265_ctl->bitrate                  = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].bitrate;
    h265_ctl->min_bitrate              = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].min_bitrate;
    h265_ctl->max_bitrate              = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].max_bitrate;
    h265_ctl->qp                       = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].qp;
    h265_ctl->min_qp                   = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].min_qp;
    h265_ctl->max_qp                   = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].max_qp;
    h265_ctl->intra_min_qp             = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].intra_min_qp;
    h265_ctl->intra_max_qp             = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].intra_max_qp;
    h265_ctl->intra_qp_offset          = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].intra_qp_offset;
    h265_ctl->max_pic_size             = p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].max_pic_size;
    // h265_ctl->hvs_qp_enable = 1;
    // h265_ctl->hvs_qp_scale = 4;

    ret = rts_av_set_h265_ctrl(h265_ctl);
    if (ret) {
        rts_av_release_h265_ctrl(h265_ctl);
        printf("[%s:%d] Failed to set h265 ctrl, ret: %d, chn: %d\n", __func__, __LINE__, ret, chn);
        ret = IPC_FAILED;
        goto h265_encoder_err;
    }
    rts_av_release_h265_ctrl(h265_ctl);

    return IPC_SUCCESS;

h265_encoder_err:
    return ret;
}

static void save_yuv(void* priv, struct rts_av_profile* profile, struct rts_av_buffer* buffer)
{
    if (_g_in_process_yuv) {
        return;
    }

    memcpy(_g_yuv_buffer, buffer->vm_addr, 640 * 360 * 3 / 2);

    _g_in_process_yuv = 2;
}

static s32 _gvrt_create_stream(s32 chn, s32 encode_type)
{
    IPC_VIDEO_PRINT("====== start create stream chn: %d ======\n", chn);
    int ret = 0;

    struct rts_video_rect crop        = { 0 };
    struct rts_vin_attr isp_attr      = { 0 };
    struct rts_av_profile isp_profile = { 0 };

    P_IPRT_VIDEO_CONTEXT_S p_video_context = &_gvrt_video_attr.video_context[chn];

    isp_attr.vin_id      = p_video_context->isp_id;
    isp_attr.vin_buf_num = p_video_context->isp_buf_num;
    isp_attr.vin_mode    = isp_attr.vin_id ? RTS_AV_VIN_FRAME_MODE : p_video_context->isp_mode;
    ret                  = __gvrt_check_isp_attr_cfg(&isp_attr);
    if (RTS_IS_ERR(ret)) {
        printf("[%s:%d] fail to check cfg, ret %d, chn: %d\n", __func__, __LINE__, ret, chn);
        return IPC_FAILED;
    }

    p_video_context->isp_ch = rts_av_create_vin_chn(&isp_attr);
    if (p_video_context->isp_ch < 0) {
        printf("[%s:%d] creat isp channel error, chn: %d, ret: %d\n", __func__, __LINE__, chn, p_video_context->isp_ch);
        ret = IPC_FAILED;
        goto create_stream_err;
    }

    isp_profile.fmt               = RTS_V_FMT_YUV420SEMIPLANAR;
    isp_profile.video.width       = p_video_context->width;
    isp_profile.video.height      = p_video_context->height;
    isp_profile.video.numerator   = 1;
    isp_profile.video.denominator = p_video_context->framerate;
    ret                           = rts_av_set_profile(p_video_context->isp_ch, &isp_profile);
    if (ret) {
        printf("[%s:%d] rts_av_set_profile error, chn:%d, ret:%d\n", __func__, __LINE__, chn, ret);
        ret = IPC_FAILED;
        goto create_stream_err;
    }

    if (IPC_VIDEO_ENC_TYPE_JPEG & encode_type) {
        printf("[%s:%d] chn: %d support mjpeg, init mjpeg encoder\n", __func__, __LINE__, chn);
        ret = __gvrt_create_mjpeg_encoder(p_video_context, chn);
        if (ret != IPC_SUCCESS) {
            goto create_stream_err;
        }
    }

    if (IPC_VIDEO_ENC_TYPE_H264 & encode_type) {
        printf("[%s:%d] chn: %d support H264, init H264 encoder\n", __func__, __LINE__, chn);
        ret = __gvrt_create_h264_encoder(p_video_context, chn);
        if (ret != IPC_SUCCESS) {
            goto create_stream_err;
        }
    }

    if (IPC_VIDEO_ENC_TYPE_H265 & encode_type) {
        printf("[%s:%d] chn: %d support H265, init H265 encoder\n", __func__, __LINE__, chn);
        ret = __gvrt_create_h265_encoder(p_video_context, chn);
        if (ret != IPC_SUCCESS) {
            goto create_stream_err;
        }
    }

    if (chn == IPRT_VIDEO_CHN_SUB) {
        struct rts_av_callback cb;
    
        cb.func     = save_yuv;
        cb.start    = 0;
        cb.times    = -1;
        cb.interval = 0;
        cb.type     = RTS_AV_CB_TYPE_SYNC;
        cb.priv     = NULL;
        ret         = rts_av_set_callback(p_video_context->isp_ch, &cb, 0);
        if (ret) {
            printf("fail to set yuv callback, ret = %d\n", ret);
            return IPC_FAILED;
        }
    }


    ret = rts_av_enable_chn(p_video_context->isp_ch);
    if (ret) {
        printf("[%s:%d] enable isp ch error, chn:%d, ret:%d\n", __func__, __LINE__, chn, ret);
        return IPC_FAILED;
    }

    ret = rts_av_set_isp_fov_mode(p_video_context->isp_ch, 1);
    if (ret) {
        printf("set fov mode of channel %d fail, ret = %d\n", chn, ret);
        return IPC_FAILED;
    }

    ret = rts_av_get_isp_crop(p_video_context->isp_ch, &crop);
    if (RTS_IS_ERR(ret)) {
        RTS_ERR("Failed to set isp crop, ret = %d\n", ret);
        return IPC_FAILED;
    }

    printf("[%d]isp crop:start_x = %d,start_y = %d,end_x = %d,end_y = %d\n", p_video_context->isp_ch, crop.start.x,
           crop.start.y, crop.end.x, crop.end.y);

    return IPC_SUCCESS;

create_stream_err:
    if (p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].h264_ch >= 0) {
        rts_av_destroy_chn(p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].h264_ch);
        p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].h264_ch = -1;
    }

    if (p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].h265_ch >= 0) {
        rts_av_destroy_chn(p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].h265_ch);
        p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].h265_ch = -1;
    }

    if (p_video_context->mjpeg_ch >= 0) {
        rts_av_destroy_chn(p_video_context->mjpeg_ch);
        p_video_context->mjpeg_ch = -1;
    }

    if (p_video_context->isp_ch >= 0) {
        rts_av_destroy_chn(p_video_context->isp_ch);
        p_video_context->isp_ch = -1;
    }

    return ret;
}

static s32 _gvrt_stream_uninit(s32 chn)
{
    printf("%s:%d\n", __func__, chn);
    P_IPRT_VIDEO_CONTEXT_S p_video_context = &_gvrt_video_attr.video_context[chn];

    IPC_VIDEO_PRINT("====== destory h264 chn[%d] ======\n",
                   p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].h264_ch);
    if (p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].h264_ch >= 0) {
        rts_av_destroy_chn(p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].h264_ch);
        p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].h264_ch = -1;
    }

    IPC_VIDEO_PRINT("====== destory h265 chn[%d] ======\n",
                   p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].h265_ch);
    if (p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].h265_ch >= 0) {
        rts_av_destroy_chn(p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].h265_ch);
        p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].h265_ch = -1;
    }

    IPC_VIDEO_PRINT("====== destory mjpeg chn[%d] ======\n", p_video_context->mjpeg_ch);
    if (p_video_context->mjpeg_ch >= 0) {
        rts_av_destroy_chn(p_video_context->mjpeg_ch);
        p_video_context->mjpeg_ch = -1;
    }

    IPC_VIDEO_PRINT("====== destory isp chn[%d] ======\n", p_video_context->isp_ch);
    if (p_video_context->isp_ch >= 0) {
        rts_av_destroy_chn(p_video_context->isp_ch);
        p_video_context->isp_ch = -1;
    }

    return 0;
}

static s32 _gvrt_start_stream(s32 chn)
{
    s32 ret                                = 0;
    P_IPRT_VIDEO_CONTEXT_S p_video_context = NULL;

    if (IPRT_VIDEO_CHN_JPEG == chn) {
        // IPC_VIDEO_PRINT("### To get picture ###\n");
        p_video_context = &_gvrt_video_attr.video_context[_gvrt_video_attr.jpeg_res_chn];
        rts_av_start_recv(p_video_context->mjpeg_ch);
        ret = rts_av_enable_chn(p_video_context->mjpeg_ch);
        if (ret) {
            printf("[%s:%d] enable mjpeg ch error, chn:%d, ret:%d\n", __func__, __LINE__, chn, ret);
            return IPC_FAILED;
        }

        return IPC_SUCCESS;
    }

    p_video_context = &_gvrt_video_attr.video_context[chn];


    if (p_video_context->encode_type & IPC_VIDEO_ENC_TYPE_H264) {
        rts_av_start_recv(p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].h264_ch);
        ret = rts_av_enable_chn(p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].h264_ch);
        if (ret) {
            printf("[%s:%d] enable h264 ch error, chn:%d, ret:%d\n", __func__, __LINE__, chn, ret);
            return IPC_FAILED;
        }
    }

    if (p_video_context->encode_type & IPC_VIDEO_ENC_TYPE_H265) {
        rts_av_start_recv(p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].h265_ch);
        ret = rts_av_enable_chn(p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].h265_ch);
        if (ret) {
            printf("[%s:%d] enable h265 ch error, chn:%d, ret:%d\n", __func__, __LINE__, chn, ret);
            return IPC_FAILED;
        }
    }

    return IPC_SUCCESS;
}

static void _gvrt_stop_stream(s32 chn)
{
    P_IPRT_VIDEO_CONTEXT_S p_video_context = NULL;

    if (IPRT_VIDEO_CHN_JPEG == chn) {
        p_video_context = &_gvrt_video_attr.video_context[_gvrt_video_attr.jpeg_res_chn];
        rts_av_stop_recv(p_video_context->mjpeg_ch);
        rts_av_disable_chn(p_video_context->mjpeg_ch);

        return;
    }

    p_video_context = &_gvrt_video_attr.video_context[chn];

    if (p_video_context->video_encoder[IPC_VIDEO_ENC_TYPE_H264].h264_ch >= 0) {
        rts_av_stop_recv(p_video_context->video_encoder[IPC_VIDEO_ENC_TYPE_H264].h264_ch);
        rts_av_disable_chn(p_video_context->video_encoder[IPC_VIDEO_ENC_TYPE_H264].h264_ch);
    }
    if (p_video_context->video_encoder[IPC_VIDEO_ENC_TYPE_H264].h265_ch >= 0) {
        rts_av_stop_recv(p_video_context->video_encoder[IPC_VIDEO_ENC_TYPE_H264].h265_ch);
        rts_av_disable_chn(p_video_context->video_encoder[IPC_VIDEO_ENC_TYPE_H264].h265_ch);
    }

    if (p_video_context->isp_ch >= 0)
        rts_av_disable_chn(p_video_context->isp_ch);
}

static s32 _gvrt_isp_ctrl_init(void)
{
    /* 3DNR */
    __gvrt_isp_set_attr(RTS_ISP_CTRL_ID_3DNR, 1);

    /* WDR */
    __gvrt_isp_set_attr(RTS_ISP_CTRL_ID_WDR_MODE, RTS_ISP_WDR_AUTO);
    __gvrt_isp_set_attr(RTS_ISP_CTRL_ID_WDR_LEVEL, IPRT_ISP_WDR_LEVEL_D);

    /* SMART IR */
    __gvrt_isp_set_attr(RTS_ISP_CTRL_ID_SMART_IR_MODE, RTS_ISP_SMART_IR_MODE_HIGH_LIGHT_PRIORITY);

    return IPC_SUCCESS;
}

static int _enc_qos_adjust(int channel, struct ipc_bitrate_adjust arg)
{

    return 0;
}

static int _isp_mirror_filp(IPC_ISP_MIRRORFLIP_TYPE type)
{
    // coverity[UNUSED_VALUE :SUPPRESS] 
    int ret = -1;

    // extern int ipc_sensor_flip_and_mirror(char* name, IPC_ISP_MIRRORFLIP_TYPE type);

    // ipc_sensor_flip_and_mirror(_gvrt_video_attr.sensor_name, type);
    // IPC_VIDEO_PRINT("set MIRRORFLIP_TYPE: %d, ret: %d\n", type, ret);

    int value = (type == IPC_ISP_MIRRORFLIP_TYPE_MIRROR_FLIP) ? 3 : 0;
    ret       = __gvrt_isp_set_attr(RTS_ISP_CTRL_ID_MIRROR_FLIP, value);

    return ret;
}

static s32 __ipc_get_ae_gain(struct ipc_plat_isp_exp_status* status)
{
    if (NULL == _gvrt_video_attr.isp_awb) {
        rts_av_query_isp_awb(&_gvrt_video_attr.isp_awb);
        // IPC_VIDEO_PRINT("rts_av_query_isp_awb ret: %d\n", ret);
    }
    if (NULL == _gvrt_video_attr.isp_ae) {
        rts_av_query_isp_ae(&_gvrt_video_attr.isp_ae);
        // IPC_VIDEO_PRINT("rts_av_query_isp_ae ret: %d\n", ret);
    }

    if (NULL != _gvrt_video_attr.isp_awb) {
        rts_av_get_isp_awb(_gvrt_video_attr.isp_awb);
        print_awb_ctrl(_gvrt_video_attr.isp_awb, status);
    }

    if (NULL != _gvrt_video_attr.isp_ae) {
        rts_av_get_isp_ae(_gvrt_video_attr.isp_ae);
        print_ae_ctrl(_gvrt_video_attr.isp_ae, status);
    }

    return 0;
}

static s32 __ipc_get_isp_sensor_metering_threshold_val(struct ipc_plat_isp_sensor_metering_threshold_val* val)
{
    if (strstr(_gvrt_video_attr.sensor_name, "SC2335")) {
        val->day_to_night_exp_val     = 298000;
        val->night_to_day_exp_val     = 60000;
        val->night_to_day_wb_r_g_diff = 2;
        val->night_to_day_wb_b_g_diff = 0;
    } else if (strstr(_gvrt_video_attr.sensor_name, "SC2300")) {
        val->day_to_night_exp_val     = 46000;
        val->night_to_day_exp_val     = 29500;
        val->night_to_day_wb_r_g_diff = 180;
        val->night_to_day_wb_b_g_diff = 0;
    } else {
        val->day_to_night_exp_val     = 300000;
        val->night_to_day_exp_val     = 85000;
        val->night_to_day_wb_r_g_diff = 30;
        val->night_to_day_wb_b_g_diff = 10;
    }

    return 0;
}

static s32 _gvrt_set_isp_sharp(u8* arg)
{
    if (NULL == arg) {
        return IPC_FAILED;
    }

    u8 value = *arg;

    /* min = 0, max = 100, step = 1, default = 50, cur = 50 */
    // __gvrt_isp_get_attr(RTS_ISP_CTRL_ID_SHARPNESS);
    IPC_VIDEO_PRINT("====== value: %d ======\n", value);
    __gvrt_isp_set_attr(RTS_ISP_CTRL_ID_SHARPNESS, value);

    return IPC_SUCCESS;
}

static s32 _gvrt_set_isp_contrast(u8* arg)
{
    if (NULL == arg) {
        return IPC_FAILED;
    }

    u8 value = *arg;

    /* min = 0, max = 100, step = 1, default = 50, cur = 50 */
    // __gvrt_isp_get_attr(RTS_ISP_CTRL_ID_CONTRAST);
    IPC_VIDEO_PRINT("====== value: %d ======\n", value);
    __gvrt_isp_set_attr(RTS_ISP_CTRL_ID_CONTRAST, value);

    return IPC_SUCCESS;
}

static s32 _gvrt_set_isp_bright(u8* arg)
{
    if (NULL == arg) {
        return IPC_FAILED;
    }

    u8 value   = *arg;
    s8 s_value = 0;

    if (value < 50) {
        s_value = (50 - value) * (-1);
    } else if (value > 50) {
        s_value = value - 50;
    } else if (value == 50) {
        s_value = 0;
    } else {
        IPC_VIDEO_PRINT("Error, value: %d invaild\n", value);
        return IPC_FAILED;
    }

    /* min = -64, max = 64, step = 1, default = 0, cur = 0 */
    // __gvrt_isp_get_attr(RTS_ISP_CTRL_ID_BRIGHTNESS);
    IPC_VIDEO_PRINT("====== value: %d ======\n", s_value);
    __gvrt_isp_set_attr(RTS_ISP_CTRL_ID_BRIGHTNESS, s_value);

    return IPC_SUCCESS;
}

static s32 _gvrt_set_isp_wdr(s32 mode)
{
    if (mode) {
        __gvrt_isp_set_attr(RTS_ISP_CTRL_ID_WDR_MODE, RTS_ISP_WDR_MANUAL);
        __gvrt_isp_set_attr(RTS_ISP_CTRL_ID_WDR_LEVEL, 100);
    } else {
        __gvrt_isp_set_attr(RTS_ISP_CTRL_ID_WDR_MODE, RTS_ISP_WDR_AUTO);
        __gvrt_isp_set_attr(RTS_ISP_CTRL_ID_WDR_LEVEL, IPRT_ISP_WDR_LEVEL_D);
    }

    return IPC_SUCCESS;
}

static s32 _gvrt_zoom_in_out_chn(s32 isp_ch, struct rts_video_rect new_crop)
{
    s32 ret = 0;
    ret     = rts_av_set_isp_crop(isp_ch, &new_crop);
    if (ret) {
        IPC_VIDEO_PRINT("Error, zoom fail \n");
        return ret;
    }
    IPC_VIDEO_PRINT("zoom %d crop[%d %d %d %d] \n", isp_ch, new_crop.left, new_crop.top, new_crop.right,
                   new_crop.bottom);
    return ret;
}

static s32 __zoom_multiplier_check(double multiplier, double* min_mult, double* max_mult)
{
    struct res_multiplier_limit {
        s32 width;
        s32 height;
        double min_multiplier;
        double max_multiplier;
    };

    /* Non-interpolated */
    struct res_multiplier_limit nor_limit_map[] = {
        { 1280, 720, 1.0, 3.0 },  /* 1MP */
        { 1920, 1080, 1.0, 3.0 }, /* 2MP */
        { 2304, 1296, 1.0, 3.0 }, /* 3MP */
        { 2560, 1440, 1.0, 3.0 }, /* 4MP */
        { 2816, 1584, 1.0, 3.0 }, /* 5MP */
        { 3840, 2160, 1.0, 3.0 }, /* 8MP */
        { 0, 0, 1.0, 3.0 },       /* Default, placed last */
    };

    /* Interpolated (opening an additional OSD channel causes the maximum multiplier to decrease) */
    struct res_multiplier_limit scal_limit_map[] = {
        { 1920, 1080, 1.0, 3.0 }, /* 2MP */
        { 2304, 1296, 1.0, 3.0 }, /* 3MP */
        { 2560, 1440, 1.0, 2.6 }, /* 4MP */
        { 2816, 1584, 1.0, 3.0 }, /* 5MP */
        { 3840, 2160, 1.0, 3.0 }, /* 8MP */
        { 0, 0, 1.0, 2.5 },       /* Default, placed last */
    };

    s32 idx                                   = 0;
    P_IPRT_VIDEO_CONTEXT_S main_video_context = &_gvrt_video_attr.video_context[IPRT_VIDEO_CHN_MAIN];
    s32 limit_map_size = (main_video_context->width != _gvrt_video_attr.sensor_width) ? RTS_ARRAY_SIZE(scal_limit_map)
                                                                                      : RTS_ARRAY_SIZE(nor_limit_map);
    struct res_multiplier_limit* limit_map
        = (main_video_context->width != _gvrt_video_attr.sensor_width) ? scal_limit_map : nor_limit_map;

    for (idx = 0; idx < limit_map_size; idx++) {
        if (main_video_context->width == limit_map[idx].width && main_video_context->height == limit_map[idx].height) {
            break;
        }
    }

    /* If no matching resolution is found, use the default value */
    if (limit_map_size == idx) {
        printf("Warning, no matching resolution found, using default argument!\n");
        idx--;
    }

    if ((NULL != min_mult) && (NULL != max_mult)) {
        *min_mult = limit_map[idx].min_multiplier;
        *max_mult = limit_map[idx].max_multiplier;
        return IPC_SUCCESS;
    }

    /* Floating-point numbers cannot be compared directly; compare after rounding to one decimal place */
    if ((((int)(multiplier * 10)) < ((int)(limit_map[idx].min_multiplier * 10)))
        || (((int)(multiplier * 10)) > ((int)(limit_map[idx].max_multiplier * 10)))) {
        printf("Error, multiplier: %f, min: %f, max: %f\n", multiplier, limit_map[idx].min_multiplier,
               limit_map[idx].max_multiplier);
        return IPC_FAILED;
    }

    return IPC_SUCCESS;
}

static s32 _gvrt_get_isp_crop(struct ipc_plat_video_isp_crop* isp_crop)
{
    // coverity[UNUSED_VALUE :SUPPRESS] 
    s32 ret           = -1;
    static s32 scal_w = 0;
    static s32 scal_h = 0;
    struct rts_video_rect rt_crop;
    P_IPRT_VIDEO_CONTEXT_S main_video_context = &_gvrt_video_attr.video_context[IPRT_VIDEO_CHN_MAIN];

    if (scal_w == 0 && scal_h == 0) {
        printf("======================= get isp crop ====================\n");
        double min_mult;
        double max_mult;
        double result;

        __zoom_multiplier_check(0, &min_mult, &max_mult);

        result = sqrt(max_mult); /* Take the square root (because the magnification factor calculates the area multiple,
                                    so a square root is needed), calculate the width and height reduction coefficient */
        scal_w = ((int)((_gvrt_video_attr.sensor_width) / result)) & (~1);  /* Width must be an even number */
        scal_h = ((int)((_gvrt_video_attr.sensor_height) / result)) & (~1); /* Height must be an even number */
    }

    isp_crop->max_width  = _gvrt_video_attr.sensor_width;
    isp_crop->max_height = _gvrt_video_attr.sensor_height;
    isp_crop->min_width  = scal_w;
    isp_crop->min_height = scal_h;

    ret = rts_av_get_isp_crop(main_video_context->isp_ch, &rt_crop);
    if (RTS_IS_ERR(ret)) {
        RTS_ERR("Failed to get isp crop, ret = %d\n", ret);
        return IPC_FAILED;
    }

    isp_crop->cur_width      = rt_crop.end.x - rt_crop.start.x;
    isp_crop->cur_height     = rt_crop.end.y - rt_crop.start.y;
    isp_crop->cur_x_position = rt_crop.start.x;
    isp_crop->cur_y_position = rt_crop.start.y;

    return 0;
}

static s32 _gvrt_set_isp_crop(struct ipc_plat_video_isp_crop* isp_crop)
{
    struct rts_video_rect new_crop;

    P_IPRT_VIDEO_CONTEXT_S main_video_context = &_gvrt_video_attr.video_context[IPRT_VIDEO_CHN_MAIN];
    P_IPRT_VIDEO_CONTEXT_S sub_video_context  = &_gvrt_video_attr.video_context[IPRT_VIDEO_CHN_SUB];

    u64 ipc_mono_tms(void)
    {
        struct timespec time;
        clock_gettime(CLOCK_MONOTONIC, &time);
        return time.tv_sec * 1000LL + time.tv_nsec / (1000LL * 1000LL);
    }

    _gvrt_video_attr.image_change_time = ipc_mono_tms();

    new_crop.left   = isp_crop->cur_x_position;
    new_crop.top    = isp_crop->cur_y_position;
    new_crop.right  = isp_crop->cur_x_position + isp_crop->cur_width;
    new_crop.bottom = isp_crop->cur_y_position + isp_crop->cur_height;

    _gvrt_zoom_in_out_chn(main_video_context->isp_ch, new_crop);
    _gvrt_zoom_in_out_chn(sub_video_context->isp_ch, new_crop);

    extern int ivrt_get_alarm_video_chn();
    _gvrt_zoom_in_out_chn(ivrt_get_alarm_video_chn(), new_crop);

    return IPC_SUCCESS;
}

static s32 __qr_enhance(s32 mode)
{
    static int contrast   = 0;
    static int saturation = 0;

    IPC_VIDEO_PRINT("qr en hance mode: %d\n", mode);
    if (mode) {

        contrast   = __gvrt_isp_get_attr(RTS_ISP_CTRL_ID_CONTRAST);
        saturation = __gvrt_isp_get_attr(RTS_ISP_CTRL_ID_SATURATION);

        __gvrt_isp_set_attr(RTS_ISP_CTRL_ID_CONTRAST, 100);
        __gvrt_isp_set_attr(RTS_ISP_CTRL_ID_SATURATION, 0);

        struct rts_isp_ae_roi_areas areas = { 0 };

        rts_av_isp_get_ae_roi(0, &areas);
        areas.roi[0].level  = RTS_AE_ROI_LEVEL4;
        areas.roi[0].rect.x = _gvrt_video_attr.sensor_width / 7 * 3;
        areas.roi[0].rect.y = _gvrt_video_attr.sensor_height / 5 * 2;
        areas.roi[0].rect.w = _gvrt_video_attr.sensor_width / 7;
        areas.roi[0].rect.h = _gvrt_video_attr.sensor_height / 5;
        rts_av_isp_set_ae_roi(0, &areas);

        printf("[%d:%d:%d:%d]\n", areas.roi[0].rect.x, areas.roi[0].rect.y, areas.roi[0].rect.w, areas.roi[0].rect.h);

    } else {

        __gvrt_isp_set_attr(RTS_ISP_CTRL_ID_CONTRAST, contrast);
        __gvrt_isp_set_attr(RTS_ISP_CTRL_ID_SATURATION, saturation);

        struct rts_isp_ae_roi_areas areas = { 0 };

        rts_av_isp_set_ae_roi(0, &areas);
    }

    return 0;
}

static s32 _gvrt_set_isp_anti_flicker(IPC_ISP_ANTIFLICKER_TYPE type)
{
    __gvrt_isp_set_attr(RTS_ISP_CTRL_ID_PWR_FREQUENCY, type);
    return 0;
}

void _gvrt_video_encode_param_init(void)
{
#define IPRT_TARGET_PIXEL_30W_16_9_D (640 * 360)
#define IPRT_TARGET_PIXEL_100W_16_9_D (1280 * 720)
#define IPRT_TARGET_PIXEL_200W_16_9_D (1920 * 1080)
#define IPRT_TARGET_PIXEL_300W_16_9_D (2304 * 1296)
#define IPRT_TARGET_PIXEL_400W_16_9_D (2560 * 1440)
#define IPRT_TARGET_PIXEL_500W_16_9_D (2816 * 1584)
#define IPRT_TARGET_PIXEL_800W_16_9_D (3840 * 2160)

    P_IPRT_VIDEO_CONTEXT_S main_video_context = &_gvrt_video_attr.video_context[IPRT_VIDEO_CHN_MAIN];
    P_IPRT_VIDEO_CONTEXT_S sub_video_context  = &_gvrt_video_attr.video_context[IPRT_VIDEO_CHN_SUB];
    s32 target_pixel                          = 0;

    /* main channel */
    target_pixel = main_video_context->width * main_video_context->height;
    switch (target_pixel) {
        case IPRT_TARGET_PIXEL_200W_16_9_D:
        case IPRT_TARGET_PIXEL_300W_16_9_D: {
            IPC_VIDEO_PRINT("main channel init %dW pixel encode\n", target_pixel);
            /* h264 encoder */
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].level           = H264_LEVEL_4;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].rotation        = RTS_AV_ROTATION_0;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].mirror          = RTS_AV_MIRROR_NO;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].qp              = 43;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].min_qp          = 38;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].max_qp          = 45;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].intra_min_qp    = 32;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].intra_max_qp    = 45;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].max_pic_size    = 192 * 1024 * 8;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].intra_qp_offset = -10;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].bitrate         = 650 * 1024;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].min_bitrate     = 512 * 1024;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].max_bitrate     = 1000 * 1024;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].gop = main_video_context->framerate * 3;
            /* h265 encoder */
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].level           = H265_LEVEL_5;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].tier            = MAIN_TIER;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].rotation        = RTS_AV_ROTATION_0;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].mirror          = RTS_AV_MIRROR_NO;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].qp              = 31;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].min_qp          = 30;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].max_qp          = 51;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].intra_min_qp    = 28;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].intra_max_qp    = 51;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].max_pic_size    = 192 * 1024 * 8;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].intra_qp_offset = 4;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].bitrate         = 912 * 1024;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].min_bitrate     = 726 * 1024;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].max_bitrate
                = (1024 * 1024) * 1.4; //(1024 * 1024) * 2;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].gop = main_video_context->framerate * 3;
            break;
        }
        case IPRT_TARGET_PIXEL_400W_16_9_D:
        case IPRT_TARGET_PIXEL_500W_16_9_D: {
            IPC_VIDEO_PRINT("main channel init %dW pixel encode\n", target_pixel);
            /* h264 encoder */
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].level           = H264_LEVEL_4;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].rotation        = RTS_AV_ROTATION_0;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].mirror          = RTS_AV_MIRROR_NO;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].qp              = 43;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].min_qp          = 38;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].max_qp          = 45;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].intra_min_qp    = 32;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].intra_max_qp    = 45;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].max_pic_size    = 192 * 1024 * 8;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].intra_qp_offset = -10;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].bitrate         = 650 * 1024;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].min_bitrate     = 512 * 1024;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].max_bitrate     = 1000 * 1024;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].gop = main_video_context->framerate * 3;
            /* h265 encoder */
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].level           = H265_LEVEL_5;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].tier            = MAIN_TIER;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].rotation        = RTS_AV_ROTATION_0;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].mirror          = RTS_AV_MIRROR_NO;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].qp              = 31;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].min_qp          = 30;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].max_qp          = 51;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].intra_min_qp    = 28;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].intra_max_qp    = 51;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].max_pic_size    = 192 * 1024 * 8;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].intra_qp_offset = 4;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].bitrate         = (1024 * 1024) * 1.2;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].min_bitrate     = 1024 * 1024;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].max_bitrate     = (1024 * 1024) * 1.5;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].gop = main_video_context->framerate * 3;
            break;
        }
        case IPRT_TARGET_PIXEL_800W_16_9_D: {
            IPC_VIDEO_PRINT("main channel init %dW pixel encode\n", target_pixel);
            /* h264 encoder */
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].level           = H264_LEVEL_4;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].rotation        = RTS_AV_ROTATION_0;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].mirror          = RTS_AV_MIRROR_NO;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].qp              = 43;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].min_qp          = 38;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].max_qp          = 45;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].intra_min_qp    = 33;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].intra_max_qp    = 51;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].max_pic_size    = 192 * 1024 * 8;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].intra_qp_offset = -10;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].bitrate         = 650 * 1024;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].min_bitrate     = 512 * 1024;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].max_bitrate     = 1000 * 1024;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].gop = main_video_context->framerate * 3;
            /* h265 encoder */
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].level           = H265_LEVEL_5;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].tier            = MAIN_TIER;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].rotation        = RTS_AV_ROTATION_0;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].mirror          = RTS_AV_MIRROR_NO;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].qp              = 38;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].min_qp          = 35;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].max_qp          = 51;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].intra_min_qp    = 28;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].intra_max_qp    = 51;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].max_pic_size    = 192 * 1024 * 8;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].intra_qp_offset = 4;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].bitrate         = (1024 * 1024) * 1.3;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].min_bitrate     = (1024 * 1024) * 1;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].max_bitrate
                = (1024 * 1024) * 1.5; //(1024 * 1024) * 2;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].gop = main_video_context->framerate * 3;
            break;
        }
        default: {
            IPC_VIDEO_PRINT("main channel init %dW pixel encode\n", target_pixel);
            /* h264 encoder */
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].level           = H264_LEVEL_4;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].rotation        = RTS_AV_ROTATION_0;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].mirror          = RTS_AV_MIRROR_NO;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].qp              = 43;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].min_qp          = 38;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].max_qp          = 45;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].intra_min_qp    = 32;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].intra_max_qp    = 45;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].max_pic_size    = 192 * 1024 * 8;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].intra_qp_offset = -10;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].bitrate         = 650 * 1024;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].min_bitrate     = 512 * 1024;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].max_bitrate     = 1000 * 1024;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].gop = main_video_context->framerate * 3;
            /* h265 encoder */
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].level           = H265_LEVEL_5;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].tier            = MAIN_TIER;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].rotation        = RTS_AV_ROTATION_0;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].mirror          = RTS_AV_MIRROR_NO;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].qp              = 31;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].min_qp          = 30;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].max_qp          = 51;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].intra_min_qp    = 28;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].intra_max_qp    = 51;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].max_pic_size    = 192 * 1024 * 8;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].intra_qp_offset = 4;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].bitrate         = 912 * 1024;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].min_bitrate     = 726 * 1024;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].max_bitrate     = (1024 * 1024) * 1.2;
            main_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].gop = main_video_context->framerate * 3;
            break;
        }
    }

    /* sub channel */
    target_pixel = sub_video_context->width * sub_video_context->height;
    switch (target_pixel) {
        case IPRT_TARGET_PIXEL_100W_16_9_D: {
            IPC_VIDEO_PRINT("sub channel init %dW pixel encode\n", target_pixel);
            /* h264 encoder */
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].level           = H264_LEVEL_4;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].rotation        = RTS_AV_ROTATION_0;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].mirror          = RTS_AV_MIRROR_NO;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].qp              = 31;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].min_qp          = 30;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].max_qp          = 45;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].intra_min_qp    = 28;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].intra_max_qp    = 51;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].max_pic_size    = 96 * 1024 * 8;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].intra_qp_offset = -4;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].bitrate         = 512 * 1024;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].min_bitrate     = 618 * 1024;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].max_bitrate     = 1024 * 1024;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].gop             = sub_video_context->framerate * 3;
            /* h265 encoder */
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].level           = H265_LEVEL_5;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].tier            = MAIN_TIER;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].rotation        = RTS_AV_ROTATION_0;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].mirror          = RTS_AV_MIRROR_NO;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].qp              = 30;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].min_qp          = 28;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].max_qp          = 40;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].intra_min_qp    = 28;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].intra_max_qp    = 51;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].max_pic_size    = 96 * 1024 * 8;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].intra_qp_offset = -4;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].bitrate         = 368 * 1024;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].min_bitrate     = 256 * 1024;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].max_bitrate     = 512 * 1024;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].gop             = sub_video_context->framerate * 3;
            break;
        }
        case IPRT_TARGET_PIXEL_30W_16_9_D:
        default: {
            IPC_VIDEO_PRINT("sub channel init %dW pixel encode\n", target_pixel);
            /* h264 encoder */
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].level           = H264_LEVEL_4;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].rotation        = RTS_AV_ROTATION_0;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].mirror          = RTS_AV_MIRROR_NO;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].qp              = 31;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].min_qp          = 30;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].max_qp          = 45;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].intra_min_qp    = 20;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].intra_max_qp    = 48;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].max_pic_size    = 96 * 1024 * 8;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].intra_qp_offset = -4;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].bitrate         = 368 * 1024;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].min_bitrate     = 256 * 1024;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].max_bitrate     = 512 * 1024;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].gop             = sub_video_context->framerate * 3;
            /* h265 encoder */
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].level           = H265_LEVEL_5;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].tier            = MAIN_TIER;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].rotation        = RTS_AV_ROTATION_0;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].mirror          = RTS_AV_MIRROR_NO;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].qp              = 30;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].min_qp          = 28;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].max_qp          = 40;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].intra_min_qp    = 20;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].intra_max_qp    = 43;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].max_pic_size    = 96 * 1024 * 8;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].intra_qp_offset = -4;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].bitrate         = 368 * 1024;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].min_bitrate     = 256 * 1024;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].max_bitrate     = 512 * 1024;
            sub_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].gop             = sub_video_context->framerate * 3;
            break;
        }
    }
}

static IPC_PRODUCT_TYPE g_product_type = 0;

IPC_PRODUCT_TYPE __sys_get_product_type(void)
{
    return g_product_type;
}

s32 __ipc_plat_video_get_resolution(ps32 width, ps32 height)
{
    *width  = _gvrt_video_attr.sensor_width;
    *height = _gvrt_video_attr.sensor_height;
    return 0;
}

#ifdef STREAM_TEST
static int _g_main_fd = -1;
#endif
s32 ipc_plat_sys_init(IPC_PRODUCT_TYPE type)
{
    s32 ret                        = 0;
    IPRT_SENSOR_INFO_S sensor_info = { 0 };

    ret = _gvrt_isp_init(&sensor_info);
    if (IPC_SUCCESS != ret) {
        IPC_VIDEO_PRINT("Error, sensor init failed\n");
        return IPC_FAILED;
    }

    _gvrt_video_attr.sensor_width  = sensor_info.sensor_width;
    _gvrt_video_attr.sensor_height = sensor_info.sensor_height;
    memcpy(_gvrt_video_attr.sensor_name, sensor_info.sensor_name, strlen(sensor_info.sensor_name));

    _gvrt_video_attr.jpeg_res_chn = IPC_VIDEO_CHN_SUB; // IPC_VIDEO_CHN_YUV;//IPC_VIDEO_CHN_SUB;
    g_product_type                = type;

    ret = rts_av_init();
    if (0 != ret) {
        printf("[%s] rts_av_init failed\n", __func__);
        return IPC_NOT_INIT;
    }

    rts_set_log_ident("RTS3917");
    rts_set_log_mask(RTS_LOG_MASK_SYSLOG);
    rts_set_log_level(1 << RTS_LOG_ERR);

    P_IPRT_VIDEO_CONTEXT_S main_video_context = &_gvrt_video_attr.video_context[IPRT_VIDEO_CHN_MAIN];
    P_IPRT_VIDEO_CONTEXT_S sub_video_context  = &_gvrt_video_attr.video_context[IPRT_VIDEO_CHN_SUB];
    /* main channel */
    {
        main_video_context->encode_h26x_type = IPC_VIDEO_ENC_TYPE_H265;
        main_video_context->encode_type      = (_gvrt_video_attr.jpeg_res_chn == IPC_VIDEO_CHN_MAIN)
                                                   ? (main_video_context->encode_h26x_type | IPC_VIDEO_ENC_TYPE_JPEG)
                                                   : (main_video_context->encode_h26x_type);
        main_video_context->framerate        = 20;
        main_video_context->width            = sensor_info.main_width;
        main_video_context->height           = sensor_info.main_height;
        main_video_context->isp_id           = 0;
        main_video_context->isp_buf_num      = 2;
        main_video_context->isp_mode         = RTS_AV_VIN_RING_MODE; // 0;

        if (_gvrt_video_attr.sensor_width * _gvrt_video_attr.sensor_height >= (2816 * 1584)) {
            main_video_context->isp_mode = RTS_AV_VIN_FRAME_MODE; // 0;
        }
    }

    /* sub channel */
    {
        sub_video_context->encode_h26x_type = IPC_VIDEO_ENC_TYPE_H265;
        sub_video_context->encode_type      = (_gvrt_video_attr.jpeg_res_chn == IPC_VIDEO_CHN_SUB)
                                                  ? (sub_video_context->encode_h26x_type | IPC_VIDEO_ENC_TYPE_JPEG)
                                                  : sub_video_context->encode_h26x_type;
        sub_video_context->framerate        = 20;
        sub_video_context->width            = sensor_info.sub_width;
        sub_video_context->height           = sensor_info.sub_height;
        sub_video_context->isp_id           = 1;
        sub_video_context->isp_buf_num      = 2;
        sub_video_context->isp_mode         = RTS_AV_VIN_FRAME_MODE; // 0;
    }


    _gvrt_video_encode_param_init();

    _gvrt_init_sys_vmem(main_video_context->isp_mode, sub_video_context->isp_mode);

    // if (type == IPC_PRODUCT_TYPE_PANO_360) {

    //     struct rts_isp_snr_crop crop;
    //     rts_av_get_isp_snr_crop(&crop);
    //     crop.current_value.x = (g_rt_video.isp_config[0].width - 1088) / 2;
    //     printf("crop:%d:%d\n", crop.default_value.x, crop.current_value.x);
    //     rts_av_set_isp_snr_crop(&crop);

    //     g_rt_video.isp_config[0].width = 1088;
    //     g_rt_video.isp_config[0].height = 1080;

    //     g_rt_video.isp_config[1].width = 480;
    //     g_rt_video.isp_config[1].height = 480;

    //     g_rt_video.isp_config[2].width = 480;
    //     g_rt_video.isp_config[2].height = 480;

    //     g_rt_video.h264_config[0].min_qp = 25;
    //     g_rt_video.h264_config[0].qp = 28;
    //     g_rt_video.h264_config[0].max_qp = 35;
    //     g_rt_video.h264_config[0].min_bitrate = 800 * 1024;
    //     g_rt_video.h264_config[0].bitrate = 1024 * 1024;
    //     g_rt_video.h264_config[0].max_bitrate = 1.2 * 1024 * 1024;

    //     g_rt_video.h264_config[1].min_qp = 25;
    //     g_rt_video.h264_config[1].qp = 28;
    //     g_rt_video.h264_config[1].max_qp = 35;
    //     g_rt_video.h264_config[1].min_bitrate = 368 * 1024;
    //     g_rt_video.h264_config[1].bitrate = 512 * 1024;
    //     g_rt_video.h264_config[1].max_bitrate = 800 * 1024;

    // }

    _gvrt_video_attr.init_ok = 1;
    _gvrt_frame_rate_statistics();
    printf("====== %s success ======\n", __func__);

    return 0;
}

s32 ipc_plat_sys_uninit(void)
{
    IPC_VIDEO_PRINT("=== begin sys uninit ===\n");

    _gvrt_video_attr.init_ok = 0;
    _gvrt_release_sys_vmem();
    rts_av_release();
    _gvrt_isp_uninit();
    IPC_VIDEO_PRINT("=== sys uninit success ===\n");

    ipc_sleep(3);
    ipc_exec("lsof");
    
    ipc_exec("rmmod rts_camera_jpgenc");
    ipc_exec("rmmod vpu_w521mp");
    ipc_exec("rmmod rts_cam_isp");
    ipc_exec("rmmod rts_cam_zoom");
    ipc_exec("rmmod rts_cam_soc");
    ipc_exec("rmmod rts_cam_mem");
    ipc_exec("rmmod rts_cam_lock");
    ipc_exec("rmmod rts_cam_md");
    ipc_exec("rmmod rts_camera_osd2");
    ipc_exec("rmmod rts_camera_osdi");
    ipc_exec("rmmod rtstream");
    ipc_exec("rmmod rts_cam");
    ipc_exec("rmmod videobuf2-v4l2");
    ipc_exec("rmmod videobuf2-memops");
    ipc_exec("rmmod videobuf2-common");

    ipc_exec("rmmod rtsx_icr");
    ipc_exec("rmmod mmc_block");
    ipc_exec("rmmod mmc_core");
    return 0;
}

s32 ipc_plat_video_init(s32 arg)
{
    s32 ret = 0;
    // s32 chn = 0;

    ret = _gvrt_create_stream(IPRT_VIDEO_CHN_MAIN, _gvrt_video_attr.video_context[IPRT_VIDEO_CHN_MAIN].encode_type);
    if (ret != IPC_SUCCESS) {
        return IPC_FAILED;
    }

    ret = _gvrt_create_stream(IPRT_VIDEO_CHN_SUB, _gvrt_video_attr.video_context[IPRT_VIDEO_CHN_SUB].encode_type);
    if (ret != IPC_SUCCESS) {
        return IPC_FAILED;
    }


    // return IPC_SUCCESS;

    return _gvrt_isp_ctrl_init();
}

s32 ipc_plat_video_uninit(void)
{
    s32 chn = 0;

    IPC_VIDEO_PRINT("=== begin video uninit ===\n");
    for (chn = 0; chn < IPRT_VIDEO_CHN_NUM; chn++) {
        _gvrt_stream_uninit(chn);
    }

    RTS_SAFE_RELEASE(_gvrt_video_attr.isp_awb, rts_av_release_isp_awb);
    RTS_SAFE_RELEASE(_gvrt_video_attr.isp_ae, rts_av_release_isp_ae);

    IPC_VIDEO_PRINT("=== video uninit success ===\n");

    return IPC_SUCCESS;
}

s32 ipc_plat_video_query_capability(struct ipc_plat_video_capability* cap)
{
    cap->video_enc_support = IPC_VIDEO_ENC_TYPE_H265;
    cap->channel_number    = 4;
    cap->res[0].width      = _gvrt_video_attr.video_context[0].width;
    cap->res[0].height     = _gvrt_video_attr.video_context[0].height;
    cap->res[1].width      = _gvrt_video_attr.video_context[1].width;
    cap->res[1].height     = _gvrt_video_attr.video_context[1].height;
    cap->res[2].width      = _gvrt_video_attr.video_context[_gvrt_video_attr.jpeg_res_chn].width;
    cap->res[2].height     = _gvrt_video_attr.video_context[_gvrt_video_attr.jpeg_res_chn].height;
    cap->res[3].width      = _gvrt_video_attr.video_context[1].width;
    cap->res[3].height     = _gvrt_video_attr.video_context[1].height;
    cap->type[0]           = _gvrt_video_attr.video_context[0].encode_h26x_type;
    cap->type[1]           = _gvrt_video_attr.video_context[1].encode_h26x_type;
    cap->type[2]           = IPC_VIDEO_ENC_TYPE_JPEG;
    cap->type[3]           = IPC_VIDEO_ENC_TYPE_YUV;

    snprintf(cap->sensor_name, sizeof(cap->sensor_name) - 1, "%s", _gvrt_video_attr.sensor_name);

    return 0;
}

s32 ipc_plat_video_start(s32 channel, vptr arg)
{
    // printf("%s:%d\n", __func__, channel);
    s32 ret = 0;

    if (channel < 2) {
        ret = _gvrt_start_stream(channel);
    } else if (channel == IPC_VIDEO_CHN_JPEG) {
        ret = _gvrt_start_stream(IPRT_VIDEO_CHN_JPEG);
    }

    return ret;
}

s32 ipc_plat_video_stop(s32 channel)
{
    // IPC_VIDEO_PRINT("=== begin video channel[%d] stop ===\n", channel);
    if (channel < 2) {
        _gvrt_stop_stream(channel);
    } else if (channel == IPC_VIDEO_CHN_JPEG) {
        _gvrt_stop_stream(IPRT_VIDEO_CHN_JPEG);
    }

    return 0;
}

/*
static void hex_dump(char *buff, int len, char *frame_type)
{
    int i = 0;
    int hex_len = 0;

    printf("====== frame_type: %s ======\n", frame_type);
    for(i=0; i<len; i++)
    {
        printf("%02x ", buff[i]);

        hex_len++;
        if(hex_len >= 16) {
            printf("\n");
            hex_len = 0;
        }
    }
    printf("======\n");
}
*/

s32 ipc_plat_video_recv_frame(s32 channel, ipc_plat_recv_frame_cb_f cb, vptr __user, s32 ms)
{
    s32 chn_num = 0;
    // int ret = 0;

    P_IPRT_VIDEO_CONTEXT_S p_video_context = &_gvrt_video_attr.video_context[channel];
    struct rts_av_buffer* buffer           = NULL;

    switch (channel) {
        case IPC_VIDEO_CHN_MAIN:
        case IPC_VIDEO_CHN_SUB:
            chn_num = (p_video_context->encode_type & IPC_VIDEO_ENC_TYPE_H265)
                          ? p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].h265_ch
                          : p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].h264_ch;
            break;
        case IPC_VIDEO_CHN_JPEG:
            chn_num = _gvrt_video_attr.video_context[_gvrt_video_attr.jpeg_res_chn].mjpeg_ch;
            break;
        case IPC_VIDEO_CHN_YUV:
            // chn_num = p_video_context->isp_ch; // p_video_context->isp_ch; //g_rt_video.isp_config[2].isp_ch;
            //  printf("channel: %d, chun_num: %d\n", channel, chn_num);

            if (_g_in_process_yuv != 2) {
                usleep(20 * 1000);
                return -2;
            }

            struct ipc_frame_data_s frame = { 0 };
            frame.is_key                 = 0;
            frame.pack_num               = 1;
            frame.timestamp              = 0;
            frame.pack[0].data           = _g_yuv_buffer;
            frame.pack[0].data_len       = 640 * 360 * 3 / 2;

            _g_in_process_yuv = 1;

            cb(&frame, __user);

            _g_in_process_yuv = 0;

            _g_frame_cnt[channel]++;
            _g_frame_size[channel] += frame.pack[0].data_len;

            return 0;

            break;
        default:
            printf("ipc_plat_video_recv_frame channel [%d] error\n", channel);
            return -1;
            break;
    }

    if (rts_av_recv_block(chn_num, &buffer, ms)) {
        IPC_VIDEO_PRINT("###### Error, rts_av_recv_block channel[%d] failed ##########\n", channel);
        return -2;
    }

    if ((IPC_VIDEO_CHN_MAIN == channel) && (buffer->flags & RTSTREAM_PKT_FLAG_KEY)) {
        IPC_VIDEO_PRINT("[%ld]main channel I Frame size: %d, frame_idx: %d\n", time(NULL), buffer->bytesused,
                       buffer->frame_idx);
        // hex_dump(buffer->vm_addr, 300, "IFrame");
    } else if ((IPC_VIDEO_CHN_SUB == channel) && (buffer->flags & RTSTREAM_PKT_FLAG_KEY)) {
        IPC_VIDEO_PRINT("[%ld]sub channel I Frame size: %d, frame_idx: %d\n", time(NULL), buffer->bytesused,
                       buffer->frame_idx);
        // hex_dump(buffer->vm_addr, 300, "IFrame");
    }

    _g_frame_cnt[channel]++;
    _g_frame_size[channel] += buffer->bytesused;

    struct ipc_frame_data_s frame = { 0 };
    frame.is_key                 = (buffer->flags & RTSTREAM_PKT_FLAG_KEY) ? 1 : 0;
    frame.pack_num               = 1;
    frame.timestamp              = buffer->timestamp / 1000;
    frame.pack[0].data           = buffer->vm_addr;
    frame.pack[0].data_len       = buffer->bytesused;

    cb(&frame, __user);

    RTS_SAFE_RELEASE(buffer, rts_av_put_buffer);

    return 0;
}

s32 ipc_plat_video_request_key_frame(s32 channel)
{
    P_IPRT_VIDEO_CONTEXT_S p_video_context = NULL;

    switch (channel) {
        case IPC_VIDEO_CHN_MAIN:
        case IPC_VIDEO_CHN_SUB:
            p_video_context = &_gvrt_video_attr.video_context[channel];
            break;
        default:
            printf("ipc_plat_video_request_key_frame channel error\n");
            return -1;
            break;
    }

    if (p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].h264_ch >= 0) {
        rts_av_request_h264_key_frame(p_video_context->video_encoder[IPRT_ENCODER_TYPE_H264].h264_ch);
    }

    if (p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].h265_ch >= 0) {
        rts_av_request_h265_key_frame(p_video_context->video_encoder[IPRT_ENCODER_TYPE_H265].h265_ch);
    }

    return 0;
}

s32 ipc_plat_video_ctrl(s32 channel, IPC_VIDEO_CTRL_CMD cmd, vptr arg)
{
    int ret = 0;

    // coverity[CHECKED_RETURN :SUPPRESS]
    pthread_mutex_lock(&_gvrt_video_ctrl_mutex);
    switch (cmd) {
        case IPC_VIDEO_CTRL_CMD_MIRRORFILP:
            ret = _isp_mirror_filp(*(IPC_ISP_MIRRORFLIP_TYPE*)arg);
            break;
        case IPC_VIDEO_CTRL_CMD_QOS_ADJUST:
            ret = _enc_qos_adjust(channel, *(struct ipc_bitrate_adjust*)arg);
            break;
        case IPC_VIDEO_CTRL_CMD_GET_AE_EXP_STATUS:
            ret = __ipc_get_ae_gain((struct ipc_plat_isp_exp_status*)arg);
            break;
        case IPC_VIDEO_CTRL_CMD_GET_SENSOR_METERING_THRESHOLD_VAL:
            ret = __ipc_get_isp_sensor_metering_threshold_val((struct ipc_plat_isp_sensor_metering_threshold_val*)arg);
            break;
        case IPC_VIDEO_CTRL_CMD_QR_ENHANCE_BY_ISP_MODE:
            ret = __qr_enhance((s32)arg);
            break;
        case IPC_VIDEO_CTRL_CMD_SET_SHARP_PARAM:
            ret = _gvrt_set_isp_sharp((u8*)arg);
            break;
        case IPC_VIDEO_CTRL_CMD_SET_CONTRAST_PARAM:
            ret = _gvrt_set_isp_contrast((u8*)arg);
            break;
        case IPC_VIDEO_CTRL_CMD_SET_BRIGHT_PARAM:
            ret = _gvrt_set_isp_bright((u8*)arg);
            break;
        case IPC_VIDEO_CTRL_CMD_WDR_MODE:
            ret = _gvrt_set_isp_wdr((s32)arg);
            break;
        case IPC_VIDEO_CTRL_CMD_GET_ISP_CROP:
            ret = _gvrt_get_isp_crop((struct ipc_plat_video_isp_crop*)arg);
            break;
        case IPC_VIDEO_CTRL_CMD_SET_ISP_CROP:
            ret = _gvrt_set_isp_crop((struct ipc_plat_video_isp_crop*)arg);
            break;
        case IPC_VIDEO_CTRL_CMD_CHECK_IMAGE_IS_IN_CHANGING:
            *(pu64)arg = _gvrt_video_attr.image_change_time;
            ret        = 0;
            break;
        case IPC_VIDEO_CTRL_CMD_SET_ANTI_FLICKER_MODE:
            ret = _gvrt_set_isp_anti_flicker(*(IPC_ISP_ANTIFLICKER_TYPE*)arg);
            break;
        default:
            ret = -1;
            break;
    }
    pthread_mutex_unlock(&_gvrt_video_ctrl_mutex);

    return ret;
}

s32 ipc_plat_video_isp_image_mode_set(IPC_VIDEO_MODE mode, vptr arg)
{

    static IPC_VIDEO_MODE last_mode = -1;
    if (last_mode == mode) {
        return -2;
    }
    last_mode = mode;

    s32 value                                 = 0;
    P_IPRT_VIDEO_CONTEXT_S main_video_context = &_gvrt_video_attr.video_context[IPRT_VIDEO_CHN_MAIN];
    P_IPRT_VIDEO_CONTEXT_S sub_video_context  = &_gvrt_video_attr.video_context[IPRT_VIDEO_CHN_SUB];
    u8 main_fps                               = main_video_context->framerate;
    u8 sub_fps                                = sub_video_context->framerate;

    switch (mode) {
        case IPC_VIDEO_MODE_DAY:
            value = 0;
            break;
        case IPC_VIDEO_MODE_NIGHT_FULL_COLOR:
            value    = 0;
            main_fps = 8;
            sub_fps  = 8;
            break;
        case IPC_VIDEO_MODE_NIGHT:
            value    = 1;
            main_fps = 12;
            sub_fps  = 12;
            break;
        default:
            break;
    }

    // coverity[CHECKED_RETURN :SUPPRESS]
    pthread_mutex_lock(&_gvrt_video_ctrl_mutex);

    printf("--------GRAY_MODE--------\n");
    __gvrt_isp_set_attr(RTS_ISP_CTRL_ID_GRAY_MODE, value);
    // __gvrt_isp_get_attr(RTS_ISP_CTRL_ID_GRAY_MODE);
    printf("---------IR_MODE---------\n");
    __gvrt_isp_set_attr(RTS_ISP_CTRL_ID_IR_MODE, value);
    // __gvrt_isp_get_attr(RTS_ISP_CTRL_ID_IR_MODE);

    _gvrt_set_sensor_fps(IPRT_VIDEO_CHN_MAIN, main_fps);
    _gvrt_set_sensor_fps(IPRT_VIDEO_CHN_SUB, sub_fps);


    pthread_mutex_unlock(&_gvrt_video_ctrl_mutex);

    return 0;
}

s32 ipc_plat_video_osd_init(struct ipc_video_osd_attr_s* attr)
{
    int ret = 0;

    for (s32 isp_chn = 0; isp_chn < 2; isp_chn++) {
        P_IPRT_VIDEO_CONTEXT_S p_video_context = &_gvrt_video_attr.video_context[isp_chn];

        ret = rts_av_query_osdi(p_video_context->isp_ch, &p_video_context->osd_attr);
        if (ret) {
            IPC_VIDEO_PRINT("Error, query osdi failed, chn: %d, ret: %d\n", isp_chn, ret);
            continue;
        }

        for (s32 osd_chn = 0; osd_chn < 2; osd_chn++) {
            p_video_context->osd_attr->blocks[osd_chn].enable            = RTS_TRUE;
            p_video_context->osd_attr->blocks[osd_chn].picture.pixel_fmt = RTS_OSDI_BLK_FMT_RGBA8888;
            p_video_context->osd_attr->blocks[osd_chn].picture.pdata     = NULL;
            p_video_context->osd_attr->blocks[osd_chn].picture.length    = 0;
            p_video_context->osd_attr->blocks[osd_chn].rect.right
                = attr->chn[isp_chn].rgn[osd_chn].x + attr->chn[isp_chn].rgn[osd_chn].width;
            p_video_context->osd_attr->blocks[osd_chn].rect.bottom
                = attr->chn[isp_chn].rgn[osd_chn].y + attr->chn[isp_chn].rgn[osd_chn].height;
            p_video_context->osd_attr->blocks[osd_chn].rect.left = attr->chn[isp_chn].rgn[osd_chn].x;
            p_video_context->osd_attr->blocks[osd_chn].rect.top  = attr->chn[isp_chn].rgn[osd_chn].y;

            if (p_video_context->osd_attr->blocks[osd_chn].rect.right % 2 > 0) {
                p_video_context->osd_attr->blocks[osd_chn].rect.right -= 1;
                p_video_context->osd_attr->blocks[osd_chn].rect.left -= 1;
            }

            if (p_video_context->osd_attr->blocks[osd_chn].rect.bottom % 2 > 0) {
                p_video_context->osd_attr->blocks[osd_chn].rect.bottom -= 1;
                p_video_context->osd_attr->blocks[osd_chn].rect.top -= 1;
            }

            // IPC_VIDEO_PRINT("right:%d, bottom: %d, left: %d, top: %d, isp_channel:%d, osd_channel: %d\n",
            //     p_video_context->osd_attr->blocks[osd_chn].rect.right,
            //     p_video_context->osd_attr->blocks[osd_chn].rect.bottom,
            //     p_video_context->osd_attr->blocks[osd_chn].rect.left,
            //     p_video_context->osd_attr->blocks[osd_chn].rect.top, isp_chn, osd_chn);
        }
    }

    return ret;
}

s32 ipc_plat_video_osd_uninit(void)
{
    int ret = 0;

    RTS_SAFE_RELEASE(_gvrt_video_attr.video_context[IPC_VIDEO_CHN_MAIN].osd_attr, rts_av_release_osdi);
    RTS_SAFE_RELEASE(_gvrt_video_attr.video_context[IPC_VIDEO_CHN_SUB].osd_attr, rts_av_release_osdi);

    return ret;
}

s32 ipc_plat_video_osd_set(s32 channel, s32 rgn_num, s32 is_show, void* data, s32 data_len)
{
    // IPC_VIDEO_PRINT("channel: %d, rgn_num: %d, is_show: %d, data_len: %d\n",
    //     channel, rgn_num, is_show, data_len);

    s32 ret                                = 0;
    P_IPRT_VIDEO_CONTEXT_S p_video_context = &_gvrt_video_attr.video_context[channel];

    if (NULL == p_video_context->osd_attr) {
        IPC_VIDEO_PRINT("Error, osd attr is NULL, channel: %d\n", channel);
        return IPC_FAILED;
    }

    p_video_context->osd_attr->blocks[rgn_num].picture.pdata  = data;
    p_video_context->osd_attr->blocks[rgn_num].picture.length = data_len;

    if (is_show) {
        ret = rts_av_set_osdi_single(p_video_context->osd_attr, rgn_num);
        if (ret) {
            IPC_VIDEO_PRINT("Error, set osdi single faile, isp_chn: %d, osd_chn: %d, ret: %d\n", channel, rgn_num, ret);
            return IPC_FAILED;
        }
    } else {
        ret = rts_av_pause_osdi_single(p_video_context->osd_attr, rgn_num);
        if (ret) {
            IPC_VIDEO_PRINT("Error, pause osdi single faile, isp_chn: %d, osd_chn: %d, ret: %d\n", channel, rgn_num,
                           ret);
            return IPC_FAILED;
        }
    }

    return 0;
}
