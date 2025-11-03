#
#  Copyright (C) 2020 Realtek Semiconductor Corp.
#  All Rights Reserved
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#

KBUILD_CPPFLAGS += -I$(srctree)/arch/arm/mach-realtek/ -I$(srctree)/arch/arm/mach-realtek/include

obj-y := setup.o
obj-$(CONFIG_SMP) += smp.o headsmp.o

obj-$(CONFIG_PM) += pm.o
obj-$(CONFIG_RTS3917_SUSPEND_TO_RAM) += pm-rts3917.o
obj-$(CONFIG_RTS3917_SUSPEND_TO_RAM) += suspend-rts3917.o
