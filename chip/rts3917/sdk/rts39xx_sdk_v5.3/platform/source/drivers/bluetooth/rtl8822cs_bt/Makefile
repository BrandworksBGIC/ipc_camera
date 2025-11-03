KSRC := $(DIR_LINUX)
ARCH := mips

obj-m := 8822cst.o
8822cst-y = hci_ldisc.o hci_h4.o hci_rtk_h5.o rtk_coex.o

tools := rtk_hciattach
tools-obj = hciattach.o hciattach_h4.o hciattach_rtk.o rtb_fwc.o

ccflags-y += -Wall

.PHONY: all clean

all: modules

modules:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KSRC) M=$(shell pwd) modules \

clean:
	rm -f *.o
	rm -f *.mod.c *.mod.o
	rm -f *.order
	rm -f *.ko *.a
	rm -f Module.symvers

#rtk_hciattach: $(tools-obj)
#	$(CC) -o rtk_hciattach $(tools-obj)
