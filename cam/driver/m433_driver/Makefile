# Makefile for 433MHz Linux Kernel Driver

CROSS_COMPILE ?= mips-linux-gnu-

obj-m += m433_driver.o

KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

test: m433_test.c m433_ioctl.h
	$(CROSS_COMPILE)gcc -o m433_test m433_test.c

.PHONY: all clean install load unload test
