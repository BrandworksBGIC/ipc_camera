#include "ipc_platform_api.h"
#include "ipc_video.h"
#include "ipc_osd.h"
#include "ipc_audio.h"
#include "ipc_alarm.h"
#include "ipc_io.h"
#include "ipc_misc_ctrl.h"

static struct ipc_api_s g_api  = {
    .apiver                   = 1,
    .platver                  = "1",
    .sys_init                 = ipc_plat_sys_init,
    .sys_uninit               = ipc_plat_sys_uninit,
    .video_init               = ipc_plat_video_init,
    .video_uninit             = ipc_plat_video_uninit,
    .video_query_capability   = ipc_plat_video_query_capability,
    .video_start              = ipc_plat_video_start,
    .video_stop               = ipc_plat_video_stop,
    .video_recv_frame         = ipc_plat_video_recv_frame,
    .video_request_key_frame  = ipc_plat_video_request_key_frame,
    .video_isp_image_mode_set = ipc_plat_video_isp_image_mode_set,
    .video_ctrl               = ipc_plat_video_ctrl,
    .video_osd_init           = ipc_plat_video_osd_init,
    .video_osd_uninit         = ipc_plat_video_osd_uninit,
    .video_osd_set            = ipc_plat_video_osd_set,
    .audio_init               = ipc_plat_audio_init,
    .audio_uninit             = ipc_plat_audio_uninit,
    .audio_query_capability   = ipc_plat_audio_query_capability,
    .audio_start              = ipc_plat_audio_start,
    .audio_stop               = ipc_plat_audio_stop,
    .audio_ai_recv_frame      = ipc_plat_audio_ai_recv_frame,
    .audio_ao_send_frame      = ipc_plat_audio_ao_send_frame,
    .audio_ao_flush_buffer    = ipc_plat_audio_ao_flush_buffer,
    .audio_set_vol            = ipc_plat_audio_set_vol,
    .io_init                  = ipc_plat_io_init,
    .io_uninit                = ipc_plat_io_uninit,
    .io_read                  = ipc_plat_io_read,
    .io_write                 = ipc_plat_io_write,
    .misc_ctrl                = ipc_plat_misc_ctrl,
    .alarm_init               = ipc_plat_alarm_init,
    .alarm_uninit             = ipc_plat_alarm_uninit,
    .alarm_ctrl               = ipc_plat_alarm_ctrl,
    .alarm_recv_result        = ipc_plat_alarm_recv_result,
    .alarm_release_result     = ipc_plat_alarm_release_result,
};

struct ipc_api_s* ipc_plat_api(s32 arg)
{
    return &g_api;
}