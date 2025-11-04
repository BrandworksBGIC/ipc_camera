
#include <asm/irq.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/version.h>

#include <asm/atomic.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 11, 12)
#include <linux/uaccess.h>
#else
#include <asm/uaccess.h>
#endif
#include <asm/unistd.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>

#include "ipc_motor_gpio.h"
#include "ipc_motor.h"
#include "ipc_timer.h"

static char* g_motor_gpioV_seq = "0,1,2,3";
module_param(g_motor_gpioV_seq, charp, S_IRUGO);
MODULE_PARM_DESC(g_motor_gpioV_seq, "select the motor gpio vertical sequence");

static char* g_motor_gpioH_seq = "0,1,2,3";
module_param(g_motor_gpioH_seq, charp, S_IRUGO);
MODULE_PARM_DESC(g_motor_gpioH_seq, "select the motor gpio h sequence");

static int g_motor_init_at_model_build = 0;
struct step_motor_ctrl {
    int gpio_group_index;             // GPIO group index
    int per_zero_point_val;           // Previous zero point value
    int cur_step;                     // Represents the current position of the motor
    int min_step;                     // Minimum step
    int max_step;                     // Maximum step
    int limit_min_step;               // Minimum limit step
    int limit_max_step;               // Maximum limit step
    int init_pos_step;                // Initial position step
    unsigned int run_time_step_count; // Running time step count
    int remain_run_step;              // Remaining steps to run
    int motor_speed;                  // Motor speed
    IPC_MOTOR_DIR direction;           // Direction of rotation
    IPC_MOTOR_STATUS motor_status;     // Motor status
    spinlock_t spin_lock;             // Spin lock
    unsigned long spinirq_lock_flags; // Flags for spin lock IRQ save/restore
} g_motor_dev_ctrl[IPC_MOTOR_INDEX_NUM];

#define motor_lock(dev_array, index) spin_lock_irqsave(&(dev_array[index].spin_lock), (dev_array[index].spinirq_lock_flags));
#define motor_unlock(dev_array, index) spin_unlock_irqrestore(&(dev_array[index].spin_lock), (dev_array[index].spinirq_lock_flags));

#define IS_NOT_SUPPORT_ZERO_POINT_GET_STATUS(dev) ((dev)->per_zero_point_val < 0)

static int _check_motor_is_in_zero_point(struct step_motor_ctrl* dev)
{
    int in_zero_point = 0;
    int val           = ipc_motor_gpio_get_zero_point_val(dev->gpio_group_index);
    if (val < 0) {
        return -1;
    }

    if (dev->direction == IPC_MOTOR_DIR_CLOCKWISE) {
        in_zero_point = (dev->per_zero_point_val && !val); // Falling edge
    } else {
        in_zero_point = (!dev->per_zero_point_val && val); // Rising edge
    }

    dev->per_zero_point_val = val;

    return in_zero_point;
}

static IPC_MOTOR_STATUS motor_dev_move_one_step(struct step_motor_ctrl* dev)
{
    static const unsigned char timing[2][8] = { /* 0b0001->0b0011->0b0010->0b0110->0b0100->0b1100->0b1000->0b1001 */
                                                /* Clockwise */
                                                { 1, 3, 2, 6, 4, 12, 8, 9 },
                                                /* Counter-clockwise */
                                                { 9, 8, 12, 4, 6, 2, 3, 1 }
    };
    int i           = 0;
    int val         = 0;
    int order_index = 0;

    // Range limits apply only during self-test
    if (g_motor_init_at_model_build) {
        if (dev->direction == IPC_MOTOR_DIR_CLOCKWISE) {
            // Clockwise, reaching maximum step

            dev->cur_step++;
            if (dev->cur_step >= dev->max_step) {
                if (IS_NOT_SUPPORT_ZERO_POINT_GET_STATUS(dev)) {
                    printk("[%d] reached max step [%d]\n", dev->gpio_group_index, dev->cur_step);
                    dev->cur_step        = dev->max_step;
                    dev->remain_run_step = 0;
                    return IPC_MOTOR_STATUS_STOP; // Stop
                } else {
                    // Exceeding the maximum resets to zero
                    dev->cur_step = dev->min_step;
                }
            }

        } else {
            // Counter-clockwise, reaching minimum step

            dev->cur_step--;
            if (dev->cur_step <= dev->min_step) {
                if (IS_NOT_SUPPORT_ZERO_POINT_GET_STATUS(dev)) {
                    printk("[%d] reached min step [%d]\n", dev->gpio_group_index, dev->cur_step);
                    dev->cur_step        = dev->min_step;
                    dev->remain_run_step = 0;
                    return IPC_MOTOR_STATUS_STOP; // Stop
                } else {
                    // Exceeding the minimum reaches the maximum position
                    dev->cur_step = dev->max_step;
                }
            }
        }
    }

    dev->run_time_step_count++;
    dev->remain_run_step--;

    order_index = dev->run_time_step_count % 8;

    for (i = 0; i < IPC_MOTOR_PIN_NUM; i++) {
        val = (timing[dev->direction][order_index] >> i) & 0x1;

        ipc_motor_gpio_set(dev->gpio_group_index, i, val);
    }

    if (dev->remain_run_step <= 0) {
        printk("[%d][%d] finished spinning\n", dev->gpio_group_index, dev->direction);
        return IPC_MOTOR_STATUS_STOP; // Stop
    }

    return IPC_MOTOR_STATUS_RUNNING;
}

