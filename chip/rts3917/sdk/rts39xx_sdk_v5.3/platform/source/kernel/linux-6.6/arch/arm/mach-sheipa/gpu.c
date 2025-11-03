#include <linux/platform_device.h>

#include <linux/mali/mali_utgard.h>
#include "sheipa.h"

static struct mali_gpu_device_data mali_gpu_data = {
	/* Mali OS memory limit */
	.shared_mem_size = 256 * 1024 * 1024, /* 256MB */
	/* DVFS */
	/* Utilization is not implemented
	.utilization_interval = 1000,
	.utilization_callback = <utilization function>,
	*/
	/* PMU power domain configuration */
//	.pmu_domain_config = {0x1, 0x2, 0x4, 0x4, 0x4, 0x8, 0x8, 0x8, 0x8, 0x1, 0x2, 0x8},
};

/* Resource for the LCD controller platform device */
static struct resource mali_gpu_resources[] = {
	MALI_GPU_RESOURCES_MALI400_MP2(	/* Mali400 MP2 */
		BSP_GPU_MAPBASE,	/* Base addr */
		BSP_IRQ_MALI,		/* GP IRQ */
		BSP_IRQ_MALI,		/* GP MMU IRQ */
		BSP_IRQ_MALI,		/* PP0 IRQ */
		BSP_IRQ_MALI,		/* PP0 MMU IRQ */
		BSP_IRQ_MALI,		/* PP1 IRQ */
		BSP_IRQ_MALI)		/* PP1 MMU IRQ */
};

/* Platform device definition */
struct platform_device mali_gpu_device = {
	.name = MALI_GPU_NAME_UTGARD,
	.id = 0,
	.num_resources = ARRAY_SIZE(mali_gpu_resources),
	.resource = mali_gpu_resources,
	.dev = {
		.coherent_dma_mask = 0xffffffff,
		.platform_data = &mali_gpu_data,
	},
};

/* Module Initialization */
int __init plat_gpu_init(void)
{
	int retval;
	printk("BSP MALI400 GPU INIT\n");
	retval = platform_device_register(&mali_gpu_device);
	if (retval < 0) {
		printk("ERROR: unable to add mali400 gpu devices\n");
		return retval;
	}
	return retval;
}
arch_initcall(plat_gpu_init);

