#ifndef __CP_PWM_H__
#define __CP_PWM_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <ipc_std.h>

s32 ipc_pwm_chn_init(s32 ch, s32 period, s32 duty);

s32 ipc_pwm_modify_chn_duty(s32 ch, s32 duty);

s32 ipc_pwm_chn_uninit(s32 ch);

#ifdef __cplusplus
}
#endif

#endif //__CP_PWM_H__