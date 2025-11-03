/* Realtek Semiconductor Corp.
 *
 * bsp/cpuidle.c:
 *     bsp cpuidle sorce code
 *
 * Copyright (C) 2006-2013 Jethro Hsu (jethro@realtek.com)
 */

#include <linux/sched.h>
#include <linux/cpuidle.h>
#include <linux/export.h>
#include <linux/cpu_pm.h>

#ifdef CONFIG_CPU_IDLE
#define SHEIPA3_NUM_STATES 7

/* Powerdomain basic power states */
#define PWRDM_POWER_OFF         0x0
#define PWRDM_POWER_RET         0x1
#define PWRDM_POWER_INACTIVE    0x2
#define PWRDM_POWER_ON          0x3

/*
 * cpuidle mach specific parameters
 *
 * The board code can override the default C-states definition using
 * sheipa3_pm_init_cpuidle
 */
struct cpuidle_params {
	u32 exit_latency;       /* exit_latency = sleep + wake-up latencies */
	u32 target_residency;
	u8 valid;               /* validates the C-state */
};

/*
 * The latencies/thresholds for various C states have
 * to be configured from the respective board files.
 * These are some default values (which might not provide
 * the best power savings) used on boards which do not
 * pass these details from the board file.
 */

static struct cpuidle_params cpuidle_params_table[] = {
	/* C1 */
	{2 + 2, 5, 1},
	/* C2 */
	{10 + 10, 30, 1},
	/* C3 */
	{50 + 50, 300, 1},
	/* C4 */
	{1500 + 1800, 4000, 1},
	/* C5 */
	{2500 + 7500, 12000, 1},
	/* C6 */
	{3000 + 8500, 15000, 1},
	/* C7 */
	{10000 + 30000, 300000, 1},
};

#define SHIEPA3_NUM_STATES ARRAY_SIZE(cpuidle_params_table)

/* Mach specific information to be recorded in the C-state driver_data */
struct sheipa3_idle_statedata {
	u32 mpu_state;
	u32 core_state;
	u8 valid;
};
struct sheipa3_idle_statedata sheipa3_idle_data[SHEIPA3_NUM_STATES];

/**
 * sheipa3_enter_idle - Programs SHEIPA3 to enter the specified state
 * @dev: cpuidle device
 * @drv: cpuidle driver
 * @index: the index of state to be entered
 *
 * Called from the CPUidle framework to program the device to the
 * specified target state selected by the governor.
 */
static int sheipa3_enter_idle(struct cpuidle_device *dev,
	                        struct cpuidle_driver *drv,
	                        int index)
{
	local_irq_disable();
	local_fiq_disable();
	cpu_pm_enter();
	cpu_pm_exit();
	local_irq_enable();
	local_fiq_enable();
	return index;
}

DEFINE_PER_CPU(struct cpuidle_device, sheipa3_idle_dev);

void sheipa3_pm_init_cpuidle(struct cpuidle_params *cpuidle_board_params)
{
	int i;

	if (!cpuidle_board_params)
	        return;

	for (i = 0; i < SHEIPA3_NUM_STATES; i++) {
	        cpuidle_params_table[i].valid = cpuidle_board_params[i].valid;
	        cpuidle_params_table[i].exit_latency =
	                cpuidle_board_params[i].exit_latency;
	        cpuidle_params_table[i].target_residency =
	                cpuidle_board_params[i].target_residency;
	}
	return;
}

struct cpuidle_driver sheipa3_idle_driver = {
	.name =         "sheipa3_idle",
	.owner =        THIS_MODULE,
};

/* Helper to fill the C-state common data*/
static inline void _fill_cstate(struct cpuidle_driver *drv,
	                                int idx, const char *descr)
{
	struct cpuidle_state *state = &drv->states[idx];

	state->exit_latency     = cpuidle_params_table[idx].exit_latency;
	state->target_residency = cpuidle_params_table[idx].target_residency;
	state->flags            = CPUIDLE_FLAG_TIME_VALID;
	state->enter            = sheipa3_enter_idle;
	sprintf(state->name, "C%d", idx + 1);
	strncpy(state->desc, descr, CPUIDLE_DESC_LEN);

}

/* Helper to register the driver_data */
static inline struct sheipa3_idle_statedata *_fill_cstate_usage(
	                                struct cpuidle_device *dev,
	                                int idx)
{
	struct sheipa3_idle_statedata *cx = &sheipa3_idle_data[idx];
	struct cpuidle_state_usage *state_usage = &dev->states_usage[idx];

	cx->valid               = cpuidle_params_table[idx].valid;
	cpuidle_set_statedata(state_usage, cx);

	return cx;
}

