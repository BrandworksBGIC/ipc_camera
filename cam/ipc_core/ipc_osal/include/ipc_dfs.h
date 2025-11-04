/**
 * @file ipc_dfs.h
 * @author ouyang
 * @brief
 * @version 1.0
 * @date 2021-05-06
 *
 * @copyright Copyright (c) 2021
 */
#ifndef __IPC_DFS_H__
#define __IPC_DFS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "ipc_iter.h"
#include "ipc_log.h"
#include <ipc_std.h>

typedef struct {
    s32 fd;
    vptr fa_log;
} ipc_file_t[1], *ipc_file_p;

typedef enum {
    IPC_SEEK_HEAD, ///< Relative to the beginning of the file
    IPC_SEEK_CUR,  ///< Relative to the current position
    IPC_SEEK_TAIL, ///< Relative to the end of the file
} ipc_file_seek_e;

typedef enum {
    IPC_FILE_RDONLY,
    IPC_FILE_WRONLY,
    IPC_FILE_RDWR,
    IPC_FILE_APPEND,
} ipc_file_mode_e;

/**
 * @brief Open the file in read-write mode. If the file does not exist, create a file with permissions of 666
 * ps: The advantage of this cluster of functions is that it handles interrupts and has detailed logs, but it is
 * recommended to only use them in tool packaging in this library, and theoretically, external use is not necessary
 *
 * @param h_file The handle that needs to be initialized
 * @param path The file path
 * @param mode Opening mode
 * @param fa_log External module log handle, used by the internal log module to inherit its control mode
 * @note Only file classes and node operations are supported
 * @return Standard return value of ipc_std.h
 */
EXAPI s32 ipc_file_open(ipc_file_p h_file, pv8 path, ipc_file_mode_e mode, ipc_log_p fa_log);

/**
 * @brief Close file handle resources
 *
 * @param h_file The file handle initialized by ipc_file_open
 */
EXAPI void ipc_file_close(ipc_file_p h_file);

/**
 * @brief Read file data, the usage is basically the same as the read function
 *
 * @param h_file The file handle initialized by ipc_file_open
 * @param buff The buffer that receives data from the outside
 * @param max The size of the receive data buffer
 * @return <0: Standard return value of ipc_std.h; 0: The end of the file is reached; >0: The length of the data read
 */
EXAPI s32 ipc_file_read(ipc_file_p h_file, pv8 buff, s32 max);

/**
 * @brief Write file data, use basically the same as the write function
 *
 * @param h_file The file handle initialized by ipc_file_open
 * @param buff Data to be written outside
 * @param len The length of the data to be written
 * @return <0: Standard return value of ipc_std.h; >0: The length of the data written; cannot be 0
 */
EXAPI s32 ipc_file_write(ipc_file_p h_file, pv8 buff, s32 len);

/**
 * @brief Move the file read and write position, basically the same as the lseek usage
 *
 * @param h_file The file handle initialized by ipc_file_open
 * @param offset The offset address relative to whence, which can be negative
 * @param whence See ipc_file_seek_e
 * @return The file offset after moving
 */
EXAPI s32 ipc_file_seek(ipc_file_p h_file, s32 offset, ipc_file_seek_e whence);

/**
 * @brief Empty the file
 *
 * @param h_file The file handle initialized by ipc_file_open
 * @return The return value of ipc_file_seek
 */
EXAPI s32 ipc_file_clear(ipc_file_p h_file);

/**
 * @brief Generic iterative series functions, iterative reading of files, built-in open and close
 *
 * @param h_iter The iterator handle initialized by ITER_INIT, see ipc_iter.h
 * @param path The file path that needs to be read
 * @param buf The external receiving data buffer
 * @param p_len For the first time, the size of the receiving buffer is passed in, and each time the received data
 * length is returned
 * @return IPC_ITER_BREAK: Exit the iteration; IPC_ITER_CONTINUE: Continue the iteration
 * @attention It will actively exit the iteration when it finishes reading or encounters an error
 * @note Get the real return value of the iterator set through the ipc_iter_retval function
 */
