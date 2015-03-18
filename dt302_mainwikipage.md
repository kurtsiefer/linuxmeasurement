# Introduction #
The dt302 directory tree contains drivers and some basic applications for the Data translation DT300 series PCI bus multifunction cards. Specifically, we have and are using the dt302 and dt322 cards, which differ in the resolution of their analog-to-digital converter.

The drivers are currently available as modules for legacy 2.4 kernels, but are mostly used with 2.6 kernels. At the time of writing this, verison 2.6.27 is a reasonably current one.

# Details #

## Driver usage ##
Here should go something which outlines the communication with the card. The card driver essentially generates a character device, which can be opened from userland if the device nodes are generated properly with the loader script (or by hand).

There is a loader script, which tries to load the kernel module during boot time. It can be included into whatever runlevel manager the distribution has. It also creates device nodes into the directory /dev/ioboards, with the following naming convention (To be written):

The main way of interacting with the DT300 series cards is via custom ioctl() commands, which largely insert values into the various PCI card registers. The main purpose of the card is a reasonably high throughput sampling of analog inputs, which is established via a read() method on the device entry. For details, please see some of the application programs and partially the header file dt302.h

## Installation ##
After copying the driver tree into some local (stable) directory with a subversion client,  all files can be generated with a 'make' command in the dt302 directory. That generates also the loaderscript, and with a command 'make install' as superuser the corresponding loader file is copied into the /etc/init.d directory. Then, a runlevel editor can be used to activate the module. (This needs to be cleaned up, or at least made more aligned with common driver conventions).

Upon successful module loading, the device nodes should have been created into the directory /dev/ioboards . For that, the card needs to be present in the computer.

The initial make also generates all the applications, and puts some links to the executables into the main dt302 directory.

## Available application programs ##
There are always more important things than proper documentation; hence, there is not (yet) very much useful here. However, if you want to know how to use the various helper programs, have a look at into the C code; most programs have a man-page like structure in their top part, with a documentation of all the options and a reasobably complete functionality description as they this is/was the basis for writing the code in the first place.

<b>dacout_dt302:</b> small toy program to put an analog voltage onto one of the two analog output ports.

<b>readsinglechan_dt302:</b>  small program to read in the value of one of the eight analog differential inputs.

<b>readallchannels_dt302:</b> small program to read in all eight differential channels in one go.

<b>oscilloscope:</b>          program to acquire a longer set of data from several channels with an adjustable timing down to 10 usec.

<b>oscilloscope2:</b>         advanced version of oscilloscope, can trigger on a signal on one of the channels.

<b>streaming:</b>             a simplified version of the oscilloscope programs to read in a continuous stream of analog data.

<b>dt302_counter:</b>    program to count pulses for a given time interval. see source code for usage.

# Known bugs #

# Confirmed working kernels #
  * Compiles nicely on a 2.6.27 kernel on a i586 and on a x86\_64 machine. [Revision 29](https://code.google.com/p/linuxmeasurement/source/detail?r=29) has been tested to work on x86\_64