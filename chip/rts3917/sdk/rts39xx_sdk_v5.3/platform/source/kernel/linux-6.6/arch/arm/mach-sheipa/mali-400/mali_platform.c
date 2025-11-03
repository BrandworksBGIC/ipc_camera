/*
 * Copyright (C) 2010 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file mali_platform.c
 * Platform specific Mali driver functions for a default platform
 */
#include <linux/version.h>
#include "mali_kernel_common.h"
#include "mali_osk.h"
#include "mali_platform.h"
//#include "mali_linux_pm.h"

#ifdef USING_MALI_PMM
#include "mali_pmm.h"
#endif

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>


#include <asm/io.h>

typedef struct mali_runtime_resumeTag{
	int clk;
	int vol;
}mali_runtime_resume_table;

mali_runtime_resume_table mali_runtime_resume = {400, 1100000};

static struct clk  *ext_xtal_clock = 0;
static struct clk  *vpll_src_clock = 0;
static struct clk  *fout_vpll_clock = 0;
static struct clk  *sclk_vpll_clock = 0;

static struct clk  *mpll_clock = 0;
static struct clk  *mali_parent_clock = 0;
static struct clk  *mali_clock = 0;
static struct clk  *mali_mout_clock = 0;


static unsigned int GPU_MHZ	= 1000000;

/* Please take special care lowering these values, specially the voltage
 * as it can cause system stability problems: random oops, usb hub resets */
int mali_gpu_clk = 533; /* 533 MHz */
int mali_gpu_vol = 1125000; /* 1.125 V */

#ifdef CONFIG_MALI_DVFS
#define MALI_DVFS_DEFAULT_STEP 0
#endif

int  gpu_power_state;
static int bPoweroff;

#ifdef CONFIG_REGULATOR
struct regulator {
	struct device *dev;
	struct list_head list;
	unsigned int always_on:1;
	int uA_load;
	int min_uV;
	int max_uV;
	char *supply_name;
	struct device_attribute dev_attr;
	struct regulator_dev *rdev;
	struct dentry *debugfs;
};
struct regulator *g3d_regulator = NULL;
#endif


mali_io_address clk_register_map=0;

_mali_osk_mutex_t *mali_dvfs_lock = 0;

void mali_set_runtime_resume_params(int clk, int volt)
{
	mali_runtime_resume.clk = clk;
	mali_runtime_resume.vol = volt;
}

#ifdef CONFIG_REGULATOR
int mali_regulator_get_usecount(void)
{
	return 0;
}

void mali_regulator_disable(void)
{
}

void mali_regulator_enable(void)
{
}

void mali_regulator_set_voltage(int min_uV, int max_uV)
{
}
#endif

unsigned long mali_clk_get_rate(void)
{
	return clk_get_rate(mali_clock);
}

mali_bool mali_clk_get(mali_bool bis_vpll)
{
	return MALI_TRUE;
}

void mali_clk_put(mali_bool binc_mali_clock)
{
}

mali_bool mali_clk_set_rate(unsigned int clk, unsigned int mhz)
{
	return MALI_TRUE;
}

static mali_bool init_mali_clock(struct platform_device *pdev)
{
	return MALI_TRUE;
}

static mali_bool deinit_mali_clock(void)
{
	return MALI_TRUE;
}

_mali_osk_errcode_t mali_platform_init()
{
	MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_deinit()
{

	MALI_SUCCESS;
}

void mali_gpu_utilization_handler(u32 utilization)
{
}
