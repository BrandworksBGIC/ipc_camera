/*
 *  Copyright (C) 2021 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU LESSER General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ENTROPY_H__
#define __ENTROPY_H__

#define CONF_DIR_RAMFS		"/var/conf"
#define CONF_DIR_JFFS2		"/usr/conf"
#define CONF_DIR_ETC		"/etc/conf"

extern uint32_t ep_log_level;

#define ep_log(level, fmt, args...)		\
	do { \
		if (level & ep_log_level) \
			syslog(LOG_ERR, "%-20s--> "fmt, __func__, ##args); \
	} while (0)

/* log utils */
enum {
	EP_LOG_FINE		= (1 << 0),
	EP_LOG_INFO		= (1 << 1),
	EP_LOG_WARNING		= (1 << 2),
	EP_LOG_ERROR		= (1 << 3),
	EP_LOG_FATAL		= (1 << 4),
};

#define ep_fine(fmt, args...)    ep_log(EP_LOG_FINE, fmt, ##args)
#define ep_info(fmt, args...)    ep_log(EP_LOG_INFO, fmt, ##args)
#define ep_warning(fmt, args...) ep_log(EP_LOG_WARNING, fmt, ##args)
#define ep_error(fmt, args...)   ep_log(EP_LOG_ERROR, fmt, ##args)
#define ep_fatal(fmt, args...)   ep_log(EP_LOG_FINE, fmt, ##args)

#endif
