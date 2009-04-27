# Makefile for the Owis SM32 driver for multiple kernel versions. Currently
# generates the drivers and corresponding applications.
#
# ToDo: - should allow to choose different install directories than
#	  the source directories.
#	- Should use a cleaner install routine (no OS-probing)
#	- should packport multiple card feature to 2.4	
# 
# current status: start rewriting 7.11.06 christian Kurtsiefer

version=$(shell uname -r  |sed  "s/\([0-9].[0-9]\).*/\\1/")

# directory for the driver for the local OS version
localdir=$(subst /,\/,$(PWD))
loaderdir=devstuff
loadersrc=$(loaderdir)/sm32.loadersource
loadertarget=$(loaderdir)/sm32loader

all: driver $(loadertarget) apps

# execute version-specific makefile
.PHONY: driver
driver:	
	$(MAKE) -C $(version)

$(loadertarget): $(loadersrc)
	cat $(loadersrc) | sed "s/DRIVERPATH/$(localdir)/g" >$(loadertarget)

.PHONY: apps
apps:
	$(MAKE) -C apps

.PHONY: clean
clean:	
	rm -f $(loadertarget)
	rm -f 2.4/*.o
	rm -f 2.6/*.o 2.6/*.mod.* 2.6/*.ko 2.6/.d*
	rm -fr 2.6/.tmp* 2.6/.*.cmd
	rm -fr *~
	$(MAKE) -C apps clean

install: driver $(loadertarget)
	cp $(loadertarget) /etc/init.d/
	chmod 754 /etc/init.d/sm32loader

