#ifndef __RTS_MOTOR_H__
#define __RTS_MOTOR_H__

#include <linux/list.h>
#include <linux/timer.h>
#include <linux/miscdevice.h>

/* #define __DEBUG */
#define __DEBUG

#define STEPS_AROUND	4030
#define STEP_MOTOR_NUM	2

#define DIR_NONE	0
#define DIR_UP		1
#define DIR_DOWN	2
#define DIR_LEFT	3
#define DIR_RIGHT	4

#define SPEED_NORMAL	2
#define SPEED_LOW	1
#define SPEED_HIGH	4

/* us */
#define SPEED_PERIOD	8000

#define X_MOTOR_GPIO1       14
#define X_MOTOR_GPIO2		4
#define X_MOTOR_GPIO3		5
#define X_MOTOR_GPIO4		6

#define Y_MOTOR_GPIO1		26
#define Y_MOTOR_GPIO2		27
#define Y_MOTOR_GPIO3		15
#define Y_MOTOR_GPIO4	    12

#define RTS_GET_MOTOR_POS_CTL _IOR('m', 1, int)
#define RTS_RUN_MOTOR_TIME_CTL _IOW('m', 2, int)
#define RTS_RUN_MOTOR_DEGREE_CTL _IOW('m', 3, int)
#define RTS_IS_RUNNING_MOTOR_CTL _IOR('m', 4, int)
#define RTS_STOP_MOTOR_CTL _IOW('m', 5, int)

enum {
	X_AXIS = 0,
	Y_AXIS,
};

struct step_motor_dev {
	char *name;
	int speed_factor;
	int speed_pulse_period;
	struct hrtimer pulse_timer;

	const int *timing_info;
	int timing_info_len;

	spinlock_t _lock;
	unsigned long _lock_flags;

	int is_running;
	int direction;
	int step;

	int max_steps;
	int setting_steps;
	int running_steps;
	int pos;
	int run_time;

	int gpio[4];
	char *gpio_label[4];
};


struct step_motor_ctrldev {
	const char *name;
	int is_running;

	struct miscdevice *misc_dev;
	struct step_motor_dev **step_motors;
	int motor_nums;

	int x_pos;
	int y_pos;

	int default_x_pos;
	int default_y_pos;
};

#define rts_motor_log(level, fmt, arg...) \
		printk(level "%s[%d]"fmt"\n", __func__, __LINE__, ##arg)

#ifdef __DEBUG
#define rts_motor_debug(fmt, arg...) \
	rts_motor_log(KERN_ERR, fmt, ##arg)
#else
#define rts_motor_debug(fmt, arg...)
#endif

#define rts_motor_info(fmt, arg...) \
	rts_motor_log(KERN_INFO, fmt, ##arg)
#define rts_motor_err(fmt, arg...) \
	rts_motor_log(KERN_ERR, fmt, ##arg)

#endif
