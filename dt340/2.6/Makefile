# this makefile containes only the device driver itself. helper programs
# have made it into the dt340/apps directory.

# kernel 2.6 style


ifneq ($(KERNELRELEASE),)
obj-m :=  dt340.o

else
KDIR	:= /lib/modules/`uname -r`/build
PWD		:= $(shell pwd)

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
endif