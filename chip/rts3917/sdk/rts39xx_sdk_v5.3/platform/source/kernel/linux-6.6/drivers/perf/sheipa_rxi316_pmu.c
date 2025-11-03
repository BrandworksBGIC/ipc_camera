/* SPDX-License-Identifier: GPL-2.0 */
/*
 * SHEIPA RXI316 PMU (Performance Monitor Unit)
 *
 * Copyright(c) 2021, Realtek Semiconductor Corp. All rights reserved.
 * Author: CTC PSP Software
 */
#define pr_fmt(fmt) "RXI316 PMU: " fmt

#include <linux/kernel.h>
#include <linux/bitmap.h>
#include <linux/cpu_pm.h>
#include <linux/irq.h>
#include <linux/perf_event.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/of.h>
#include <linux/sysfs.h>

#define PMU_CTRL		0x100
#define PMU_TIME_WIN_CYC	0x104
#define PMU_CNT_SEL_0		0x108
#define PMU_CNT_SEL_1		0x140
#define PMU_CNT_SEL_2		0x180
#define PMU_STATUS		0x120
#define PMU_CNT_VALUE_0		0x124
#define PMU_CNT_VALUE_1		0x160
#define PMU_CNT_VALUE_2		0x1A0
#define PMU_CNT_VALUE_3		0x1E0

#define PMU_CTRL_START_PMU	0x1
#define PMU_CTRL_START_PTRACE	0x10000
#define PMU_CNT_SEL_LOW_MASK	0xFFFF0000
#define PMU_CNT_SEL_HIGH_MASK	0x0000FFFF

#define RXI316_PMU_MAX_COUNTERS		31

#define EVENT_COUNT_APC 192
#define EVENT_ID_BB_RD_TICKS 127
#define EVENT_ID_BB_WD_TICKS 125
#define EVENT_ID_CDB_DRAM_RD_TICKS 3444
#define EVENT_ID_CDB_DRAM_WR_TICKS 3440

#define to_rxi316_pmu(p) (container_of(p, struct rxi316_pmu, pmu))

struct rxi316_pmu {
	struct pmu	pmu;
	char		*name;
	int		on_cpu;
	void            (*enable)(struct perf_event *event);
	void            (*disable)(struct perf_event *event);
	u64             (*read_counter)(struct perf_event *event);
	void            (*start)(struct rxi316_pmu *);
	void            (*stop)(struct rxi316_pmu *);
	int             (*get_event_idx)(struct rxi316_pmu *rxi316_pmu,
					 struct hw_perf_event *hwc);
	void            (*clear_event_idx)(struct rxi316_pmu *rxi316_pmu,
					   struct hw_perf_event *hwc);
	DECLARE_BITMAP(used_mask, RXI316_PMU_MAX_COUNTERS);
	void __iomem	*reg_base;
	int		num_counters;
	u64		counter_mask;
	struct platform_device	*pdev;
};

static u32 rxi316_pmu_time_window_read(struct rxi316_pmu *rxi316_pmu);
static void rxi316_pmu_time_window_write(struct rxi316_pmu *rxi316_pmu,
					 u32 value);

/*
 * sysfs format attributes
 */
PMU_FORMAT_ATTR(event, "config:0-11");

static struct attribute *rxi316_pmu_format_attrs[] = {
	&format_attr_event.attr,
	NULL,
};

static struct attribute_group rxi316_pmu_format_attr_group = {
	.name = "format",
	.attrs = rxi316_pmu_format_attrs,
};

/*
 * sysfs common attributes
 */
static ssize_t time_win_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct rxi316_pmu *rxi316_pmu = dev_get_drvdata(dev);

	return sprintf(buf, "0x%x\n", rxi316_pmu_time_window_read(rxi316_pmu));
}

static ssize_t time_win_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct rxi316_pmu *rxi316_pmu = dev_get_drvdata(dev);
	unsigned long val;
	ssize_t ret;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return ret;

	rxi316_pmu_time_window_write(rxi316_pmu, val);

	return count;
}

static DEVICE_ATTR_RW(time_win);
static struct attribute *rxi316_pmu_common_attrs[] = {
	&dev_attr_time_win.attr,
	NULL,
};

static struct attribute_group rxi316_pmu_common_attr_group = {
	.attrs = rxi316_pmu_common_attrs,
};

/*
 * sysfs event attributes
 */
static ssize_t rxi316_pmu_event_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct dev_ext_attribute *eattr;

	eattr = container_of(attr, struct dev_ext_attribute, attr);
	return sprintf(buf, "event=0x%lx\n", (unsigned long)eattr->var);
}

