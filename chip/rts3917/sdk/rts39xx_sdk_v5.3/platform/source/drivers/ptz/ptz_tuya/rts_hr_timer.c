#include "rts_hr_timer.h"

void rts_hrtimer_init(struct hrtimer *timer)
{
    hrtimer_init(timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
}

//hrtimer启动
void rts_hrtimer_enable(struct hrtimer *timer, ktime_t kt)
{
    hrtimer_start(timer, kt, HRTIMER_MODE_REL);
}    

//hrtimer关闭
void rts_hrtimer_disable(struct hrtimer *timer)
{
    hrtimer_cancel(timer);
}

//时间推移
void rts_hrtimer_forward(struct hrtimer *timer, ktime_t kt)
{
    hrtimer_forward_now(timer, kt);
}
