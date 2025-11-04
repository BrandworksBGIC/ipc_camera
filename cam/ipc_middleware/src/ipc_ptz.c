#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "ipc_motor.h"
#include "ipc_ptz.h"
#include "ipc_factory.h"
#include "ipc_platform_api.h"
#include "ipc_decrypt.h"

#define MOTOR "/dev/ipc-motor"

/* Convert speed level to speed in the driver layer */
#define SPEED(speed) ((s32)(90 + (speed - 1) * ((450 - 90) / (10 - 1))))
#define DIR_TO_ACT(dir) (dir >> 0x1)

static struct {
    s32 fd;                             ///< Control Motors
    s8 filp[IPC_PTZ_ACTION_NUM];         ///< Motor flipping
    s32 circle_step[IPC_PTZ_ACTION_NUM]; ///< The total number of steps in a week of the PTZ motor, which is not allowed to be 0
    f32 now_speed[IPC_PTZ_ACTION_NUM];   ///< PTZ motor speed level (1-10, the smaller the number, the faster the speed)
    f32 usr_speed[IPC_PTZ_ACTION_NUM];   ///< The motor speed controlled by the user (1-10, the smaller the number, the faster the speed)
    u8 self_check;                      ///< Whether self-check is required
    u8 ptz_user_ctrl;                   ///< Flag indicating that the motor is manually controlled by the user
    f32 init_angle[IPC_PTZ_ACTION_NUM];  ///< The reference position of the motor redirected externally
} _g_ptz = {
    .fd = -1, .init_angle = { -1, -1 }, // 0 is a valid value, invalid by default
};

/******************************** check *****************************************/

inline static u8 _not_init(void) 
{
    if (_g_ptz.fd < 0) {
        ipcdebug("Not initialization！");
        return 1;
    }
    return 0;
}

inline static u8 _not_self_check(void)
{
    if (!_g_ptz.self_check) {
        ipcdebug("No self-check, ignore");
        return 1;
    }
    return 0;
}

inline static u8 _illegal_action(ipc_ptz_action_e act) 
{
    // coverity[NO_EFFECT :SUPPRESS]
    if (act < 0 || act >= IPC_PTZ_ACTION_NUM) {
        ipcerror("Parameter error! Action=[%d]", act);
        return 1;
    }
    return 0;
}

inline static u8 _illegal_dir(ipc_ptz_dir_e dir) 
{
    // coverity[NO_EFFECT :SUPPRESS]
    if (dir < 0 || dir >= IPC_PTZ_CTRL_NUM) {
        ipcerror("Parameter error! dir=[%d]", dir);
        return 1;
    } 
    return 0;
}

inline static u8 _illegal_speed(f32 speed)
{
    if (speed < 1 || speed > 10) {
        ipcerror("Parameter error! speed=[%f]", speed);
        return 1;
    }
    return 0;
}

inline static u8 _illegal_angle(f32 angle)
{
    if (angle < 0 || angle > 360) {
        ipcerror("Parameter error! angle=[%f]", angle);
        return 1;
    }
    return 0;
}

static u8 _user_ctrl_ptz(void)
{
    if (_g_ptz.ptz_user_ctrl) {
        if (!ipc_ptz_is_stop(IPC_PTZ_H) || !ipc_ptz_is_stop(IPC_PTZ_V)) {
            return 1;
        }
        else {
            _g_ptz.ptz_user_ctrl = 0;
        }
    }
    return 0;
}

/****************************************************************************/

