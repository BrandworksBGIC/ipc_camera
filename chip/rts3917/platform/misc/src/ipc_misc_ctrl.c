#include "ipc_misc_ctrl.h"

s32 ipc_plat_misc_ctrl(IPC_PLAT_MISC_CTRL_CMD cmd, vptr req, vptr rsp)
{
    int ret = 0;
    switch (cmd) {
    case IPC_PLAT_MISC_CTRL_CMD_GET_24C02_DEV_NODE: {
        // coverity[DC.STRING_BUFFER :SUPPRESS]
        // coverity[SECURE_CODING :SUPPRESS]
        strcpy(rsp, "/dev/i2c-0");
        break;
    }
    default: {
        ret = -1;
    }
    }
    return ret;
}