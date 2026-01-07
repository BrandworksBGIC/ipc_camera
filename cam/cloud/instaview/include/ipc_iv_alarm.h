#ifndef __IPC_IV_ALARM_H__
#define __IPC_IV_ALARM_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "ipc_middleware.h"
int MFG_InitMD(IV_MDConfig_t MDconfig);
int MFG_GetMDResult();
void MFG_DeinitMD();
int MFG_SetMDSensitivity(int Sensitivity);
int MFG_SetMDSensitivityEx(int SensitivityEx);
#if !defined(__CHIP_AKV300__) || !defined(__CHIP_AKV130__)
E_AI_EVENT_TYPE MFG_GetAIResult_Callback(void);
int MFG_SetAIConfig_Callback(MfgAIConfigInfo* info);
#endif
#ifdef __cplusplus
}
#endif

#endif //__IPC_IV_ALARM_H__