static int motor_dev_io_down(struct step_motor_ctrl* dev)
{
    int i = 0;

    for (i = 0; i < IPC_MOTOR_PIN_NUM; i++) {
        ipc_motor_gpio_set(dev->gpio_group_index, i, 0);
    }

    printk("%s\n", __func__);

    return 0;
}

static void motor_enter_stop_status(struct step_motor_ctrl* dev)
{
    motor_dev_io_down(dev);

    dev->motor_status    = IPC_MOTOR_STATUS_STOP;
    dev->remain_run_step = 0;
}

static int motor_dev_move(struct step_motor_ctrl* dev)
{
    IPC_MOTOR_STATUS ret = 0;

    switch (dev->motor_status) {
        case IPC_MOTOR_STATUS_RUNNING:
        case IPC_MOTOR_STATUS_MODEL_BUILDING_HALF_CIRCLE: {

            ret = motor_dev_move_one_step(dev);

            if (ret == IPC_MOTOR_STATUS_STOP) {
                motor_enter_stop_status(dev);
                dev->min_step = dev->limit_min_step;
                dev->max_step = dev->limit_max_step;
            }

            if (_check_motor_is_in_zero_point(dev) > 0) {
                // Based on the self-test direction, reaching the zero point should be the maximum step; update this position each time to reduce
                // drift.
                dev->cur_step = dev->max_step;
                printk("motor %d in zero_point\n", dev->gpio_group_index);
            }

            break;
        }
        case IPC_MOTOR_STATUS_MODEL_BUILDING_FULL_CIRCLE: {
            ret = motor_dev_move_one_step(dev);

            if (ret == IPC_MOTOR_STATUS_STOP || _check_motor_is_in_zero_point(dev) > 0) {
                dev->motor_status    = IPC_MOTOR_STATUS_MODEL_BUILDING_HALF_CIRCLE;
                dev->direction       = IPC_MOTOR_DIR_COUNTERCLOCKWISE;
                dev->remain_run_step = dev->max_step - dev->init_pos_step;
            }
            break;
        }
        case IPC_MOTOR_STATUS_AUTOMATIC_CRUISE: {
            ret = motor_dev_move_one_step(dev);

            if (ret == IPC_MOTOR_STATUS_STOP) {

                dev->remain_run_step = dev->max_step;

                if (dev->remain_run_step == 0) {
                    dev->remain_run_step = 2048;
                }

                dev->direction = (dev->direction == IPC_MOTOR_DIR_COUNTERCLOCKWISE) ? IPC_MOTOR_DIR_CLOCKWISE : IPC_MOTOR_DIR_COUNTERCLOCKWISE;
            }
        }
        default:
            break;
    }

    ret = (dev->motor_status == IPC_MOTOR_STATUS_STOP) ? 1 : 0;

    return ret;
}

static void motor_dev_driver_handler(int timer_index, void* arg)
{
    struct step_motor_ctrl* dev_array = (struct step_motor_ctrl*)arg;

    if (motor_dev_move(&dev_array[timer_index])) {
        ipc_timer_stop(timer_index);
    }
}

static int motor_dev_init(struct step_motor_ctrl* dev, int model_build, int gpio_group_index)
{
    dev->cur_step  = 0;
    dev->direction = IPC_MOTOR_DIR_CLOCKWISE;

    dev->motor_status     = (model_build) ? IPC_MOTOR_STATUS_MODEL_BUILDING_FULL_CIRCLE : IPC_MOTOR_STATUS_STOP;
    dev->gpio_group_index = gpio_group_index;

    spin_lock_init(&(dev->spin_lock));

    if (dev->motor_status == IPC_MOTOR_STATUS_MODEL_BUILDING_FULL_CIRCLE) {
        dev->remain_run_step = dev->max_step; // Full circle state requires a full circle
        return 1;
    } else {
        dev->remain_run_step = 0;
    }

    return 0;
}

