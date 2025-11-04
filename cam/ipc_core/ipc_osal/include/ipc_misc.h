#ifndef __IPC_MISC_H__
#define __IPC_MISC_H__
#ifdef __cplusplus
extern "C" {
#endif
#include <ipc_std.h>
/**
 * @brief As the name suggests, do nothing, often used as the default pointer of the callback pointer, to remove
 * unnecessary empty judgment operations
 *
 * @return Always 0/NULL
 * @note It is recommended to only use the NOT_DO_ANYTHING macro and not directly call the main body of
 * ipc_not_do_anything
 * @note Due to the convention of always returning only 0/NULL, it is not suitable for scenarios where you want to
 * default to an error value or other return value
 */
EXAPI word ipc_not_do_anything();
#define NOT_DO_ANYTHING ((vptr)ipc_not_do_anything)
/**
 * @brief Generate a random number
 *
 * @return Random number
 * @note For the current Linux platform, it will preferentially generate random numbers from /dev/urandom, and if it
 * fails, it will call the rand function in the C library
 */
EXAPI s32 ipc_rand(void);
/**
 * @brief Shell command call (equivalent to the evolution version of the system function)
 *
 * @param format Printf formatting
 * @param... Arguments
 * @return The return value of the command indicates whether the execution of the command was successful. Positive
 * values indicate success, negative values indicate failure.
 * @note Compared with the system, enhanced formatting support, and will not inherit the descriptor fd of its calling
 * process
 */
EXAPI s8 ipc_exec(pcv8 format, ...);
/**
 * @brief Get process id
 *
 * @return process id
 */
EXAPI s32 ipc_getpid(void);

#define WORD_SIZE sizeof(vptr) /* Word length */
#define LOCAL_HOST "127.0.0.1"
#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif
#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#define ARRSIZE(arr) (sizeof((arr)) / sizeof((arr[0])))
#define ALIGN(num) (((num) + WORD_SIZE - 1) / WORD_SIZE * WORD_SIZE)

#ifdef __cplusplus
}
#endif
#endif //__IPC_MISC_H__
