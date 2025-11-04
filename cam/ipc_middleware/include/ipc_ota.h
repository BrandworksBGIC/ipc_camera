#ifndef __IPC_OTA_H__
#define __IPC_OTA_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc_core.h>

/**
 * @brief OTA upgrade preparation (deletes firmware package, unmounts SD card, drops cache)
 *
 * @param path       Temporary storage path for the upgrade file (e.g., /tmp/update.bin)
 * @param pack_size  Total size of the upgrade package
 *                   >0: Size of the upgrade package,
 *                   =0: The size of the upgrade package is unknown at present, the total size is obtained by the final statistics of ipc_ota_writing
 * function calls <0: The upgrade package is written externally, without calling ipc_ota_writing function
 * @param has_backup Whether backup partition is supported (to prevent OTA upgrade failure)
 * @return ipc_std.h standard return value
 * @note This function first destroys all internal ipc_middleware modules to free up memory for OTA download.
 * @note Ensure that all external modules that depend on or use ipc_middleware APIs have exited before calling this interface.
 * @note This function starts a software watchdog, which will trigger a system reboot if ipc_ota_writing is not called within 90 seconds (or 5 minutes
 * if pack_size is less than 0).
 */
EXAPI s32 ipc_ota_prepare(pv8 path, s32 pack_size, u8 has_backup);

/**
 * @brief Temporarily writes downloaded data for OTA
 *
 * @param data Data address
 * @param len Data length
 * @return <0: ipc_std.h standard return value >=0: Total length of the upgrade file written so far
 * @note When the total amount of data exceeds the pack_size set by ipc_ota_prepare, further writing is not possible.
 * @note If this function or ipc_ota_upgrade is not called for 90 seconds, the system will automatically reboot.
 */
EXAPI s32 ipc_ota_writing(vptr data, s32 len);

/**
 * @brief OTA upgrade
 *
 * @param f_before_reboot Last notification before the current process exits
 * @note This function terminates the entire program and enters an upgrade state where progress feedback is not possible.
 * @note If the upgrade process gets stuck for more than ten minutes due to unexpected circumstances, the software watchdog will trigger a forced
 * system reboot.
 */
EXAPI void ipc_ota_upgrade(void (*f_before_exit)(s32 ret));

#ifdef __cplusplus
}
#endif

#endif //__IPC_OTA_H__