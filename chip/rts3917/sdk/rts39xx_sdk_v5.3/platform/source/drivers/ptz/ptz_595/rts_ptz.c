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
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/power/max8903_charger.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/jiffies.h>

#include "rts_ptz.h"

#define MAX(x, y) ((x) >= (y) ? (x) : (y))
#define MIN(x, y) ((x) <= (y) ? (x) : (y))


#define PTZ_DEVICE_NAME		"rts-ptz"

#if (MOTOR_BEAT == 8)
/*
 * 4-phase-8-step
 * 0b0001->0b0011->0b0010->0b0110->
 * 0b0100->0b1100->0b1000->0b1001
 */
static const int timing[] = { 1, 3, 2, 6, 4, 12, 8, 9 };
static unsigned char beat_orders[3][8] = {
	/* counter clockwise */
	{7, 6, 5, 4, 3, 2, 1, 0},
	{0},
	/* clockwise */
	{0, 1, 2, 3, 4, 5, 6, 7}
};
#elif (MOTOR_BEAT == 4)
/*
 * 4-phase-4-step
 * 0b0001->0b0010->0b0100->0b1000
 */
static const int timing[] = { 1, 2, 4, 8 };
static unsigned char beat_orders[3][4] = {
	/* counter clockwise */
	{3, 2, 1, 0},
	{0},
	/* clockwise */
	{0, 1, 2, 3}
};
#endif

static struct gpio_group io_group;

static struct step_motor_dev x_stepmotor = {
	.name = "x_motor",
};

static struct step_motor_dev y_stepmotor = {
	.name = "y_motor",
};

static struct ptz_device ptz_dev;

static inline void serial_io_input(int value)
{
	gpio_set_value(io_group.gpio[DS], value);
	gpio_set_value(io_group.gpio[SH_CP], 1);
	gpio_set_value(io_group.gpio[SH_CP], 0);
}

static inline void serial_io_output(void)
{
	gpio_set_value(io_group.gpio[ST_CP], 1);
	gpio_set_value(io_group.gpio[ST_CP], 0);
}

static void ports_shutdown(void)
{
	int i = 0;

	for (i = 0; i < SERIAL_IO_PERIOD; i++)
		serial_io_input(0);

	serial_io_output();
}

static inline bool is_motor_skip(struct step_motor_dev *motor,
						unsigned int times)
{
	int skip = 0;

	switch (motor->speed) {
	case (SPEED_LOW):
		skip = times % 3;
		break;
	case (SPEED_NORMAL):
		skip = times % 2;
		break;
	case (SPEED_HIGH):
		skip = times % 1;
	}

	return !!skip;
}

static inline bool is_motor_finish(struct step_motor_dev *motor)
{
	bool ret = false;

	if (motor->running_steps >= motor->setting_steps) {
		rts_ptz_debug("%s finish running", motor->name);

		motor->running_pos += ((motor->setting_steps - motor->compensate) * motor->direction);
		motor->running_pos = MAX(MIN(motor->running_pos, motor->max_steps), 0);

		motor->is_running = 0;
		motor->direction = DIR_NONE;
		motor->running_steps = 0;
		motor->setting_steps = 0;

		ret = true;
	}

	return ret;
}