#define RXI316_PMU_EVENT_ATTR(_name, _config)                                  \
	(&((struct dev_ext_attribute[]){ {                                     \
		.attr = __ATTR(_name, S_IRUGO, rxi316_pmu_event_show, NULL),   \
		.var = (void *)_config,                                        \
	} })[0]                                                                \
		  .attr.attr)

static struct attribute *rxi316_pmu_events_attrs[] = {
#if CONFIG_RXI316_PORT_NUM >= 1
	RXI316_PMU_EVENT_ATTR(port0_bb_rd_ticks,
			      EVENT_ID_BB_RD_TICKS + 0 * EVENT_COUNT_APC),
	RXI316_PMU_EVENT_ATTR(port0_bb_wd_ticks,
			      EVENT_ID_BB_WD_TICKS + 0 * EVENT_COUNT_APC),
#endif
#if CONFIG_RXI316_PORT_NUM >= 2
	RXI316_PMU_EVENT_ATTR(port1_bb_rd_ticks,
			      EVENT_ID_BB_RD_TICKS + 1 * EVENT_COUNT_APC),
	RXI316_PMU_EVENT_ATTR(port1_bb_wd_ticks,
			      EVENT_ID_BB_WD_TICKS + 1 * EVENT_COUNT_APC),
#endif
#if CONFIG_RXI316_PORT_NUM >= 3
	RXI316_PMU_EVENT_ATTR(port2_bb_rd_ticks,
			      EVENT_ID_BB_RD_TICKS + 2 * EVENT_COUNT_APC),
	RXI316_PMU_EVENT_ATTR(port2_bb_wd_ticks,
			      EVENT_ID_BB_WD_TICKS + 2 * EVENT_COUNT_APC),
#endif
#if CONFIG_RXI316_PORT_NUM >= 4
	RXI316_PMU_EVENT_ATTR(port3_bb_rd_ticks,
			      EVENT_ID_BB_RD_TICKS + 3 * EVENT_COUNT_APC),
	RXI316_PMU_EVENT_ATTR(port3_bb_wd_ticks,
			      EVENT_ID_BB_WD_TICKS + 3 * EVENT_COUNT_APC),
#endif
#if CONFIG_RXI316_PORT_NUM >= 5
	RXI316_PMU_EVENT_ATTR(port4_bb_rd_ticks,
			      EVENT_ID_BB_RD_TICKS + 4 * EVENT_COUNT_APC),
	RXI316_PMU_EVENT_ATTR(port4_bb_wd_ticks,
			      EVENT_ID_BB_WD_TICKS + 4 * EVENT_COUNT_APC),
#endif
#if CONFIG_RXI316_PORT_NUM >= 6
	RXI316_PMU_EVENT_ATTR(port5_bb_rd_ticks,
			      EVENT_ID_BB_RD_TICKS + 5 * EVENT_COUNT_APC),
	RXI316_PMU_EVENT_ATTR(port5_bb_wd_ticks,
			      EVENT_ID_BB_WD_TICKS + 5 * EVENT_COUNT_APC),
#endif
#if CONFIG_RXI316_PORT_NUM >= 7
	RXI316_PMU_EVENT_ATTR(port6_bb_rd_ticks,
			      EVENT_ID_BB_RD_TICKS + 6 * EVENT_COUNT_APC),
	RXI316_PMU_EVENT_ATTR(port6_bb_wd_ticks,
			      EVENT_ID_BB_WD_TICKS + 6 * EVENT_COUNT_APC),
#endif
#if CONFIG_RXI316_PORT_NUM >= 8
	RXI316_PMU_EVENT_ATTR(port7_bb_rd_ticks,
			      EVENT_ID_BB_RD_TICKS + 7 * EVENT_COUNT_APC),
	RXI316_PMU_EVENT_ATTR(port7_bb_wd_ticks,
			      EVENT_ID_BB_WD_TICKS + 7 * EVENT_COUNT_APC),
#endif
#if CONFIG_RXI316_PORT_NUM >= 9
	RXI316_PMU_EVENT_ATTR(port8_bb_rd_ticks,
			      EVENT_ID_BB_RD_TICKS + 8 * EVENT_COUNT_APC),
	RXI316_PMU_EVENT_ATTR(port8_bb_wd_ticks,
			      EVENT_ID_BB_WD_TICKS + 8 * EVENT_COUNT_APC),
#endif
#if CONFIG_RXI316_PORT_NUM >= 10
	RXI316_PMU_EVENT_ATTR(port9_bb_rd_ticks,
			      EVENT_ID_BB_RD_TICKS + 9 * EVENT_COUNT_APC),
	RXI316_PMU_EVENT_ATTR(port9_bb_wd_ticks,
			      EVENT_ID_BB_WD_TICKS + 9 * EVENT_COUNT_APC),
#endif
#if CONFIG_RXI316_PORT_NUM >= 11
	RXI316_PMU_EVENT_ATTR(port10_bb_rd_ticks,
			      EVENT_ID_BB_RD_TICKS + 10 * EVENT_COUNT_APC),
	RXI316_PMU_EVENT_ATTR(port10_bb_wd_ticks,
			      EVENT_ID_BB_WD_TICKS + 10 * EVENT_COUNT_APC),
#endif
#if CONFIG_RXI316_PORT_NUM >= 12
	RXI316_PMU_EVENT_ATTR(port11_bb_rd_ticks,
			      EVENT_ID_BB_RD_TICKS + 11 * EVENT_COUNT_APC),
	RXI316_PMU_EVENT_ATTR(port11_bb_wd_ticks,
			      EVENT_ID_BB_WD_TICKS + 11 * EVENT_COUNT_APC),
#endif
#if CONFIG_RXI316_PORT_NUM >= 13
	RXI316_PMU_EVENT_ATTR(port12_bb_rd_ticks,
			      EVENT_ID_BB_RD_TICKS + 12 * EVENT_COUNT_APC),
	RXI316_PMU_EVENT_ATTR(port12_bb_wd_ticks,
			      EVENT_ID_BB_WD_TICKS + 12 * EVENT_COUNT_APC),
#endif
#if CONFIG_RXI316_PORT_NUM >= 14
	RXI316_PMU_EVENT_ATTR(port13_bb_rd_ticks,
			      EVENT_ID_BB_RD_TICKS + 13 * EVENT_COUNT_APC),
	RXI316_PMU_EVENT_ATTR(port13_bb_wd_ticks,
			      EVENT_ID_BB_WD_TICKS + 13 * EVENT_COUNT_APC),
#endif
#if CONFIG_RXI316_PORT_NUM >= 15
	RXI316_PMU_EVENT_ATTR(port14_bb_rd_ticks,
			      EVENT_ID_BB_RD_TICKS + 14 * EVENT_COUNT_APC),
	RXI316_PMU_EVENT_ATTR(port14_bb_wd_ticks,
			      EVENT_ID_BB_WD_TICKS + 14 * EVENT_COUNT_APC),
#endif
#if CONFIG_RXI316_PORT_NUM >= 16
	RXI316_PMU_EVENT_ATTR(port15_bb_rd_ticks,
			      EVENT_ID_BB_RD_TICKS + 15 * EVENT_COUNT_APC),
	RXI316_PMU_EVENT_ATTR(port15_bb_wd_ticks,
			      EVENT_ID_BB_WD_TICKS + 15 * EVENT_COUNT_APC),
#endif
	RXI316_PMU_EVENT_ATTR(cdb_dram_rd_ticks,
			      EVENT_ID_CDB_DRAM_RD_TICKS -
				      (16 - CONFIG_RXI316_PORT_NUM) *
					      EVENT_COUNT_APC),
	RXI316_PMU_EVENT_ATTR(cdb_dram_wr_ticks,
			      EVENT_ID_CDB_DRAM_WR_TICKS -
				      (16 - CONFIG_RXI316_PORT_NUM) *
					      EVENT_COUNT_APC),
	NULL,
};

