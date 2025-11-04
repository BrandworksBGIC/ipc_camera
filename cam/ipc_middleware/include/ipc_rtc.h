#ifndef __IPC_RTC_H__
#define __IPC_RTC_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <ipc_std.h>

EXAPI s32 ipc_rtc_init(void);

EXAPI s32 ipc_rtc_get_time(void);

EXAPI s32 ipc_rtc_set_time(void);

#ifdef __cplusplus
}
#endif

#endif //__IPC_RTC_H__