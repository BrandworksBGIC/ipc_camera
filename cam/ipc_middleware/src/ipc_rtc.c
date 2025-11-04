#include "ipc_rtc.h"
#include "ipc_core.h"
#include <ipc_platform_api.h>
#include <stdio.h>

s32 ipc_rtc_init(void)
{
#define DEV_NODE_24C02 "/dev/i2c-1"
    pv8 dev_node            = DEV_NODE_24C02;
    v8 dev_node_buffer[256] = { 0 };
    if (ipc_plat_api(0)->misc_ctrl != NULL) {
        if (ipc_plat_api(0)->misc_ctrl(IPC_PLAT_MISC_CTRL_CMD_GET_24C02_DEV_NODE, NULL, dev_node_buffer) == 0) {
            dev_node = dev_node_buffer;
        }
    }

    dev_node = strrchr(dev_node, '/');
    dev_node += 1;

    printf("rtc device node %s\n", dev_node);

    ipc_exec("echo \"ds1307 0x68\" > /sys/class/i2c-adapter/%s/new_device", dev_node);

    return 0;
}

s32 ipc_rtc_get_time(void)
{
    printf("%s\n", __FUNCTION__);
    ipc_exec("hwclock -s");

    return 0;
}

s32 ipc_rtc_set_time(void)
{
    printf("%s\n", __FUNCTION__);
    ipc_exec("hwclock -w");

    return 0;
}