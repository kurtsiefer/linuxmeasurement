# this makefile containes only the device driver itself. helper programs
# have made it into the dt330apps directory outside this driver directory

# kernel 2.6 style


ifneq ($(KERNELRELEASE),)
obj-m :=  dt330.o

else
KDIR	:= /lib/modules/`uname -r`/build
PWD		:= $(shell pwd)

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
endif