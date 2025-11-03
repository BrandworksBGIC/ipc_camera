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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include "rts_camera.h"
#include "rts_camera_waitqueue.h"

#define RTS_WQUEUE_NUM_DFT		1

struct rtscam_wq_item {
	struct list_head list;
	struct completion completion;
};

struct rts_wait_queue *rtscam_allocate_wait_queue(struct device *dev)
{
	struct rts_wait_queue *queue;

	queue = vzalloc(sizeof(*queue));
	if (!queue)
		return NULL;

	queue->refcount = 0;
	queue->value = RTS_WQUEUE_NUM_DFT;
	mutex_init(&queue->lock);
	INIT_LIST_HEAD(&queue->queue);
	queue->dev = get_device(dev);

	return queue;
}

int rtscam_release_wait_queue(struct rts_wait_queue *queue)
{
	struct rtscam_wq_item *item;
	struct rtscam_wq_item *tmp;

	if (!queue)
		return -EINVAL;

	mutex_lock(&queue->lock);

	list_for_each_entry_safe(item, tmp, &queue->queue, list) {
		list_del_init(&item->list);
		complete(&item->completion);
	}
	INIT_LIST_HEAD(&queue->queue);

	mutex_unlock(&queue->lock);

	put_device(queue->dev);
	queue->dev = NULL;

	vfree(queue);

	return 0;
}

int rtscam_init_wait_queue(struct rts_wait_queue *queue, int num)
{
	if (!queue)
		return -EINVAL;

	if (num <= 0)
		num = RTS_WQUEUE_NUM_DFT;

	queue->value = num;

	return 0;
}

static int __wait(struct rts_wait_queue *queue, unsigned long timeout)
{
	struct rtscam_wq_item *item;
	int ret;

	item = vzalloc(sizeof(*item));
	if (!item) {
		rtsprintk(RTS_TRACE_WQ, "alloc wait item fail\n");
		return -ENOMEM;
	}

	init_completion(&item->completion);
	list_add_tail(&item->list, &queue->queue);

	mutex_unlock(&queue->lock);
	if (timeout > 0)
		ret = wait_for_completion_timeout(&item->completion,
						  timeout * HZ / 1000);
	else
		wait_for_completion(&item->completion);

	mutex_lock(&queue->lock);
	if (timeout > 0 && ret == 0) {
		list_del_init(&item->list);
		ret = -ETIMEDOUT;
	}
	vfree(item);
	item = NULL;

	return ret;
}

static int __post(struct rts_wait_queue *queue)
{
	struct rtscam_wq_item *item;

	if (list_empty(&queue->queue))
		return 0;

	item = list_first_entry(&queue->queue, struct rtscam_wq_item, list);
	list_del_init(&item->list);
	complete(&item->completion);

	return 0;
}

int rtscam_wait_timeout(struct rts_wait_queue *queue, unsigned long timeout)
{
	int ret = 0;

	if (!queue)
		return -EINVAL;

	mutex_lock(&queue->lock);

	queue->refcount++;
	if (queue->refcount > queue->value)
		ret = __wait(queue, timeout);

	mutex_unlock(&queue->lock);

	return ret;
}

int rtscam_wait(struct rts_wait_queue *queue)
{
	return rtscam_wait_timeout(queue, 0);
}

int rtscam_post(struct rts_wait_queue *queue)
{

	if (!queue)
		return -EINVAL;

	mutex_lock(&queue->lock);

	__post(queue);
	if (queue->refcount > 0)
		queue->refcount--;

	mutex_unlock(&queue->lock);

	return 0;
}
