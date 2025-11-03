KSRC := $(DIR_LINUX)
obj-m := 8723dbt.o
8723dbt-y = rtk_coex.o rtk_bt.o rtk_misc.o
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
