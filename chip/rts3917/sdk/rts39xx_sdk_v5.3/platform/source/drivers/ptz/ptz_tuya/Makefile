KSRC := $(DIR_LINUX)
ARCH := arm

obj-m += rts_ptz.o
ccflags-y += -Werror

rts_ptz-y += ty_driver.o \
	ty_gpio.o \
	ty_motor.o \
    rts_xb2_timer.o \
    rts_hr_timer.o

.PHONY: all clean

all: modules

modules:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KSRC) M=$(shell pwd) modules
	$(CROSS_COMPILE)gcc src/ty_app_ptz.c -o ty_app_ptz

clean:
	rm -f *.o
	rm -f *.mod.c *.mod.o
	rm -f modules.order
	rm -f *.ko
	rm -f Module.symvers
