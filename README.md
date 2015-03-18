# linuxmeasurement
Speciality device drivers for data acquisition hardware under linux - exported from Google code
##What
The collection of linux drivers is for various measurement devices we use in our labs, which came with some sensible documentation and in our mind a good value/cost ratio. The drivers come with commandline-based application programmes suitable for embedding them into various scripts. Some of the code has a graphical front end based on gnuplot and Tcl/Tk - dirty hacks, but they remain the 'temporary' solution since a while now...

At the moment, the drivers for the following measurement cards together with some application examples are there:

 * **Data Translation DT302/DT322:** A PCI-bus based multipurpose analog I/O card with some digital I/O lines as well.
 * **Data Translation DT340:** A PCI card with multiple counters and timers. We use them to count events from photodetectors
 * **Data Translation DT330:** A general PCI-based digital I/O card
 * **Adlink Nudaq PCI7200:** A PCI card with a DMA engine to pipe data to/from the PC with about 12 MByte/sec. This driver is efficient in the sense that it does DMA to/from large user-space memory using the mmap method, avoiding the copying through bounce buffers. The card has an electrical connection which makes it somwhat difficult to really achieve the high data rate, since there is no good ground layout and hence one cannot use data cables with well-defined impedances. We fixed this partly with an adaper board wich plugs directly into the card, and then continues with a standard 68 pin SCSI connection.
 * **OWIS SM32 stepper motor driver card:** This card can drive up to 3 stepper motors, interpolating 1/64 of steps
 * **Ocean Optics USB2000/2000+ spectrometer:** This is a USB-based grating spectrometer, the driver offers enough code to retreive an optical spectrum for various exposure times.

As with all loadable kernel modules, compilation of the drivers requires that kernel sources or at least the header files are present on your machine. We tried them on various  intel platforms. 

The code works on the machines we use, but we use different kernels on different machines, so some cross testing still needs to be done. There is a wiki page listing [Verified_operation already verified operation] for the code in this repo. 

##How
There is a quick getting-started guideline in the wiki on [Installation how to install the code on your machine].

##Why
Measurement cards are a very common way in research to turn a standard PC into a powerful data acquisition system - however, many manufacturers have only a limited bandwidth to cater for those who want to have their speciality hardware running under an open source operating system, be it for its reliability or simply the highly transparent documentation-by-code of what a machine really does with your data. This is an important element in a chain of trust from the physical system you measure to the interpretation of results in scientific work. 

The idea of this project is to open device driver code to operate commercial measurement hardware under linux such that you may not need to get a license of an integrated measurement system under the market-dominating OS.

##Reference
In case you want to play with the drivers or understand what is going on, I suggest the excellent Book on device drivers by Rubini et al. http://oreilly.com/catalog/9780596005900/ at OReilly.

##Manufacturers
The drivers published here could be written because the manufacturers provided enough information to do so. You can find out about the hardware from their web sites:
 * Data translation, Inc.: http://www.dtax.com
 * OWIS GmbH: http://www.owis-staufen.de
 * Adlink Technology, Inc.: http://www.adlinktech.com
 * http://www.oceanoptics.com
