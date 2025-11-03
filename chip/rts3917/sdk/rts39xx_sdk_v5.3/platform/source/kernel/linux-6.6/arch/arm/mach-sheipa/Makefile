# Realtek Semiconductor Corp.
#
# sheipa/Makefile
#     Sheipa Platform Makefile
#
# Copyright (C) 2006-2022 Tony Wu (tonywu@realtek.com)
#

obj-y := setup.o
obj-$(CONFIG_PCI) += pci.o pci-fixup.o pci-ops.o
obj-$(CONFIG_SMP) += smp.o headsmp.o
obj-$(CONFIG_HOTPLUG_CPU) += hotplug.o
obj-$(CONFIG_CPU_FREQ) += cpufreq.o

KBUILD_CPPFLAGS += -I$(srctree)/arch/arm/mach-sheipa/ -I$(srctree)/arch/arm/mach-sheipa/include
