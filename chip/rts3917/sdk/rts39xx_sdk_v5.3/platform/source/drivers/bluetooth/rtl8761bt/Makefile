KSRC := $(DIR_LINUX)
obj-m := 8761bt.o
8761bt-y = rtk_coex.o rtk_misc.o rtk_bt.o
ARCH := rlx
EXTRA_CFLAGS += -Werror

.PHONY: all clean

all: modules

modules:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KSRC) M=$(shell pwd) modules

clean:
	rm -f *.o
	rm -f *.mod.c *.mod.o
	rm -f modules.order
	rm -f *.ko
	rm -f Module.symvers
