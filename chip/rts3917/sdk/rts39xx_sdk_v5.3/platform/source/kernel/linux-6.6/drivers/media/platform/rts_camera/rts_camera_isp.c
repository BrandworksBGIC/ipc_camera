// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025 Realtek Semiconductor Corp. All rights reserved.
 *
 * THIS SOFTWARE IS CONFIDENTIAL AND PROPRIETARY TO REALTEK SEMICONDUCTOR
 * CORP. DISCLOSURE, REPRODUCTION, REDISTRIBUTION, IN WHOLE OR IN PART, OF
 * THIS WORK AND ITS DERIVATIVES WITHOUT EXPRESS PERMISSION IS PROHIBITED.
 *
 * REALTEK SEMICONDUCTOR CORP. RESERVES THE RIGHT TO UPDATE, MODIFY, OR
 * DISCONTINUE THIS SOFTWARE AT ANY TIME WITHOUT NOTICE. THIS SOFTWARE IS
 * PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define TAG "ISP"
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/i2c.h>
#include <linux/list.h>
#include <linux/random.h>

#include "linux/rts_camera_isp.h"
#include "rts_camera_isp_regs.h"
#include "rts_camera.h"
#include "rts_camera_zoom.h"
#include "rts_isp_mem.h"

#define RTS_ISP_DRV_NAME		"rts_isp"
#define RTS_ISP_DEV_NAME		"rtsisp"

#define MAX_SYNC_DELAY 4

#define STATIS_BUF_NUM 4
#define STATIS_MSG_SIZE (sizeof(struct rts_isp_msg_hdr) + \
			 sizeof(struct rts_isp_statis_data))

#define MOD_CONTROL v4l2_fourcc('c', 't', 'r', 'l')

struct rtscam_isp_message {
	struct mutex *dev_lock;
	struct completion completion;

	u32 sequence;
	int msg_pending;
	size_t read_remain;
	u8 read_buf[256];
	off_t write_offset;
	u8 write_buf[256];

	wait_queue_head_t wq;
	u32 timeout;
};

struct rtscam_isp_snr_power {
	enum rts_isp_snr_pwr_type type;
	const char *name;
	void *handle; /* can be power, gpio, clk */
	int current_value;
};

struct rtscam_isp_statis_node {
	dma_addr_t addr;
	struct rts_isp_statis_data data;
	struct list_head list;
};

struct rtscam_isp_statis {
	atomic_t valid;

	struct rtscam_mem_info *mem_info;
	struct rts_isp_statis_info info;
	void *virt_addr;
	u32 total_size;

	spinlock_t lock;
	struct list_head idle;
	struct list_head busy;
	struct list_head done;
	struct list_head user;

	size_t read_remain;
	u8 msg_buf[STATIS_MSG_SIZE];
};

struct rtscam_isp_sync_item {
	struct list_head list;
	struct rts_isp_sync_reg reg;
	struct rts_isp_i2c_info i2c_info;
};

struct rtscam_isp_sync_delay {
	/* no item for RTS_ISP_INT_NONE */
	struct list_head irq[_MAX_RTS_ISP_INT - 1];
};

struct rtscam_isp_sync {
	struct mutex lock;
	atomic_t irq;
	struct rtscam_isp_sync_delay delay[MAX_SYNC_DELAY + 1];
	u32 index;
	struct list_head idle;
	u32 idle_num;
};

struct rtscam_isp_mem_item {
	struct list_head list;
	dma_addr_t phy_addr;
	size_t size;
};

struct rtscam_isp {
	struct device *dev;
	struct rtscam_ge_device *gdev;
	atomic_t user_count;
	struct mutex lock;

	atomic_t frame_count;
	atomic_t statis_count;

	int irq;
	void __iomem *base;
	long io_start;
	long io_size;

	struct {
		struct clk *isp_clk;
		int isp_clk_refcnt;
		struct clk *inf_clk;
		int inf_clk_refcnt;
		struct clk *mipiout_clk;
		int mipiout_clk_refcnt;

		u32 isp_clk_fix;
	};
	struct {
		struct pinctrl *p;
		struct pinctrl_state *default_state;
		struct pinctrl_state *dvp_state;
		struct pinctrl_state *mipi_state;
	} pins;

	struct rtscam_zoom_isp zoom_isp;
	int (*hook)(void *master, int id, void *arg);
	atomic_t active;
	bool need_reset_amba;

	void *pwrctl_gpio_handle;
	struct rtscam_isp_snr_power power[_MAX_SNR_POWER_TYPE];
	struct rtscam_isp_message message;
	struct rtscam_isp_statis statis;
	struct rtscam_isp_sync sync;
	struct rtscam_mem_info *mem_info;
	struct list_head mem_list;
	struct i2c_adapter *adapter;
	struct rtscam_region statis_cfg;

	struct completion data_start_completion;
	struct completion frame_end_completion;

	struct reset_control *sysmem;
	struct reset_control *mipiout_sysmem;
	struct reset_control *isp_reset;
	struct reset_control *mipi_reset;
	struct reset_control *mipiout_reset;
	struct v4l2_fract current_fps;
	struct v4l2_fract dynamic_fps;

	u32 is_fpga:1;
	u32 has_pmu:1;
	u32 stopping:1;
	u8 tnr_bit;
};


struct rtscam_isp_ioctl_info {
	unsigned int cmd;
	int (*func)(struct rtscam_isp *isp, void *arg);
};

#define ISP_IOCTL_INFO(isp_cmd, isp_func) \
	[_IOC_NR(isp_cmd)] = { \
		.cmd = isp_cmd, \
		.func = isp_func, \
	}

static inline u32 rtscam_isp_read_reg(struct rtscam_isp *isp, u32 offset)
{
	return ioread32(isp->base + offset);
}

static inline void rtscam_isp_write_reg(struct rtscam_isp *isp,
					u32 value, u32 offset)
{
	return iowrite32(value, isp->base + offset);
}

static bool fps_zero(const struct v4l2_fract *fps)
{
	return !fps->denominator || !fps->numerator;
}

static bool fps_equal(const struct v4l2_fract *f1, const struct v4l2_fract *f2)
{
	if (fps_zero(f1) && fps_zero(f2))
		return true;
	return (!fps_zero(f1) && !fps_zero(f2) &&
		f1->numerator * f2->denominator ==
		f1->denominator * f2->numerator);
}

static inline int power_is_gpio(const struct rtscam_isp_snr_power *power)
{
	return power->type <= SNR_PWDN_GPIO;
}

static inline int power_is_clk(const struct rtscam_isp_snr_power *power)
{
	return power->type == SNR_HCLK;
}

static inline int power_is_voltage(const struct rtscam_isp_snr_power *power)
{
	return power->type >= SNR_IO_POWER;
}

static int rtscam_isp_snr_power_check(struct rtscam_isp *isp,
				      struct rtscam_isp_snr_power *power)
{
	if (power_is_voltage(power) && !isp->has_pmu)
		return 0;
	if (!power->handle)
		return -EINVAL;
	if (IS_ERR(power->handle))
		return PTR_ERR(power->handle);
	return 0;
}

static int rtscam_isp_snr_power_init(struct rtscam_isp *isp,
				     struct rtscam_isp_snr_power *power,
				     u32 type)
{
	static const char * const type_str[] = {
		"rst", "pwdn", "hclk", "io", "analog", "core",
	};

	if (!power)
		return -EINVAL;

	power->type = type;
	power->name = type_str[type];
	power->handle = NULL;
	power->current_value = 0;

	return 0;
}

static int rtscam_isp_snr_power_get(struct rtscam_isp *isp,
				    struct rtscam_isp_snr_power *power)
{
	if (!power)
		return -EINVAL;

	if (power->handle)
		return 0;

	if (power_is_gpio(power))
		power->handle = devm_gpiod_get(isp->dev, power->name, 0);
	else if (power_is_clk(power))
		power->handle = devm_clk_get(isp->dev, power->name);
	else if (power_is_voltage(power) && isp->has_pmu)
		power->handle = devm_regulator_get(isp->dev, power->name);

	return rtscam_isp_snr_power_check(isp, power);
}

static int rtscam_isp_snr_power_put(struct rtscam_isp *isp,
				    struct rtscam_isp_snr_power *power)
{
	if (!power)
		return -EINVAL;

	if (!power->handle)
		return 0;

	if (power_is_gpio(power)) {
		devm_gpiod_put(isp->dev, power->handle);
	} else if (power_is_clk(power)) {
		if (power->current_value)
			clk_disable_unprepare(power->handle);
		devm_clk_put(isp->dev, power->handle);
	} else if (power_is_voltage(power) && isp->has_pmu) {
		if (power->current_value)
			regulator_disable(power->handle);
		devm_regulator_put(power->handle);
	}

	power->handle = NULL;
	power->current_value = 0;

	return 0;
}

static int rtscam_isp_snr_power_set(struct rtscam_isp *isp,
				    struct rtscam_isp_snr_power *power,
				    u32 value)
{
	int ret = 0;

	if (!power)
		return -EINVAL;

	if (rtscam_isp_snr_power_check(isp, power))
		return -EFAULT;

	if (power_is_gpio(power)) {
		gpiod_direction_output(power->handle, value);
	} else if (power_is_clk(power)) {
		if (power->current_value)
			clk_disable_unprepare(power->handle);
		if (value) {
			ret = clk_set_rate(power->handle, value);
			if (!ret)
				ret = clk_prepare_enable(power->handle);
		}
	} else if (power_is_voltage(power)) {
		if (isp->has_pmu) {
			if (power->current_value)
				regulator_disable(power->handle);
			if (value) {
				ret = regulator_set_voltage(power->handle,
							    value, value);
				if (!ret)
					ret = regulator_enable(power->handle);
			}
		} else if (isp->pwrctl_gpio_handle) {
			if (value)
				gpiod_direction_output(isp->pwrctl_gpio_handle,
						       1);
			else
				gpiod_direction_output(isp->pwrctl_gpio_handle,
						       0);
		}
	}

	if (!ret)
		power->current_value = value;

	return ret;
}

static int rtscam_isp_msg_check(const struct rts_isp_msg_hdr *req,
				const struct rts_isp_msg_hdr *resp)
{
	return (req->sequence != resp->sequence ||
		req->ret_len != resp->msg_len || req->isp_id != resp->isp_id ||
		req->mod_id != resp->mod_id || req->action != resp->action);
}

static inline bool
rtscam_isp_message_can_read(struct rtscam_isp_message *message)
{
	return message->read_remain > 0;
}

static inline bool
rtscam_isp_message_can_write(struct rtscam_isp_message *message)
{
	struct rts_isp_msg_hdr *hdr;

	hdr = (struct rts_isp_msg_hdr *)message->write_buf;
	return (message->write_offset == 0 ||
		message->write_offset < hdr->msg_len);
}

static inline void
rtscam_isp_message_set_sequence(struct rtscam_isp_message *message,
				struct rts_isp_msg_hdr *hdr)
{
	hdr->sequence = ++message->sequence;
}

static inline bool
rtscam_isp_message_check_sequence(struct rtscam_isp_message *message,
				  struct rts_isp_msg_hdr *hdr)
{
	return hdr->sequence == message->sequence;
}

