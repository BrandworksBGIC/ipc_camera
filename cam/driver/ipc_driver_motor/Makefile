CROSS_COMPILE ?=
KERNEL_DIR ?=
MODULE_NAME := ipc_step_motor
IPC_ARCH ?=

ifeq ($(IPC_ARCH),)
$(error MUST SPECIFY A CHIP PLATFORM, IPC_ARCH=RTS3917)
endif

ccflags-y+=-DIPC_ARCH_$(IPC_ARCH)

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs += ipc_hr_timer.o
$(MODULE_NAME)-objs += ipc_motor.o
$(MODULE_NAME)-objs += linux_gpio.o
obj-m := $(MODULE_NAME).o

modules:
	@echo building
	@$(MAKE) -C $(KERNEL_DIR) M=$(shell pwd) $@

clean:
	@rm -rf *.o *~.depend.*.cmd  *.mod.c.tmp_versions *.ko *.symvers modules.order
