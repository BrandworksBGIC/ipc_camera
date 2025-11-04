#ifndef __IPC_LOG_H__
#define __IPC_LOG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc_std.h>

#ifndef IPC_LOG_CTRL_NUM
#define IPC_LOG_CTRL_NUM 4
#endif

#ifndef IPC_LOG_NAME_LEN
#define IPC_LOG_NAME_LEN 8
#endif

/**
 * structure representing a log record
 */
typedef struct {
    vptr h_self;   ///< pointer to the log handle of this module
    vptr h_father; ///< pointer to the ipc_lot_t of the previous level, that is, a single-linked list
} ipc_log_t[1], *ipc_log_p;

typedef struct {
    u8 level[IPC_LOG_CTRL_NUM];    ///< log level of each agent (0 for local printing)
    v8 name[IPC_LOG_NAME_LEN + 1]; ///< log The name length of the module + '\0'
    u16 desc_len;                 ///< The length of the corresponding description of the log module (including '\0')
    v8 desc[0];                   ///< logged module description
} ipc_log_info_t, *ipc_log_info_p;

typedef struct {
    u8 level;      ///< this log level
    u64 uptime;    ///< uptime in milliseconds when this log was generated
    s32 log_len;   ///< log Length
    v8 log_ctx[0]; ///< log content
} ipc_log_proxy_t, *ipc_log_proxy_p;

typedef enum {
    IPC_LOG_NONE,  ///< no printing
    IPC_LOG_FATAL, ///< log level: fatal information, exception information, unexpected information, system-level errors
                  ///< (extremely low frequency printing)
    IPC_LOG_ERROR, ///< log level: logical error information (low frequency printing)
    IPC_LOG_WARN,  ///< log level: some expected but not satisfactory warning information (can be solved by retry, medium
                  ///< and low frequency printing)
    IPC_LOG_INFO,  ///< log level: some module-level information describing the state of the module (medium and low
                  ///< frequency printing)
    IPC_LOG_DEBUG, ///< log level: program flow information, focusing on the running process (medium and high frequency
                  ///< printing)
    IPC_LOG_TRACE, ///< log level: verbose tracing information, focused on the data itself (high frequency printing)
} ipc_log_level_e;

/* Users */
/**
 * initialize a log record
 *
 * @param h_this    pointer to the log record structure to be initialized
 * @param father    pointer to the parent structure, used to connect to a higher-level log record
 * @param name      pointer to the character string containing the name of the log module
 * @param level     initial log level
 * @param desc      pointer to the character string that describes the log module
 * @return          0 if successful, an error code otherwise
 */
EXAPI s32 ipc_log_init(ipc_log_p h_this, ipc_log_p father, pcv8 name, u8 level, pcv8 desc);

/**
 * output log information according to a certain format
 *
 * @param h_this    pointer to the log record structure
 * @param level     log level
 * @param format    character string format specifying the log content
 * @param...       additional parameters
 * @return          the number of characters printed, or a negative value if an error occurs
 */
EXAPI __attribute__((format(printf, 3, 4))) void ipc_log_printf(ipc_log_p h_this, u8 level, pcv8 format, ...);

/**
 * dump hexadecimal data
 *
 * @param h_this    pointer to the log record structure
 * @param level     log level
 * @param data      pointer to the data to be displayed
 * @param len       length of the data to be displayed
 * @return          None
 */
EXAPI void ipc_log_hexdump(ipc_log_p h_this, u8 level, vptr data, s32 len);
EXAPI s32 ipc_log_check(ipc_log_p h_this, u8 level);

/* Controllers */
/**
 * control the log level of a certain proxy
 *
 * @param proxy_port   proxy port number
 * @param name         pointer to the character string containing the name of the log module
 * @param level        new log level
 * @return          0 if successful, an error code otherwise
 */
EXAPI s32 ipc_log_ctrl(u16 proxy_port, pv8 name, u8 level);

/**
 * iteratively traverse all log information by a certain linked list structure
 *
 * @param h_info   starting point of the linked list structure
 * @return      NULL if an error occurs, otherwise a pointer to the next log information
 */
EXAPI ipc_log_info_p ipc_log_iter(ipc_log_info_p h_info);

__attribute__((unused)) static /* __thread */ ipc_log_t __IPC_LOG__ = { { NULL, NULL } };
#define clog_init(name, desc, ...)                                                                                      \
    ipc_log_init(__IPC_LOG__, (vptr)(word)(#__VA_ARGS__[0] ? __VA_ARGS__ : 0), name, IPC_LOG_INFO, desc)
#define ipc_log_ref(father) __attribute__((unused)) ipc_log_p __IPC_LOG__ = (ipc_log_p)father;
#define ipc_log_extend(father)                                                                                             \
    __attribute__((unused)) ipc_log_t __IPC_LOG_TMP__                                                                    \
        = { { __IPC_LOG__->h_self, father == __IPC_LOG__ ? NULL : father } };                                            \
    __attribute__((unused)) ipc_log_t __IPC_LOG__ = { { __IPC_LOG_TMP__->h_self, __IPC_LOG_TMP__->h_father } }
#define cptrace(...) ipc_log_printf(__IPC_LOG__, IPC_LOG_TRACE, __VA_ARGS__)
#define cpdebug(...) ipc_log_printf(__IPC_LOG__, IPC_LOG_DEBUG, __VA_ARGS__)
#define cpinfo(...) ipc_log_printf(__IPC_LOG__, IPC_LOG_INFO, __VA_ARGS__)
#define cpwarn(...) ipc_log_printf(__IPC_LOG__, IPC_LOG_WARN, __VA_ARGS__)
#define cperror(...) ipc_log_printf(__IPC_LOG__, IPC_LOG_ERROR, __VA_ARGS__)
#define cpfatal(...) ipc_log_printf(__IPC_LOG__, IPC_LOG_FATAL, __VA_ARGS__)
#define chtrace(...) ipc_log_hexdump(__IPC_LOG__, IPC_LOG_TRACE, __VA_ARGS__)
#define chdebug(...) ipc_log_hexdump(__IPC_LOG__, IPC_LOG_DEBUG, __VA_ARGS__)
#define chinfo(...) ipc_log_hexdump(__IPC_LOG__, IPC_LOG_INFO, __VA_ARGS__)
#define chwarn(...) ipc_log_hexdump(__IPC_LOG__, IPC_LOG_WARN, __VA_ARGS__)
#define cherror(...) ipc_log_hexdump(__IPC_LOG__, IPC_LOG_ERROR, __VA_ARGS__)
#define chfatal(...) ipc_log_hexdump(__IPC_LOG__, IPC_LOG_FATAL, __VA_ARGS__)

/* Other Tools / Hooks */
/**
 * @brief The built-in default formatted color printing
 *
 * @param p_proxy The proxy data, including all the information of the entire log
 * @param printf_f The incoming printing function (if it is NULL, then use printf)
 * @return The return value of printf_f
 */
EXAPI s32 ipc_log_display(ipc_log_proxy_p p_proxy, s32 (*printf_f)(pcv8 format, ...));

/**
 * @brief Hook the default print display function
 *
 * @param hook_f The callback function that hooks the default print implementation
 * @param p_proxy The proxy data passed into the hook callback function pointer includes all the information of the
 * entire log
 */
EXAPI void ipc_log_display_hook(void (*hook_f)(ipc_log_proxy_p proxy));

#ifdef __cplusplus
}
#endif

#endif // __IPC_LOG_H__