static const struct attribute_group rxi316_pmu_events_attr_group = {
	.name = "events",
	.attrs = rxi316_pmu_events_attrs,
};

static const struct attribute_group *rxi316_pmu_attr_groups[] = {
	&rxi316_pmu_common_attr_group,
	&rxi316_pmu_format_attr_group,
	&rxi316_pmu_events_attr_group,
	NULL,
};

/*
 * Low-level functions
 */
static u32 rxi316_pmu_time_window_read(struct rxi316_pmu *rxi316_pmu)
{
	return readl(rxi316_pmu->reg_base + PMU_TIME_WIN_CYC);
}

static void rxi316_pmu_time_window_write(struct rxi316_pmu *rxi316_pmu,
					 u32 value)
{
	writel(value, rxi316_pmu->reg_base + PMU_TIME_WIN_CYC);
}

static void rxi316_pmu_enable_ptrace(struct rxi316_pmu *rxi316_pmu)
{
	writel(PMU_CTRL_START_PTRACE, rxi316_pmu->reg_base + PMU_CTRL);
}

static void rxi316_pmu_enable_counter(struct rxi316_pmu *rxi316_pmu)
{
	u32 value;

	value = readl(rxi316_pmu->reg_base + PMU_CTRL);
	value |= PMU_CTRL_START_PMU;
	writel(value, rxi316_pmu->reg_base + PMU_CTRL);
}

