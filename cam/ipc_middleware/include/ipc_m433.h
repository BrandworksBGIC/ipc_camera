#ifndef __IPC_M433_H__
#define __IPC_M433_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc_core.h>

/**
 * @brief Initialize M433 433MHz RF transmitter
 *
 * Opens the M433 device file and prepares for communication.
 * The GPIO pin needs to be configured separately using driver IOCTL.
 *
 * @return 0 on success, negative error code on failure
 */
EXAPI s32 ipc_m433_init(void);

/**
 * @brief Set MAC address for M433 ID generation
 *
 * Sets the MAC address used to generate the 20-bit ID for RF transmission.
 * The MAC address should be a 12-character hex string (e.g., "112233445566").
 *
 * @param mac Pointer to 12-character MAC address string
 * @return 0 on success, negative error code on failure
 */
EXAPI s32 ipc_m433_set_mac_addr(pv8 mac);

/**
 * @brief Send alarm signal via M433 transmitter
 *
 * Triggers the transmission of a 433MHz alarm signal using the
 * configured GPIO pin and MAC address-derived ID.
 *
 * @return 0 on success, negative error code on failure
 */
EXAPI s32 ipc_m433_send_alarm(void);

#ifdef __cplusplus
}
#endif

#endif // __CP_M433_H__