static int rtscam_isp_message_init(struct rtscam_isp_message *message,
				   struct mutex *dev_lock)
{
	if (!message)
		return -EINVAL;

	message->dev_lock = dev_lock;
	message->timeout = 1000;
	init_completion(&message->completion);
	init_waitqueue_head(&message->wq);

	return 0;
}

static int rtscam_isp_message_reinit(struct rtscam_isp_message *message)
{
	if (!message)
		return -EINVAL;

	message->msg_pending = false;
	message->read_remain = 0;
	message->write_offset = 0;
	reinit_completion(&message->completion);
	message->sequence = get_random_long();

	return 0;
}

static int rtscam_isp_message_call(struct rtscam_isp_message *message,
				   u32 isp_id, u32 cmd, void *buf, size_t len)
{
	int remain;
	struct rts_isp_msg_hdr *req;
	struct rts_isp_msg_hdr *resp;

	if (!message)
		return -EINVAL;
	if (!buf && len)
		return -EINVAL;

	if (len + sizeof(*req) > sizeof(message->read_buf))
		return -ERANGE;
	if (message->msg_pending || message->read_remain)
		return -EBUSY;

	req = (struct rts_isp_msg_hdr *)message->read_buf;
	resp = (struct rts_isp_msg_hdr *)message->write_buf;

	rtscam_isp_message_set_sequence(message, req);
	req->msg_len = sizeof(*req) + len;
	req->ret_len = sizeof(*req);
	req->isp_id = isp_id;
	req->mod_id = MOD_CONTROL;
	req->action = cmd;
	req->ret_val = 0;
	req->reloc_pos = 0;
	req->reloc_num = 0;
	memcpy(req + 1, buf, len);

	message->msg_pending = true;
	message->read_remain = req->msg_len;
	/* wake up read request */
	wake_up(&message->wq);
	mutex_unlock(message->dev_lock);
	remain = wait_for_completion_timeout(
		&message->completion, msecs_to_jiffies(message->timeout));
	mutex_lock(message->dev_lock);
	message->msg_pending = false;
	if (remain <= 0) {
		/* does not read any read message */
		if (req->msg_len == message->read_remain)
			message->read_remain = 0;
		/* timeout ocurred before wakeup */
		if (resp->msg_len == message->write_offset) {
			message->write_offset = 0;
			wake_up(&message->wq);
		}
		rtsprintk(RTS_TRACE_ERROR, "message call timeout\n");
		return -ETIMEDOUT;
	}

	/* wakeup success mean write done */
	message->write_offset = 0;
	wake_up(&message->wq);
	if (rtscam_isp_msg_check(req, resp))
		return -ENOMSG;
	return resp->ret_val;
}

static ssize_t rtscam_isp_message_read(struct rtscam_isp_message *message,
				       char __user *buf, size_t size)
{
	int ret;
	off_t off;
	struct rts_isp_msg_hdr *hdr;

	hdr = (struct rts_isp_msg_hdr *)message->read_buf;
	off = hdr->msg_len - message->read_remain;
	size = min(size, message->read_remain);

	ret = copy_to_user(buf, (char *)hdr + off, size);
	if (ret)
		return -EFAULT;
	message->read_remain -= size;

	return size;
}

static ssize_t rtscam_isp_message_write(struct rtscam_isp_message *message,
					const char __user *buf, size_t size)
{
	struct rts_isp_msg_hdr *hdr;

	hdr = (struct rts_isp_msg_hdr *)message->write_buf;

	if (message->write_offset == 0) {
		/* write header */
		if (size < sizeof(*hdr))
			return -ERANGE;
		size = sizeof(*hdr);
		if (copy_from_user(hdr, buf, size))
			return -EFAULT;
		message->write_offset += size;
		if (hdr->msg_len == size)
			goto check;
		return size;
	}

	/* write body */
	size = min(hdr->msg_len - sizeof(*hdr), size);
	if (size == 0)
		return -EAGAIN;
	if (copy_from_user(hdr, buf, size))
		return -EFAULT;
	message->write_offset += size;

	if (message->write_offset < hdr->msg_len)
		return size;

check:
	if (rtscam_isp_message_check_sequence(message, hdr)) {
		if (message->msg_pending)
			complete(&message->completion);
		else
			message->write_offset = 0;
	} else {
		message->write_offset = 0;
		rtsprintk(RTS_TRACE_ERROR, "error sequence: %u != %u\n",
			  hdr->sequence, message->sequence);
	}

	return size;
}

static inline int rtscam_isp_statis_valid(struct rtscam_isp_statis *statis)
{
	return atomic_read(&statis->valid);
}

static inline bool
rtscam_isp_statis_read_partial(struct rtscam_isp_statis *statis)
{
	struct rts_isp_msg_hdr *hdr;

	hdr = (struct rts_isp_msg_hdr *)statis->msg_buf;
	return statis->read_remain > 0 && statis->read_remain < hdr->msg_len;
}

static int rtscam_isp_statis_mem_alloc(struct rtscam_isp_statis *statis,
				       struct rtscam_mem_info *mem_info)
{
	if (!statis || !mem_info)
		return -EINVAL;

	statis->total_size = 84 * 1024;
	statis->virt_addr =
		rtscam_mem_alloc(mem_info, statis->total_size,
				 &statis->info.phy_addr,
				 RTSCAM_ISP_BUF_FROM_DEVICE, 0, "ISP Statis");
	if (!statis->virt_addr)
		return -ENOMEM;
	rtscam_mem_add_property(mem_info, statis->info.phy_addr,
				RTSMEM_PROBE_ALLOCATED);
	statis->mem_info = mem_info;

	return 0;
}

static void rtscam_isp_statis_mem_free(struct rtscam_isp_statis *statis)
{
	if (!statis || !statis->virt_addr)
		return;

	rtscam_mem_free_V2(statis->mem_info, statis->info.phy_addr);
	statis->virt_addr = NULL;
	statis->info.phy_addr = 0;
	statis->mem_info = NULL;
}

static int rtscam_isp_statis_cleanup(struct rtscam_isp_statis *statis);

static int rtscam_isp_statis_init(struct rtscam_isp_statis *statis,
				  u32 each_buf_size)
{
	int i;
	struct rts_isp_statis_info *info;

	if (!statis)
		return -EINVAL;

	info = &statis->info;
	info->num = STATIS_BUF_NUM;
	info->size = ALIGN(each_buf_size, 16);

	if (info->size * info->num > statis->total_size)
		return -ERANGE;

	atomic_set(&statis->valid, 0);
	spin_lock_init(&statis->lock);
	INIT_LIST_HEAD(&statis->idle);
	INIT_LIST_HEAD(&statis->busy);
	INIT_LIST_HEAD(&statis->done);
	INIT_LIST_HEAD(&statis->user);

	for (i = 0; i < info->num; i++) {
		struct rtscam_isp_statis_node *node;

		node = kzalloc(sizeof(*node), GFP_KERNEL);
		if (!node) {
			rtsprintk(RTS_TRACE_ERROR, "alloc statis node fail\n");
			rtscam_isp_statis_cleanup(statis);
			return -ENOMEM;
		}
		INIT_LIST_HEAD(&node->list);
		node->data.buf_id = i;
		node->addr = info->phy_addr + i * info->size;
		list_add_tail(&node->list, &statis->idle);
	}

	list_move_tail(statis->idle.next, &statis->busy);

	return 0;
}

static int rtscam_isp_statis_cleanup(struct rtscam_isp_statis *statis)
{
	unsigned long flags;
	struct rtscam_isp_statis_node *node;
	struct rtscam_isp_statis_node *next;

	if (!statis->info.num)
		return 0;

	statis->info.num = 0;

	spin_lock_irqsave(&statis->lock, flags);
	list_for_each_entry_safe(node, next, &statis->user, list) {
		list_del_init(&node->list);
		kfree(node);
	}
	list_for_each_entry_safe(node, next, &statis->idle, list) {
		list_del_init(&node->list);
		kfree(node);
	}
	list_for_each_entry_safe(node, next, &statis->busy, list) {
		list_del_init(&node->list);
		kfree(node);
	}
	list_for_each_entry_safe(node, next, &statis->done, list) {
		list_del_init(&node->list);
		kfree(node);
	}
	spin_unlock_irqrestore(&statis->lock, flags);

	atomic_set(&statis->valid, 0);

	return 0;
}

static struct rtscam_isp_statis_node *
__rtscam_isp_statis_get_node(struct rtscam_isp_statis *statis,
			     struct list_head *list, u32 buf_id)

{
	struct rtscam_isp_statis_node *node;

	if (!statis || buf_id >= statis->info.num || !list)
		return NULL;

	list_for_each_entry(node, list, list) {
		if (buf_id == node->data.buf_id)
			return node;
	}
	return NULL;
}

static void __rtscam_isp_statis_msg_prepare(struct rtscam_isp_statis *statis)
{
	unsigned long flags;
	struct rts_isp_msg_hdr *hdr;
	struct rtscam_isp_statis_node *node;

	if (statis->read_remain)
		return;

	spin_lock_irqsave(&statis->lock, flags);
	WARN_ON(list_empty(&statis->done));
	node = list_first_entry(&statis->done,
				struct rtscam_isp_statis_node, list);
	list_move(&node->list, &statis->user);
	spin_unlock_irqrestore(&statis->lock, flags);

	dma_sync_single_range_for_cpu(statis->mem_info->dev,
				      statis->info.phy_addr,
				      node->addr - statis->info.phy_addr,
				      statis->info.size, DMA_FROM_DEVICE);

	hdr = (struct rts_isp_msg_hdr *)statis->msg_buf;
	hdr->sequence = node->data.frame_count;
	hdr->msg_len = STATIS_MSG_SIZE;
	hdr->ret_len = 0;
	hdr->isp_id = 0;
	hdr->mod_id = MOD_CONTROL;
	hdr->action = RTS_ISP_STATIS_DONE;
	hdr->ret_val = 0;
	hdr->reloc_pos = 0;
	hdr->reloc_num = 0;
	memcpy(hdr + 1, &node->data, sizeof(node->data));

	statis->read_remain = STATIS_MSG_SIZE;
}

static ssize_t __rtscam_isp_statis_msg_read(struct rtscam_isp_statis *statis,
					    char __user *buf, size_t size)
{
	int ret;
	off_t off;
	unsigned long flags;

	size = min(size, statis->read_remain);
	off = STATIS_MSG_SIZE - statis->read_remain;
	ret = copy_to_user(buf, statis->msg_buf + off, size);
	if (ret)
		return -EFAULT;
	statis->read_remain -= size;
	if (statis->read_remain > 0)
		return size;

	spin_lock_irqsave(&statis->lock, flags);
	if (list_empty(&statis->done))
		atomic_set(&statis->valid, 0);
	spin_unlock_irqrestore(&statis->lock, flags);

	return size;
}

static ssize_t rtscam_isp_statis_read(struct rtscam_isp_statis *statis,
				      char __user *buf, size_t size)
{
	__rtscam_isp_statis_msg_prepare(statis);
	return __rtscam_isp_statis_msg_read(statis, buf, size);
}

static inline int rtscam_isp_sync_max_delay(struct rtscam_isp_sync *sync)
{
	return ARRAY_SIZE(sync->delay) - 1;
}