/**
 * sheipa3_idle_init - Init routine for SHEIPA3 idle
 *
 * Registers the SHEIPA3 specific cpuidle driver to the cpuidle
 * framework with the valid set of states.
 */
int __init sheipa3_idle_init(void)
{
	struct cpuidle_device *dev, *dev2;
	struct cpuidle_driver *drv = &sheipa3_idle_driver;
	struct sheipa3_idle_statedata *cx;
	int cpu;

	drv->safe_state_index = -1;

	/* C1 . MPU WFI + Core active */
	_fill_cstate(drv, 0, "MPU ON + CORE ON");
	(&drv->states[0])->enter = sheipa3_enter_idle;
	drv->safe_state_index = 0;
	for_each_online_cpu(cpu) {
		dev = &per_cpu(sheipa3_idle_dev, cpu);
		cx = _fill_cstate_usage(dev, 0);
	}
	cx->valid = 1;  /* C1 is always valid */
	cx->mpu_state = PWRDM_POWER_ON;
	cx->core_state = PWRDM_POWER_ON;

	/* C2 . MPU WFI + Core inactive */
	_fill_cstate(drv, 1, "MPU ON + CORE ON");
	for_each_online_cpu(cpu) {
	        dev = &per_cpu(sheipa3_idle_dev, cpu);
	        cx = _fill_cstate_usage(dev, 1);
	}
	cx->mpu_state = PWRDM_POWER_ON;
	cx->core_state = PWRDM_POWER_ON;

	/* C3 . MPU CSWR + Core inactive */
	_fill_cstate(drv, 2, "MPU RET + CORE ON");
	for_each_online_cpu(cpu) {
	        dev = &per_cpu(sheipa3_idle_dev, cpu);
	        cx = _fill_cstate_usage(dev, 2);
	}
	cx->mpu_state = PWRDM_POWER_RET;
	cx->core_state = PWRDM_POWER_ON;

	/* C4 . MPU OFF + Core inactive */
	_fill_cstate(drv, 3, "MPU OFF + CORE ON");
	for_each_online_cpu(cpu) {
	        dev = &per_cpu(sheipa3_idle_dev, cpu);
	        cx = _fill_cstate_usage(dev, 3);
	}
	cx->mpu_state = PWRDM_POWER_OFF;
	cx->core_state = PWRDM_POWER_ON;

	/* C5 . MPU RET + Core RET */
	_fill_cstate(drv, 4, "MPU RET + CORE RET");
	for_each_online_cpu(cpu) {
	        dev = &per_cpu(sheipa3_idle_dev, cpu);
	        cx = _fill_cstate_usage(dev, 4);
	}
	cx->mpu_state = PWRDM_POWER_RET;
	cx->core_state = PWRDM_POWER_RET;

	/* C6 . MPU OFF + Core RET */
	_fill_cstate(drv, 5, "MPU OFF + CORE RET");
	for_each_online_cpu(cpu) {
	        dev = &per_cpu(sheipa3_idle_dev, cpu);
	        cx = _fill_cstate_usage(dev, 5);
	}
	cx->mpu_state = PWRDM_POWER_OFF;
	cx->core_state = PWRDM_POWER_RET;

	/* C7 . MPU OFF + Core OFF */
	_fill_cstate(drv, 6, "MPU OFF + CORE OFF");
	for_each_online_cpu(cpu) {
	        dev = &per_cpu(sheipa3_idle_dev, cpu);
	        cx = _fill_cstate_usage(dev, 6);
	}
	cx->mpu_state = PWRDM_POWER_OFF;
	cx->core_state = PWRDM_POWER_OFF;

	drv->state_count = SHEIPA3_NUM_STATES;
	cpuidle_register_driver(&sheipa3_idle_driver);

	for_each_online_cpu(cpu) {
		dev = &per_cpu(sheipa3_idle_dev, cpu);
		dev->state_count = SHEIPA3_NUM_STATES;
		dev->cpu = cpu;
		printk("Setup online cpuidle device for cpu%d\n", cpu);

		if (cpuidle_register_device(dev)) {
	        printk(KERN_ERR "%s: CPUidle register device failed\n",
	               __func__);
	        return -EIO;
		}
	}

	return 0;
}
#else
int __init sheipa3_idle_init(void)
{
	return 0;
}
#endif /* CONFIG_CPU_IDLE */
late_initcall(sheipa3_idle_init);