static s32 _ptz_init(u8 self_check)
{
    s32 fd = open(MOTOR, 0);
    if (fd < 0) {
        ipcerror("Open %s failed! fd=[%d], errmsg=[%s]", MOTOR, fd, strerror(errno));
        return IPC_OPEN_ERROR;
    }

    _g_ptz.now_speed[IPC_PTZ_H] = ipc_factory(ptz_h_track_speed); // For self-inspection, use the speed of the mobile tracking
    _g_ptz.now_speed[IPC_PTZ_V] = ipc_factory(ptz_v_track_speed);
    
    struct ipc_motor_attr attr = {
        .max_step      = { (s32)(ipc_factory(ptz_h_max_angle)  * _g_ptz.circle_step[IPC_PTZ_H] / 360), (s32)(ipc_factory(ptz_v_max_angle)  * _g_ptz.circle_step[IPC_PTZ_V] / 360) },
        .init_pos_step = { (s32)(ipc_ptz_get_init_angle(IPC_PTZ_H) * _g_ptz.circle_step[IPC_PTZ_H] / 360), (s32)(ipc_ptz_get_init_angle(IPC_PTZ_V) * _g_ptz.circle_step[IPC_PTZ_V] / 360) },
        .model_build   = self_check,
        .ptz_product_type = 0,
        .speed = { SPEED(_g_ptz.now_speed[IPC_PTZ_H]), SPEED(_g_ptz.now_speed[IPC_PTZ_V]) },
        .limit_min_step = { (s32)(ipc_factory(ptz_h_limit_min_angle) * _g_ptz.circle_step[IPC_PTZ_H] / 360), (s32)(ipc_factory(ptz_v_limit_min_angle) * _g_ptz.circle_step[IPC_PTZ_V] / 360) },
        .limit_max_step = { (s32)(ipc_factory(ptz_h_limit_max_angle) * _g_ptz.circle_step[IPC_PTZ_H] / 360), (s32)(ipc_factory(ptz_v_limit_max_angle) * _g_ptz.circle_step[IPC_PTZ_V] / 360) }
    };

    if (ipc_plat_api(0)->misc_ctrl != NULL) {
        ipc_plat_api(0)->misc_ctrl(IPC_PLAT_MISC_CTRL_CMD_GET_PTZ_IO_GROUP_INDEX, NULL, &attr.ptz_product_type);
    }

    ipcinfo("ptz product type %d\n", attr.ptz_product_type);

    s32 ret = ioctl(fd, IPC_IOCTL_MOTOR_INIT, (word)&attr);
    if (ret < 0) {
        ipcerror("Ioctl ptz init failed! retcode=[%d], errmsg=[%s]", ret, strerror(errno));
        close(fd);
        return IPC_IOCTL_ERROR;
    }

    return fd;
}

static s32 _ptz_recheck(s32 fd)
{
    if (fd < 0) {
        ipcerror("fd not init\n");
        return IPC_NOT_INIT;
    }

    s32 ret = 0;

    ret = ioctl(fd, IPC_IOCTL_MOTOR_UNINIT, NULL);
    if (ret !=0) {
        ipcerror("Ioctl ptz uninit failed! retcode=[%d], errmsg=[%s]", ret, strerror(errno));
    }

    _g_ptz.now_speed[IPC_PTZ_H] = ipc_factory(ptz_h_track_speed); // For self-inspection, use the speed of the mobile tracking
    _g_ptz.now_speed[IPC_PTZ_V] = ipc_factory(ptz_v_track_speed);
    
    struct ipc_motor_attr attr = {
        .max_step      = { (s32)(ipc_factory(ptz_h_max_angle)  * _g_ptz.circle_step[IPC_PTZ_H] / 360), (s32)(ipc_factory(ptz_v_max_angle)  * _g_ptz.circle_step[IPC_PTZ_V] / 360) },
        .init_pos_step = { (s32)(ipc_ptz_get_init_angle(IPC_PTZ_H) * _g_ptz.circle_step[IPC_PTZ_H] / 360), (s32)(ipc_ptz_get_init_angle(IPC_PTZ_V) * _g_ptz.circle_step[IPC_PTZ_V] / 360) },
        .model_build   = 1,
        .ptz_product_type = 0,
        .speed = { SPEED(_g_ptz.now_speed[IPC_PTZ_H]), SPEED(_g_ptz.now_speed[IPC_PTZ_V]) },
        .limit_min_step = { (s32)(ipc_factory(ptz_h_limit_min_angle) * _g_ptz.circle_step[IPC_PTZ_H] / 360), (s32)(ipc_factory(ptz_v_limit_min_angle) * _g_ptz.circle_step[IPC_PTZ_V] / 360) },
        .limit_max_step = { (s32)(ipc_factory(ptz_h_limit_max_angle) * _g_ptz.circle_step[IPC_PTZ_H] / 360), (s32)(ipc_factory(ptz_v_limit_max_angle) * _g_ptz.circle_step[IPC_PTZ_V] / 360) }
    };

    if (ipc_plat_api(0)->misc_ctrl != NULL) {
        ipc_plat_api(0)->misc_ctrl(IPC_PLAT_MISC_CTRL_CMD_GET_PTZ_IO_GROUP_INDEX, NULL, &attr.ptz_product_type);
    }

    ipcinfo("ptz product type %d\n", attr.ptz_product_type);

    ret = ioctl(fd, IPC_IOCTL_MOTOR_INIT, (word)&attr);
    if (ret < 0) {
        ipcerror("Ioctl ptz init failed! retcode=[%d], errmsg=[%s]", ret, strerror(errno));
        return IPC_IOCTL_ERROR;
    }

    return IPC_SUCCESS;
}