static void rxi316_pmu_disable_counter(struct rxi316_pmu *rxi316_pmu)
{
	u32 value;

	value = readl(rxi316_pmu->reg_base + PMU_CTRL);
	value &= ~PMU_CTRL_START_PMU;
	writel(value, rxi316_pmu->reg_base + PMU_CTRL);
}

static u64 rxi316_pmu_read_counter(struct rxi316_pmu *rxi316_pmu, int idx)
{
	u32 pmu_cnt_value;

	switch (idx) {
	case 0 ... 6:
		pmu_cnt_value = PMU_CNT_VALUE_0 + (idx << 2);
		break;
	case 7 ... 14:
		pmu_cnt_value = PMU_CNT_VALUE_1 + ((idx - 7) << 2);
		break;
	case 15 ... 22:
		pmu_cnt_value = PMU_CNT_VALUE_2 + ((idx - 15) << 2);
		break;
	case 23 ... 30:
		pmu_cnt_value = PMU_CNT_VALUE_3 + ((idx - 23) << 2);
		break;
	default:
		return -EINVAL;
	}

	return readl(rxi316_pmu->reg_base + pmu_cnt_value);
}

static void rxi316_pmu_counter_event_select(struct rxi316_pmu *rxi316_pmu,
					    int idx, int event)
{
	u32 pmu_cnt_sel, value;

	switch (idx) {
	case 0 ... 11:
		pmu_cnt_sel = PMU_CNT_SEL_0 + ((idx >> 1) << 2);
		break;
	case 12 ... 27:
		pmu_cnt_sel = PMU_CNT_SEL_1 + (((idx - 12) >> 1) << 2);
		break;
	case 28 ... 30:
		pmu_cnt_sel = PMU_CNT_SEL_2 + (((idx - 28) >> 1) << 2);
		break;
	default:
		return;
	}

	value = readl(rxi316_pmu->reg_base + pmu_cnt_sel);
	if (idx & 0x1)
		value = (value & PMU_CNT_SEL_HIGH_MASK) | (event << 16);
	else
		value = (value & PMU_CNT_SEL_LOW_MASK) | event;
	writel(value, rxi316_pmu->reg_base + pmu_cnt_sel);
}

static void rxi316_pmu_enable_event(struct perf_event *event)
{
	struct rxi316_pmu *rxi316_pmu = to_rxi316_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;

	rxi316_pmu_counter_event_select(rxi316_pmu, hwc->idx, hwc->config);
}

static void rxi316_pmu_disable_event(struct perf_event *event)
{
	struct rxi316_pmu *rxi316_pmu = to_rxi316_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;

	rxi316_pmu_counter_event_select(rxi316_pmu, hwc->idx, 0);
}

static int rxi316_pmu_get_event_idx(struct rxi316_pmu *rxi316_pmu,
				    struct hw_perf_event *hwc)
{
	int idx;

	for (idx = 0; idx < rxi316_pmu->num_counters; ++idx) {
		if (!test_and_set_bit(idx, rxi316_pmu->used_mask))
			return idx;
	}

	/* The counters are all in use. */
	return -ENOSPC;
}

static void rxi316_pmu_clear_event_idx(struct rxi316_pmu *rxi316_pmu,
				       struct hw_perf_event *hwc)
{
	clear_bit(hwc->idx, rxi316_pmu->used_mask);
}

static void rxi316_pmu_set_period(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;

	local64_set(&hwc->prev_count, 0);
	perf_event_update_userpage(event);
}

static void rxi316_pmu_event_update(struct perf_event *event)
{
	struct rxi316_pmu *rxi316_pmu = to_rxi316_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	u64 delta, prev, now;

	do {
		prev = local64_read(&hwc->prev_count);
		now = rxi316_pmu_read_counter(rxi316_pmu, hwc->idx);
	} while (local64_cmpxchg(&hwc->prev_count, prev, now) != prev);

	/* handle overflow. */
	delta = now - prev;
	delta &= rxi316_pmu->counter_mask;

	local64_add(delta, &event->count);
}