static inline int rtscam_isp_sync_index(struct rtscam_isp_sync *sync, int delay)
{
	return (sync->index + delay) % ARRAY_SIZE(sync->delay);
}

static inline struct list_head *
rtscam_isp_sync_get_head(struct rtscam_isp_sync *sync, int delay, int irq)
{
	return &sync->delay[rtscam_isp_sync_index(sync, delay)].irq[irq - 1];
}

static int rtscam_isp_sync_init(struct rtscam_isp_sync *sync)
{
	int i;
	int j;

	if (!sync)
		return -EINVAL;

	mutex_init(&sync->lock);
	atomic_set(&sync->irq, RTS_ISP_INT_NONE);
	for (i = 0; i < ARRAY_SIZE(sync->delay); i++)
		for (j = 0; j < ARRAY_SIZE(sync->delay->irq); j++)
			INIT_LIST_HEAD(&sync->delay[i].irq[j]);
	INIT_LIST_HEAD(&sync->idle);
	sync->idle_num = 0;

	return 0;
}

static int rtscam_isp_sync_clean_list(struct list_head *head)
{
	struct rtscam_isp_sync_item *item;
	struct rtscam_isp_sync_item *next;

	list_for_each_entry_safe(item, next, head, list) {
		list_del(&item->list);
		kfree(item);
	}
	return 0;
}

static int rtscam_isp_sync_cleanup(struct rtscam_isp_sync *sync)
{
	int i;
	int j;

	mutex_lock(&sync->lock);
	atomic_set(&sync->irq, RTS_ISP_INT_NONE);
	for (i = 0; i < ARRAY_SIZE(sync->delay); i++)
		for (j = 0; j < ARRAY_SIZE(sync->delay->irq); j++)
			rtscam_isp_sync_clean_list(&sync->delay[i].irq[j]);
	sync->index = 0;
	rtscam_isp_sync_clean_list(&sync->idle);
	sync->idle_num = 0;
	mutex_unlock(&sync->lock);

	return 0;
}

static struct rtscam_isp_sync_item *
__rtscam_isp_sync_get_idle(struct rtscam_isp_sync *sync)
{
	struct rtscam_isp_sync_item *item;

	if (!sync)
		return NULL;

	if (sync->idle_num) {
		item = list_last_entry(&sync->idle,
				       struct rtscam_isp_sync_item, list);
		list_del_init(&item->list);
		sync->idle_num--;
	} else {
		item = kmalloc(sizeof(*item), GFP_ATOMIC);
		if (item)
			INIT_LIST_HEAD(&item->list);
	}

	return item;
}

static int __rtscam_isp_sync_put_idle(struct rtscam_isp_sync *sync,
				      struct rtscam_isp_sync_item *item)
{
	if (!sync || !item)
		return -EINVAL;

	list_add(&item->list, &sync->idle);
	sync->idle_num++;
	if (sync->idle_num <= 32)
		return 0;
	while (sync->idle_num > 16) {
		item = list_last_entry(&sync->idle,
				       struct rtscam_isp_sync_item, list);
		list_del(&item->list);
		kfree(item);
		sync->idle_num--;
	}
	return 0;
}

static int rtscam_isp_sync_try_or_set_regs(struct rtscam_isp_sync *sync,
		int cur_int, const struct rts_isp_sync_regs *regs, int set)
{
	int i;
	int delay;
	int interrupt;

	if (!sync || !regs || regs->split_index > regs->num)
		return -EINVAL;

	for (i = 0; i < regs->num; i++) {
		int start_delay;
		int start_interrupt;
		struct rts_isp_sync_reg *reg = &regs->reg[i];

		if (reg->type < 0 || reg->type >= _MAX_RTS_ISP_SYNC_TYPE)
			return -EINVAL;

		if (i == 0) {
			if (reg->type == RTS_ISP_SYNC_TYPE_INFO) {
				delay = 0;
				interrupt = cur_int;
			} else {
				delay = cur_int > RTS_ISP_INT_DATA_START;
				interrupt = RTS_ISP_INT_DATA_START;
			}
		} else {
			if (i == 1) {
				start_delay = delay;
				start_interrupt = interrupt;
			}
			if (i == regs->split_index) {
				delay = start_delay;
				interrupt = start_interrupt;
			}
		}

		if (reg->type == RTS_ISP_SYNC_TYPE_INFO) {
			delay += reg->info.delay_frames;
			if (reg->info.delay_frames == 0 &&
			    reg->info.interrupt <= interrupt)
				delay++;
			interrupt = reg->info.interrupt;

			if (delay > rtscam_isp_sync_max_delay(sync))
				return -EINVAL;
			if (reg->info.interrupt <= RTS_ISP_INT_NONE ||
			    reg->info.interrupt >= _MAX_RTS_ISP_INT)
				return -EINVAL;
		} else if (set) {
			struct list_head *head;
			struct rtscam_isp_sync_item *item;

			item = __rtscam_isp_sync_get_idle(sync);
			if (!item)
				return -ENOMEM;
			item->reg = regs->reg[i];
			item->i2c_info = regs->i2c_info;
			head = rtscam_isp_sync_get_head(sync, delay, interrupt);
			list_add_tail(&item->list, head);
		}
	}

	return 0;
}

static int rtscam_isp_add_sync_regs(struct rtscam_isp *isp,
				    struct rts_isp_sync_regs *regs)
{
	int ret;
	int cur_int;
	char sbuf[128];
	int miss_data_start = 0;
	struct rts_isp_sync_reg *reg;
	struct rtscam_isp_sync *sync = &isp->sync;
	const size_t size = regs->num * sizeof(*reg);
	const int need_alloc = size > sizeof(sbuf);

	if (regs->num > 128)
		return -EINVAL;

	if (need_alloc) {
		reg = kmalloc(size, GFP_KERNEL);
		if (!reg)
			return -ENOMEM;
	} else {
		reg = (struct rts_isp_sync_reg *)sbuf;
	}
	if (copy_from_user(reg, regs->reg, size)) {
		if (need_alloc)
			kfree(reg);
		return -EFAULT;
	}
	regs->reg = reg;
	reg = NULL;

	mutex_lock(&sync->lock);
	preempt_disable();
	cur_int = atomic_read(&sync->irq);
	if (rtscam_isp_read_reg(isp, SYS_INT_FLAG0) & ISP_OUT_LINE_CNT_INT) {
		cur_int++;
		miss_data_start = 1;
	}
	ret = rtscam_isp_sync_try_or_set_regs(sync, cur_int, regs, false);
	if (ret)
		goto out;
	ret = rtscam_isp_sync_try_or_set_regs(sync, cur_int, regs, true);
	if (!ret && !miss_data_start)
		irq_wake_thread(isp->irq, isp);
out:
	mutex_unlock(&sync->lock);
	preempt_enable();
	if (need_alloc)
		kfree(regs->reg);
	return ret;
}

static int rtscam_isp_get_io_start(struct rtscam_isp *isp, void *args)
{
	*(u32 *)args = isp->io_start;
	return 0;
}

static int rtscam_isp_get_io_size(struct rtscam_isp *isp, void *args)
{
	*(u32 *)args = isp->io_size;
	return 0;
}

static int rtscam_isp_set_power(struct rtscam_isp *isp, void *arg)
{
	int i;
	int ret = 0;
	struct rts_isp_snr_pwr *power = arg;

	if (!isp || !arg)
		return -EINVAL;

	if (power->num > ARRAY_SIZE(power->items))
		return -EINVAL;

	for (i = 0; i < power->num; i++)
		if (power->items[i].type >= _MAX_SNR_POWER_TYPE)
			return -EINVAL;

	for (i = 0; i < power->num; i++) {
		u32 type = power->items[i].type;
		u32 value = power->items[i].value;

		ret = rtscam_isp_snr_power_get(isp, &isp->power[type]);
		if (ret)
			break;
		ret = rtscam_isp_snr_power_set(isp, &isp->power[type], value);
		if (ret)
			break;
		usleep_range(power->items[i].delay,
			     power->items[i].delay + 100);
	}

	return ret;
}

static int __rtscam_isp_i2c_transfer(struct rtscam_isp *isp, u8 slave_id,
				     u8 *buf, u16 len, u8 is_read)
{
	int ret;
	struct i2c_msg msg;

	msg.addr = slave_id;
	msg.flags = is_read ? I2C_M_RD : 0;
	msg.buf = buf;
	msg.len = len;
	ret = i2c_transfer(isp->adapter, &msg, 1);
	/* EGAIN is caused by snr soft reset(arbitration lost), we bypass it */
	if (ret != 1 && ret != -EAGAIN) {
		/* ignore error message during sensor detecting */
		if (!fps_zero(&isp->current_fps))
			rtsprintk(RTS_TRACE_ERROR,
				  "i2c %#x transfer fail\n", slave_id);
		return ret;
	}
	return 0;
}

static inline int __rtscam_isp_i2c_read(struct rtscam_isp *isp, u8 slave_id,
					u8 *buf, u16 len)
{
	return __rtscam_isp_i2c_transfer(isp, slave_id, buf, len, 1);
}

static inline int __rtscam_isp_i2c_write(struct rtscam_isp *isp, u8 slave_id,
					 u8 *buf, u16 len)
{
	return __rtscam_isp_i2c_transfer(isp, slave_id, buf, len, 0);
}

static int __rtscam_isp_set_clk_rate(struct rtscam_isp *isp,
				     int type, unsigned long *rate)
{
	struct clk *clk;

	if (!rate)
		return -EINVAL;

	switch (type) {
	case CLOCK_INTERFACE:
		clk = isp->inf_clk;
		break;
	case CLOCK_MIPIOUT:
		clk = isp->mipiout_clk;
		break;
	case CLOCK_ISP:
		clk = isp->isp_clk;
		*rate = isp->isp_clk_fix ? isp->isp_clk_fix : *rate;
		break;
	default:
		return -EINVAL;
	}

	if (*rate) {
		if (!isp->is_fpga) {
			int ret = 0;

			ret = clk_set_rate(clk, clk_round_rate(clk, *rate));
			if (ret)
				return ret;
		}
		*rate = clk_get_rate(clk);
	}
	return 0;
}

static void __rtscam_isp_ref_clk(struct rtscam_isp *isp, int type)
{
	switch (type) {
	case CLOCK_INTERFACE:
		isp->inf_clk_refcnt++;
		clk_prepare_enable(isp->inf_clk);
		break;
	case CLOCK_ISP:
		isp->isp_clk_refcnt++;
		clk_prepare_enable(isp->isp_clk);
		break;
	case CLOCK_MIPIOUT:
		isp->mipiout_clk_refcnt++;
		clk_prepare_enable(isp->mipiout_clk);
		break;
	}
}

static void __rtscam_isp_unref_clk(struct rtscam_isp *isp, int type)
{
	switch (type) {
	case CLOCK_INTERFACE:
		WARN_ON(!isp->inf_clk_refcnt);
		isp->inf_clk_refcnt--;
		clk_disable_unprepare(isp->inf_clk);
		break;
	case CLOCK_ISP:
		WARN_ON(!isp->isp_clk_refcnt);
		isp->isp_clk_refcnt--;
		clk_disable_unprepare(isp->isp_clk);
		break;
	case CLOCK_MIPIOUT:
		WARN_ON(!isp->mipiout_clk_refcnt);
		isp->mipiout_clk_refcnt--;
		clk_disable_unprepare(isp->mipiout_clk);
		break;
	}
}