s32 ipc_ptz_init(void)
{
    // coverity[RETURNED_NULL :SUPPRESS]
    // coverity[VAR_ASSIGNED :SUPPRESS]
    ipc_decrypt_ininfo_p decrypt = ipc_decrypt_ininfo();

    // coverity[DEREFERENCE :SUPPRESS]
    if (decrypt && ((decrypt->product_type != IPC_PRODUCT_TYPE_PTZ) && (decrypt->product_type != IPC_PRODUCT_TYPE_38_PTZ))) {
        return IPC_SUCCESS;
    }

    s32 count = 30;
    ipc_exec("insmod /app/drivers/ipc_step_motor.ko g_motor_gpioV_seq=%s g_motor_gpioH_seq=%s", ipc_factory(ptz_gpioV_seq), ipc_factory(ptz_gpioH_seq));
    while (access(MOTOR, F_OK) && count--)
        ipc_msleep(100);

    clog_init("ptz", "pan-tilt-zoom");

    _g_ptz.circle_step[IPC_PTZ_H] = ipc_factory(ptz_circle_step) * ipc_factory(ptz_h_gear_ratio);
    _g_ptz.circle_step[IPC_PTZ_V] = ipc_factory(ptz_circle_step) * ipc_factory(ptz_v_gear_ratio);
    _g_ptz.usr_speed[IPC_PTZ_H]   = ipc_factory(ptz_h_ctrl_speed);
    _g_ptz.usr_speed[IPC_PTZ_V]   = ipc_factory(ptz_v_ctrl_speed);
    _g_ptz.self_check            = ipc_factory(ptz_self_check);

    s32 fd = _ptz_init(_g_ptz.self_check);
    if (fd < 0)
        return fd;

    _g_ptz.fd = fd;
    ipcinfo("Init complete!");

    return IPC_SUCCESS;
}

void ipc_ptz_uninit(void)
{
    if (_not_init())
        return;

    if (ioctl(_g_ptz.fd, IPC_IOCTL_MOTOR_UNINIT, NULL) < 0) {
        ipcerror("Failed to uninitialize PTZ motor: %s", strerror(errno));
        // Optionally add retry logic or return here if critical
    }

    if (close(_g_ptz.fd) < 0) {
        ipcerror("Failed to close PTZ file descriptor: %s", strerror(errno));
    }
    _g_ptz.fd = -1;

    ipcinfo("Exit complete!");
}

s32 ipc_ptz_recheck(void)
{
    if (_not_init()) return IPC_NOT_INIT;

    s32 ret = 0;

    ret = _ptz_recheck(_g_ptz.fd);
    if (IPC_SUCCESS != ret) {
        ipcerror("ptz recheck failed\n");
    }
    else {
        ipcinfo("Recheck Success!");
    }

    _g_ptz.self_check = 1;

    return ret;
}

s32 ipc_ptz_is_stop(ipc_ptz_action_e act)
{
    if (_not_init() || _illegal_action(act)) return 1;

    struct ipc_motor_status status = { 0 };
    status.motor_index = act == IPC_PTZ_H ? IPC_MOTOR_INDEX_0 : IPC_MOTOR_INDEX_1;

    s32 ret = ioctl(_g_ptz.fd, IPC_IOCTL_MOTOR_GET_STATUS, (word)&status);
    if (ret < 0) {
        ipcerror("Ioctl check status failed! retcode=[%d], errmsg=[%s]", ret, strerror(errno));
        return 1;
    }

    return status.status == IPC_MOTOR_STATUS_STOP ? 1 : 0;
}

void ipc_ptz_stop(ipc_ptz_action_e act)
{
    if (_not_init() || _illegal_action(act)) return ;

    struct ipc_motor_step motor = {
        .motor_index = act == IPC_PTZ_H ? IPC_MOTOR_INDEX_0 : IPC_MOTOR_INDEX_1,
        .direction = 0, .step = 0,
    };

    s32 ret = ioctl(_g_ptz.fd, IPC_IOCTL_MOTOR_RUN_STEPS, (word)&motor);
    if (ret < 0) ipcerror("Ioctl set stop failed! retcode=[%d], errmsg=[%s]", ret, strerror(errno));
}

void ipc_ptz_flip(ipc_ptz_action_e act, s8 is_flip)
{
    if (_illegal_action(act)) return ;
    
    _g_ptz.filp[act] = !!is_flip;
    ipcinfo("%s motor flip=%d", act == IPC_PTZ_H ? "Horizontal" : "Vertical", _g_ptz.filp[act]);
}

