

#include <ipc_log.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "ipc_m433.h"

#include "m433_ioctl.h"

#define M433_DEVICE_PATH "/dev/m433"

static s32 __g_m433_fd = -1;

s32 ipc_m433_init(void)
{
    // Open M433 device

    clog_init("m433", "m433");

    __g_m433_fd = open(M433_DEVICE_PATH, O_RDWR);
    if (__g_m433_fd < 0) {
        ipcerror("Failed to open M433 device %s: %s", M433_DEVICE_PATH, strerror(errno));
        return -ENODEV;
    }

    ipcinfo("M433 device initialized successfully");
    return 0;
}

s32 ipc_m433_set_mac_addr(pv8 mac)
{
    s32 ret;
    char* mac_str = (char*)mac;

    if (__g_m433_fd < 0) {
        ipcerror("M433 device not initialized");
        return -ENODEV;
    }

    if (!mac) {
        ipcerror("Invalid MAC address pointer");
        return -EINVAL;
    }

    // Set MAC address via IOCTL (expecting 12-character hex string)
    ret = ioctl(__g_m433_fd, M433_IOCTL_SET_MAC, mac_str);
    if (ret < 0) {
        ipcerror("Failed to set M433 MAC address: %s", strerror(errno));
        return ret;
    }

    ipcinfo("M433 MAC address set to: %.2s:%.2s:%.2s:%.2s:%.2s:%.2s", mac_str, mac_str + 2, mac_str + 4, mac_str + 6, mac_str + 8, mac_str + 10);

    return 0;
}

s32 ipc_m433_send_alarm(void)
{
    s32 ret;

    if (__g_m433_fd < 0) {
        ipcerror("M433 device not initialized");
        return -ENODEV;
    }

    // Trigger alarm transmission
    ret = ioctl(__g_m433_fd, M433_IOCTL_TRIGGER_TX);
    if (ret < 0) {
        ipcerror("Failed to trigger M433 alarm transmission: %s", strerror(errno));
        return ret;
    }

    ipcinfo("M433 alarm transmission triggered successfully");
    return 0;
}