static void __rtscam_isp_free_clk(struct rtscam_isp *isp)
{
	while (isp->inf_clk_refcnt)
		__rtscam_isp_unref_clk(isp, CLOCK_INTERFACE);
	while (isp->isp_clk_refcnt)
		__rtscam_isp_unref_clk(isp, CLOCK_ISP);
	while (isp->mipiout_clk_refcnt)
		__rtscam_isp_unref_clk(isp, CLOCK_MIPIOUT);
}

static int rtscam_isp_read_i2c_reg(struct rtscam_isp *isp,
				   const struct rts_isp_i2c_info *info,
				   struct rts_isp_i2c_reg *reg)
{
	int ret;
	u8 buf[2];

	if (!info || !reg)
		return -EINVAL;

	if (info->addr_len == 1) {
		buf[0] = reg->addr & 0xff;
	} else if (info->addr_len == 2) {
		buf[0] = reg->addr >> 8;
		buf[1] = reg->addr & 0xff;
	} else {
		return -EINVAL;
	}

	ret = __rtscam_isp_i2c_write(isp, info->i2c_id, buf, info->addr_len);
	if (ret)
		return ret;
	ret = __rtscam_isp_i2c_read(isp, info->i2c_id, buf, info->data_len);
	if (ret)
		return ret;

	if (info->data_len == 1)
		reg->data = buf[0];
	else
		reg->data = (buf[0] << 8) | buf[1];

	return 0;
}

static int rtscam_isp_write_i2c_reg(struct rtscam_isp *isp,
				    const struct rts_isp_i2c_info *info,
				    const struct rts_isp_i2c_reg *reg)
{
	int j = 0;
	u8 buf[4];

	if (info->addr_len == 1) {
		buf[j++] = reg->addr & 0xff;
	} else {
		buf[j++] = reg->addr >> 8;
		buf[j++] = reg->addr & 0xff;
	}
	if (info->data_len == 1) {
		buf[j++] = reg->data & 0xff;
	} else {
		buf[j++] = reg->data >> 8;
		buf[j++] = reg->data & 0xff;
	}

	return __rtscam_isp_i2c_write(isp, info->i2c_id, buf, j);
}

static int rtscam_isp_i2c_read_regs(struct rtscam_isp *isp, void *args)
{
	u32 i;
	struct rts_isp_i2c *i2c = args;

	if (!isp || !args)
		return -EINVAL;

	if (!i2c || !i2c->num || i2c->num > ARRAY_SIZE(i2c->regs))
		return -EINVAL;

	if (!i2c->info.addr_len || i2c->info.addr_len > 2 ||
	    !i2c->info.data_len || i2c->info.data_len > 2)
		return -EINVAL;

	for (i = 0; i < i2c->num; i++) {
		int ret;

		ret = rtscam_isp_read_i2c_reg(isp, &i2c->info, i2c->regs + i);
		if (ret)
			return ret;
	}

	return 0;
}

static int rtscam_isp_i2c_write_regs(struct rtscam_isp *isp, void *args)
{
	u32 i;
	struct rts_isp_i2c *i2c = args;

	if (!isp || !args)
		return -EINVAL;

	if (!i2c || !i2c->num || i2c->num > ARRAY_SIZE(i2c->regs))
		return -EINVAL;

	if (!i2c->info.addr_len || i2c->info.addr_len > 2 ||
	    !i2c->info.data_len || i2c->info.data_len > 2)
		return -EINVAL;

	for (i = 0; i < i2c->num; i++) {
		int ret;

		ret = rtscam_isp_write_i2c_reg(isp, &i2c->info, i2c->regs + i);
		if (ret)
			return ret;
	}

	return 0;
}

static int __rtscam_isp_set_fps(struct rtscam_isp *isp,
				const struct v4l2_fract *fps)
{
	int ret = 0;
	struct v4l2_fract fps_backup;
	struct rts_isp_preview_info info;

	if (!isp || !fps)
		return -EINVAL;

	if (fps_equal(&isp->current_fps, fps))
		return 0;
	__rtscam_isp_ref_clk(isp, CLOCK_ISP);

	if (fps_zero(&isp->current_fps)) {
		atomic_set(&isp->frame_count, 0);
		atomic_set(&isp->statis_count, 0);
		rtscam_isp_write_reg(isp, 0xffffffff, SYS_INT_FLAG0);
		rtscam_isp_write_reg(isp, ISP_OUT_DATA_START_INT |
				     SENSOR0_FRAME_END_INT, SYS_INT_EN0);
		rtscam_isp_write_reg(isp, 0xffffffff, SYS_INT_FLAG1);
		rtscam_isp_write_reg(isp, STATIS_DONE_INT, SYS_INT_EN1);
	}
	info.fps = *fps;
	fps_backup = isp->current_fps;

	if (!fps_zero(fps))
		isp->current_fps = *fps;
	else
		isp->stopping = true;

	ret = rtscam_isp_message_call(&isp->message, 0, RTS_ISP_SET_FPS,
				      &info, sizeof(info));
	if (ret) {
		isp->current_fps = fps_backup;
		goto out;
	}

	if (fps_zero(fps)) {
		rtscam_isp_sync_cleanup(&isp->sync);
		isp->current_fps = *fps;
		isp->stopping = false;
	}
	if (fps_zero(&isp->current_fps)) {
		rtscam_isp_write_reg(isp, 0x0, SYS_INT_EN0);
		rtscam_isp_write_reg(isp, 0xffffffff, SYS_INT_FLAG0);
		rtscam_isp_write_reg(isp, 0x0, SYS_INT_EN1);
		rtscam_isp_write_reg(isp, 0xffffffff, SYS_INT_FLAG1);
	}
out:
	isp->dynamic_fps = isp->current_fps;
	__rtscam_isp_unref_clk(isp, CLOCK_ISP);
	return ret;
}

static int rtscam_isp_subdev_set_fps(struct rtscam_zoom_isp *zoom_isp,
				     struct v4l2_fract fps)
{
	int ret;
	struct rtscam_isp *isp;

	if (!zoom_isp)
		return -EINVAL;

	isp = container_of(zoom_isp, struct rtscam_isp, zoom_isp);

	if (atomic_read(&isp->active) == 0)
		return -EHOSTDOWN;

	if (mutex_lock_interruptible(&isp->lock))
		return -ERESTARTSYS;

	ret = __rtscam_isp_set_fps(isp, &fps);

	mutex_unlock(&isp->lock);
	return ret;
}

static int rtscam_isp_subdev_set_hook(struct rtscam_zoom_isp *zoom_isp,
		void *master, int (*hook)(void *master, int id, void *arg))
{
	struct rtscam_isp *isp;

	if (!zoom_isp)
		return -EINVAL;

	isp = container_of(zoom_isp, struct rtscam_isp, zoom_isp);
	if (mutex_lock_interruptible(&isp->lock))
		return -ERESTARTSYS;

	isp->hook = hook;

	mutex_unlock(&isp->lock);
	return 0;
}

static int rtscam_isp_set_info(struct rtscam_isp *isp, void *args)
{
	int ret;
	struct rts_isp_info backup;
	const struct rts_isp_info *info = args;

	if (!info->width && !info->height) {
		atomic_set(&isp->active, 0);
		return 0;
	}
	backup = isp->zoom_isp.info;
	isp->zoom_isp.info = *info;

	ret = rtscam_zoom_refresh_isp_info(&isp->zoom_isp,
					   isp->need_reset_amba);
	if (ret) {
		isp->zoom_isp.info = backup;
		rtscam_zoom_refresh_isp_info(&isp->zoom_isp, 0);
		return ret;
	}
	atomic_set(&isp->active, 1);
	isp->need_reset_amba = false;
	return 0;
}

static int rtscam_isp_sync_write(struct rtscam_isp *isp, void *args)
{
	struct rts_isp_sync_regs *regs = args;

	if (!isp || !args)
		return -EINVAL;

	return rtscam_isp_add_sync_regs(isp, regs);
}

static int rtscam_isp_init_statis(struct rtscam_isp *isp, void *args)
{
	int ret;
	struct rtscam_isp_statis_node *node;

	ret = rtscam_isp_statis_init(&isp->statis, *(u32 *)args);
	if (ret)
		return ret;

	node = list_first_entry(&isp->statis.busy,
				struct rtscam_isp_statis_node, list);
	rtscam_isp_write_reg(isp, node->addr, STATIS_DDR_ADDR);
	rtscam_isp_write_reg(isp, isp->statis.info.size, STATIS_DDR_LEN);

	return 0;
}

static int rtscam_isp_cleanup_statis(struct rtscam_isp *isp, void *args)
{
	return rtscam_isp_statis_cleanup(&isp->statis);
}

static int rtscam_isp_get_statis_info(struct rtscam_isp *isp, void *args)
{
	if (!isp || !args)
		return -EINVAL;

	*(struct rts_isp_statis_info *)args = isp->statis.info;
	return 0;
}

static int rtscam_isp_put_statis(struct rtscam_isp *isp, void *args)
{
	u32 id;
	unsigned long flags;
	struct rtscam_isp_statis *statis;
	struct rtscam_isp_statis_node *node;

	if (!isp || !args)
		return -EINVAL;

	id = *(u32 *)args;
	statis = &isp->statis;

	spin_lock_irqsave(&statis->lock, flags);
	node = __rtscam_isp_statis_get_node(statis, &statis->user, id);
	if (!node) {
		rtsprintk(RTS_TRACE_ERROR, "invalid put statis buf %u\n", id);
		spin_unlock_irqrestore(&isp->statis.lock, flags);
		return -ENOENT;
	}
	list_del(&node->list);
	spin_unlock_irqrestore(&statis->lock, flags);

	dma_sync_single_range_for_device(statis->mem_info->dev,
					 statis->info.phy_addr,
					 node->addr - statis->info.phy_addr,
					 statis->info.size, DMA_FROM_DEVICE);

	spin_lock_irqsave(&statis->lock, flags);
	list_add_tail(&node->list, &statis->idle);
	spin_unlock_irqrestore(&statis->lock, flags);

	return 0;
}

static int rtscam_isp_mem_alloc(struct rtscam_isp *isp, void *args)
{
	void *virt_addr;
	struct rtscam_isp_mem_alloc_info *info = args;
	struct rtscam_isp_mem_item *item;

	item = kmalloc(sizeof(*item), GFP_KERNEL);
	if (!item)
		return -ENOMEM;
	virt_addr = rtscam_mem_alloc(isp->mem_info, info->length,
				     &info->phy_addr, RTSCAM_ISP_BUF_COHERENT,
				     0, info->info);
	if (!virt_addr) {
		rtsprintk(RTS_TRACE_ERROR, "alloc buffer fail\n");
		kfree(item);
		return -ENOMEM;
	}
	rtscam_mem_add_property(isp->mem_info, info->phy_addr,
				RTSMEM_PROBE_ALLOCATED);

	INIT_LIST_HEAD(&item->list);
	item->phy_addr = info->phy_addr;
	item->size = info->length;
	list_add_tail(&item->list, &isp->mem_list);

	return 0;
}

