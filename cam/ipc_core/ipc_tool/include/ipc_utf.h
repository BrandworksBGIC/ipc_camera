#ifndef __IPC_UTF_H__
#define __IPC_UTF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc_std.h>

/**
 * @brief Decode utf8 to unicode (single character)
 *
 * @param[in] utf8 The input utf-8 character
 * @param[out] unicode The decoded unicode value
 * @return IPC_INVALID_ARGS: Parameter error or empty string
 *         IPC_PARSE_FAILED: Decoding error
 *         >0: The length of this utf-8 character
 * @note Comply with UCS4 standard
 */
EXAPI s32 ipc_utf8_decode(pv8 utf8, pu32 unicode);

#ifdef __cplusplus
}
#endif

#endif //__IPC_UTF_H__
