/*
 * Realtek Semiconductor Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Copyright 2014  Tony Wu (tonywu@realtek.com)
 */

#include <linux/cpufreq.h>

#ifdef CONFIG_CPU_FREQ
static void plat_set_cpu_voltage(unsigned long khz, int force)
{
	pr_info("cpufreq: set voltage to %lu\n", khz);
}

static int plat_cpufreq_notifier(struct notifier_block *nb,
				unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = data;

	/* TODO: Adjust timings??? */

	switch (val) {
	case CPUFREQ_PRECHANGE:
		if (freq->old < freq->new) {
			/* we are getting faster so raise the voltage
			 * before we change freq */
			plat_set_cpu_voltage(freq->new, 0);
		}
		break;
	case CPUFREQ_POSTCHANGE:
		if (freq->old > freq->new) {
			/* we are slowing down so drop the power
			 * after we change freq */
			plat_set_cpu_voltage(freq->new, 0);
		}
		break;
	default:
		/* ignore */
		break;
	}

	return 0;
}

static struct notifier_block plat_cpufreq_notifier_block = {
	.notifier_call  = plat_cpufreq_notifier
};

void __init plat_init_cpufreq(void)
{
	if (cpufreq_register_notifier(&plat_cpufreq_notifier_block,
				      CPUFREQ_TRANSITION_NOTIFIER))
		pr_err("rtk: Failed to setup cpufreq notifier\n");
}
#else
static inline void plat_init_cpufreq(void) {}
#endif
