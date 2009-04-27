# Makefile for the nudaq driver for multiple kernel versions. Currently
# only generates the drivers, but does not comile them or install the drivers
# and/or loaders in the correct directory.
#
# ToDo: should allow to choose different install directories than the compiler
# directories. Should use a cleaner install routine (no OS-probing)
# current status: 21.9.06 Chistian Kurtsiefer

version=$(shell uname -r  |sed  "s/\([0-9].[0-9]\).*/\\1/")

# directory of this driver
localdir=$(subst /,\/,$(PWD))

all: driver loader

# execute version-specific makefile
.PHONY: driver
driver:	
	$(MAKE) -C $(version)

loader: nudaqcard

nudaqcard: nudaqcard.loadersource
	cat nudaqcard.loadersource | sed "s/DRIVERPATH/$(localdir)/g" >nudaqcard

.PHONY: clean
clean:	
	rm -f ./nudaqcard
	rm -f 2.4/*.o
	rm -f 2.6/*.o 2.6/*.mod.* 2.6/*.ko 2.6/.ndd*
	rm -fr 2.6/.tmp*
	rm -fr *~

install: driver loader
	cp ./nudaqcard /etc/init.d/
	chmod 755 /etc/init.d/nudaqcard