/*
 * Implementation of abstract pmu functionality required by
 * the core perf events code.
 */
static void rxi316_pmu_read(struct perf_event *event)
{
	rxi316_pmu_event_update(event);
}

static void rxi316_pmu_start(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;

	hwc->state = 0;

	rxi316_pmu_set_period(event);
	rxi316_pmu_enable_event(event);
}

static void rxi316_pmu_stop(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;

	if (hwc->state & PERF_HES_STOPPED)
		return;

	rxi316_pmu_disable_event(event);
	rxi316_pmu_event_update(event);

	hwc->state |= PERF_HES_STOPPED | PERF_HES_UPTODATE;
}

static int rxi316_pmu_add(struct perf_event *event, int flags)
{
	struct rxi316_pmu *rxi316_pmu = to_rxi316_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	int idx;

	/* If we don't have a space for the counter then finish early. */
	idx = rxi316_pmu_get_event_idx(rxi316_pmu, hwc);
	if (idx < 0)
		return idx;

	/*
	 * If there is an event in the counter we are going to use then make
	 * sure it is disabled.
	 */
	hwc->idx = idx;
	rxi316_pmu_disable_event(event);

	hwc->state = PERF_HES_UPTODATE | PERF_HES_STOPPED;
	if (flags & PERF_EF_START)
		rxi316_pmu_start(event, flags);

	/* Propagate our changes to the userspace mapping. */
	perf_event_update_userpage(event);

	return 0;
}

static void rxi316_pmu_del(struct perf_event *event, int flags)
{
	struct rxi316_pmu *rxi316_pmu = to_rxi316_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;

	rxi316_pmu_stop(event, flags | PERF_EF_UPDATE);
	rxi316_pmu_clear_event_idx(rxi316_pmu, hwc);

	perf_event_update_userpage(event);

	/* Clear the allocated counter */
	hwc->idx = -1;
}

static int validate_event(struct pmu *pmu, struct perf_event *event,
			  int *counters)
{
	/* Don't allow groups with mixed PMUs, except for s/w events */
	if (is_software_event(event))
		return 0;

	/* Reject groups spanning multiple HW PMUs. */
	if (event->pmu != pmu)
		return -EINVAL;

	*counters = *counters + 1;
	return 0;
}

static int validate_group(struct perf_event *event)
{
	struct perf_event *sibling, *leader = event->group_leader;
	int counters = 0;

	if (validate_event(event->pmu, leader, &counters))
		return -EINVAL;

	for_each_sibling_event (sibling, event->group_leader) {
		if (validate_event(event->pmu, sibling, &counters))
			return -EINVAL;
	}

	if (validate_event(event->pmu, event, &counters))
		return -EINVAL;

	/*
	 * If the group requires more counters than the HW has,
	 * it cannot ever be scheduled.
	 */
	if (counters > RXI316_PMU_MAX_COUNTERS)
		return -EINVAL;

	return 0;
}

static int rxi316_pmu_event_init(struct perf_event *event)
{
	struct rxi316_pmu *rxi316_pmu = to_rxi316_pmu(event->pmu);
	struct perf_event_attr *attr = &event->attr;
	struct hw_perf_event *hwc = &event->hw;

	/* This is, of course, deeply driver-specific */
	if (attr->type != event->pmu->type)
		return -ENOENT;

	if (attr->exclude_idle)
		return -EOPNOTSUPP;

	if (is_sampling_event(event)) {
		pr_debug("Sampling not supported\n");
		return -EOPNOTSUPP;
	}

	if (event->attach_state & PERF_ATTACH_TASK) {
		pr_debug("Per-task mode not supported\n");
		return -EOPNOTSUPP;
	}

	hwc->idx = -1;
	hwc->config = attr->config;

	if (event->group_leader != event) {
		if (validate_group(event) != 0)
			return -EINVAL;
	}

	/*
	 * Ensure all events are on the same cpu so all events are in the
	 * same cpu context, to avoid races on pmu_enable etc.
	 */
	event->cpu = rxi316_pmu->on_cpu;
	return 0;
}

void rxi316_pmu_enable(struct pmu *pmu)
{
	struct rxi316_pmu *rxi316_pmu = to_rxi316_pmu(pmu);

	if (bitmap_weight(rxi316_pmu->used_mask, RXI316_PMU_MAX_COUNTERS))
		rxi316_pmu_enable_counter(rxi316_pmu);
}

