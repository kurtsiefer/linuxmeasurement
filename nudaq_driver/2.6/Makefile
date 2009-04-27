# this makefile containes only the device driver itself. No helper programs

# kernel 2.6 style


ifneq ($(KERNELRELEASE),)
obj-m :=  ndd2p6.o

else
KDIR	:= /lib/modules/`uname -r`/build
PWD		:= $(shell pwd)

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
endif