static long motor_ioctl_cmd_init(struct ipc_motor_attr* attr)
{
    int i = 0;
    printk("speed[%d:%d]\n", attr->speed[0], attr->speed[1]);
    printk("max_step[%d:%d]\n", attr->max_step[0], attr->max_step[1]);
    printk("limit_min_step[%d:%d]\n", attr->limit_min_step[0], attr->limit_min_step[1]);
    printk("limit_max_step[%d:%d]\n", attr->limit_max_step[0], attr->limit_max_step[1]);
    printk("model_build:%d\n", attr->model_build);
    printk("ptz_product_type:%d\n", attr->ptz_product_type);
    printk("motor gpio seq[V:%s, H:%s]\n", g_motor_gpioV_seq, g_motor_gpioH_seq);

    ipc_motor_gpio_init(attr->ptz_product_type, g_motor_gpioV_seq, g_motor_gpioH_seq);

    ipc_timer_set_timeout_cb(motor_dev_driver_handler, &g_motor_dev_ctrl);

    for (i = 0; i < (sizeof(g_motor_dev_ctrl) / sizeof(g_motor_dev_ctrl[0])); i++) {
        struct step_motor_ctrl* dev = &g_motor_dev_ctrl[i];
        if (attr->speed[i] < 100) {
            attr->speed[i] = 100;
        }
        dev->motor_speed = attr->speed[i];
        ipc_timer_set_period(i, attr->speed[i]);

        dev->init_pos_step = (attr->init_pos_step[i] >= 0) ? attr->init_pos_step[i] : (attr->max_step[i] / 2);

        dev->limit_min_step = attr->limit_min_step[i];
        dev->limit_max_step = attr->limit_max_step[i];
        dev->max_step       = attr->max_step[i];
        dev->min_step       = 0;

        // If the minimum limit step is less than the minimum step or greater than the maximum step, use the minimum step as the limit
        if ((dev->limit_min_step <= dev->min_step) || (dev->limit_min_step >= dev->max_step)) {
            dev->limit_min_step = dev->min_step;
        }

        // If the maximum limit step is greater than the maximum step or less than the minimum step, use the maximum step as the limit
        if ((dev->limit_max_step >= dev->max_step) || (dev->limit_max_step <= dev->min_step)) {
            dev->limit_max_step = dev->max_step;
        }

        if (ipc_motor_gpio_get_zero_point_val(i) >= 0) {
            dev->limit_min_step = 0;
            dev->limit_max_step = 4096; // The product test was incorrectly configured, so we hard-code the parameters for a 64 reduction ratio, 4096
                                        // steps per revolution motor
        }

        // If the initial position is not within the two limit ranges, set it to the middle position
        if ((dev->init_pos_step > dev->limit_max_step) || (dev->init_pos_step < dev->limit_min_step)) {
            dev->init_pos_step = (dev->limit_max_step + dev->limit_min_step) / 2;
        }

        // Get the initial level state
        dev->per_zero_point_val = ipc_motor_gpio_get_zero_point_val(i);

        if (motor_dev_init(dev, attr->model_build, i)) {
            if (attr->model_build == 2) {
                dev->motor_status    = IPC_MOTOR_STATUS_STOP;
                dev->remain_run_step = 0;

                // Avoid ineffective limits if not run before
                dev->min_step = dev->limit_min_step;
                dev->max_step = dev->limit_max_step;

            } else if (attr->model_build) {
                ipc_timer_start(i);
            }
        }
    }

    g_motor_init_at_model_build = attr->model_build;

    return 0;
}

static void motor_ioctl_cmd_uninit(void)
{
    int i                       = 0;
    struct step_motor_ctrl* dev = NULL;

    printk("uninit\n");

    for (i = 0; i < IPC_MOTOR_INDEX_NUM; i++) {
        dev = &g_motor_dev_ctrl[i];

        motor_lock(g_motor_dev_ctrl, i);

        ipc_timer_stop(i);

        motor_enter_stop_status(dev);

        motor_unlock(g_motor_dev_ctrl, i);
    }
}