static void do_drive_motor(struct timer_list *data)
{
	struct step_motor_dev *xmotor = NULL;
	struct step_motor_dev *ymotor = NULL;
	int i = 0, j = 0;
	int val = 0;
	int xbeat = 0, ybeat = 0;
	int xfinish = 0, yfinish = 0;
	static unsigned int times = 1;

	unsigned char (*xorder)[MOTOR_BEAT] = &(beat_orders[1]);
	unsigned char (*yorder)[MOTOR_BEAT] = &(beat_orders[1]);

	xmotor = &x_stepmotor;
	ymotor = &y_stepmotor;

	mutex_lock(&ptz_dev.mutex);
	xfinish = is_motor_finish(xmotor);
	yfinish = is_motor_finish(ymotor);
	mutex_unlock(&ptz_dev.mutex);
	if (unlikely(xfinish) &&
			unlikely(yfinish)) {
		times = 1;
		ports_shutdown();
		if (ptz_dev.need_wake) {
			ptz_dev.wait_condition = 1;
			wake_up_interruptible(&ptz_dev.wait);
		}
		return;
	}

	if (!xmotor->is_running) {
		for (j = 0; j < 4; j++)
			serial_io_input(0);
	} else {
		if (is_motor_skip(xmotor, times)) {
			for (j = 0; j < 4; j++)
				serial_io_input(0);
			goto x_out;
		}

		xbeat = xorder[xmotor->direction][xmotor->running_steps % MOTOR_BEAT];

		for (i = 0; i < MOTOR_PHASE; i++) {
			val = (xmotor->timing_info[xbeat] >> i) & 0x1;

			serial_io_input(val);
		}

		xmotor->running_steps++;
		xmotor->beat = xbeat;
	}

x_out:
	if (!ymotor->is_running) {
		for (j = 0; j < 4; j++)
			serial_io_input(0);
	} else {
		if (is_motor_skip(ymotor, times)) {
			for (j = 0; j < 4; j++)
				serial_io_input(0);
			goto y_out;
		}

		ybeat = yorder[ymotor->direction][ymotor->running_steps % MOTOR_BEAT];

		for (i = 0; i < MOTOR_PHASE; i++) {
			val = (ymotor->timing_info[ybeat] >> i) & 0x1;

			serial_io_input(val);
		}

		ymotor->running_steps++;
		ymotor->beat = ybeat;
	}

y_out:
	serial_io_output();

	times++;
	mod_timer(&ptz_dev.pulse_timer, jiffies + usecs_to_jiffies(1000));
}

static void deinit_gpios(struct gpio_group *io_group)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(io_group->gpio); i++)
		gpio_free(io_group->gpio[i]);
}

static int init_gpios(struct gpio_group *io_group)
{
	int i = 0;
	int ret = 0;

	for (i = 0; i < ARRAY_SIZE(io_group->gpio); i++) {
		ret = gpio_request(io_group->gpio[i], io_group->gpio_label[i]);
		if (ret) {
			rts_ptz_err("request gpio[%d] fail",
					io_group->gpio[i]);
			goto out;
		} else
			rts_ptz_info("request gpio[%d] success",
					io_group->gpio[i]);

		ret = gpio_direction_output(io_group->gpio[i], 0);
		if (ret) {
			rts_ptz_err("set gpio[%d] output fail",
					io_group->gpio[i]);
			goto out;
		} else
			rts_ptz_info("set gpio[%d] output success",
					io_group->gpio[i]);
	}

	return 0;
out:
	for (i = 0; i < ARRAY_SIZE(io_group->gpio); i++)
		gpio_free(io_group->gpio[i]);

	return ret;
}

static void run_motor(struct step_motor_dev *xmotor,
				struct step_motor_dev *ymotor)
{
	mod_timer(&ptz_dev.pulse_timer, jiffies + usecs_to_jiffies(10));
}

static void motor_run(void)
{
	struct step_motor_dev *xmotor = NULL;
	struct step_motor_dev *ymotor = NULL;

	xmotor = &x_stepmotor;
	ymotor = &y_stepmotor;

	xmotor->setting_steps =
		MAX(MIN(xmotor->setting_steps, xmotor->max_steps), 0);
	ymotor->setting_steps =
		MAX(MIN(ymotor->setting_steps, ymotor->max_steps), 0);
	xmotor->is_running = 1;
	ymotor->is_running = 1;

	/* to avoid lose steps, we shell figure out the exact next beat */
	if (xmotor->direction == DIR_RIGHT) {
		xmotor->running_steps = xmotor->beat + 1;
		xmotor->setting_steps += (xmotor->beat + 1);
		xmotor->compensate = xmotor->beat + 1;
	} else {
		xmotor->running_steps = (MOTOR_BEAT - 1) - xmotor->beat + 1;
		xmotor->setting_steps += ((MOTOR_BEAT - 1) - xmotor->beat + 1);
		xmotor->compensate = (MOTOR_BEAT - 1) - xmotor->beat + 1;
	}

	if (ymotor->direction == DIR_UP) {
		ymotor->running_steps = ymotor->beat + 1;
		ymotor->setting_steps += (ymotor->beat + 1);
		ymotor->compensate = ymotor->beat + 1;
	} else {
		ymotor->running_steps = (MOTOR_BEAT - 1) - ymotor->beat + 1;
		ymotor->setting_steps += ((MOTOR_BEAT - 1) - ymotor->beat + 1);
		ymotor->compensate = (MOTOR_BEAT - 1) - ymotor->beat + 1;
	}

	rts_ptz_info("%s setting_steps=%d",
			xmotor->name, xmotor->setting_steps);
	rts_ptz_info("%s setting_steps=%d",
			ymotor->name, ymotor->setting_steps);

	run_motor(xmotor, ymotor);
}

