#ifndef __RTS_HR_TIMER__
#define __RTS_HR_TIMER__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/mutex.h>

void rts_hrtimer_init(struct hrtimer *timer);

void rts_hrtimer_enable(struct hrtimer *timer, ktime_t kt);

void rts_hrtimer_disable(struct hrtimer *timer);

void rts_hrtimer_forward(struct hrtimer *timer, ktime_t kt);
#endif
