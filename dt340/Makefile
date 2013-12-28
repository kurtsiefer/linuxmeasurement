# Makefile for the DT340 driver for multiple kernel versions. Currently
# generates the drivers and corresponding applications.
#
# ToDo: - should allow to choose different install directories than
#	  the source directories.
#	- Should use a cleaner install routine (no OS-probing)
#	- backport multiple card feature to 2.4	
# 
# current status:
#         start rewriting 6.11.06 christian Kurtsiefer
#         transition to better device generation...still dirty

# For legacy versions, there exists a 2.4 kernel directory. If you use this kernel,
# set the sourcedir to 2.4, otherwise leave it at 2.6
sourcedir=$(PWD)/2.6

# directory for the driver for the local OS version
localdir=$(subst /,\/,$(PWD))
loaderdir=devstuff
loadersrc=$(loaderdir)/dt340.loadersource
loadersrc2=$(loaderdir)/udevsrc
loadertarget=$(loaderdir)/dt340loader
loadertarget2=$(loaderdir)/60-dt340.rules

all: driver $(loadertarget) apps

# execute version-specific makefile
.PHONY: driver
driver:
	$(MAKE) -C /lib/modules/`uname -r`/build M=$(sourcedir)

$(loadertarget): $(loadersrc) $(loadersrc2)
	cat $(loadersrc) | sed "s/DRIVERPATH/$(localdir)/g" >$(loadertarget)
	cat $(loadersrc2) | sed "s/DRIVERPATH/$(localdir)/g" >$(loadertarget2)

.PHONY: apps
apps:
	$(MAKE) -C apps

.PHONY: clean
clean:	
	rm -f $(loadertarget)
	rm -f $(loadertarget2)
	rm -f 2.4/*.o
	rm -f 2.6/*.o 2.6/*.mod.* 2.6/*.ko 2.6/.d*
	rm -fr 2.6/.tmp*
	rm -fr *~
	$(MAKE) -C apps clean

install: driver $(loadertarget) $(loadertarget2)
        sudo $(MAKE) -C /lib/modules/`uname -r`/build M=$(sourcedir) modules_install
#       sudo /sbin/depmod -a
#       sudo cp $(loadertarget) /etc/init.d/
#       sudo chmod 755 /etc/init.d/dt302loader
        sudo cp $(loadertarget2) /etc/udev/rules.d/

