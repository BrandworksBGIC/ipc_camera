#
# DTS Makefile for 4.1+
#
# Tony Wu (tonywu@realtek.com)
# Apr. 17, 2015
#

dtb-$(CONFIG_ARCH_SHEIPA) += tr9-rv32-mp.dtb
dtb-$(CONFIG_ARCH_SHEIPA) += tr9-rv64-mp.dtb
dtb-$(CONFIG_ARCH_SHEIPA) += tr9-rv32.dtb
dtb-$(CONFIG_ARCH_SHEIPA) += tr9-rv64.dtb

obj-$(CONFIG_BUILTIN_DTB) += $(addsuffix .o, $(dtb-y))