void rxi316_pmu_disable(struct pmu *pmu)
{
	struct rxi316_pmu *rxi316_pmu = to_rxi316_pmu(pmu);

	rxi316_pmu_disable_counter(rxi316_pmu);
}
/*
 * Generic device handlers
 */
typedef int (*rxi316_pmu_init_fn)(struct rxi316_pmu *rxi316_pmu);

static int rxi316_pmu_init(struct rxi316_pmu *rxi316_pmu)
{
	struct resource *res;
	void __iomem *base;

	res = platform_get_resource(rxi316_pmu->pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&rxi316_pmu->pdev->dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	rxi316_pmu->reg_base = base;
	rxi316_pmu->num_counters = RXI316_PMU_MAX_COUNTERS;
	rxi316_pmu->counter_mask = GENMASK_ULL(31, 0);

	/* time window default cycles: 0x1000 */
	rxi316_pmu_time_window_write(rxi316_pmu, 0x1000);

	/* set start performance trace bit */
	rxi316_pmu_enable_ptrace(rxi316_pmu);

	return 0;
}

static const struct of_device_id rxi316_pmu_of_device_ids[] = {
	{ .compatible	= "realtek,rxi316-pmu", .data = rxi316_pmu_init },
	{},
};
MODULE_DEVICE_TABLE(of, rxi316_pmu_of_match);

static struct rxi316_pmu *rxi316_pmu_alloc(void)
{
	struct rxi316_pmu *pmu;

	pmu = kzalloc(sizeof(*pmu), GFP_KERNEL);
	if (!pmu) {
		pr_info("failed to allocate PMU device!\n");
		goto out;
	}

	pmu->pmu = (struct pmu){
		.module		= THIS_MODULE,
		.task_ctx_nr    = perf_invalid_context,
		.pmu_enable	= rxi316_pmu_enable,
		.pmu_disable	= rxi316_pmu_disable,
		.event_init	= rxi316_pmu_event_init,
		.add		= rxi316_pmu_add,
		.del		= rxi316_pmu_del,
		.start		= rxi316_pmu_start,
		.stop		= rxi316_pmu_stop,
		.read		= rxi316_pmu_read,
		.attr_groups	= rxi316_pmu_attr_groups,
		.capabilities	= PERF_PMU_CAP_NO_EXCLUDE |
				  PERF_PMU_CAP_NO_INTERRUPT,
	};

	/* Pick one CPU to be the preferred one to use */
	pmu->on_cpu = raw_smp_processor_id();

	return pmu;
out:
	return NULL;
}

static void rxi316_pmu_free(struct rxi316_pmu *pmu)
{
	kfree(pmu);
}

int rxi316_pmu_device_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id;
	struct device_node *node = pdev->dev.of_node;
	rxi316_pmu_init_fn init_fn;
	struct rxi316_pmu *pmu;
	int ret = -ENODEV;

	pmu = rxi316_pmu_alloc();
	if (!pmu)
		return -ENOMEM;

	pmu->pdev = pdev;
	platform_set_drvdata(pdev, pmu);

	if (node) {
		of_id = of_match_node(rxi316_pmu_of_device_ids, node);
		if (of_id) {
			init_fn = of_id->data;
			ret = init_fn(pmu);
		}
	}

	if (ret) {
		pr_err("%pOF: failed to probe PMU!\n", node);
		goto out_free;
	}

	if (!pmu->name)
		pmu->name = "rxi316";

	ret = perf_pmu_register(&pmu->pmu, pmu->name, -1);
	if (ret)
		goto out_free;

	return 0;

out_free:
	pr_err("%pOF: failed to register PMU devices!\n", node);
	rxi316_pmu_free(pmu);
	return ret;
}

int rxi316_pmu_device_remove(struct platform_device *pdev)
{
	struct rxi316_pmu *pmu = platform_get_drvdata(pdev);

	perf_pmu_unregister(&pmu->pmu);
	rxi316_pmu_free(pmu);

	return 0;
}

static struct platform_driver rxi316_pmu_driver = {
	.driver = {
		.name		= "rxi316-pmu",
		.of_match_table = rxi316_pmu_of_device_ids,
		.suppress_bind_attrs = true,
	},
	.probe = rxi316_pmu_device_probe,
	.remove = rxi316_pmu_device_remove,
};

module_platform_driver(rxi316_pmu_driver);

MODULE_DESCRIPTION("RXI316 PMU driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Mickey Zhu <mickey_zhu@realsil.com.cn>");
