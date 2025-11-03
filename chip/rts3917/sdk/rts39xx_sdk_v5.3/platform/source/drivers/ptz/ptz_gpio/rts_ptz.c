#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>
#include <linux/power/max8903_charger.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#include "rts_ptz.h"

#define MAX(x, y) ((x) >= (y) ? (x) : (y))
#define MIN(x, y) ((x) <= (y) ? (x) : (y))

struct motorctrl_info {
	int x;
	int y;
	unsigned int dir;
	unsigned int speed;
	unsigned int degree;
	unsigned int run_time;
	unsigned int is_running;
	unsigned int motor_run_time[2];
};

#define X_MAX_STEPS			4096
#define Y_MAX_STEPS			1440

#define ORIGIN_ZERO			0
#define STEPMOTOR_DEVICE_NAME		"rts_motor"

static struct step_motor_ctrldev *stepmotor_ctrl;

/* 0b0001->0b0011->0b0010->0b0110->0b0100->0b1100->0b1000->0b1001 */
const int timing[] = { 1, 3, 2, 6, 4, 12, 8, 9 };

struct step_motor_dev x_stepmotor = {
	.name = "x_motor",
	.gpio = {
		X_MOTOR_GPIO1,
		X_MOTOR_GPIO2,
		X_MOTOR_GPIO3,
		X_MOTOR_GPIO4
	},
	.gpio_label = {
		"x_motor_1",
		"x_motor_2",
		"x_motor_3",
		"x_motor_4"
	},
	.max_steps = X_MAX_STEPS,
};

struct step_motor_dev y_stepmotor = {
	.name = "y_motor",
	.gpio = {
		Y_MOTOR_GPIO1,
		Y_MOTOR_GPIO2,
		Y_MOTOR_GPIO3,
		Y_MOTOR_GPIO4
	},
	.gpio_label = {
		"y_motor_1",
		"y_motor_2",
		"y_motor_3",
		"y_motor_4"
	},
	.max_steps = Y_MAX_STEPS,
};

struct step_motor_dev *step_motor_array[] = {
	&x_stepmotor,
	&y_stepmotor
};

static void ports_shutdown(struct step_motor_dev *motor)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(motor->gpio); i++)
		gpio_set_value(motor->gpio[i], 0);
}

static void do_drive_motor(void *arg)
{
	int i = 0;
	int val = 0;
	struct step_motor_dev *motor = (struct step_motor_dev *)arg;

	unsigned char gpio_orders[3][8] = {
		/* counter clockwise */
		{7, 6, 5, 4, 3, 2, 1, 0},
		{0},
		/* clockwise */
		{0, 1, 2, 3, 4, 5, 6, 7}
	};

	unsigned char (*order)[8] = &(gpio_orders[1]);

	motor->running_steps++;
	if (motor->running_steps >= motor->setting_steps) {
		rts_motor_debug("%s finish running", motor->name);

		motor->pos += (motor->setting_steps * motor->step);
		motor->pos = MAX(MIN(motor->pos, motor->max_steps), 0);

		motor->run_time += motor->setting_steps *
			motor->speed_pulse_period / motor->speed_factor;

		motor->is_running = 0;
		motor->step = 0;
		motor->direction = DIR_NONE;
		motor->running_steps = 0;
		motor->setting_steps = 0;

		ports_shutdown(motor);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(motor->gpio); i++) {
		val = order[motor->step][motor->running_steps % 8];
		val = (motor->timing_info[val] >> i) & 0x1;

		gpio_set_value(motor->gpio[i], val);
	}
}

static void rts_deinit_gpios(struct step_motor_dev *motor)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(motor->gpio); i++)
		gpio_free(motor->gpio[i]);
}

static int rts_init_gpios(struct step_motor_dev *motor)
{
	int i = 0;
	int ret = 0;

	for (i = 0; i < ARRAY_SIZE(motor->gpio); i++) {
		ret = gpio_request(motor->gpio[i], motor->gpio_label[i]);
		if (ret) {
			rts_motor_err("request gpio[%d] fail",
					motor->gpio[i]);
			goto out;
		} else
			rts_motor_info("request gpio[%d] success",
					motor->gpio[i]);

		ret = gpio_direction_output(motor->gpio[i], 0);
		if (ret) {
			rts_motor_err("set gpio[%d] output fail",
					motor->gpio[i]);
			goto out;
		} else
			rts_motor_info("set gpio[%d] output success",
					motor->gpio[i]);

		gpio_set_value(motor->gpio[i], 0);
	}

	return 0;
out:
	for (i = 0; i < ARRAY_SIZE(motor->gpio); i++)
		gpio_free(motor->gpio[i]);

	return ret;
}

static char *dir_to_string(const int dir)
{
	switch (dir) {
	case DIR_LEFT:
		return "left";
	case DIR_RIGHT:
		return "right";
	case DIR_UP:
		return "up";
	case DIR_DOWN:
		return "down";
	default:
		return "none";
	}

	return "none";
}