static long motor_ioctl_cmd_run_steps(struct step_motor_ctrl* dev, struct ipc_motor_step* step)
{
    int ret = 0;

    // Range limits apply only during self-test
    if (g_motor_init_at_model_build) {
        if ((dev->motor_status == IPC_MOTOR_STATUS_MODEL_BUILDING_FULL_CIRCLE) || (dev->motor_status == IPC_MOTOR_STATUS_MODEL_BUILDING_HALF_CIRCLE)) {
            ret = -EBUSY;
            goto exit;
        }

        do {

            if (!IS_NOT_SUPPORT_ZERO_POINT_GET_STATUS(dev)) {
                break;
            }

            if (step->direction == IPC_MOTOR_DIR_CLOCKWISE) {
                // Clockwise, reaching maximum step
                if (dev->cur_step >= dev->max_step) {
                    printk("set step error motor [%d] reached max step [%d]\n", dev->gpio_group_index, dev->cur_step);
                    ret = -ENOSPC;
                    goto exit;
                }
            } else {
                // Counter-clockwise, reaching minimum step
                if (dev->cur_step <= dev->min_step) {
                    printk("set step error motor [%d] reached min step [%d]\n", dev->gpio_group_index, dev->cur_step);
                    ret = -ENOSPC;
                    goto exit;
                }
            }

        } while (0);
    }

    if (step->step > 0) {
        dev->remain_run_step = step->step;
        dev->direction       = step->direction;
        dev->motor_status    = IPC_MOTOR_STATUS_RUNNING;

        ipc_timer_start(dev->gpio_group_index);
    } else {
        motor_enter_stop_status(dev);
    }

exit:
    return ret;
}

static long motor_ioctl_goto_spec_pos(struct step_motor_ctrl* dev, struct ipc_motor_step* step)
{
    int distance = 0;
    int ret      = 0;
    if (g_motor_init_at_model_build) {
        if (dev->motor_status == IPC_MOTOR_STATUS_MODEL_BUILDING_FULL_CIRCLE) {
            ret = -EBUSY;
            goto exit;
        }
    } else {
        ret = -EINVAL;
        goto exit;
    }

    distance = step->step - dev->cur_step;

    if (distance < 0) {
        dev->direction = IPC_MOTOR_DIR_COUNTERCLOCKWISE;
    } else if (distance > 0) {
        dev->direction = IPC_MOTOR_DIR_CLOCKWISE;
    }

    if (distance != 0) {
        distance = abs(distance);

        if (!IS_NOT_SUPPORT_ZERO_POINT_GET_STATUS(dev)) {
            // If the distance is more than half, it can turn in the opposite direction for a shorter path
            if (distance > dev->max_step / 2) {
                distance       = dev->max_step - distance;
                dev->direction = (dev->direction == IPC_MOTOR_DIR_CLOCKWISE) ? IPC_MOTOR_DIR_COUNTERCLOCKWISE : IPC_MOTOR_DIR_CLOCKWISE;
            }
        }

        dev->remain_run_step = distance;
        dev->motor_status    = IPC_MOTOR_STATUS_RUNNING;

        printk("goto specific position [%d]\n", dev->remain_run_step);
        ipc_timer_start(dev->gpio_group_index);
    }

exit:
    return ret;
}

static long motor_ioctl_set_motor_status(struct step_motor_ctrl* dev, IPC_MOTOR_STATUS status)
{
    long ret = 0;
    if (status == IPC_MOTOR_STATUS_AUTOMATIC_CRUISE) {

        dev->motor_status = status;

        dev->remain_run_step = dev->max_step;

        if (dev->remain_run_step == 0) {
            dev->remain_run_step = 2048;
        }

        ipc_timer_start(dev->gpio_group_index);
    } else if (status == IPC_MOTOR_STATUS_STOP) {
        motor_enter_stop_status(dev);
    } else {
        ret = -EINVAL;
    }

    return ret;
}

