OPT = -O3 #-g

# makefile for the data translation card driver for kernel version 2.4

all: dt302.o

dt302.o: dt302.c dt302.h
	gcc -c -I/lib/modules/`uname -r`/build/include -D__KERNEL__ -DMODULE  -Wall -O dt302.c