static void motor_stop(void)
{
	struct step_motor_dev *xmotor = &x_stepmotor;
	struct step_motor_dev *ymotor = &y_stepmotor;

	mutex_lock(&ptz_dev.mutex);
	if (xmotor->is_running)
		xmotor->setting_steps = xmotor->running_steps;
	if (ymotor->is_running)
		ymotor->setting_steps = ymotor->running_steps;
	mutex_unlock(&ptz_dev.mutex);

	while ((xmotor->is_running) || (ymotor->is_running))
		msleep(5);

	ports_shutdown();
	del_timer(&ptz_dev.pulse_timer);
}

static void motor_move_to_base_point(struct step_motor_dev *xmotor,
				struct step_motor_dev *ymotor)
{
	xmotor->direction = DIR_LEFT;
	xmotor->speed = SPEED_HIGH;
	xmotor->setting_steps = xmotor->max_steps;
	ymotor->direction = DIR_DOWN;
	ymotor->speed = SPEED_HIGH;
	ymotor->setting_steps = ymotor->max_steps;

	ptz_dev.need_wake = 1;
	ptz_dev.wait_condition = 0;

	rts_ptz_info("motor move to origin point\n");
	motor_run();

	wait_event_interruptible(ptz_dev.wait, ptz_dev.wait_condition);
	ptz_dev.need_wake = 0;
	ptz_dev.wait_condition = 0;

	xmotor->running_pos = 0;
	ymotor->running_pos = 0;
}

static void motor_move_to_middle_point(struct step_motor_dev *xmotor,
				struct step_motor_dev *ymotor)
{
	xmotor->direction = DIR_RIGHT;
	xmotor->speed = SPEED_HIGH;
	xmotor->setting_steps = xmotor->max_steps / 2;
	ymotor->direction = DIR_UP;
	ymotor->speed = SPEED_HIGH;
	ymotor->setting_steps = ymotor->max_steps / 2;

	ptz_dev.need_wake = 1;
	ptz_dev.wait_condition = 0;

	rts_ptz_info("motor move to middle point\n");
	motor_run();

	wait_event_interruptible(ptz_dev.wait, ptz_dev.wait_condition);
	ptz_dev.need_wake = 0;
	ptz_dev.wait_condition = 0;
}

static void motor_move_reset_pos(void)
{
	struct step_motor_dev *xmotor = NULL;
	struct step_motor_dev *ymotor = NULL;

	xmotor = &x_stepmotor;
	ymotor = &y_stepmotor;

	motor_move_to_base_point(xmotor, ymotor);
	motor_move_to_middle_point(xmotor, ymotor);
}

static void motor_move_to_pos(bool block)
{
	struct step_motor_dev *xmotor = NULL;
	struct step_motor_dev *ymotor = NULL;

	xmotor = &x_stepmotor;
	ymotor = &y_stepmotor;

	if (xmotor->setting_pos >= xmotor->running_pos) {
		xmotor->direction = DIR_RIGHT;
		xmotor->setting_steps =
				xmotor->setting_pos - xmotor->running_pos;
	} else {
		xmotor->direction = DIR_LEFT;
		xmotor->setting_steps =
				xmotor->running_pos - xmotor->setting_pos;
	}

	if (ymotor->setting_pos >= ymotor->running_pos) {
		ymotor->direction = DIR_UP;
		ymotor->setting_steps =
				ymotor->setting_pos - ymotor->running_pos;
	} else {
		ymotor->direction = DIR_DOWN;
		ymotor->setting_steps =
				ymotor->running_pos - ymotor->setting_pos;
	}

	rts_ptz_info("xmotor:pos:%d, dir:%d, steps:%d\n", xmotor->running_pos,
			xmotor->direction, xmotor->setting_steps);
	rts_ptz_info("ymotor:pos:%d, dir:%d, steps:%d\n", ymotor->running_pos,
			ymotor->direction, ymotor->setting_steps);

	if (block) {
		ptz_dev.need_wake = 1;
		ptz_dev.wait_condition = 0;
	}

	motor_run();

	if (block) {
		wait_event_interruptible(ptz_dev.wait, ptz_dev.wait_condition);
		ptz_dev.need_wake = 0;
		ptz_dev.wait_condition = 0;
	}
}

