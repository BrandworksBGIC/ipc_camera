#include "ipc_timer.h"

#include <linux/hrtimer.h>

static struct hrtimer g_hrtimer[2];
static int g_timeout_10us[2]                = { 100, 100 };
static enum hrtimer_restart g_is_restart[2] = { HRTIMER_NORESTART, HRTIMER_NORESTART };
static void* g_usr_arg                      = NULL;
static ipc_timer_handler_t g_motor_handler;

static void timer_handler_null(int index, void* arg)
{
}

static enum hrtimer_restart timer_hr_interrupt_0(struct hrtimer* hrtimer)
{
    if (g_is_restart[0] == HRTIMER_NORESTART) {
        goto end;
    }

    hrtimer_forward_now(hrtimer, ktime_set(0, g_timeout_10us[0] * 10000));

    g_motor_handler(0, g_usr_arg);

end:
    return g_is_restart[0];
}

static enum hrtimer_restart timer_hr_interrupt_1(struct hrtimer* hrtimer)
{
    if (g_is_restart[1] == HRTIMER_NORESTART) {
        goto end;
    }

    hrtimer_forward_now(hrtimer, ktime_set(0, g_timeout_10us[1] * 10000));

    g_motor_handler(1, g_usr_arg);

end:
    return g_is_restart[1];
}

int ipc_timer_init(int timer_num)
{
    g_motor_handler = timer_handler_null;

    hrtimer_init(&g_hrtimer[0], CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
    g_hrtimer[0].function = timer_hr_interrupt_0;

    hrtimer_init(&g_hrtimer[1], CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
    g_hrtimer[1].function = timer_hr_interrupt_1;

    return 0;
}

int ipc_timer_set_period(int timer_index, int time10us)
{
    g_timeout_10us[timer_index] = time10us;
    return 0;
}

int ipc_timer_start(int timer_index)
{
    g_is_restart[timer_index] = HRTIMER_RESTART;

    hrtimer_start(&g_hrtimer[timer_index], ktime_set(0, g_timeout_10us[timer_index] * 10000), HRTIMER_MODE_ABS);

    return 0;
}

int ipc_timer_stop(int timer_index)
{
    g_is_restart[timer_index] = HRTIMER_NORESTART;
    return 0;
}

int ipc_timer_set_timeout_cb(ipc_timer_handler_t cb, void* arg)
{
    g_usr_arg       = arg;
    g_motor_handler = cb;
    return 0;
}

int ipc_timer_uninit(void)
{
    hrtimer_cancel(&g_hrtimer[0]);
    hrtimer_cancel(&g_hrtimer[1]);
    return 0;
}