EXAPI s32 ipc_file_read_iter(ipc_iter_p h_iter, pv8 path, pv8 buf, ps32 p_len);

/**
 * @brief Generic iterative series functions, iterative writing of files, built-in open and close
 *
 * @param h_iter The iterator handle initialized by ITER_INIT, see ipc_iter.h
 * @param path The file path that needs to be written
 * @param buf Data that needs to be written outside
 * @param len The length of the data that needs to be written
 * @return IPC_ITER_BREAK: Exit the iteration; IPC_ITER_CONTINUE: Continue the iteration
 * @attention It will only actively exit the iteration when there is an error in writing. In general, it cooperates with
 * an iterator such as ipc_file_read_iter, which is responsible for generating data sources, and can complete automatic
 * iteration and exit to destroy resources perfectly. If the data source generation is not a generic iterator and
 * external If iteration is interrupted, then the external call to ipc_iter_break_off is required to destroy resources
 * for all iterative objects (which is actually what the internal iterator calls in the end)
 * @note Get the real return value of the iterator set through the ipc_iter_retval function
 */
EXAPI s32 ipc_file_write_iter(ipc_iter_p h_iter, pv8 path, pv8 buf, s32 len);

/**
 * @brief File copy (Best demonstration of using ipc_file_read_iter and ipc_file_write_iter for encapsulation)
 *
 * @param file_src Copy source file path
 * @param file_desc Destination file path
 * @param __IPC_LOG__ External log handle, used by the internal log module to inherit its control mode
 * @return Standard return value of ipc_std.h
 */
EXAPI s32 ipc_file_copy(pv8 file_src, pv8 file_desc, ipc_log_p __IPC_LOG__);

/**
 * @brief Write a file once (open and close internally)
 *
 * @param path File path
 * @param buff The data buffer to be written
 * @param len The length of the data to be written
 * @param fa_log External module log handle, used by the internal log module to inherit its control mode
 * @return The length of data written
 */
EXAPI s32 ipc_file_write_once(pv8 path, pv8 buff, s32 len, ipc_log_p fa_log);

/**
 * @brief Read a file once (open and close internally)
 *
 * @param path File path
 * @param buff The buffer for reading data
 * @param max The size of the buffer for reading data
 * @param fa_log External module log handle, used by the internal log module to inherit its control mode
 * @return The length of the data read
 */
EXAPI s32 ipc_file_read_once(pv8 path, pv8 buff, s32 max, ipc_log_p fa_log);

/**
 * @brief Append and write a file once (open and close internally)
 *
 * @param path File path
 * @param buff The data buffer to be appended and written
 * @param len The length of the data to be appended and written
 * @param fa_log External module log handle, used by the internal log module to inherit its control mode
 * @return The length of the data written
 */
EXAPI s32 ipc_file_append_once(pv8 path, pv8 buff, s32 len, ipc_log_p fa_log);

/**
 * @brief Recursively create a folder
 *
 * @param path The folder path that needs to be generated
 * @return The standard return value of ipc_std
 */
EXAPI s32 ipc_mkdirs(pv8 path);

/**
 * @brief Recursively delete folders or files
 *
 * @param path The folder/file path to be deleted
 * @return The standard return value of ipc_std
 */
EXAPI s32 ipc_rm(pv8 path);

/**
 * @brief Get partition information corresponding to the path file
 *
 * @param path Folder/file path
 * @param total_k The total size of the corresponding partition (in k)
 * @param used_k The used size of the corresponding partition (in k)
 * @param free_k The idle size of the corresponding partition (in k)
 * @return The standard return value of ipc_std
 */
EXAPI s32 ipc_df(pv8 path, pu32 total_k, pu32 used_k, pu32 free_k);

#ifdef __cplusplus
}
#endif

#endif // __IPC_DFS_H__
