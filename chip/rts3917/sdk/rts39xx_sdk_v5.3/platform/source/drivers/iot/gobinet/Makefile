KSRC := $(DIR_LINUX)
obj-m := GobiNet.o
GobiNet-objs := GobiUSBNet.o QMIDevice.o QMI.o

ARCH := rlx
ccflags-y += -Wall -g
PWD := $(shell pwd)
#OUTPUTDIR=/lib/modules/`uname -r`/kernel/drivers/net/usb/

.PHONY: all clean

all: modules

modules:
	$(MAKE) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -C $(KDIR) M=$(PWD) modules



clean:
	rm -rf Makefile
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions Module.* modules.order
