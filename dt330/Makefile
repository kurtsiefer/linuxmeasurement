# Makefile for the DT330 driver for multiple kernel versions. Currently
# generates the drivers and corresponding applications.
#
# ToDo: - should allow to choose different install directories than
#	  the source directories.
#	- Should use a cleaner install routine (no OS-probing)
#	- should packport multiple card feature to 2.4	
# 
# current status: start rewriting 6.11.06 christian Kurtsiefer

version=$(shell uname -r  |sed  "s/\([0-9].[0-9]\).*/\\1/")

# directory for the driver for the local OS version
localdir=$(subst /,\/,$(PWD))
loaderdir=devstuff
loadersrc=$(loaderdir)/dt330.loadersource
loadertarget=$(loaderdir)/dt330loader

all: driver loader

# execute version-specific makefile
.PHONY: driver
driver:	
	$(MAKE) -C $(version)

loader: $(loadersrc)
	cat $(loadersrc) | sed "s/DRIVERPATH/$(localdir)/g" >$(loadertarget)

.PHONY: clean
clean:	
	rm -f $(loadertarget)
	rm -f 2.4/*.o
	rm -f 2.6/*.o 2.6/*.mod.* 2.6/*.ko 2.6/.d*
	rm -fr 2.6/.tmp*
	rm -fr *~
	rm -f $(loadertarget)


install: driver $(loadertarget)
	cp $(loadertarget) /etc/init.d/
	chmod 755 /etc/init.d/dt330loader

