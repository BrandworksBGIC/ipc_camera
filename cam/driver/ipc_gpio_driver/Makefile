CROSS_COMPILE ?= 

KERNEL_DIR ?= 

KDIR := ${KERNEL_DIR}
MODULE_NAME := ipc-gpio

all: modules

.PHONY: modules clean

$(MODULE_NAME)-objs := gpio.o
obj-m := $(MODULE_NAME).o

modules:
	@$(MAKE) -C $(KDIR) M=$(shell pwd) $@

clean:
	@rm -rf *.o *~ .depend .*.cmd  *.mod.c .tmp_versions *.ko *.symvers modules.order
