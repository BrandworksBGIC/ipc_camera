#include <asm/irq.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
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
#include <linux/gpio.h>
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

/* Compatibility for GPIOF_EXPORT deprecation */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 5, 0)
/* GPIOF_EXPORT was deprecated in kernel 5.5+ */
#define GPIOF_EXPORT_DEPRECATED 0
#else
/* Keep old definition for older kernels */
#define GPIOF_EXPORT_DEPRECATED GPIOF_EXPORT
#endif

#include "ipc_gpio_dri.h"

#define DEVICE_NAME "ipc-gpio-dri"


static long gpio_ioctl(struct file* file, unsigned int cmd, unsigned long arg)
{
    void __user* argp        = (void __user*)arg;
    int ret                  = 0;
    struct ipc_gpio_attr attr = { 0 };

    if (copy_from_user(&attr, argp, sizeof(struct ipc_gpio_attr))) {
        printk("ipc_gpio_attr copy_from_user error!!!\n");
        ret = -EFAULT;
        goto exit;
    }

    switch (cmd) {
        case IOCTL_IO_INIT: {
            char name[8];
            unsigned long flags = GPIOF_EXPORT_DEPRECATED;
            sprintf(name, "gpio%d", attr.gpio_num);
            if (attr.gpio_dir == 0) {
                flags |= GPIOF_DIR_IN;
            } else {
                flags |= attr.value ? GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW;
            }
            gpio_request_one(attr.gpio_num, flags, name);
            printk("ipc_gpio init :%s:%d\n", name, attr.gpio_dir);

            break;
        }

        case IOCTL_IO_UNINIT: {
            gpio_free(attr.gpio_num);
            printk("ipc_gpio uninit :gpio%d\n", attr.gpio_num);
            break;
        }
        case IOCTL_IO_SET: {
            gpio_set_value(attr.gpio_num, attr.value);
            // printk("ipc_gpio_set:%d:%d\n", attr.gpio_num, attr.value);
            break;
        }
        case IOCTL_IO_GET: {
            ret = gpio_get_value(attr.gpio_num);
            // printk("ipc_gpio_set:%d:%d\n", attr.gpio_num, attr.value);
            attr.value = ret;
            break;
        }
        default:
            ret = -ENOENT;
            break;
    }

    if (copy_to_user(argp, &attr, sizeof(struct ipc_gpio_attr))) {
        printk("ipc_gpio_attr copy_to_user error!!!\n");
    }

exit:
    return ret;
}

static struct file_operations dev_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = gpio_ioctl,
};

static struct miscdevice misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = DEVICE_NAME,
    .fops  = &dev_fops,
};

static int __init dev_init(void)
{
    int ret;

    ret = misc_register(&misc);

    printk(DEVICE_NAME " initialized\n");

    return ret;
}

static void __exit dev_exit(void)
{
    misc_deregister(&misc);
}

module_init(dev_init);
module_exit(dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ipc");
MODULE_DESCRIPTION("GPIO control for IPC Device");