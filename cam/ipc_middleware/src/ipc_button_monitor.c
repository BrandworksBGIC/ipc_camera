#include "ipc_button_monitor.h"
#include "ipc_platform_api.h"

static struct {
    u8   gorun;
    u8   alive;
    vptr usr_args;
    ipc_button_trigger_f handler;
} _gh_button;

#define INTERVAL_TMS 40 

static vptr _pth_button_monitor(vptr arg)
{
    // coverity[UNUSED_VALUE :SUPPRESS]
    s32 ret = -1;
    _gh_button.alive = 1;

    struct {
        s32  platform_id;
        pcv8 name;
        s32  count;
    } buttons[] = {
        [IPC_BUTTON_RESET] = { IPC_IO_NAME_RESET_BUTTON, "Reset", 0 },
        [IPC_BUTTON_FRONT] = { IPC_IO_NAME_FRONT_BUTTON, "Front", 0 },
    };

    IPC_IO_VALUE_TYPE value = 0;

    while (_gh_button.gorun) {

        for (s32 idx = 0; idx < ARRSIZE(buttons); idx++) {
            value = 0;
            ret = ipc_plat_api(0)->io_read(buttons[idx].platform_id, &value);
            if (ret < 0) {
                continue;
            }
            
            if (value == IPC_IO_VALUE_IS_ACTIVE) {
                buttons[idx].count++;
                _gh_button.handler(_gh_button.usr_args, idx, 1, buttons[idx].count * INTERVAL_TMS); 
                ipctrace("%s: press down %d ms", buttons[idx].name, buttons[idx].count * INTERVAL_TMS);
            }
            else {
                if (buttons[idx].count > 0) {
                    _gh_button.handler(_gh_button.usr_args, idx, 0, buttons[idx].count * INTERVAL_TMS);
                    buttons[idx].count = 0;
                    ipctrace("%s: release the key", buttons[idx].name);
                }
            }
        }
        
        ipc_msleep(INTERVAL_TMS);
    }

    _gh_button.alive = 0;

    return NULL;
}

s32 ipc_button_monitor_init(ipc_button_trigger_f handler, vptr usr_args)
{
    clog_init("btn", "button_monitor");

    if (!handler) return IPC_INVALID_ARGS;

    _gh_button.gorun = 1;
    _gh_button.alive = 0;
    _gh_button.usr_args = usr_args;
    _gh_button.handler  = handler;
    s32 ret = ipc_create_thread("ipc_button", _pth_button_monitor, NULL, 128 * 1024, 0);
    if (ret < 0) {
        ipcfatal("Create thread failed! retcode=[%d]", ret);
        return ret;
    }
    ipcinfo("Init complete!");
    return ret;
}

void ipc_button_monitor_uninit(s32 is_wait)
{
    _gh_button.gorun = 0;

    if (!is_wait) return ;

    while(_gh_button.alive) ipc_msleep(100);
    ipcinfo("Exit complete!");
}
