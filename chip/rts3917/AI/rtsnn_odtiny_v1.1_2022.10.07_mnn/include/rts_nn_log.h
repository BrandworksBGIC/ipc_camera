/*
* Realtek Semiconductor Corp.
*
* rts_nn_log.h
*
* Copyright (C) 2021	  Rui Ge<guru_ge@realsil.com.cn>
*/

/**
 * @file rts_nn_log.h
 * @brief rts nn log API reference
 *
 * @defgroup log Realnet Log API
 * @brief nn log API reference
 * @{
 */
#ifndef _RTS_NN_LOG_H
#define _RTS_NN_LOG_H

#include <stdint.h>
#include <stdarg.h>

#ifndef RTS_NN_API
#if defined(_WIN32)
#define RTS_NN_API __declspec(dllexport)
#else
#define RTS_NN_API __attribute__((visibility("default")))
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum RTS_NN_LOG_TYPE {
	RTS_NN_LOG_IDX_CONS = 0,
	RTS_NN_LOG_IDX_SYSLOG,
	RTS_NN_LOG_IDX_FILE,
	RTS_NN_LOG_IDX_CALLBACK,
	RTS_NN_LOG_IDX_RESERVED
};

enum RTS_NN_LOG_MASK {
	RTS_NN_LOG_MASK_CONS = (0x1 << RTS_NN_LOG_IDX_CONS),
	RTS_NN_LOG_MASK_SYSLOG = (0x1 << RTS_NN_LOG_IDX_SYSLOG),
	RTS_NN_LOG_MASK_FILE = (0x1 << RTS_NN_LOG_IDX_FILE),
	RTS_NN_LOG_MASK_CALLBACK = (0x1 << RTS_NN_LOG_IDX_CALLBACK),
};

/**
 * @brief log priority id
 */
enum RTS_NN_LOG_PRIORITY {
	RTS_NN_LOG_DEBUG = 0, /**< debug level */
	RTS_NN_LOG_INFO, /**< inform level */
	RTS_NN_LOG_NOTICE, /**< notice level */
	RTS_NN_LOG_WARNING, /**< warning level */
	RTS_NN_LOG_ERR, /**< error level */
	RTS_NN_LOG_CRIT, /**< critical level */
	RTS_NN_LOG_ALERT, /**< alert level */
	RTS_NN_LOG_EMERG, /**< emerg level */
	RTS_NN_LOG_MEM, /**< memory level */
	RTS_NN_LOG_REG, /**< register level */
	RTS_NN_LOG_RESERVED /**< reserved */
};

/**
 * @brief Print nn log
 * @param[in] priority control level of the log, defined as RTS_NN_LOG_PRIORITY
 * @param[in] format refer to format of printf
 */
RTS_NN_API void rts_nn_log(int priority, const char *format, ...);

/**
 * @brief Set log ident
 * @param[in] ident each log is prefixed with the ident,
 * to distinguish between different modules.
 * one process can only have one ident.
 * @return 0 : success, minus : fail
 */
RTS_NN_API int rts_nn_set_log_ident(const char *ident);

/**
 * @brief Set log mask
 * @param[in] mask defined as follows,\n
 * RTS_NN_LOG_MASK_CONS: if RTS_NN_LOG_MASK_CONS bit is set,
 * log will be printed on the terminal;\n
 * RTS_NN_LOG_MASK_SYSLOG, if RTS_NN_LOG_MASK_SYSLOG bit is set,
 * log will be printed to syslog;\n
 * RTS_NN_LOG_MASK_CALLBACK, if RTS_NN_LOG_MASK_CALLBACK bit is set, \n
 * the callback function will be called to print log\n
 * if more than one bit is set, log will be printed in multiple places;\n
 * if the mask argument is 0, the current log mask is not modified\n
 * @attention setting RTS_NN_LOG_MASK_FILE requires calling
 * rts_set_log_file function first to set the path of log file.\n
 * setting RTS_NN_LOG_MASK_CALLBACK,
 * you need to call rts_set_log_callback function first to set
 * the log print function.\n
 * @return current log_mask
 */
RTS_NN_API uint32_t rts_nn_set_log_mask(uint32_t mask);

RTS_NN_API uint32_t rts_nn_get_log_mask(void);
RTS_NN_API uint32_t rts_nn_clr_log_mask(void);

/**
 * @brief Set log file
 * @param[in] filename set the location of log file,
 * set NULL will close log file
 * @return 0 : success, minus : fail
 */
RTS_NN_API int rts_nn_set_log_file(const char *filename);

/**
 * @brief Set log callback function
 * @param[in] callback callback function to be assigned
 * @return 0 : success, minus : fail
 */
RTS_NN_API int rts_nn_set_log_callback(void (*callback)(const char *, va_list));

/**
 * @brief Set log level
 * @param[in] level_mask defined as RTS_NN_LOG_PRIORITY
 * if level_mask argument is 0, the current level_mask is not modified
 * @return current log_level
 */
RTS_NN_API uint32_t rts_nn_set_log_level(uint32_t level_mask);
RTS_NN_API uint32_t rts_nn_get_log_level(void);
RTS_NN_API uint32_t rts_nn_clr_log_level(void);

RTS_NN_API int rts_nn_set_log_enable(int priority, int enable);
RTS_NN_API int rts_nn_get_log_enable(int priority, int enable);

RTS_NN_API void rts_nn_dump_data(char *data, unsigned int len, unsigned int row);
/*!@}*/

#ifdef RTS_NN_LOG_TAG
#define _TAG	" ["RTS_NN_LOG_TAG"] "
#else
#define _TAG	""
#endif

#define __RTS_NN_LOG(level, fmt, arg...) \
	rts_nn_log(level, _TAG "<%s, %d>" fmt, __func__, __LINE__, ##arg)

#define RTS_NN_LOG(level, fmt, arg...) \
	rts_nn_log(level, _TAG "" fmt, ##arg)

#define RTS_NN_DBG(...) \
	do {\
		__RTS_NN_LOG(RTS_NN_LOG_DEBUG, __VA_ARGS__);\
	} while (0)

#define RTS_NN_INFO(...) \
	do {\
		__RTS_NN_LOG(RTS_NN_LOG_INFO, __VA_ARGS__);\
	} while (0)

#define RTS_NN_NOTICE(...) \
	do {\
		__RTS_NN_LOG(RTS_NN_LOG_NOTICE, __VA_ARGS__);\
	} while (0)

#define RTS_NN_WARN(...) \
	do {\
		__RTS_NN_LOG(RTS_NN_LOG_WARNING, __VA_ARGS__);\
	} while (0)

#define RTS_NN_ERR(...) \
	do {\
		__RTS_NN_LOG(RTS_NN_LOG_ERR, __VA_ARGS__);\
	} while (0)

#define RTS_NN_CRIT(...) \
	do {\
		__RTS_NN_LOG(RTS_NN_LOG_CRIT, __VA_ARGS__);\
	} while (0)

#define RTS_NN_ALERT(...) \
	do {\
		__RTS_NN_LOG(RTS_NN_LOG_ALERT, __VA_ARGS__);\
	} while (0)

#define RTS_NN_EMERG(...) \
	do {\
		__RTS_NN_LOG(RTS_NN_LOG_EMERG, __VA_ARGS__);\
	} while (0)

#define RTS_NN_OPT(...) \
	do {\
		fprintf(stdout, __VA_ARGS__);\
	} while (0)

#ifdef __cplusplus
}
#endif

#endif