static void rts_run_motor(struct step_motor_dev *motor)
{
	int i = 0;
	int step = 0;

	spin_lock_irqsave(&motor->_lock, motor->_lock_flags);

	if (motor->direction == DIR_DOWN || motor->direction == DIR_LEFT)
		step = 1;
	else
		step = -1;

	rts_motor_info("motor speed=%d step=%d dir=%s", motor->speed_factor,
			motor->setting_steps, dir_to_string(motor->direction));

	motor->step = step;
	motor->is_running = 1;

	spin_unlock_irqrestore(&motor->_lock, motor->_lock_flags);

	for (i = 0; i < motor->setting_steps; i++) {
		do_drive_motor(motor);
		usleep_range(500, 1000);
	}
}

static void rts_stop_motor(struct step_motor_dev *motor)
{
	if (unlikely(!motor))
		return;

	rts_motor_info("stop %s", motor->name);

	spin_lock_irqsave(&motor->_lock, motor->_lock_flags);

	if (motor->is_running) {
		motor->pos += (motor->running_steps * motor->step);
		motor->pos = MAX(MIN(motor->pos, motor->max_steps), 0);

		motor->run_time += motor->running_steps *
			motor->speed_pulse_period / motor->speed_factor;

		motor->is_running = 0;
		motor->step = 0;
		motor->direction = DIR_NONE;
		motor->running_steps = 0;
		motor->setting_steps = 0;

		ports_shutdown(motor);
	}

	spin_unlock_irqrestore(&motor->_lock, motor->_lock_flags);

	return;

}

static int rts_init_motors(struct step_motor_dev *dev)
{
	int ret  = 0;

	if (unlikely(!dev->gpio[0] || !dev->gpio[1]
				|| !dev->gpio[2] || !dev->gpio[3]))
		return -1;

	dev->speed_factor = SPEED_LOW;
	dev->speed_pulse_period = SPEED_PERIOD;

	dev->is_running = 0;
	dev->step = 0;
	dev->direction = DIR_NONE;
	dev->running_steps = 0;
	dev->setting_steps = 0;
	dev->pos = 0;
	dev->run_time = 0;

	dev->timing_info = timing;
	dev->timing_info_len = ARRAY_SIZE(timing);

	spin_lock_init(&dev->_lock);

	ret = rts_init_gpios(dev);

	return ret;
}

static void ctrl_run_motor(struct step_motor_dev *motor,
		int direction, int speed, unsigned int run_steps)
{
	if (speed != SPEED_LOW && speed !=  SPEED_HIGH)
		speed = SPEED_NORMAL;

	spin_lock_irqsave(&motor->_lock, motor->_lock_flags);

	motor->direction = direction;
	motor->speed_factor = speed;
	motor->setting_steps = MAX(MIN(run_steps, motor->max_steps), 0);
	spin_unlock_irqrestore(&motor->_lock, motor->_lock_flags);

	rts_motor_info("%s setting_steps=%d, setting_speed=%d",
		motor->name, motor->setting_steps, motor->speed_factor);

	rts_run_motor(motor);
}

static int ctrl_get_position(struct step_motor_dev *motor)
{
	int pos = 0;

	spin_lock_irqsave(&motor->_lock, motor->_lock_flags);
	pos =  motor->pos + (motor->running_steps * motor->step);
	spin_unlock_irqrestore(&motor->_lock, motor->_lock_flags);

	return pos;
}

static void ctrl_stop_motor(struct step_motor_dev *motor)
{
	if (unlikely(!motor))
		return;

	rts_stop_motor(motor);
}