static int rtscam_isp_mem_free(struct rtscam_isp *isp, void *args)
{
	dma_addr_t phy_addr;
	struct rtscam_isp_mem_item *item;
	struct rtscam_isp_mem_item *next;

	phy_addr = *(__u32 *)args;
	list_for_each_entry_safe(item, next, &isp->mem_list, list) {
		if (item->phy_addr == phy_addr) {
			list_del(&item->list);
			rtscam_mem_free_V2(isp->mem_info, phy_addr);
			kfree(item);
			break;
		}
	}
	return 0;
}

static int rtscam_isp_sel_pin_state(struct rtscam_isp *isp, void *args)
{
	int ret;
	enum rtscam_isp_pin_state state;

	state = *(enum rtscam_isp_pin_state *)args;
	switch (state) {
	case PIN_STATE_NONE:
		ret = pinctrl_select_state(isp->pins.p,
					   isp->pins.default_state);
		break;
	case PIN_STATE_DVP:
		ret = pinctrl_select_state(isp->pins.p, isp->pins.dvp_state);
		break;
	case PIN_STATE_MIPI:
		ret = pinctrl_select_state(isp->pins.p, isp->pins.mipi_state);
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static int rtscam_isp_notify_dyn_fps(struct rtscam_isp *isp, void *args)
{
	int ret = 0;

	isp->dynamic_fps = *(struct v4l2_fract *)args;
	mutex_unlock(&isp->lock);
	if (isp->hook)
		ret = isp->hook(isp->zoom_isp.master,
				RTSCAM_ZOOM_EVT_DYN_FPS_CHANGED, args);
	mutex_lock(&isp->lock);

	return ret;
}

static int rtscam_isp_get_isp_clock(struct rtscam_isp *isp, void *args)
{
	__rtscam_isp_ref_clk(isp, CLOCK_ISP);
	return 0;
}

static int rtscam_isp_put_isp_clock(struct rtscam_isp *isp, void *args)
{
	__rtscam_isp_unref_clk(isp, CLOCK_ISP);
	return 0;
}

static int rtscam_isp_set_clock(struct rtscam_isp *isp, void *args)
{
	int ret;
	struct rtscam_isp_clock *clk = args;

	if (clk->rate) {
		if (clk->rate != 1) {
			ret = __rtscam_isp_set_clk_rate(isp, clk->type,
			      &clk->rate);
			if (ret)
				return ret;
		}
		__rtscam_isp_ref_clk(isp, clk->type);
	} else {
		__rtscam_isp_unref_clk(isp, clk->type);
	}

	return 0;
}

static int rtscam_isp_is_fpga(struct rtscam_isp *isp, void *args)
{
	*(__s32 *)args = isp->is_fpga;
	return 0;
}

static int rtscam_isp_wait_event(struct rtscam_isp *isp, void *args)
{
	enum rtscam_isp_event event;
	u32 timeout = isp->message.timeout / 4;

	if (!fps_zero(&isp->dynamic_fps)) {
		u32 timeout_change;

		timeout_change = 1000 * isp->dynamic_fps.numerator /
				 isp->dynamic_fps.denominator;
		timeout = max(timeout_change, timeout);
	}

	event = *(__s32 *)args;
	switch (event) {
	case RTSCAM_ISP_DATA_START:
		reinit_completion(&isp->data_start_completion);
		wait_for_completion_timeout(&isp->data_start_completion,
					    msecs_to_jiffies(timeout));
		return 0;
	case RTSCAM_ISP_FRAME_END:
		reinit_completion(&isp->frame_end_completion);
		wait_for_completion_timeout(&isp->frame_end_completion,
					    msecs_to_jiffies(timeout));
		return 0;
	default:
		return -EINVAL;
	}
}

static int rtscam_isp_sync_lock(struct rtscam_isp *isp, void *args)
{
	__s32 lock = *(__s32 *)args;

	if (lock)
		mutex_lock(&isp->sync.lock);
	else
		mutex_unlock(&isp->sync.lock);

	return 0;
}

static int rtscam_isp_test_preview(struct rtscam_isp *isp, void *args)
{
	return __rtscam_isp_set_fps(isp, (const struct v4l2_fract *)args);
}

static int rtscam_isp_get_tnr_bit(struct rtscam_isp *isp, void *args)
{
	*(__u32 *)args = isp->tnr_bit;
	return 0;
}

static int rtscam_isp_mem_sync(struct rtscam_isp *isp, void *args)
{
	int dir;
	struct rts_isp_mem_sync_info *info = args;

	if (info->dma_dir == RTS_ISP_DMA_FROM_DEVICE)
		dir = DMA_FROM_DEVICE;
	else if (info->dma_dir == RTS_ISP_DMA_TO_DEVICE)
		dir = DMA_TO_DEVICE;
	else if (info->dma_dir == RTS_ISP_DMA_BIDIRECTIONAL)
		dir = DMA_BIDIRECTIONAL;
	else
		return -ERANGE;

	if (info->sync_type == RTS_ISP_SYNC_FOR_CPU)
		dma_sync_single_for_cpu(isp->mem_info->dev, info->addr,
					info->size, dir);
	else if (info->sync_type == RTS_ISP_SYNC_FOR_DEVICE)
		dma_sync_single_for_device(isp->mem_info->dev, info->addr,
					   info->size, dir);
	else
		return -ERANGE;

	return 0;
}

static int rtscam_isp_i2c_transfer(struct rtscam_isp *isp, void *args)
{
	int ret;
	void *buf;
	void __user *buf_user;
	struct i2c_msg *msg = args;

	if (msg->len > 512)
		return -ERANGE;

	buf = kmalloc(msg->len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	if (!(msg->flags & I2C_M_RD)) {
		if (copy_from_user(buf, msg->buf, msg->len)) {
			ret = -EFAULT;
			goto out;
		}
	}
	buf_user = msg->buf;
	msg->buf = buf;

	ret = i2c_transfer(isp->adapter, msg, 1);
	if (ret != 1)
		goto out;
	else
		ret = 0;

	msg->buf = buf_user;
	if (msg->flags & I2C_M_RD) {
		if (copy_to_user(msg->buf, buf, msg->len)) {
			ret = -EFAULT;
			goto out;
		}
	}
out:
	kfree(buf);
	return ret;
}

static int rtscam_isp_notify_color_range(struct rtscam_isp *isp, void *args)
{
	int ret = 0;

	mutex_unlock(&isp->lock);
	if (isp->hook)
		ret = isp->hook(isp->zoom_isp.master,
				RTSCAM_ZOOM_EVT_COLOR_RANGE_CHANGED, args);
	mutex_lock(&isp->lock);

	return ret;
}

static int rtscam_isp_get_timeout(struct rtscam_isp *isp, void *args)
{
	*(s32 *)args = isp->message.timeout;

	return 0;
}

static struct rtscam_isp_ioctl_info rtscam_isp_ioctl_infos[] = {
	ISP_IOCTL_INFO(RTSISP_IOC_GET_IO_START, rtscam_isp_get_io_start),
	ISP_IOCTL_INFO(RTSISP_IOC_GET_IO_SIZE, rtscam_isp_get_io_size),
	ISP_IOCTL_INFO(RTSISP_IOC_POWER, rtscam_isp_set_power),
	ISP_IOCTL_INFO(RTSISP_IOC_I2C_READ_REGS, rtscam_isp_i2c_read_regs),
	ISP_IOCTL_INFO(RTSISP_IOC_I2C_WRITE_REGS, rtscam_isp_i2c_write_regs),
	ISP_IOCTL_INFO(RTSISP_IOC_SET_INFO, rtscam_isp_set_info),
	ISP_IOCTL_INFO(RTSISP_IOC_SYNC_WRITE, rtscam_isp_sync_write),
	ISP_IOCTL_INFO(RTSISP_IOC_INIT_STATIS, rtscam_isp_init_statis),
	ISP_IOCTL_INFO(RTSISP_IOC_CLEANUP_STATIS, rtscam_isp_cleanup_statis),
	ISP_IOCTL_INFO(RTSISP_IOC_GET_STATIS_INFO, rtscam_isp_get_statis_info),
	ISP_IOCTL_INFO(RTSISP_IOC_PUT_STATIS, rtscam_isp_put_statis),
	ISP_IOCTL_INFO(RTSISP_IOC_MEM_ALLOC, rtscam_isp_mem_alloc),
	ISP_IOCTL_INFO(RTSISP_IOC_MEM_FREE, rtscam_isp_mem_free),
	ISP_IOCTL_INFO(RTSISP_IOC_SEL_PIN_STATE, rtscam_isp_sel_pin_state),
	ISP_IOCTL_INFO(RTSISP_IOC_NOTIFY_DYN_FPS, rtscam_isp_notify_dyn_fps),
	ISP_IOCTL_INFO(RTSISP_IOC_GET_ISP_CLOCK, rtscam_isp_get_isp_clock),
	ISP_IOCTL_INFO(RTSISP_IOC_PUT_ISP_CLOCK, rtscam_isp_put_isp_clock),
	ISP_IOCTL_INFO(RTSISP_IOC_SET_CLOCK, rtscam_isp_set_clock),
	ISP_IOCTL_INFO(RTSISP_IOC_IS_FPGA, rtscam_isp_is_fpga),
	ISP_IOCTL_INFO(RTSISP_IOC_WAIT_EVENT, rtscam_isp_wait_event),
	ISP_IOCTL_INFO(RTSISP_IOC_SYNC_LOCK, rtscam_isp_sync_lock),
	ISP_IOCTL_INFO(RTSISP_IOC_TEST_PREVIEW, rtscam_isp_test_preview),
	ISP_IOCTL_INFO(RTSISP_IOC_GET_TNR_BIT, rtscam_isp_get_tnr_bit),
	ISP_IOCTL_INFO(RTSISP_IOC_MEM_SYNC, rtscam_isp_mem_sync),
	ISP_IOCTL_INFO(RTSISP_IOC_I2C_TRANSTER, rtscam_isp_i2c_transfer),
	ISP_IOCTL_INFO(RTSISP_IOC_NOTIFY_COLOR_RANGE,
		       rtscam_isp_notify_color_range),
	ISP_IOCTL_INFO(RTSISP_IOC_GET_TIMEOUT, rtscam_isp_get_timeout),
};

static int rtscam_isp_reset_device(struct rtscam_isp *isp)
{
	int ret;

	ret = reset_control_reset(isp->isp_reset);
	if (ret)
		goto exit;

	ret = reset_control_reset(isp->mipi_reset);
	if (ret)
		goto exit;
	ret = reset_control_deassert(isp->mipiout_reset);
	if (ret)
		goto exit;

exit:
	return ret;
}

static void rtscam_isp_assert_device(struct rtscam_isp *isp)
{
	reset_control_assert(isp->mipiout_reset);
}

static int rtscam_isp_config_buffer(struct rtscam_isp *isp)
{
	u32 value;

	value = isp->statis_cfg.base << 16 | isp->statis_cfg.size;
	rtscam_isp_write_reg(isp, value, STATIS_BUF_SIZE);

	return 0;
}

static void rtscam_isp_memories_release(struct rtscam_isp *isp)
{
	struct rtscam_isp_mem_item *item;
	struct rtscam_isp_mem_item *next;

	list_for_each_entry_safe(item, next, &isp->mem_list, list) {
		list_del(&item->list);
		rtscam_mem_free_V2(isp->mem_info, item->phy_addr);
		kfree(item);
	}
}

static ssize_t get_msg_timeout(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct rtscam_isp *isp = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", isp->message.timeout);
}
static ssize_t set_msg_timeout(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int ret;
	u32 timeout;
	struct rtscam_isp *isp = dev_get_drvdata(dev);

	ret = kstrtou32(buf, 10, &timeout);
	if (ret)
		return ret;
	isp->message.timeout = clamp_t(u32, timeout, 100, 10000);
	return count;
}
static DEVICE_ATTR(msg_timeout, 0644, get_msg_timeout, set_msg_timeout);

static ssize_t get_fix_clk(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct rtscam_isp *isp = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", isp->isp_clk_fix);
}
static ssize_t set_fix_clk(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	int ret;
	struct rtscam_isp *isp = dev_get_drvdata(dev);

	ret = kstrtou32(buf, 10, &isp->isp_clk_fix);
	if (ret)
		return ret;
	return count;
}
static DEVICE_ATTR(fix_clk, 0644, get_fix_clk, set_fix_clk);

static ssize_t show_frame_count(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	size_t size = 0;
	struct rtscam_isp *isp = dev_get_drvdata(dev);

	size += scnprintf(buf + size, PAGE_SIZE - size,
			  "frame count     statis count\n");
	size += scnprintf(buf + size, PAGE_SIZE - size, "%-15u %-15u\n",
			  atomic_read(&isp->frame_count),
			  atomic_read(&isp->statis_count));
	return size;
}

static DEVICE_ATTR(frame_count, 0444, show_frame_count, NULL);

static void __rtscam_isp_force_stop_preview(struct rtscam_isp *isp)
{
	enum rtscam_isp_event event = RTSCAM_ISP_DATA_START;

	if (fps_zero(&isp->current_fps))
		return;

	/* stop tnr */
	rtscam_isp_write_reg(isp, 0xe, TNR_CTRL);
	/* stop statis */
	rtscam_isp_write_reg(isp, 0x2, SYS_STATIS_CTRL);
	/* data disable */
	rtscam_isp_write_reg(isp, 0x0, SYS_DATA_DELAY);
	/* wait 2 data start */
	rtscam_isp_wait_event(isp, &event);
	rtscam_isp_wait_event(isp, &event);
	/* stop isp */
	rtscam_isp_write_reg(isp, 0x0, SYS_INT_EN0);
	rtscam_isp_write_reg(isp, 0xffffffff, SYS_INT_FLAG0);
	rtscam_isp_write_reg(isp, 0x0, SYS_INT_EN1);
	rtscam_isp_write_reg(isp, 0xffffffff, SYS_INT_FLAG1);
	rtscam_isp_write_reg(isp, 0x2, SYS_CONTROL0);

	isp->current_fps.numerator = 0;
	isp->current_fps.denominator = 0;
	isp->dynamic_fps.numerator = 0;
	isp->dynamic_fps.denominator = 0;
}

static int rtscam_isp_open(struct file *filp)
{
	int ret = 0;
	struct rtscam_ge_device *gdev = rtscam_devdata(filp);
	struct rtscam_isp *isp = rtscam_ge_get_drvdata(gdev);

	if (mutex_lock_interruptible(&isp->lock))
		return -ERESTARTSYS;

	__rtscam_isp_ref_clk(isp, CLOCK_ISP);
	if (atomic_inc_return(&isp->user_count) == 1) {
		ret = rtscam_isp_message_reinit(&isp->message);
		if (ret)
			goto out;
		ret = rtscam_isp_reset_device(isp);
		if (ret)
			goto out;
		ret = rtscam_isp_config_buffer(isp);
		if (ret)
			goto out;
		isp->need_reset_amba = true;
	}

	filp->private_data = isp;

out:
	if (ret)
		atomic_dec(&isp->user_count);
	__rtscam_isp_unref_clk(isp, CLOCK_ISP);
	mutex_unlock(&isp->lock);
	return ret;
}

static int rtscam_isp_close(struct file *filp)
{
	struct rtscam_isp *isp = filp->private_data;

	if (mutex_lock_interruptible(&isp->lock))
		return -ERESTARTSYS;

	if (atomic_dec_return(&isp->user_count) == 0) {
		u32 i;

		atomic_set(&isp->active, 0);
		__rtscam_isp_force_stop_preview(isp);
		rtscam_isp_assert_device(isp);
		for (i = 0; i < ARRAY_SIZE(isp->power); i++)
			rtscam_isp_snr_power_put(isp, &isp->power[i]);
		if (isp->pwrctl_gpio_handle) {
			gpiod_direction_output(isp->pwrctl_gpio_handle, 0);
			devm_gpiod_put(isp->dev, isp->pwrctl_gpio_handle);
			isp->pwrctl_gpio_handle = NULL;
		}
		rtscam_isp_statis_cleanup(&isp->statis);
		rtscam_isp_memories_release(isp);
		__rtscam_isp_free_clk(isp);
	}

	mutex_unlock(&isp->lock);

	filp->private_data = NULL;

	return 0;
}

static ssize_t rtscam_isp_read(struct file *filp, char __user *buf,
			       size_t size, loff_t *offset)
{
	int ret;
	struct rtscam_isp *isp = filp->private_data;

	if (mutex_lock_interruptible(&isp->lock))
		return -ERESTARTSYS;
	if (rtscam_isp_message_can_read(&isp->message) &&
	    !rtscam_isp_statis_read_partial(&isp->statis))
		ret = rtscam_isp_message_read(&isp->message, buf, size);
	else if (rtscam_isp_statis_valid(&isp->statis))
		ret = rtscam_isp_statis_read(&isp->statis, buf, size);
	else
		ret = -EAGAIN;
	mutex_unlock(&isp->lock);
	return ret;
}

static ssize_t rtscam_isp_write(struct file *filp, const char __user *buf,
				size_t size, loff_t *offset)
{
	int ret;
	struct rtscam_isp *isp = filp->private_data;

	if (mutex_lock_interruptible(&isp->lock))
		return -ERESTARTSYS;
	ret = rtscam_isp_message_write(&isp->message, buf, size);
	mutex_unlock(&isp->lock);

	return ret;
}

static long rtscam_isp_do_ioctl(struct file *filp, unsigned int cmd, void *arg)
{
	int ret;
	struct rtscam_isp *isp = filp->private_data;

	if (_IOC_NR(cmd) > ARRAY_SIZE(rtscam_isp_ioctl_infos))
		return -ENOTTY;
	if (cmd != rtscam_isp_ioctl_infos[_IOC_NR(cmd)].cmd)
		return -ENOTTY;
	if (mutex_lock_interruptible(&isp->lock))
		return -ERESTARTSYS;
	__rtscam_isp_ref_clk(isp, CLOCK_ISP);
	ret = rtscam_isp_ioctl_infos[_IOC_NR(cmd)].func(isp, arg);
	__rtscam_isp_unref_clk(isp, CLOCK_ISP);
	mutex_unlock(&isp->lock);

	return ret;
}

static long rtscam_isp_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg)
{
	return rtscam_usercopy(filp, cmd, arg, rtscam_isp_do_ioctl);
}

static unsigned int rtscam_isp_poll(struct file *filp,
				    struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	struct rtscam_isp *isp = filp->private_data;
	unsigned long req_events = poll_requested_events(wait);

	if (!(req_events & (POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM)))
		return mask;

	poll_wait(filp, &isp->message.wq, wait);

	if (rtscam_isp_message_can_read(&isp->message) ||
	    rtscam_isp_statis_valid(&isp->statis))
		mask |= POLLIN | POLLRDNORM;

	if (rtscam_isp_message_can_write(&isp->message))
		mask |= POLLOUT | POLLWRNORM;

	return mask;
}

static bool __is_rtscam_isp_regs(struct rtscam_isp *isp,
				 dma_addr_t phy_addr, size_t size)
{
	return phy_addr == isp->io_start && size == PAGE_ALIGN(isp->io_size);
}

static bool __is_rtscam_isp_statis_buf(struct rtscam_isp *isp,
				       dma_addr_t phy_addr, size_t size)
{
	size_t statis_size;

	statis_size = PAGE_ALIGN(isp->statis.info.num * isp->statis.info.size);
	return (phy_addr == isp->statis.info.phy_addr && size == statis_size);
}

static bool __is_rtscam_isp_allocated(struct rtscam_isp *isp,
				      dma_addr_t phy_addr, size_t size)
{
	struct rtscam_isp_mem_item *item;

	list_for_each_entry(item, &isp->mem_list, list)
		if (phy_addr == item->phy_addr &&
		    size == PAGE_ALIGN(item->size))
			return true;
	return false;
}

static int rtscam_isp_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret;
	size_t size;
	off_t phy_addr;
	struct rtscam_isp *isp = filp->private_data;

	phy_addr = vma->vm_pgoff << PAGE_SHIFT;
	size = vma->vm_end - vma->vm_start;

	if (mutex_lock_interruptible(&isp->lock))
		return -ERESTARTSYS;
	if (__is_rtscam_isp_regs(isp, phy_addr, size)) {
		vma->vm_page_prot = pgprot_device(vma->vm_page_prot);
		ret = remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
				      size, vma->vm_page_prot);
	} else if (__is_rtscam_isp_statis_buf(isp, phy_addr, size) ||
		   __is_rtscam_isp_allocated(isp, phy_addr, size)) {
		ret = remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
				      size, vma->vm_page_prot);
	} else {
		ret = -EPERM;
	}
	mutex_unlock(&isp->lock);
	if (ret)
		rtsprintk(RTS_TRACE_ERROR,
			  "mmap param 0x%lx, 0x%x invalid", phy_addr, size);
	return ret;
}

