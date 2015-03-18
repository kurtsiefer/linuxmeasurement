# Introduction #
This is a linux device driver for the DT340 PCI multiple counter card. (add link to product).

The  driver should be reasobably generic in its usability, and gives access to all the internal registers of the card via ioctl calls. The driver spawns off two minor devices, a simpler one (minor 0) which allows to communicate with the internal registers, and a more advanced one (minor 1) which allows to propagete internal interrupts of the card to the application software. We use it mainly for internal, timer-generated interrupts, but it is also possible to pass on external hardware signals to a user program.

We use this card typically to count photodetection events during a well-defined time, where we use one of the counters to provide a well-defined hardware time window, providing a gate for the remaining 6 counter units in the card. Hardware spec for this operation mode needs to be documented (perhaps also with the circuit board gerber file for a counter connector).

The device driver provides char devides in the /dev/ioboards/ directory. At least the 2.6 kernel version allows for installation of several cards in one PC.

# Details #
To be added.

# Installation #
  1. Choose a reasonably stable location where the driver will reside. This can be in user space.
  1. cd into the dt340 directory, and issue a "make" . This compiles the diver, apps and adjusts a loaderscript to be used in /etc/init.d to the kernel version and driver location
  1. as su, issue a "make install" command to copy the loaderscript into the /etc/init.d directory
  1. make sure that the loader script is actually executed. In the various SuSE distros, this requires an activation with the runlevel editor.
  1. If a card is installed, it should load the driver automatically, and generate entires in the /dev/ioboards/ directory. During insertion of the driver via the loader script, the devicenames are chosen as "dt340\_x" for card x=0,1,2..., and they implement the minor device id 1 (allowing for interrupt handling). For compatibility with old applications, the device name entry "dt340test1" is linked to dt340\_0.

# Repository content #
  * main directory:
    * Makefile: makes drivers, apps, device loaders
  * devstuff directory: contains setup script to be included in /etc/init.d. On SuSE versions, the startup script needs to be activated with some sysconfig tool such that the driver is loaded on startup.
  * 2.4 directory:
    * dt340.c: source code of the device driver.
    * dt340.h: header file containing all the definitions you need to talk to the driver with ioctl() commands
    * dt340.o: the compiled driver; can be manually inserted with insmod, but see below for scripted insertion
  * 2.6 directory:
    * dt340.c: source code
    * dt340.h: extended version of the header file; contains a few more ioctls.
  * Apps directory:
    * counter2: small program to count events with this card during definable integration time. This needs a particular connection of various gate lines and counter outputs. Need to write documentaion on his.
    * counterscript\_dt340: displays count rates for the first four inputs
    * digiout\_dt340: small program to send something to the digital outputs for a short time.

# Current state of the drivers #
  * 2.4: version which runs with a fixed major address, and can only handle one card

  * 2.6: dynamical major number, can handle up to 5 cards. Device files are automatically created and removed with card.

## Change history ##
  * modified PCI registration to get correct IRQ number; this was a problem, with a SuSE 10.0 distro on one of the machines. 5.9.07chk
  * added counterscript\_dt340 to apps directory, no more addrows. 2.4.08chk
  * added digiout\_dt340 as an example app to talk to the digital output lines 23.7.2009chk