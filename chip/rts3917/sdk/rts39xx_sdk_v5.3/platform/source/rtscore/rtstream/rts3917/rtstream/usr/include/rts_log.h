/*
 * Copyright (c) 2022 Realtek Semiconductor Corp. All rights reserved.
 *
 * SPDX-License-Identifier: LicenseRef-Realtek-Proprietary
 *
 * This software component is confidential and proprietary to Realtek
 * Semiconductor Corp. Disclosure, reproduction, redistribution, in whole
 * or in part, of this work and its derivatives without express permission
 * is prohibited.
 */

/**
 * @file rts_log.h
 * @brief rts log API reference
 *
 * @defgroup log RTStream Log API
 * @brief RTStream log API reference
 * @{
 */
#ifndef _LIBRTSCAMKIT_RTS_LOG_H
#define _LIBRTSCAMKIT_RTS_LOG_H

#include <stdint.h>
#include <stdarg.h>

enum RTS_LOG_TYPE {
	RTS_LOG_IDX_CONS = 0,
	RTS_LOG_IDX_SYSLOG,
	RTS_LOG_IDX_FILE,
	RTS_LOG_IDX_CALLBACK,
	RTS_LOG_IDX_RESERVED
};

enum RTS_LOG_MASK {
	RTS_LOG_MASK_CONS = (0x1 << RTS_LOG_IDX_CONS),
	RTS_LOG_MASK_SYSLOG = (0x1 << RTS_LOG_IDX_SYSLOG),
	RTS_LOG_MASK_FILE = (0x1 << RTS_LOG_IDX_FILE),
	RTS_LOG_MASK_CALLBACK = (0x1 << RTS_LOG_IDX_CALLBACK),
};

/**
 * @brief log priority id
 */
enum RTS_LOG_PRIORITY {
	RTS_LOG_DEBUG = 0, /**< debug level */
	RTS_LOG_INFO, /**< inform level */
	RTS_LOG_NOTICE, /**< notice level */
	RTS_LOG_WARNING, /**< warning level */
	RTS_LOG_ERR, /**< error level */
	RTS_LOG_CRIT, /**< critical level */
	RTS_LOG_ALERT, /**< alert level */
	RTS_LOG_EMERG, /**< emerg level */
	RTS_LOG_MEM, /**< memory level */
	RTS_LOG_REG, /**< register level */
	RTS_LOG_RESERVED /**< reserved */
};

/**
 * @brief Print rts log
 * @param[in] priority control level of the log, defined as RTS_LOG_PRIORITY
 * @param[in] format refer to format of printf
 */
void rts_log(int priority, const char *format, ...);

/**
 * @brief Set log ident
 * @param[in] ident each log is prefixed with the ident,
 * to distinguish between different modules.
 * one process can only have one ident.
 * @return 0 : success, minus : fail
 */
int rts_set_log_ident(const char *ident);

/**
 * @brief Set log mask
 * @param[in] mask defined as follows,\n
 * RTS_LOG_MASK_CONS: if RTS_LOG_MASK_CONS bit is set,
 * log will be printed on the terminal;\n
 * RTS_LOG_MASK_SYSLOG, if RTS_LOG_MASK_SYSLOG bit is set,
 * log will be printed to syslog;\n
 * RTS_LOG_MASK_CALLBACK, if RTS_LOG_MASK_CALLBACK bit is set, \n
 * the callback function will be called to print log\n
 * if more than one bit is set, log will be printed in multiple places;\n
 * if the mask argument is 0, the current log mask is not modified\n
 * @attention setting RTS_LOG_MASK_FILE requires calling
 * rts_set_log_file function first to set the path of log file.\n
 * setting RTS_LOG_MASK_CALLBACK,
 * you need to call rts_set_log_callback function first to set
 * the log print function.\n
 * @return current log_mask
 */
uint32_t rts_set_log_mask(uint32_t mask);

uint32_t rts_get_log_mask(void);
uint32_t rts_clr_log_mask(void);

/**
 * @brief Set log file
 * @param[in] filename set the location of log file,
 * set NULL will close log file
 * @return 0 : success, minus : fail
 */
int rts_set_log_file(const char *filename);

/**
 * @brief Set log callback function
 * @param[in] callback callback function to be assigned
 * @return 0 : success, minus : fail
 */
int rts_set_log_callback(void (*callback)(const char *, va_list));

/**
 * @brief Set log level
 * @param[in] level_mask defined as RTS_LOG_PRIORITY
 * if level_mask argument is 0, the current level_mask is not modified
 * @return current log_level
 */
uint32_t rts_set_log_level(uint32_t level_mask);
uint32_t rts_get_log_level(void);
uint32_t rts_clr_log_level(void);

int rts_set_log_enable(int priority, int enable);
int rts_get_log_enable(int priority, int enable);

void rts_dump_data(char *data, unsigned int len, unsigned int row);
void rts_log_set_file_attr(uint64_t maxsize, uint64_t remain,
		uint8_t append, uint8_t flush);
/*!@}*/

#ifdef RTS_LOG_TAG
#define _TAG	" ["RTS_LOG_TAG"] "
#else
#define _TAG	""
#endif

#define __RTS_LOG(level, fmt, arg...) \
	rts_log(level, _TAG"<%s, %d>"fmt, __func__, __LINE__, ##arg)

#define RTS_LOG(level, fmt, arg...) \
	rts_log(level, _TAG""fmt, ##arg)

#define RTS_DEBUG(...) \
	do {\
		rts_log(RTS_LOG_DEBUG, _TAG""__VA_ARGS__);\
	} while (0)

#define RTS_INFO(...) \
	do {\
		rts_log(RTS_LOG_INFO, _TAG""__VA_ARGS__);\
	} while (0)

#define RTS_NOTICE(...) \
	do {\
		__RTS_LOG(RTS_LOG_NOTICE, __VA_ARGS__);\
	} while (0)

#define RTS_WARNING(...) \
	do {\
		__RTS_LOG(RTS_LOG_WARNING, __VA_ARGS__);\
	} while (0)

#define RTS_ERR(...) \
	do {\
		__RTS_LOG(RTS_LOG_ERR, __VA_ARGS__);\
	} while (0)

#define RTS_CRIT(...) \
	do {\
		__RTS_LOG(RTS_LOG_CRIT, __VA_ARGS__);\
	} while (0)

#define RTS_ALERT(...) \
	do {\
		__RTS_LOG(RTS_LOG_ALERT, __VA_ARGS__);\
	} while (0)

#define RTS_EMERG(...) \
	do {\
		__RTS_LOG(RTS_LOG_EMERG, __VA_ARGS__);\
	} while (0)

#define RTS_OPT(...) \
	do {\
		fprintf(stdout, __VA_ARGS__);\
	} while (0)

#endif
