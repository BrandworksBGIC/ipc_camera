#include <linux/fb.h>
#include <linux/platform_device.h>
#include "sheipa.h"

/* Resource for the LCD controller platform device */
static struct resource hdmi_resource[] = {
	[0] = {
		.start = BSP_HDMI_MAPBASE,
		.end   = (BSP_HDMI_MAPBASE + BSP_HDMI_MAPSIZE - 1),
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = BSP_VDMA_MAPBASE,
		.end   = (BSP_VDMA_MAPBASE + BSP_VDMA_MAPSIZE -1),
		.flags = IORESOURCE_MEM,
	},
};

/* Platform device definition */
static struct platform_device hdmi_device = {
	.name = "sheipa_hdmi",
	.id   = -1,
	.num_resources = ARRAY_SIZE(hdmi_resource),
	.resource = hdmi_resource,
	.dev = {
		.coherent_dma_mask = 0xffffffff,
	},
};

/* Module Initialization */
int __init plat_hdmi_init(void)
{
	int retval;
	printk("BSP SHEIPA HDMI INIT\n");
	retval = platform_device_register(&hdmi_device);
	if (retval < 0) {
		printk("ERROR: unable to add frame buff devices\n");
		return retval;
	}
	return retval;
}
arch_initcall(plat_hdmi_init);