static void _ptz_speed(ipc_ptz_action_e act, f32 speed)
{
    if (_g_ptz.now_speed[act] == speed) return ;
    
    _g_ptz.now_speed[act] = speed;
    struct ipc_motor_speed motor_speed = {
        .motor_index = act == IPC_PTZ_H ? IPC_MOTOR_INDEX_0 : IPC_MOTOR_INDEX_1,
        .speed = SPEED(speed)
    };

    s32 ret = ioctl(_g_ptz.fd, IPC_IOCTL_MOTOR_SET_SPEED, (word)&motor_speed);
    if (ret < 0) ipcerror("Ioctl set speed failed! retcod=[%d], errmsg=[%s]", ret, strerror(errno));
}

static void _ptz_turn(ipc_ptz_dir_e dir, f32 angle)
{
    ipctrace("Turn direction=[%d], angle=[%f]", dir, angle);
 
    s8 factory_ptz_filp[] = {
        [IPC_PTZ_H] = !!ipc_factory(ptz_h_flip),
        [IPC_PTZ_V] = !!ipc_factory(ptz_v_flip),
    };

    u8 act  = DIR_TO_ACT(dir);
    u8 turn = (dir & 0x1) ^ _g_ptz.filp[act] ^ factory_ptz_filp[act];

    struct ipc_motor_step motor = {
        .motor_index = act == IPC_PTZ_H ? IPC_MOTOR_INDEX_0 : IPC_MOTOR_INDEX_1,
        .direction = turn == IPC_PTZ_ANTICLKWISE ? IPC_MOTOR_DIR_COUNTERCLOCKWISE : IPC_MOTOR_DIR_CLOCKWISE,
        .step = (s32)(angle * _g_ptz.circle_step[motor.motor_index] / 360)
    };

    s32 ret = ioctl(_g_ptz.fd, IPC_IOCTL_MOTOR_RUN_STEPS, (word)&motor);
    if (ret < 0) ipcdebug("Ioctl set steps failed! retcode=[%d], errmsg=[%s]", ret, strerror(errno));
}

void ipc_ptz_speed(ipc_ptz_action_e act, f32 speed)
{
    if (_illegal_action(act) || _illegal_speed(speed)) return ;
    _g_ptz.usr_speed[act] = speed;
}

void ipc_ptz_turn(ipc_ptz_dir_e dir, f32 angle)
{
    if (_not_init() || _illegal_dir(dir) || _illegal_angle(angle)) return ;

    _g_ptz.ptz_user_ctrl = 1;
    
    _ptz_speed(DIR_TO_ACT(dir), _g_ptz.usr_speed[DIR_TO_ACT(dir)]);
    _ptz_turn(dir, angle);
}

void ipc_ptz_track(ipc_ptz_dir_e dir, f32 angle, f32 speed)
{
    if (_not_init() || _illegal_dir(dir) || _illegal_angle(angle) || _illegal_speed(speed) || _user_ctrl_ptz()) return ;

    _ptz_speed(DIR_TO_ACT(dir), speed);
    _ptz_turn(dir, angle);
}

void ipc_ptz_turn_abs(ipc_ptz_action_e act, f32 angle)
{
    if (_not_init() || _not_self_check() || _illegal_action(act) || _illegal_angle(angle)) return ;

    _g_ptz.ptz_user_ctrl = 1;

    struct ipc_motor_step motor;
    memset(&motor, 0, sizeof(motor));
    _ptz_speed(act, _g_ptz.usr_speed[act]);

    motor.motor_index = act == IPC_PTZ_H ? IPC_MOTOR_INDEX_0 : IPC_MOTOR_INDEX_1;
    motor.step = (s32)(angle * _g_ptz.circle_step[motor.motor_index] / 360);
    s32 ret = ioctl(_g_ptz.fd, IPC_IOCTL_MOTOR_GOTO_SPEC_POS, (word)&motor);
    if (ret < 0) ipcerror("Ioctl trun abs failed! retcode=[%d], errmsg=[%s]", ret, strerror(errno));
}

s32 ipc_ptz_get_abs(ipc_ptz_action_e act, pf32 angle)
{
    if (_not_init()) return IPC_NOT_INIT;
    else if (_not_self_check()) return IPC_NOT_SUPPORT;
    else if (_illegal_action(act)) return IPC_INVALID_ARGS;

    struct ipc_motor_step motor;
    memset(&motor, 0, sizeof(motor));
    motor.motor_index = act == IPC_PTZ_H ? IPC_MOTOR_INDEX_0 : IPC_MOTOR_INDEX_1;

    s32 ret = ioctl(_g_ptz.fd, IPC_IOCTL_MOTOR_GET_CUR_STEPS, (word)&motor);
    if (ret < 0) {
        ipcerror("Ioctl get abs failed! retcode=[%d], errmsg=[%s]", ret, strerror(errno));
        return IPC_IOCTL_ERROR;
    }

    *angle = motor.step * 360.0 / _g_ptz.circle_step[motor.motor_index];

    return IPC_SUCCESS;
}

