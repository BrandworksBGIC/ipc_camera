#ifndef __IPC_MOTION_DETECT_H__
#define __IPC_MOTION_DETECT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc_core.h>

#define MAX_RECT_NUM 128

typedef struct {
    s16 lux; // Top-left x coordinate
    s16 luy; // Top-left y coordinate
    s16 rdx; // Bottom-right x coordinate
    s16 rdy; // Bottom-right x coordinate
} rect_t, *rect_p;

typedef struct {
    s32 rect_num;
    rect_t rect[MAX_RECT_NUM];
    f32 alarm_image_percent; // Alarm image percentage
} ipc_alarm_result_t, *ipc_alarm_result_p;

typedef enum {
    IPC_MOTION_SAD_4x4   = 4,
    IPC_MOTION_SAD_8x8   = 8,
    IPC_MOTION_SAD_16x16 = 16,
} ipc_motion_sad_e;

typedef struct {
    s32 width;
    s32 height;
    ipc_motion_sad_e sad_type; ///< It is recommended to use 16 * 16, which has the strongest anti-interference ability, and is not higher than 12 fps
                              ///< and not lower than 6 fps
    f32 sensitivity; ///< It is recommended not to be lower than 0.25 and not to be higher than 0.55. For a resolution of 640*360, the recommended
                     ///< value is 0.35
} ipc_motion_detect_attr_t, *ipc_motion_detect_attr_p;

s32 ipc_motion_detect_init(ipc_motion_detect_attr_p attr);

void ipc_motion_detect_set_sensitivity(f32 sensitivity);

s32 ipc_motion_detect_process(pu8 y_data, ipc_alarm_result_p result);

void ipc_motion_detect_uninit(void);

#ifdef __cplusplus
}
#endif

#endif //__IPC_MOTION_DETECT_H__
