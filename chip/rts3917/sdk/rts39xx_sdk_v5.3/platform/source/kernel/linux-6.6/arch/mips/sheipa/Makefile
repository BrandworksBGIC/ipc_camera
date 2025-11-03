# Realtek Semiconductor Corp.
#
# sheipa/Makefile
#     Sheipa Platform Makefile
#
# Copyright (C) 2006-2022 Tony Wu (tonywu@realtek.com)
#

obj-y := setup.o prom.o timer.o
obj-$(CONFIG_PCI) += pci.o pci-fixup.o pci-ops.o
obj-$(CONFIG_SMP) += smp.o
obj-$(CONFIG_EARLY_PRINTK) += early-printk.o
obj-$(CONFIG_MTD_SPI)	   += flash.o
