#
# DTS Makefile for 4.1+
#
# Tony Wu (tonywu@realtek.com)
# Apr. 17, 2015
#

dtb-$(CONFIG_ARCH_SHEIPA) += cortex-a55.dtb
dtb-$(CONFIG_ARCH_SHEIPA) += cortex-a53.dtb
dtb-$(CONFIG_ARCH_SHEIPA) += cortex-a75-a55.dtb

obj-$(CONFIG_BUILTIN_DTB) += $(addsuffix .o, $(dtb-y))