static int stepmotor_ctrl_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static long stepmotor_ctrl_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	int run_steps = 0;
	struct motorctrl_info info;
	struct step_motor_dev *motor;

	if (!stepmotor_ctrl) {
		rts_motor_err("step motor not init.");
		return -EFAULT;
	}

	rts_motor_debug("motor ioctl cmd=%d", cmd);

	switch (cmd) {
	case RTS_GET_MOTOR_POS_CTL:
		motor = stepmotor_ctrl->step_motors[X_AXIS];
		info.x = ctrl_get_position(motor);

		motor = stepmotor_ctrl->step_motors[Y_AXIS];
		info.y = ctrl_get_position(motor);

		if ((void *)arg != NULL) {
			ret = copy_to_user((void __user *)arg, &info,
					sizeof(struct motorctrl_info));
			if (ret) {
				ret = -EFAULT;
				return ret;
			}
		}
		break;
	case RTS_RUN_MOTOR_TIME_CTL:
		if ((void *)arg != NULL) {
			ret = copy_from_user(&info, (void __user *)arg,
					sizeof(struct motorctrl_info));
			if (ret) {
				ret = -EFAULT;
				return ret;
			}
		} else
			rts_motor_err("set speed arg is null");

		rts_motor_debug("dir=%s speed=%d run_time=%d",
				dir_to_string(info.dir),
				info.speed, info.run_time);

		if (info.dir == DIR_NONE)
			break;
		else if (info.dir == DIR_UP || info.dir == DIR_DOWN)
			motor = stepmotor_ctrl->step_motors[Y_AXIS];
		else
			motor = stepmotor_ctrl->step_motors[X_AXIS];

		run_steps = info.run_time /
			(motor->speed_pulse_period / motor->speed_factor);

		ctrl_run_motor(motor, info.dir, info.speed, run_steps);

		info.is_running = 1;
		break;
	case RTS_RUN_MOTOR_DEGREE_CTL:
		if ((void *)arg != NULL) {
			ret = copy_from_user(&info, (void __user *)arg,
					sizeof(struct motorctrl_info));
			if (ret) {
				ret = -EFAULT;
				return ret;
			}
		} else
			rts_motor_err("set degree arg is null");

		rts_motor_debug("dir=%s degree=%d speed %d",
				dir_to_string(info.dir), info.degree, info.speed);

		if (info.dir == DIR_NONE)
			break;
		else if (info.dir == DIR_UP || info.dir == DIR_DOWN)
			motor = stepmotor_ctrl->step_motors[Y_AXIS];
		else
			motor = stepmotor_ctrl->step_motors[X_AXIS];

		run_steps = info.degree * STEPS_AROUND / 360;

		ctrl_run_motor(motor, info.dir, info.speed, run_steps);

		info.is_running = 1;
		break;
	case RTS_IS_RUNNING_MOTOR_CTL:
		motor = stepmotor_ctrl->step_motors[X_AXIS];
		info.is_running = motor->is_running;

		motor = stepmotor_ctrl->step_motors[Y_AXIS];
		info.is_running |= motor->is_running;

		if ((void *)arg != NULL) {
			ret = copy_to_user((void __user *)arg, &info,
					sizeof(struct motorctrl_info));
			if (ret) {
				ret = -EFAULT;
				return ret;
			}
		}
		break;
	case RTS_STOP_MOTOR_CTL:
		if ((void *)arg != NULL) {
			ret = copy_from_user(&info, (void __user *)arg,
					sizeof(struct motorctrl_info));
			if (ret) {
				ret = -EFAULT;
				return ret;
			}
		}

		if (info.dir == DIR_NONE)
			break;
		else if (info.dir == DIR_UP || info.dir == DIR_DOWN)
			motor = stepmotor_ctrl->step_motors[Y_AXIS];
		else
			motor = stepmotor_ctrl->step_motors[X_AXIS];

		ctrl_stop_motor(motor);

		info.is_running = 0;
		break;
	default:
		ret = -ENOIOCTLCMD;
		rts_motor_err("command(0x%x) not support.", cmd);
		break;
	}

	return 0;
}

static int stepmotor_ctrl_close(struct inode *inode, struct file *filp)
{
	return 0;
}

const struct file_operations stepmotor_fops = {
	.owner = THIS_MODULE,
	.open = stepmotor_ctrl_open,
	.release = stepmotor_ctrl_close,
	.unlocked_ioctl = stepmotor_ctrl_ioctl,
};

struct miscdevice stepmotor_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = STEPMOTOR_DEVICE_NAME,
	.fops = &stepmotor_fops,
};


static int stepmotor_ctrldev_init(struct miscdevice *misc_dev)
{
	int ret = 0, i = 0;

	stepmotor_ctrl = kzalloc(sizeof(struct step_motor_ctrldev), GFP_KERNEL);

	stepmotor_ctrl->name = STEPMOTOR_DEVICE_NAME;
	stepmotor_ctrl->is_running = 0;
	stepmotor_ctrl->misc_dev = misc_dev;
	stepmotor_ctrl->step_motors = step_motor_array;
	stepmotor_ctrl->motor_nums = ARRAY_SIZE(step_motor_array);
	stepmotor_ctrl->x_pos = stepmotor_ctrl->default_x_pos = 0;
	stepmotor_ctrl->y_pos = stepmotor_ctrl->default_y_pos = 0;

	for (i = 0; i < stepmotor_ctrl->motor_nums; i++)
		rts_init_motors((stepmotor_ctrl->step_motors)[i]);

	ret = misc_register(stepmotor_ctrl->misc_dev);
	if (unlikely(ret < 0))
		rts_motor_err("register misc fail");

	rts_motor_info("successful init");

	return ret;
}

static void stepmotor_ctrl_release(void)
{
	int i = 0;

	for (i = 0; i < stepmotor_ctrl->motor_nums; i++)
		rts_stop_motor((stepmotor_ctrl->step_motors)[i]);

	for (i = 0; i < stepmotor_ctrl->motor_nums; i++)
		rts_deinit_gpios((stepmotor_ctrl->step_motors)[i]);

	misc_deregister(stepmotor_ctrl->misc_dev);

	kfree(stepmotor_ctrl);
}

static int __init stepmotor_function_init(void)
{
	int ret = 0;
	ret = stepmotor_ctrldev_init(&stepmotor_misc);

	return ret;
}

static void __exit stepmotor_function_exit(void)
{
	stepmotor_ctrl_release();
}

module_init(stepmotor_function_init);
module_exit(stepmotor_function_exit);
MODULE_AUTHOR("Neil Yan <neil_yan@realsil.com.cn>");
MODULE_DESCRIPTION("Realtek step motor driver");
MODULE_LICENSE("GPL");
