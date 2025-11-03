KSRC := $(DIR_LINUX)
ARCH := arm

obj-m += rts_ptz.o
ccflags-y += -Werror

.PHONY: all clean

all: modules

modules:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KSRC) M=$(shell pwd) modules \

clean:
	rm -f *.o
	rm -f *.mod.c *.mod.o
	rm -f modules.order
	rm -f *.ko
	rm -f Module.symvers