static struct rtscam_ge_file_operations rtscam_isp_fops = {
	.owner = THIS_MODULE,
	.open = rtscam_isp_open,
	.release = rtscam_isp_close,
	.read = rtscam_isp_read,
	.write = rtscam_isp_write,
	.unlocked_ioctl = rtscam_isp_ioctl,
	.poll = rtscam_isp_poll,
	.mmap = rtscam_isp_mmap,
};

static int __rtscam_isp_read_statis(struct rtscam_isp *isp,
				    struct rtscam_isp_statis_node *node)
{
	int i;
	struct rts_isp_statis_data *data = &node->data;

	data->frame_count = atomic_inc_return(&isp->statis_count);

	data->awb_reg.fine_r_sum = rtscam_isp_read_reg(isp, AWB_FINE_SUM_R);
	data->awb_reg.fine_g_sum = rtscam_isp_read_reg(isp, AWB_FINE_SUM_G);
	data->awb_reg.fine_b_sum = rtscam_isp_read_reg(isp, AWB_FINE_SUM_B);
	data->awb_reg.fine_white_pixels =
		rtscam_isp_read_reg(isp, AWB_FINE_WP_NUM);
	for (i = 0; i < ARRAY_SIZE(data->awb_reg.illum_white_pixels); i++)
		data->awb_reg.illum_white_pixels[i] =
			rtscam_isp_read_reg(isp, AWB_WP_NUM + i * 4);