static long motor_ioctl(struct file* file, unsigned int cmd, unsigned long arg)
{
    void __user* argp = (void __user*)arg;
    int ret           = 0;

    switch (cmd) {
        case IPC_IOCTL_MOTOR_INIT: {
            struct ipc_motor_attr attr;

            if (copy_from_user(&attr, argp, sizeof(struct ipc_motor_attr))) {
                printk("ipc_motor_attr copy_from_user error!!!\n");
                ret = -EFAULT;
                goto exit;
            }

            ret = motor_ioctl_cmd_init(&attr);

            break;
        }
        case IPC_IOCTL_MOTOR_UNINIT: {

            motor_ioctl_cmd_uninit();

            // ipc_motor_gpio_uninit();
            break;
        }
        case IPC_IOCTL_MOTOR_RUN_STEPS: {
            struct ipc_motor_step step   = { 0 };
            int motor_index             = 0;
            struct step_motor_ctrl* dev = NULL;

            if (copy_from_user(&step, argp, sizeof(struct ipc_motor_step))) {
                printk("ipc_motor_step copy_from_user error!!!\n");
                ret = -EFAULT;
                goto exit;
            }

            motor_index = step.motor_index;
            dev         = &g_motor_dev_ctrl[motor_index];

            printk("motor_index:%d\n", motor_index);
            printk("direction:%d\n", step.direction);
            printk("step:%d\n", step.step);

            motor_lock(g_motor_dev_ctrl, motor_index);

            ret = motor_ioctl_cmd_run_steps(dev, &step);

            motor_unlock(g_motor_dev_ctrl, motor_index);

            break;
        }
        case IPC_IOCTL_MOTOR_GET_REMAINING_RUNNING_STEPS: {
            struct ipc_motor_step step = { 0 };
            int motor_index           = 0;

            if (copy_from_user(&step, argp, sizeof(struct ipc_motor_step))) {
                printk("ipc_motor_step copy_from_user error!!!\n");
                ret = -EFAULT;
                goto exit;
            }

            motor_index = step.motor_index;

            motor_lock(g_motor_dev_ctrl, motor_index);

            step.step      = g_motor_dev_ctrl[motor_index].remain_run_step;
            step.direction = g_motor_dev_ctrl[motor_index].direction;

            motor_unlock(g_motor_dev_ctrl, motor_index);

            if (copy_to_user(argp, &step, sizeof(struct ipc_motor_step))) {
                printk("ipc_motor_step copy_to_user error!!!\n");
            }

            printk("get remaining running step\n");

            break;
        }
        case IPC_IOCTL_MOTOR_SET_SPEED: {
            struct ipc_motor_speed speed = { 0 };
            if (copy_from_user(&speed, argp, sizeof(struct ipc_motor_speed))) {
                printk("ipc_motor_speed copy_from_user error!!!\n");
                ret = -EFAULT;
                goto exit;
            }

            printk("set speed [%d:%d]\n", speed.motor_index, speed.speed);
            if (speed.speed > 450 || speed.speed < 80) {
                ret = -EINVAL;
                goto exit;
            }

            g_motor_dev_ctrl[speed.motor_index].motor_speed = speed.speed;

            ipc_timer_set_period(speed.motor_index, speed.speed);

            break;
        }
        case IPC_IOCTL_MOTOR_DUMP_INFO: {
            int i = 0;
            for (i = 0; i < IPC_MOTOR_INDEX_NUM; i++) {
                struct step_motor_ctrl* dev = &g_motor_dev_ctrl[i];

                printk("[%d]cur_step:%d\n", i, dev->cur_step);
                printk("[%d]direction:%d\n", i, dev->direction);
                printk("[%d]gpio_group_index:%d\n", i, dev->gpio_group_index);
                printk("[%d]max_step:%d\n", i, dev->max_step);
                printk("[%d]min_step:%d\n", i, dev->min_step);
                printk("[%d]limit_min_step:%d\n", i, dev->limit_min_step);
                printk("[%d]limit_max_step:%d\n", i, dev->limit_max_step);
                printk("[%d]motor_status:%d\n", i, dev->motor_status);
                printk("[%d]remain_run_step:%d\n", i, dev->remain_run_step);
                printk("[%d]run_time_step_count:%d\n", i, dev->run_time_step_count);
                printk("[%d]motor_speed:%d\n", i, dev->motor_speed);
            }
            break;
        }

        case IPC_IOCTL_MOTOR_GET_CUR_STEPS: {
            struct ipc_motor_step step = { 0 };
            int motor_index           = 0;

            if (!g_motor_init_at_model_build) {
                ret = -EINVAL;
                goto exit;
            }

            if (copy_from_user(&step, argp, sizeof(struct ipc_motor_step))) {
                printk("ipc_motor_step copy_from_user error!!!\n");
                ret = -EFAULT;
                goto exit;
            }

            motor_index = step.motor_index;

            motor_lock(g_motor_dev_ctrl, motor_index);

            step.step = g_motor_dev_ctrl[motor_index].cur_step;

            step.run_time_step_count = g_motor_dev_ctrl[motor_index].run_time_step_count;

            motor_unlock(g_motor_dev_ctrl, motor_index);

            if (copy_to_user(argp, &step, sizeof(struct ipc_motor_step))) {
                printk("ipc_motor_step copy_to_user error!!!\n");
            }

            printk("get cur step\n");
            break;
        }

        case IPC_IOCTL_MOTOR_SET_CUR_STEPS: {
            struct ipc_motor_step step = { 0 };
            int motor_index           = 0;

            if (!g_motor_init_at_model_build) {
                ret = -EINVAL;
                goto exit;
            }

            if (copy_from_user(&step, argp, sizeof(struct ipc_motor_step))) {
                printk("ipc_motor_step copy_from_user error!!!\n");
                ret = -EFAULT;
                goto exit;
            }

            motor_index = step.motor_index;

            motor_lock(g_motor_dev_ctrl, motor_index);

            g_motor_dev_ctrl[motor_index].cur_step = step.step;

            g_motor_dev_ctrl[motor_index].run_time_step_count = step.run_time_step_count;

            motor_unlock(g_motor_dev_ctrl, motor_index);

            if (copy_to_user(argp, &step, sizeof(struct ipc_motor_step))) {
                printk("ipc_motor_step copy_to_user error!!!\n");
            }

            printk("set cur step\n");
            break;
        }

        case IPC_IOCTL_MOTOR_GOTO_SPEC_POS: {
            struct ipc_motor_step step = { 0 };
            int motor_index           = 0;

            struct step_motor_ctrl* dev = NULL;

            if (copy_from_user(&step, argp, sizeof(struct ipc_motor_step))) {
                printk("ipc_motor_step copy_from_user error!!!\n");
                ret = -EFAULT;
                goto exit;
            }

            motor_index = step.motor_index;
            dev         = &g_motor_dev_ctrl[motor_index];

            motor_lock(g_motor_dev_ctrl, motor_index);

            ret = motor_ioctl_goto_spec_pos(dev, &step);

            motor_unlock(g_motor_dev_ctrl, motor_index);

            break;
        }
        case IPC_IOCTL_MOTOR_GET_STATUS: {
            struct ipc_motor_status status = { 0 };
            struct step_motor_ctrl* dev   = NULL;

            if (copy_from_user(&status, argp, sizeof(struct ipc_motor_status))) {
                printk("ipc_motor_status copy_from_user error!!!\n");
                ret = -EFAULT;
                goto exit;
            }

            dev           = &g_motor_dev_ctrl[status.motor_index];
            status.status = dev->motor_status;

            if (copy_to_user(argp, &status, sizeof(struct ipc_motor_status))) {
                printk("ipc_motor_status copy_to_user error!!!\n");
            }

            break;
        }

        case IPC_IOCTL_MOTOR_SET_STATUS: {
            struct ipc_motor_status status = { 0 };
            struct step_motor_ctrl* dev   = NULL;

            if (copy_from_user(&status, argp, sizeof(struct ipc_motor_status))) {
                printk("ipc_motor_status copy_from_user error!!!\n");
                ret = -EFAULT;
                goto exit;
            }

            dev = &g_motor_dev_ctrl[status.motor_index];

            motor_lock(g_motor_dev_ctrl, status.motor_index);

            motor_ioctl_set_motor_status(dev, status.status);

            motor_unlock(g_motor_dev_ctrl, status.motor_index);

            break;
        }
        default:
            ret = -EINVAL;
            break;
    }

exit:
    return ret;
}

static int motor_release(struct inode* node, struct file* file)
{
    motor_ioctl_cmd_uninit();

    return 0;
}

static int motor_open(struct inode* node, struct file* file)
{
    motor_ioctl_cmd_uninit();

    return 0;
}

#define DEVICE_NAME "ipc-motor"

static struct file_operations dev_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = motor_ioctl,
    .release        = motor_release,
    .open           = motor_open,
};

static struct miscdevice misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = DEVICE_NAME,
    .fops  = &dev_fops,
};

static int __init ipc_motor_init(void)
{
    printk("ipc_motor_driver version 1.0.3\n");

    ipc_timer_init(2);

    misc_register(&misc);

    return 0;
}

static void __exit ipc_motor_exit(void)
{
    ipc_timer_stop(0);
    ipc_timer_stop(1);

    ipc_timer_uninit();

    ipc_motor_gpio_uninit();

    misc_deregister(&misc);
}

module_init(ipc_motor_init);
module_exit(ipc_motor_exit);

MODULE_LICENSE("GPL");