static int ptz_ctrl_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int motor_info_check(struct step_motor_dev *xmotor,
				struct step_motor_dev *ymotor)
{
	int pos;

	if ((xmotor->direction != DIR_NONE) &&
			(xmotor->direction != DIR_UP) &&
			(xmotor->direction != DIR_DOWN) &&
			(xmotor->direction != DIR_RIGHT) &&
			(xmotor->direction != DIR_LEFT))
		goto err_inval;
	if ((ymotor->direction != DIR_NONE) &&
			(ymotor->direction != DIR_UP) &&
			(ymotor->direction != DIR_DOWN) &&
			(ymotor->direction != DIR_RIGHT) &&
			(ymotor->direction != DIR_LEFT))
		goto err_inval;
	if ((xmotor->speed != SPEED_NORMAL) &&
			(xmotor->speed != SPEED_LOW) &&
			(xmotor->speed != SPEED_HIGH))
		goto err_inval;
	if ((ymotor->speed != SPEED_NORMAL) &&
			(ymotor->speed != SPEED_LOW) &&
			(ymotor->speed != SPEED_HIGH))
		goto err_inval;

	xmotor->setting_steps =
		(xmotor->direction == DIR_NONE) ? 0 : xmotor->setting_steps;
	ymotor->setting_steps =
		(ymotor->direction == DIR_NONE) ? 0 : ymotor->setting_steps;

	pos = xmotor->setting_steps * xmotor->direction + xmotor->running_pos;
	if ((pos < 0) || (pos > xmotor->max_steps)) {
		rts_ptz_info("xmotor will get to the end, setting_steps[%d], running_pos[%d], direciton[%d]",
				xmotor->setting_steps, xmotor->running_pos, xmotor->direction);
		if (pos < 0)
			xmotor->setting_steps = xmotor->running_pos;
		else
			xmotor->setting_steps =
					xmotor->max_steps - xmotor->running_pos;
	}
	pos = ymotor->setting_steps * ymotor->direction + ymotor->running_pos;
	if ((pos < 0) || (pos > ymotor->max_steps)) {
		rts_ptz_info("ymotor will get to the end, setting_steps[%d], running_pos[%d], direciton[%d]",
				ymotor->setting_steps, ymotor->running_pos, ymotor->direction);

		if (pos < 0)
			ymotor->setting_steps = ymotor->running_pos;
		else
			ymotor->setting_steps =
					ymotor->max_steps - ymotor->running_pos;
	}

	return 0;

err_inval:
	return -EINVAL;
}