	if (rtscam_isp_read_reg(isp, SYS_INT_FLAG1) & FLICK_STATIS_INT) {
		rtscam_isp_write_reg(isp, FLICK_STATIS_INT, SYS_INT_FLAG1);
		data->flick_reg.fft_sum2_9 =
			rtscam_isp_read_reg(isp, FLICK_FFT_RESUT_SUM2_9);
		data->flick_reg.fft_sum2_127 =
			rtscam_isp_read_reg(isp, FLICK_FFT_RESUT_SUM2_127);
		data->flick_reg.valid = true;
	} else {
		data->flick_reg.valid = false;
	}

	return 0;
}

static int rtscam_isp_write_sync_i2c(struct rtscam_isp *isp,
				     struct rts_isp_i2c_reg_mask *i2c_mask,
				     struct rts_isp_i2c_info *i2c_info)
{
	struct rts_isp_i2c_reg i2c;

	i2c.addr = i2c_mask->addr;
	if (i2c_mask->mask) {
		rtscam_isp_read_i2c_reg(isp, i2c_info, &i2c);
		i2c.data &= ~i2c_mask->mask;
		i2c.data |= (i2c_mask->data & i2c_mask->mask);
	} else {
		i2c.data = i2c_mask->data;
	}
	rtscam_isp_write_i2c_reg(isp, i2c_info, &i2c);
	return 0;
}

static int rtscam_isp_write_sync_reg(struct rtscam_isp *isp,
				     struct rts_isp_reg_mask *reg)
{
	u32 data;

	if (reg->mask) {
		data = rtscam_isp_read_reg(isp, reg->addr);
		data &= ~reg->mask;
		data |= (reg->data & reg->mask);
	} else {
		data = reg->data;
	}
	rtscam_isp_write_reg(isp, data, reg->addr);
	return 0;
}

static void __rtscam_isp_irq_sync_write_one(struct rtscam_isp *isp,
					    struct rtscam_isp_sync_item *item)
{

	if (item->reg.type == RTS_ISP_SYNC_TYPE_REG)
		rtscam_isp_write_sync_reg(isp, &item->reg.reg);
	else if (item->reg.type == RTS_ISP_SYNC_TYPE_I2C)
		rtscam_isp_write_sync_i2c(isp, &item->reg.i2c, &item->i2c_info);
}

static int rtscam_isp_irq_sync(struct rtscam_isp *isp)
{
	struct rtscam_isp_sync *sync = &isp->sync;
	enum rts_isp_interrupt irq;
	struct rtscam_isp_sync_item *item;
	struct rtscam_isp_sync_item *next;

	mutex_lock(&sync->lock);
	for (irq = RTS_ISP_INT_NONE + 1;
	     irq <= atomic_read(&sync->irq); irq++) {
		struct list_head *head;

		head = &sync->delay[sync->index].irq[irq - 1];
		list_for_each_entry_safe(item, next, head, list) {
			list_del(&item->list);
			if (!isp->stopping)
				__rtscam_isp_irq_sync_write_one(isp, item);
			__rtscam_isp_sync_put_idle(sync, item);
		}
		if (irq == _MAX_RTS_ISP_INT - 1) {
			sync->index = rtscam_isp_sync_index(sync, 1);
			atomic_set(&sync->irq, RTS_ISP_INT_NONE);
		}
	}
	mutex_unlock(&sync->lock);
	return 0;
}

static irqreturn_t rtscam_isp_irq_thread(int irq, void *data)
{
	struct rtscam_isp *isp = data;

	rtscam_isp_irq_sync(isp);

	return IRQ_HANDLED;
}

static void rtscam_isp_irq_statis(struct rtscam_isp *isp)
{
	struct rtscam_isp_statis_node *node;
	struct rtscam_isp_statis_node *busy_node;
	struct rtscam_isp_statis *statis = &isp->statis;

	if (rtscam_isp_read_reg(isp, STATIS_FRAME_STATE)) {
		rtscam_isp_write_reg(isp, 1, STATIS_FRAME_STATE);
		rtscam_isp_write_reg(isp, STATIS_STREAM_EN, STATIS_CTRL);
		rtscam_isp_write_reg(isp, ISP_OUT_LINE_CNT_INT, SYS_INT_FLAG0);
		rtscam_isp_write_reg(isp, STATIS_ALL_INT, SYS_INT_FLAG1);
		return;
	}

	spin_lock(&statis->lock);
	while (!list_empty(&statis->done)) {
		node = list_first_entry(&statis->done,
					struct rtscam_isp_statis_node, list);
		list_move(&node->list, &statis->idle);
	}
	node = list_first_entry_or_null(&statis->idle,
					struct rtscam_isp_statis_node, list);
	busy_node = list_first_entry(&statis->busy,
				     struct rtscam_isp_statis_node, list);
	if (node) {
		rtscam_isp_write_reg(isp, node->addr, STATIS_DDR_ADDR);
		rtscam_isp_write_reg(isp, STATIS_STREAM_EN, STATIS_CTRL);
		__rtscam_isp_read_statis(isp, busy_node);
		list_move_tail(&node->list, &statis->busy);

		list_move(statis->busy.next, &statis->done);
		atomic_set(&statis->valid, 1);
		wake_up(&isp->message.wq);
	} else {
		rtscam_isp_write_reg(isp, STATIS_STREAM_EN, STATIS_CTRL);
		rtsprintk(RTS_TRACE_ERROR, "statis buffer overflow\n");
	}
	rtscam_isp_write_reg(isp, ISP_OUT_LINE_CNT_INT, SYS_INT_FLAG0);
	rtscam_isp_write_reg(isp, STATIS_ALL_INT, SYS_INT_FLAG1);
	spin_unlock(&statis->lock);
}

static irqreturn_t rtscam_isp_irq(int irq, void *data)
{
	u32 int_value;
	struct rtscam_isp *isp = data;

	int_value = (rtscam_isp_read_reg(isp, SYS_INT_EN0) &
		     rtscam_isp_read_reg(isp, SYS_INT_FLAG0));

	if (int_value & ISP_OUT_DATA_START_INT) {
		rtscam_isp_write_reg(isp, ISP_OUT_DATA_START_INT,
				     SYS_INT_FLAG0);
		atomic_set(&isp->sync.irq, RTS_ISP_INT_DATA_START);
		if (!completion_done(&isp->data_start_completion))
			complete(&isp->data_start_completion);
		return IRQ_WAKE_THREAD;
	}

	if (int_value & SENSOR0_FRAME_END_INT) {
		rtscam_isp_write_reg(isp, SENSOR0_FRAME_END_INT, SYS_INT_FLAG0);
		atomic_inc(&isp->frame_count);
		atomic_set(&isp->sync.irq, RTS_ISP_INT_FRAME_END);
		if (!completion_done(&isp->frame_end_completion))
			complete(&isp->frame_end_completion);
		return IRQ_WAKE_THREAD;
	}

	int_value = (rtscam_isp_read_reg(isp, SYS_INT_EN1) &
		     rtscam_isp_read_reg(isp, SYS_INT_FLAG1));
	if (int_value & STATIS_DONE_INT) {
		rtscam_isp_write_reg(isp, STATIS_DONE_INT, SYS_INT_FLAG1);
		rtscam_isp_irq_statis(isp);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static int __create_device(struct rtscam_isp *isp)
{
	struct rtscam_ge_device *gdev;
	int ret;

	if (isp->gdev)
		return 0;

	gdev = rtscam_ge_device_alloc();
	if (!gdev)
		return -ENOMEM;

	strlcpy(gdev->name, RTS_ISP_DEV_NAME, sizeof(gdev->name));
	gdev->parent = get_device(isp->dev);
	gdev->release = rtscam_ge_device_release;
	gdev->fops = &rtscam_isp_fops;

	rtscam_ge_set_drvdata(gdev, isp);
	ret = rtscam_ge_register_device(gdev);
	if (ret) {
		rtscam_ge_device_release(gdev);
		return ret;
	}

	isp->gdev = gdev;

	return 0;
}

static void __remove_device(struct rtscam_isp *isp)
{
	struct rtscam_ge_device *gdev;

	gdev = isp->gdev;
	if (!gdev)
		return;
	put_device(gdev->parent);
	rtscam_ge_unregister_device(gdev);

	isp->gdev = NULL;
}

static int rtscam_isp_parse_i2c_adapter(struct rtscam_isp *isp,
					struct device_node *dev_node)
{
	struct device_node *i2c_node;

	i2c_node = of_parse_phandle(dev_node, "video-i2c", 0);
	if (!i2c_node)
		goto err;
	isp->adapter = of_get_i2c_adapter_by_node(i2c_node);
	of_node_put(i2c_node);
	if (!isp->adapter)
		goto err;
	/* do not retry because sensor soft reset may cause EAGAIN */
	isp->adapter->retries = 0;

	return 0;
err:
	rtsprintk(RTS_TRACE_ERROR, "get i2c adapter fail\n");
	return -ENXIO;
}

static int rtscam_isp_parse_region(struct rtscam_region *region,
				   struct device_node *dev_node,
				   const char *node_name)
{
	int ret;
	struct device_node *node;

	node = of_parse_phandle(dev_node, node_name, 0);
	if (!node)
		return -ENXIO;

	ret = of_property_read_u32_index(node, "reg", 0, &region->base);
	if (ret)
		goto exit;
	ret = of_property_read_u32_index(node, "reg", 1, &region->size);
	if (ret)
		goto exit;

exit:
	of_node_put(node);
	if (ret)
		rtsprintk(RTS_TRACE_ERROR,
			  "failed to get region %s\n", node_name);
	return ret;
}

static int rtscam_isp_parse_buffer_config(struct rtscam_isp *isp,
					  struct device_node *dev_node)
{
	int ret;

	ret = rtscam_isp_parse_region(&isp->statis_cfg, dev_node, "statis_cfg");
	if (ret)
		return ret;
	rtsprintk(RTS_TRACE_DEBUG, "statis buffer config: <0x%x 0x%x>\n",
		  isp->statis_cfg.base, isp->statis_cfg.size);
	return 0;
}

static int rtscam_isp_parse_dts(struct rtscam_isp *isp)
{
	int ret;
	struct device_node *dev_node;

	if (!isp)
		return -EINVAL;

	dev_node = isp->dev->of_node;

	isp->is_fpga = of_machine_is_compatible("realtek,rts_fpga");
	rtsprintk(RTS_TRACE_DEBUG, "is_fpga: %u\n", isp->is_fpga);

	if (of_find_property(dev_node, "io-supply", NULL) ||
	    of_find_property(dev_node, "analog-supply", NULL) ||
	    of_find_property(dev_node, "core-supply", NULL))
		isp->has_pmu = true;
	if (!isp->has_pmu) {
		isp->pwrctl_gpio_handle = devm_gpiod_get(isp->dev, "pwrctl", 0);
		if (IS_ERR(isp->pwrctl_gpio_handle))
			isp->pwrctl_gpio_handle = NULL;
	}

	if (of_find_property(dev_node, "tnr-bit", NULL)) {
		u32 val;

		ret = of_property_read_u32(dev_node, "tnr-bit", &val);
		if (ret)
			return ret;
		if (val != 8 && val != 12) {
			rtsprintk(RTS_TRACE_ERROR, "tnr-bit %d error\n", val);
			return -EINVAL;
		}
		isp->tnr_bit = val;
	} else {
		isp->tnr_bit = 12;
	}
	rtsprintk(RTS_TRACE_DEBUG, "tnr_bit: %u\n", isp->tnr_bit);

	ret = rtscam_isp_parse_i2c_adapter(isp, dev_node);
	if (ret)
		return ret;
	return rtscam_isp_parse_buffer_config(isp, dev_node);
}

static int rtscam_isp_init_register(struct rtscam_isp *isp)
{
	void __iomem *base;
	struct resource *res;
	struct device *dev = isp->dev;

	res = platform_get_resource(to_platform_device(dev), IORESOURCE_MEM, 0);
	if (res == NULL) {
		rtsprintk(RTS_TRACE_ERROR, "Missing platform resource data\n");
		return -ENODEV;
	}
	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base)) {
		rtsprintk(RTS_TRACE_ERROR, "Couldn't ioremap isp resource\n");
		return PTR_ERR(base);
	}
	isp->base = base;
	isp->io_start = res->start;
	isp->io_size = resource_size(res);
	return 0;
}