void ipc_ptz_turn_auto(ipc_ptz_action_e act)
{
    if (_not_init() || _illegal_action(act) || _user_ctrl_ptz()) return ;

    _ptz_speed(act, _g_ptz.usr_speed[act]);

    struct ipc_motor_status status;
    memset(&status, 0, sizeof(status));

    status.motor_index = act == IPC_PTZ_H ? IPC_MOTOR_INDEX_0 : IPC_MOTOR_INDEX_1;
    status.status = IPC_MOTOR_STATUS_AUTOMATIC_CRUISE;
    s32 ret = ioctl(_g_ptz.fd, IPC_IOCTL_MOTOR_SET_STATUS, (word)&status);
    if (ret < 0) ipcdebug("Ioctl set turn auto failed! retcod=[%d], errmsg=[%s]", ret, strerror(errno));
}

void ipc_ptz_set_init_angle(ipc_ptz_action_e act, f32 angle) 
{
    if (_illegal_action(act)) return ;
    _g_ptz.init_angle[act] = angle;
}

f32 ipc_ptz_get_init_angle(ipc_ptz_action_e act) 
{
    if (_illegal_action(act)) return -1;
 
    if (_g_ptz.init_angle[act] < -0.000001) { 
        f32 act_map[] = {
            [IPC_PTZ_H] = ipc_factory(ptz_h_init_angle),
            [IPC_PTZ_V] = ipc_factory(ptz_v_init_angle),
        };
        return act_map[act];
    }

    return _g_ptz.init_angle[act] >= 0.000001 ? _g_ptz.init_angle[act] : 0;
}

s32 ipc_ptz_zoom_multiplier_set(f32 multiplier)
{
    s32 ret                                    = 0;
    s32 scal_w                                 = 0;
    s32 scal_h                                 = 0;
    f64 result                                 = 0.0;
    struct ipc_plat_video_isp_crop cur_isp_crop = { 0 };
    struct ipc_plat_video_isp_crop new_isp_crop = { 0 };

    ipcdebug("zoom multiplier set %f\n", multiplier);

    ret = ipc_plat_api(0)->video_ctrl(0, IPC_VIDEO_CTRL_CMD_GET_ISP_CROP, &cur_isp_crop);
    if (ret != 0) {
        ipcerror("zoom get isp crop");
        return IPC_FAILED;
    }

    result = sqrt(multiplier); // Square root (because the zoom factor is calculated as the area factor, so the square root is needed), calculate the
                               // width and height reduction factor
    scal_w = ((int)((cur_isp_crop.max_width) / result)) & (~1);  // The width must be even
    scal_h = ((int)((cur_isp_crop.max_height) / result)) & (~1); // The height must be even

    ipcdebug("scal_w: %d, scal_h: %d, max_width: %d, min_width: %d, max_height: %d, mim_height: %d\n", scal_w, scal_h, cur_isp_crop.max_width,
           cur_isp_crop.min_width, cur_isp_crop.max_height, cur_isp_crop.min_height);
    if (scal_w > cur_isp_crop.max_width || scal_w < cur_isp_crop.min_width) {
        ipcwarn("scal width error\n");
        return IPC_FAILED;
    }

    if (scal_h > cur_isp_crop.max_height || scal_h < cur_isp_crop.min_height) {
        ipcwarn("scal height error\n");
        return IPC_FAILED;
    }

    new_isp_crop.cur_width      = scal_w;
    new_isp_crop.cur_height     = scal_h;
    new_isp_crop.cur_x_position = (cur_isp_crop.max_width - scal_w) / 2;
    new_isp_crop.cur_y_position = (cur_isp_crop.max_height - scal_h) / 2;
    new_isp_crop.cur_multiplier = multiplier;

    ipcdebug("new isp crop [%d:%d  %d:%d]", new_isp_crop.cur_x_position, new_isp_crop.cur_y_position, new_isp_crop.cur_width, new_isp_crop.cur_height);

    ipc_plat_api(0)->video_ctrl(0, IPC_VIDEO_CTRL_CMD_SET_ISP_CROP, &new_isp_crop);

    return IPC_SUCCESS;
}