#ifndef __IPC_HEX_BIN_H__
#define __IPC_HEX_BIN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc_std.h>

/**
 * @brief Convert hexadecimal string to binary
 *
 * @param src_string Source string
 * @param dst_buffer Output binary buffer
 * @param buffer_size Buffer size
 * @return s32
 */
EXAPI s32 ipc_hex_to_bin(pv8 src_string, pu8 dst_buffer, s32 buffer_size);

/**
 * @brief Convert binary to hexadecimal string
 *
 * @param src_data
 * @param data_len
 * @param dst_buffer
 * @param buffer_size
 * @return s32
 */
EXAPI s32 ipc_bin_to_hex(pu8 src_data, s32 data_len, pv8 dst_buffer, s32 buffer_size);

#ifdef __cplusplus
}
#endif

#endif //__IPC_HEX_BIN_H__