static int rtscam_isp_init_clk(struct rtscam_isp *isp)
{
	struct device *dev = isp->dev;

	isp->isp_clk = devm_clk_get(dev, "isp_clk");
	if (IS_ERR(isp->isp_clk)) {
		rtsprintk(RTS_TRACE_ERROR, "get isp clk fail\n");
		return PTR_ERR(isp->isp_clk);
	}
	isp->inf_clk = devm_clk_get(dev, "inf_clk");
	if (IS_ERR(isp->inf_clk)) {
		rtsprintk(RTS_TRACE_ERROR, "get inf clk fail\n");
		return PTR_ERR(isp->inf_clk);
	}
	isp->mipiout_clk = devm_clk_get(dev, "mipiout_clk");
	if (IS_ERR(isp->mipiout_clk)) {
		rtsprintk(RTS_TRACE_ERROR, "get mipiout clk fail\n");
		return PTR_ERR(isp->mipiout_clk);
	}

	return 0;
}

static int rtscam_isp_init_reset(struct rtscam_isp *isp)
{
	struct device *dev = isp->dev;

	isp->sysmem = devm_reset_control_get(dev, "isp-sysmem-up");
	if (IS_ERR(isp->sysmem)) {
		rtsprintk(RTS_TRACE_ERROR, "get sysmem reset fail\n");
		return PTR_ERR(isp->sysmem);
	}
	reset_control_deassert(isp->sysmem);

	isp->mipiout_sysmem = devm_reset_control_get(dev, "mipiout_sysmem_up");
	if (IS_ERR(isp->mipiout_sysmem)) {
		rtsprintk(RTS_TRACE_ERROR, "get mipiout sysmem reset fail\n");
		return PTR_ERR(isp->mipiout_sysmem);
	}
	reset_control_deassert(isp->mipiout_sysmem);

	isp->isp_reset = devm_reset_control_get(dev, "isp_reset");
	if (IS_ERR(isp->isp_reset)) {
		rtsprintk(RTS_TRACE_ERROR, "get isp reset fail\n");
		return PTR_ERR(isp->isp_reset);
	}
	isp->mipi_reset = devm_reset_control_get(dev, "mipi_reset");
	if (IS_ERR(isp->mipi_reset)) {
		rtsprintk(RTS_TRACE_ERROR, "get mipi reset fail\n");
		return PTR_ERR(isp->mipi_reset);
	}
	isp->mipiout_reset = devm_reset_control_get(dev, "mipiout_reset");
	if (IS_ERR(isp->mipiout_reset)) {
		rtsprintk(RTS_TRACE_ERROR, "get mipiout reset fail\n");
		return PTR_ERR(isp->mipiout_reset);
	}
	return 0;
}

static int rtscam_isp_init_pinctrl(struct rtscam_isp *isp)
{
	struct device *dev = isp->dev;

	isp->pins.p = devm_pinctrl_get(dev);
	if (IS_ERR(isp->pins.p)) {
		rtsprintk(RTS_TRACE_ERROR, "get pin ctrl handler fail\n");
		return -EINVAL;
	}
	isp->pins.default_state = pinctrl_lookup_state(isp->pins.p, "default");
	isp->pins.dvp_state = pinctrl_lookup_state(isp->pins.p, "dvp");
	isp->pins.mipi_state = pinctrl_lookup_state(isp->pins.p, "mipi");
	if (IS_ERR(isp->pins.default_state) ||
	    IS_ERR(isp->pins.dvp_state) || IS_ERR(isp->pins.mipi_state)) {
		rtsprintk(RTS_TRACE_ERROR, "get pin state fail\n");
		return -EINVAL;
	}

	return 0;
}

static int rtscam_isp_init_irq(struct rtscam_isp *isp)
{
	int ret;
	int irq;
	struct device *dev = isp->dev;

	irq = platform_get_irq(to_platform_device(dev), 0);
	if (irq < 0) {
		rtsprintk(RTS_TRACE_ERROR, "Missing platform resource data\n");
		return -ENODEV;
	}
	ret = devm_request_threaded_irq(dev, irq, rtscam_isp_irq,
					rtscam_isp_irq_thread, 0,
					RTS_ISP_DRV_NAME, isp);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR, "register rts isp irq fail\n");
		return ret;
	}
	isp->irq = irq;

	return 0;
}

static int rtscam_isp_init_resource(struct rtscam_isp *isp)
{
	int ret;

	ret = rtscam_isp_init_register(isp);
	if (ret)
		return ret;
	ret = rtscam_isp_init_clk(isp);
	if (ret)
		return ret;
	ret = rtscam_isp_init_reset(isp);
	if (ret)
		return ret;
	ret = rtscam_isp_init_pinctrl(isp);
	if (ret)
		return ret;
	return rtscam_isp_init_irq(isp);
}

static int rtscam_isp_init_status(struct rtscam_isp *isp)
{
	int i;
	int ret;

	atomic_set(&isp->user_count, 0);
	mutex_init(&isp->lock);
	init_completion(&isp->data_start_completion);
	init_completion(&isp->frame_end_completion);

	for (i = 0; i < ARRAY_SIZE(isp->power); i++) {
		ret = rtscam_isp_snr_power_init(isp, &isp->power[i], i);
		if (ret)
			return ret;
	}
	ret = rtscam_isp_message_init(&isp->message, &isp->lock);
	if (ret)
		return ret;
	ret = rtscam_isp_sync_init(&isp->sync);
	if (ret)
		return ret;
	isp->mem_info = rts_get_mem_info();
	if (!isp->mem_info) {
		rtsprintk(RTS_TRACE_ERROR, "get mem info fail\n");
		return -EINVAL;
	}
	INIT_LIST_HEAD(&isp->mem_list);
	return rtscam_isp_statis_mem_alloc(&isp->statis, isp->mem_info);
}

static int rtscam_isp_register_subdev(struct rtscam_isp *isp)
{
	struct rtscam_zoom_isp *zoom_isp = &isp->zoom_isp;

	memset(zoom_isp, 0, sizeof(*zoom_isp));
	zoom_isp->dev = isp->dev;
	zoom_isp->info.width = 1920;
	zoom_isp->info.height = 1080;
	zoom_isp->info.fps_max.denominator = 30;
	zoom_isp->info.fps_max.numerator = 1;
	zoom_isp->info.fps_min.denominator = 1;
	zoom_isp->info.fps_min.numerator = 1;
	zoom_isp->set_fps = rtscam_isp_subdev_set_fps;
	zoom_isp->set_hook = rtscam_isp_subdev_set_hook;

	return rtscam_zoom_register_isp(&isp->zoom_isp);
}

static int rtscam_isp_unregister_subdev(struct rtscam_isp *isp)
{
	struct rtscam_zoom_isp *zoom_isp = &isp->zoom_isp;

	if (!zoom_isp->master)
		return 0;

	return rtscam_zoom_unregister_isp(zoom_isp);
}

static int rtscam_isp_probe(struct platform_device *pdev)
{
	int ret;
	struct rtscam_isp *isp;
	struct device *dev = &pdev->dev;

	rtsprintk(RTS_TRACE_INFO, "%s\n", __func__);

	isp = devm_kzalloc(&pdev->dev, sizeof(*isp), GFP_KERNEL);
	if (!isp) {
		rtsprintk(RTS_TRACE_ERROR,
			  "Couldn't allocate rts camera isp object\n");
		return -ENOMEM;
	}
	isp->dev = get_device(dev);

	ret = rtscam_isp_init_resource(isp);
	if (ret)
		return ret;
	ret = rtscam_isp_parse_dts(isp);
	if (ret)
		return ret;
	ret = rtscam_isp_init_status(isp);
	if (ret)
		goto err;
	ret = rtscam_isp_register_subdev(isp);
	if (ret)
		goto err;

	__create_device(isp);
	device_create_file(isp->dev, &dev_attr_msg_timeout);
	device_create_file(isp->dev, &dev_attr_frame_count);
	device_create_file(isp->dev, &dev_attr_fix_clk);

	platform_set_drvdata(pdev, isp);

	return 0;

err:
	i2c_put_adapter(isp->adapter);
	rtscam_isp_statis_mem_free(&isp->statis);
	rts_put_mem_info(isp->mem_info);
	return ret;
}

static int rtscam_isp_remove(struct platform_device *pdev)
{
	struct rtscam_isp *isp = platform_get_drvdata(pdev);

	rtscam_isp_unregister_subdev(isp);
	__remove_device(isp);
	rtscam_isp_sync_cleanup(&isp->sync);
	rtscam_zoom_unregister_isp(&isp->zoom_isp);
	i2c_put_adapter(isp->adapter);
	reset_control_assert(isp->sysmem);
	reset_control_assert(isp->mipiout_sysmem);
	put_device(isp->dev);
	rtscam_isp_statis_mem_free(&isp->statis);
	rts_put_mem_info(isp->mem_info);

	device_remove_file(isp->dev, &dev_attr_msg_timeout);
	device_remove_file(isp->dev, &dev_attr_frame_count);
	device_remove_file(isp->dev, &dev_attr_fix_clk);

	return 0;
}

static const struct of_device_id rtscam_isp_ids[] = {
	{ .compatible = "realtek,rts3917-isp" },
	{ /* sentinel */ },
};

static struct platform_driver rtscam_isp_driver = {
	.driver = {
		.name = RTS_ISP_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(rtscam_isp_ids),
	},
	.probe = rtscam_isp_probe,
	.remove = rtscam_isp_remove,
};

module_platform_driver(rtscam_isp_driver);

MODULE_DESCRIPTION("Realsil isp device driver");
MODULE_AUTHOR("Grant Shen <grant_shen@realsil.com.cn>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.1.0");
MODULE_ALIAS("platform:" RTS_ISP_DRV_NAME);