static long ptz_ctrl_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct ptzctrl_info info;
	struct step_motor_dev *xmotor, *ymotor;

	xmotor = &x_stepmotor;
	ymotor = &y_stepmotor;

	rts_ptz_debug("motor ioctl cmd=%d", cmd);

	switch (cmd) {
	case RTS_PTZ_IOC_DRIVE:
		if ((void *)arg != NULL) {
			ret = copy_from_user(&info, (void __user *)arg,
					sizeof(struct ptzctrl_info));
			if (ret)
				goto err_fault;
		}

		if (xmotor->is_running || ymotor->is_running)
			goto err_busy;

		xmotor->direction = info.xmotor_info.dir;
		xmotor->speed = info.xmotor_info.speed;
		xmotor->setting_steps = info.xmotor_info.steps;
		ymotor->direction = info.ymotor_info.dir;
		ymotor->speed = info.ymotor_info.speed;
		ymotor->setting_steps = info.ymotor_info.steps;

		if (motor_info_check(xmotor, ymotor))
			goto err_inval;

		if (info.block) {
			ptz_dev.need_wake = 1;
			ptz_dev.wait_condition = 0;
		}

		motor_run();

		if (info.block) {
			wait_event_interruptible(ptz_dev.wait,
					ptz_dev.wait_condition);
			ptz_dev.need_wake = 0;
			ptz_dev.wait_condition = 0;
		}
		break;
	case RTS_PTZ_IOC_RUN:
		/* user specify direction&speed, motor would stop at max_steps */
		if ((void *)arg != NULL) {
			ret = copy_from_user(&info, (void __user *)arg,
					sizeof(struct ptzctrl_info));
			if (ret)
				goto err_fault;
		}

		if (xmotor->is_running || ymotor->is_running)
			goto err_busy;

		xmotor->direction = info.xmotor_info.dir;
		xmotor->speed = info.xmotor_info.speed;
		xmotor->setting_steps = xmotor->max_steps;
		ymotor->direction = info.ymotor_info.dir;
		ymotor->speed = info.ymotor_info.speed;
		ymotor->setting_steps = ymotor->max_steps;

		motor_info_check(xmotor, ymotor);

		if (info.block) {
			ptz_dev.need_wake = 1;
			ptz_dev.wait_condition = 0;
		}

		motor_run();

		if (info.block) {
			wait_event_interruptible(ptz_dev.wait,
					ptz_dev.wait_condition);
			ptz_dev.need_wake = 0;
			ptz_dev.wait_condition = 0;
		}
		break;
	case RTS_PTZ_IOC_STOP:
		if (xmotor->is_running || ymotor->is_running)
			motor_stop();
		break;
	case RTS_PTZ_IOC_RESET:
		if (xmotor->is_running || ymotor->is_running)
			goto err_busy;
		motor_move_reset_pos();
		break;
	case RTS_PTZ_IOC_G_POS:
	case RTS_PTZ_IOC_G_INFO:
		info.xmotor_info.dir = xmotor->direction;
		info.xmotor_info.speed = xmotor->speed;
		info.xmotor_info.pos = xmotor->running_pos;
		info.xmotor_info.is_running = xmotor->is_running;
		info.xmotor_info.max_steps = xmotor->max_steps;
		info.xmotor_info.max_degrees = xmotor->max_degrees;

		info.ymotor_info.dir = ymotor->direction;
		info.ymotor_info.speed = ymotor->speed;
		info.ymotor_info.pos = ymotor->running_pos;
		info.ymotor_info.is_running = ymotor->is_running;
		info.ymotor_info.max_steps = ymotor->max_steps;
		info.ymotor_info.max_degrees = ymotor->max_degrees;

		if ((void *)arg != NULL) {
			ret = copy_to_user((void __user *)arg, &info,
					sizeof(struct ptzctrl_info));
			if (ret)
				goto err_fault;
		}
		break;
	case RTS_PTZ_IOC_S_POS:
		if ((void *)arg != NULL) {
			ret = copy_from_user(&info, (void __user *)arg,
					sizeof(struct ptzctrl_info));
			if (ret)
				goto err_fault;
		}

		if (xmotor->is_running || ymotor->is_running)
			goto err_busy;

		xmotor->setting_pos =
			MIN(info.xmotor_info.pos, xmotor->max_steps);
		ymotor->setting_pos =
			MIN(info.ymotor_info.pos, ymotor->max_steps);
		xmotor->speed = info.xmotor_info.speed;
		ymotor->speed = info.ymotor_info.speed;
		if ((xmotor->speed != SPEED_NORMAL) &&
				(xmotor->speed != SPEED_LOW) &&
				(xmotor->speed != SPEED_HIGH))
			xmotor->speed = SPEED_HIGH;
		if ((ymotor->speed != SPEED_NORMAL) &&
				(ymotor->speed != SPEED_LOW) &&
				(ymotor->speed != SPEED_HIGH))
			ymotor->speed = SPEED_HIGH;

		motor_move_to_pos(info.block);
		break;
	case RTS_PTZ_IOC_IS_RUNNING:
		info.xmotor_info.is_running = xmotor->is_running;
		info.ymotor_info.is_running = ymotor->is_running;

		if ((void *)arg != NULL) {
			ret = copy_to_user((void __user *)arg, &info,
					sizeof(struct ptzctrl_info));
			if (ret)
				goto err_fault;
		}
		break;
	default:
		ret = -ENOIOCTLCMD;
		rts_ptz_err("command(0x%x) not support.", cmd);
		break;
	}

	return 0;

err_fault:
	return -EFAULT;
err_inval:
	return -EINVAL;
err_busy:
	return -EAGAIN;
}

static int ptz_ctrl_close(struct inode *inode, struct file *filp)
{
	return 0;
}

static int init_motors(struct platform_device *pdev,
				struct step_motor_dev *xmotor,
				struct step_motor_dev *ymotor)
{
	int shcp_gpio = 0, stcp_gpio = 0, ds_gpio = 0;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *child_np = NULL;
	int ret = 0;

