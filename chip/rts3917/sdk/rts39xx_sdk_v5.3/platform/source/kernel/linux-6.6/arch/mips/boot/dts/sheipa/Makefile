ifneq ($(CONFIG_SMP),y)
dtb-$(CONFIG_SHEIPA_MIPS) += m24k.dtb
dtb-$(CONFIG_CPU_RLX4281) += 4281.dtb
dtb-$(CONFIG_CPU_RLX5281) += 5281.dtb
else
dtb-$(CONFIG_SHEIPA_MIPS) += mia.dtb
dtb-$(CONFIG_CPU_RLX4281) += 4381.dtb
dtb-$(CONFIG_CPU_RLX5281) += 5381.dtb
endif

obj-$(CONFIG_BUILTIN_DTB) += $(addsuffix .o, $(dtb-y))
