/*
 * Realtek Semiconductor Corp.
 *
 * Copyright (C) 2020      Steve Liu<steve_liu@realsil.com.cn>
 */
#ifndef _INCLUDE_PTZ_USR_H
#define _INCLUDE_PTZ_USR_H

#include <stdbool.h>

#define DIR_NONE		0
#define DIR_UP			1
#define DIR_DOWN		-1
#define DIR_RIGHT		1
#define DIR_LEFT		-1

#define SPEED_NORMAL		2
#define SPEED_LOW		4
#define SPEED_HIGH		1

struct motor_info {
	int dir;
	unsigned int speed;
	unsigned int steps;
	unsigned int pos;
	unsigned int is_running;

	unsigned int max_steps;
	unsigned int max_degrees;
};

struct ptzctrl_info {
	struct motor_info xmotor_info;
	struct motor_info ymotor_info;
	bool block;
};

#define RTS_PTZ_IOC_MAGIC		'm'
#define RTS_PTZ_IOC_DRIVE		_IOW(RTS_PTZ_IOC_MAGIC, 1, struct ptzctrl_info)
#define RTS_PTZ_IOC_RUN			_IOW(RTS_PTZ_IOC_MAGIC, 2, struct ptzctrl_info)
#define RTS_PTZ_IOC_STOP		_IO(RTS_PTZ_IOC_MAGIC, 3)
#define RTS_PTZ_IOC_RESET		_IO(RTS_PTZ_IOC_MAGIC, 4)
#define RTS_PTZ_IOC_G_INFO		_IOR(RTS_PTZ_IOC_MAGIC, 5, struct ptzctrl_info)
#define RTS_PTZ_IOC_G_POS		_IOR(RTS_PTZ_IOC_MAGIC, 6, struct ptzctrl_info)
#define RTS_PTZ_IOC_S_POS		_IOW(RTS_PTZ_IOC_MAGIC, 7, struct ptzctrl_info)
#define RTS_PTZ_IOC_IS_RUNNING		_IOR(RTS_PTZ_IOC_MAGIC, 8, struct ptzctrl_info)

#endif
