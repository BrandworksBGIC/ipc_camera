/**
 * @file ipc_std.h
 * @brief 
 * @version 1.0
 * @date 2025-05-06
 *
 * @copyright Copyright (c) 2025
 *
 */

#ifndef __IPC_STD_H__
#define __IPC_STD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/********************************* basic type ******************************************/
// typedef void void;
typedef void* vptr;
typedef void* const cvptr;
typedef unsigned long word; // Cross-platform, Machine word length

#define __STDTYPE__(attr, prefix)                                                                                      \
    typedef char attr prefix##v8;                                                                                      \
    typedef signed char attr prefix##s8;                                                                               \
    typedef unsigned char attr prefix##u8;                                                                             \
    typedef signed short int attr prefix##s16;                                                                         \
    typedef unsigned short int attr prefix##u16;                                                                       \
    typedef signed int attr prefix##s32;                                                                               \
    typedef unsigned int attr prefix##u32;                                                                             \
    typedef signed long long attr prefix##s64;                                                                         \
    typedef unsigned long long attr prefix##u64;                                                                       \
    typedef float attr prefix##f32;                                                                                    \
    typedef double attr prefix##f64;

__STDTYPE__(, )                // typedef char v8
__STDTYPE__(const, c)          // typedef char const cv8
__STDTYPE__(*, p)              // typedef char *pv8
__STDTYPE__(const*, pc)        // typedef char const *pcv8
__STDTYPE__(*const, cp)        // typedef char* const cpv8
__STDTYPE__(const* const, cpc) // typedef char const * const cpcv8

/****************************** return code *****************************************/

enum {
    IPC_SUCCESS = 0, ///< Call succeeded
    /* Errors that cannot be otherwise described */
    IPC_FAILED = -1, ///< Used in scenarios like converting error codes when calling third-party libraries
    /* External errors */
    IPC_NOT_INIT     = -2, ///< Resource has not been initialized
    IPC_EXIST        = -3, ///< Resource exists and should not be re-initialized
    IPC_INVALID_ARGS = -4, ///< Invalid arguments
    IPC_NOT_SUPPORT  = -5, ///< Feature or argument is unsupported
    IPC_NOT_ALLOW    = -6, ///< Operation is not allowed (insufficient permissions)
    IPC_BREAK_OFF    = -7, ///< Behavior interrupted due to an external call to exit function

    /* System errors */
    IPC_NOMEM           = -11, ///< Insufficient memory
    IPC_OPEN_ERROR      = -12, ///< Error opening file
    IPC_SOCKET_ERROR    = -13, ///< Error opening socket
    IPC_THREAD_ERROR    = -14, ///< Error creating thread
    IPC_CONNECT_ERROR   = -15, ///< Connection failure
    IPC_IOCTL_ERROR     = -16, ///< ioctl or other low-level interface failure
    IPC_GET_HOST_FAILED = -17, ///< Domain name resolution failed
    IPC_READ_ERROR      = -18, ///< Read operation failed
    IPC_WRITE_ERROR     = -19, ///< Write operation failed
    IPC_TIMEOUT         = -20, ///< Timeout on read/write IO

    /* Internal errors */
    IPC_NOBUF           = -71, ///< Insufficient buffer resources requested
    IPC_NOT_READY       = -72, ///< Resource is not ready
    IPC_NOT_FOUND       = -73, ///< Resource not found
    IPC_ACTION_BUSY     = -74, ///< Action is busy
    IPC_OUT_OF_RANGE    = -75, ///< Out of range
    IPC_NOT_MATCH       = -76, ///< Mismatch
    IPC_TIMESTAMP_ERROR = -77, ///< Time error
    IPC_VERIFY_FAILED   = -78, ///< Verification failed
    IPC_PARSE_FAILED    = -79, ///< Parsing failed
    IPC_NOT_NEED        = -80, ///< Not needed
    IPC_NOT_DATA        = -81, ///< No data
};

/**
 * @brief Variable Naming Conventions
 * For functions or variables modified by 'static', add an underscore prefix: static s32 _val = 0;
 * For global variables, add a 'g_' prefix: s32 g_val = 0; static s32 _g_val = 0;
 * For function pointers, add an 'f_' prefix: funtype f_val = NULL; static funtype _gf_val
 * For object-oriented key handles, add an 'h_' prefix: vptr h_val = NULL;
 * For other basic types and ordinary structures without special significance, there are no specific requirements.
 *
 * When including headers outside the project scope, use include <module>
 * When including internal headers within the project scope, use include "module"
 *
 */

#define EXAPI __attribute__((visibility("default")))

#if (defined __IPC_ARCH__) && (defined __IPC_PLAT__) && (defined __IPC_MODE__) && (defined __IPC_GIT_BRANCH__)             \
    && (defined __IPC_GIT_TAG__) && (defined __IPC_GIT_COMMIT__) && (defined __IPC_GIT_DATE__)
#define __IPC_INFO__                                                                                                    \
    "build information:\n"                                                                                             \
    "\tplat: " __IPC_PLAT__                                                                                             \
    "\n"                                                                                                               \
    "\tarch: " __IPC_ARCH__                                                                                             \
    "\n"                                                                                                               \
    "\tmode: " __IPC_MODE__                                                                                             \
    "\n"                                                                                                               \
    "git information:\n"                                                                                               \
    "\tbranch: " __IPC_GIT_BRANCH__                                                                                     \
    "\n"                                                                                                               \
    "\ttag: " __IPC_GIT_TAG__                                                                                           \
    "\n"                                                                                                               \
    "\tcommit id: " __IPC_GIT_COMMIT__                                                                                  \
    "\n"                                                                                                               \
    "\tcommit date: " __IPC_GIT_DATE__ "\n"
#else
#define __IPC_INFO__ "build data: " __DATE__ " " __TIME__ "\n"
#endif

#ifndef __WEAKREF
#define __WEAKREF __attribute__((weakref))
#endif
#ifndef __WEAK
#define __WEAK __attribute__((weak))
#endif

#ifndef __PACKED
#define __PACKED __attribute__((packed))
#endif

#ifdef __cplusplus
}
#endif

#endif
