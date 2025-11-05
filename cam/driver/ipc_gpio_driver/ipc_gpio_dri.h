#ifndef __IPC_GPIO_DRI_H__
#define __IPC_GPIO_DRI_H__

#ifdef __cplusplus
extern "C" {
#endif

#define GPIO_IOC_MAGIC 'C'

#define IOCTL_IO_INIT           _IOW(GPIO_IOC_MAGIC, 110, unsigned int)
#define IOCTL_IO_UNINIT         _IOW(GPIO_IOC_MAGIC, 111, unsigned int)
#define IOCTL_IO_SET            _IOW(GPIO_IOC_MAGIC, 112, unsigned int)
#define IOCTL_IO_GET            _IOW(GPIO_IOC_MAGIC, 113, unsigned int)

struct ipc_gpio_attr {
    int gpio_num;
    int gpio_dir;
    int value;
    int lock;
};

#ifdef __cplusplus
}
#endif

#endif //__IPC_GPIO_DRI_H__