#include "ipc_middleware_config.h"

#include "ipc_platform_api.h"
#include "ipc_status_led.h"

static struct {
    u8 gorun;     ///< Thread survival control
    u8 alive;     ///< Thread survival feedback
    u8 master_sw; ///< Indicator light master switch
    u8 break_off; ///< Interruption, return to the starting line
    struct {
        s8 led_sw;                 ///< led switch
        s8 led_reset;              ///< led parameters are reset (concurrently set)
        s32 effective_tms;         ///< The starting time of the validity period
        s32 light_on_tms;          ///< The on-state duty cycle time
        s32 light_off_tms;         ///< The off-state duty cycle time
        s32 ctrl_left_tms;         ///< Time in milliseconds until the next flip
    } led_ctrl[IPC_STATUS_LED_NUM]; ///< Status light control
} _gh_sled[1] = {{
   .master_sw = 1,                ///< Switch defaults to on
   .led_ctrl = {
        [0 ... IPC_STATUS_LED_NUM-1] = {.led_sw = -1 },
    },
}};

static s32 _led_set(s32 led, u8 sw)
{
    if (_gh_sled->led_ctrl[led].led_sw == sw)
        return 0;
    _gh_sled->led_ctrl[led].led_sw = sw;

    s32 ret          = 0;
    s32 _g_led_map[] = {
        [IPC_STATUS_LED_A] = IPC_IO_NAME_STATUS_INDICATOR_A,
        [IPC_STATUS_LED_B] = IPC_IO_NAME_STATUS_INDICATOR_B,
    };

    ipcdebug("led[%d]=[%d]", led, sw);
    ret = ipc_plat_api(0)->io_write(_g_led_map[led], _gh_sled->led_ctrl[led].led_sw);

    return ret;
}

static void _flashing(void)
{
    _gh_sled->break_off = 0;

    s32 led = 0;
    for (led = 0; led < IPC_STATUS_LED_NUM; led++) {

        _led_set(led, 0); // First restore to the off state

        _gh_sled->led_ctrl[led].ctrl_left_tms
            = _gh_sled->led_ctrl[led].effective_tms > 0 ? _gh_sled->led_ctrl[led].effective_tms : 0; // The first effective time
    }

    s32 sleep_tms = 0;
    s32 min_tms   = 0;

    while (_gh_sled->gorun && _gh_sled->master_sw && !_gh_sled->break_off) { // Survival and enablement and not interrupted
        min_tms = 250;                                                       // The default maximum granularity is 250ms
        for (led = 0; led < IPC_STATUS_LED_NUM; led++) {                      // Scan all led lights
            if (_gh_sled->led_ctrl[led].led_reset) {                         // led asynchronous reset
                _gh_sled->led_ctrl[led].led_reset     = 0;
                _gh_sled->led_ctrl[led].ctrl_left_tms = 0;
            }
            if (_gh_sled->led_ctrl[led].ctrl_left_tms < 0)
                continue;                                       // <0 is considered to have taken effect and no further control is required
            _gh_sled->led_ctrl[led].ctrl_left_tms -= sleep_tms; // Calculate the time left until the led takes effect
            if (_gh_sled->led_ctrl[led].ctrl_left_tms <= 0) {   // When the effective time arrives
                if (_gh_sled->led_ctrl[led].light_on_tms
                    && _gh_sled->led_ctrl[led].light_off_tms) {     // Flash (ON and OFF times are both set, then it is a blink mode)
                    _led_set(led, !_gh_sled->led_ctrl[led].led_sw); // Flip the led
                    _gh_sled->led_ctrl[led].ctrl_left_tms
                        = _gh_sled->led_ctrl[led].led_sw ? _gh_sled->led_ctrl[led].light_on_tms : _gh_sled->led_ctrl[led].light_off_tms;
                } else { // If there is an on tms then it is on, and if not, it is off
                    _led_set(led, !!_gh_sled->led_ctrl[led].light_on_tms);
                    _gh_sled->led_ctrl[led].ctrl_left_tms = -1; // No need to continue switching
                }
            }
            if (_gh_sled->led_ctrl[led].ctrl_left_tms > 0) { // Calculate the minimum scheduling time for the next effective time
                min_tms = MIN(min_tms, _gh_sled->led_ctrl[led].ctrl_left_tms);
            }
        }
        sleep_tms = min_tms;
        ipctrace("sleep %dms", sleep_tms);
        ipc_msleep(sleep_tms > 0 ? sleep_tms : 0); // To prevent possible negative numbers caused by concurrency
    }
}

static vptr _pth_status_led(vptr arg)
{
    _gh_sled->alive = 1;

    s32 led = 0;
    while (_gh_sled->gorun) {

        if (!_gh_sled->master_sw) {
            for (led = 0; led < IPC_STATUS_LED_NUM; led++) {
                _led_set(led, 0); // Off state
            }
            ipc_msleep(250);
            continue;
        }
        _flashing();
    }

    _gh_sled->alive = 0;

    return NULL;
}

void ipc_status_led_set_all(ipc_status_led_ctrl_p ctrls, u32 ctrl_num)
{
    for (s32 led = 0, idx = 0; led < IPC_STATUS_LED_NUM; led++) { // Traverse all status lights
        for (idx = 0; idx < ctrl_num && ctrls[idx].led != led; idx++)
            ; // Find the corresponding light setting
        if (idx < ctrl_num) {
            _gh_sled->led_ctrl[led].light_on_tms  = ctrls[idx].light_on_tms;
            _gh_sled->led_ctrl[led].light_off_tms = ctrls[idx].light_off_tms;
            _gh_sled->led_ctrl[led].effective_tms = ctrls[idx].effective_tms;
        } else { // Clear settings
            memset(&_gh_sled->led_ctrl[led], 0, sizeof(_gh_sled->led_ctrl[led]));
        }
    }
    _gh_sled->break_off = 1;
}

void ipc_status_led_set_one(ipc_status_led_ctrl_p ctrl)
{
    for (s32 led = 0; led < IPC_STATUS_LED_NUM; led++) { // Traverse all status lights
        if (ctrl->led != led)
            continue;
        if (_gh_sled->led_ctrl[led].light_on_tms != ctrl->light_on_tms || _gh_sled->led_ctrl[led].light_off_tms != ctrl->light_off_tms) {
            _gh_sled->led_ctrl[led].light_on_tms  = ctrl->light_on_tms;
            _gh_sled->led_ctrl[led].light_off_tms = ctrl->light_off_tms;
            _gh_sled->led_ctrl[led].led_reset     = 1;
        }
    }
}

void ipc_status_led_switch(u8 sw)
{
    _gh_sled->master_sw = !!sw;
}

s32 ipc_status_led_init(void)
{
    if (_gh_sled->alive)
        return IPC_EXIST;

    clog_init("led", "status led");
    _gh_sled->gorun = 1;
    s32 ret         = ipc_create_thread("ipc_status_led", _pth_status_led, NULL, 64 * 1024, 0);
    if (ret < 0) {
        ipcfatal("Create thread failed! retcode=[%d]", ret);
        return ret;
    }
    ipcinfo("Init complete!");
    return ret;
}

void ipc_status_led_uninit(u8 is_wait)
{
    _gh_sled->gorun = 0;

    if (!is_wait)
        return;

    while (_gh_sled->alive)
        ipc_msleep(100);
    ipcinfo("Exit complete!");
}
