#ifndef __RTS_PTZ_H__
#define __RTS_PTZ_H__

#include <linux/list.h>
#include <linux/timer.h>
#include <linux/miscdevice.h>
#include <linux/ioctl.h>
#include <linux/mutex.h>
#include <linux/wait.h>

#define DIR_NONE		0
#define DIR_UP			1
#define DIR_DOWN		-1
#define DIR_RIGHT		1
#define DIR_LEFT		-1

#define SPEED_NORMAL		2
#define SPEED_LOW		4
#define SPEED_HIGH		1

#define SERIAL_IO_PERIOD	8
#define MOTOR_PHASE		4
/*
 * 4-phase-5-wired step motor supports 4-pahse-4-step(beat)
 * or 4-phase-8-step(beat) drive way
 */
#define MOTOR_BEAT		4

#define SH_CP			0
#define ST_CP			1
#define DS			2

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
struct gpio_group {
	int gpio[3];
	char *gpio_label[3];
};

struct step_motor_dev {
	char *name;

	const int *timing_info;
	int timing_info_len;

	unsigned int is_running;
	int direction;
	/* two motor share the same speed */
	unsigned int speed;

	unsigned int max_steps;
	unsigned int max_degrees;
	unsigned int setting_steps;
	unsigned int running_steps;
	unsigned int setting_pos;
	unsigned int running_pos;
	unsigned char beat;
	unsigned char compensate;
};

struct ptz_device {
	struct mutex mutex;
	struct timer_list pulse_timer;
	wait_queue_head_t wait;
	int need_wake;
	int wait_condition;
	struct step_motor_dev *xmotor;
	struct step_motor_dev *ymotor;
};

#define rts_ptz_log(level, fmt, arg...) \
		printk(level "%s[%d]"fmt"\n", __func__, __LINE__, ##arg)

#ifdef __DEBUG
#define rts_ptz_debug(fmt, arg...) \
	rts_ptz_log(KERN_ERR, fmt, ##arg)
#else
#define rts_ptz_debug(fmt, arg...)
#endif

#define rts_ptz_info(fmt, arg...) \
	rts_ptz_log(KERN_INFO, fmt, ##arg)
#define rts_ptz_err(fmt, arg...) \
	rts_ptz_log(KERN_ERR, fmt, ##arg)

#endif
