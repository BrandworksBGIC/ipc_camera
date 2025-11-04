#ifndef __IPC_SENSOR_LIGHT_DET_H__
#define __IPC_SENSOR_LIGHT_DET_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "ipc_platform_api.h"
#include <ipc_std.h>

enum {
    IPC_SL_DET_DAY_MODE,
    IPC_SL_DET_NIGHT_MODE_WITH_INFRARED_ON,
    IPC_SL_DET_AUTO,
};

struct ipc_sensor_light_det_attr {
    u32 day_to_night_exp_val; // Exposure value
    u32 night_to_day_exp_val;
    u32 night_to_day_wb_rgain;      // Red gain
    u32 night_to_day_wb_bgain;      // Blue gain
    void (*cmd_handler)(s32, vptr); // Command handler
    void* user;                     // User data
};

s32 ipc_sensor_light_det_init(struct ipc_sensor_light_det_attr* attr);

s32 ipc_sensor_light_det_process(s32 sensor_det_mode, struct ipc_plat_isp_exp_status* status);

#ifdef __cplusplus
}
#endif

#endif //__SENSOR_LIGHT_DET_H__