	xmotor->is_running = 0;
	xmotor->direction = DIR_NONE;
	xmotor->running_steps = 0;
	xmotor->setting_steps = 0;
	xmotor->running_pos = 0;
	xmotor->setting_pos = 0;
	xmotor->beat = 0;
	xmotor->compensate = 0;
	xmotor->timing_info = timing;
	xmotor->timing_info_len = ARRAY_SIZE(timing);

	ymotor->is_running = 0;
	ymotor->direction = DIR_NONE;
	ymotor->running_steps = 0;
	ymotor->setting_steps = 0;
	ymotor->running_pos = 0;
	ymotor->setting_pos = 0;
	ymotor->beat = 0;
	ymotor->compensate = 0;
	ymotor->timing_info = timing;
	ymotor->timing_info_len = ARRAY_SIZE(timing);

	shcp_gpio = of_get_named_gpio(np, "shcp-gpio", 0);
	if (shcp_gpio < 0) {
		ret = shcp_gpio;
		goto err_of;
	}
	stcp_gpio = of_get_named_gpio(np, "stcp-gpio", 0);
	if (stcp_gpio < 0) {
		ret = stcp_gpio;
		goto err_of;
	}
	ds_gpio = of_get_named_gpio(np, "ds-gpio", 0);
	if (ds_gpio < 0) {
		ret = ds_gpio;
		goto err_of;
	}

	child_np = of_get_child_by_name(np, "x-motor");
	if (IS_ERR(child_np)) {
		ret = PTR_ERR(child_np);
		goto err_of;
	}
	ret = of_property_read_u32(child_np, "max-steps", &xmotor->max_steps);
	if (ret < 0)
		goto err_of;
	ret = of_property_read_u32(child_np, "max-degrees",
							&xmotor->max_degrees);
	if (ret < 0)
		goto err_of;
	child_np = of_get_child_by_name(np, "y-motor");
	if (IS_ERR(child_np)) {
		ret = PTR_ERR(child_np);
		goto err_of;
	}
	ret = of_property_read_u32(child_np, "max-steps", &ymotor->max_steps);
	if (ret < 0)
		goto err_of;
	ret = of_property_read_u32(child_np, "max-degrees",
							&ymotor->max_degrees);
	if (ret < 0)
		goto err_of;

	io_group.gpio[SH_CP] = shcp_gpio;
	io_group.gpio[ST_CP] = stcp_gpio;
	io_group.gpio[DS] = ds_gpio;
	init_gpios(&io_group);
	ports_shutdown();

	ptz_dev.xmotor = xmotor;
	ptz_dev.ymotor = ymotor;

	ptz_dev.need_wake = 0;
	ptz_dev.wait_condition = 0;
	init_waitqueue_head(&ptz_dev.wait);

	timer_setup(&ptz_dev.pulse_timer, do_drive_motor, 0);

	mutex_init(&ptz_dev.mutex);
err_of:
	return ret;
}


const struct file_operations ptz_fops = {
	.owner = THIS_MODULE,
	.open = ptz_ctrl_open,
	.release = ptz_ctrl_close,
	.unlocked_ioctl = ptz_ctrl_ioctl,
};

struct miscdevice ptz_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = PTZ_DEVICE_NAME,
	.fops = &ptz_fops,
};


static int ptz_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = init_motors(pdev, &x_stepmotor, &y_stepmotor);
	if (ret < 0)
		return ret;

	ret = misc_register(&ptz_misc);
	if (unlikely(ret < 0))
		rts_ptz_err("register misc fail");

	rts_ptz_info("ptz driver probe done\n");

	return ret;
}

static int ptz_remove(struct platform_device *pdev)
{
	motor_stop();

	mutex_destroy(&ptz_dev.mutex);

	deinit_gpios(&io_group);

	misc_deregister(&ptz_misc);

	rts_ptz_info("ptz driver removed\n");

	return 0;
}

static const struct of_device_id ptz_ids[] = {
	{ .compatible = "realtek,rts3917-ptz",},
	{}
};
MODULE_DEVICE_TABLE(of, ptz_ids);

static struct platform_driver ptz_driver = {
	.driver = {
		.name = "rts-ptz",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ptz_ids),
	},
	.probe = ptz_probe,
	.remove = ptz_remove,
};

module_platform_driver(ptz_driver);
MODULE_AUTHOR("Neil Yan <neil_yan@realsil.com.cn>");
MODULE_AUTHOR("Steve Liu <steve_liu@realsil.com.cn>");
MODULE_DESCRIPTION("Realtek step motor driver");
MODULE_LICENSE("GPL");
