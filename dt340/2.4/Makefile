OPT = -O3 #-g

# makefile for the data translation card driver for kernel version 2.4

all: dt340.o dtest dtest2

dt340.o: dt340.c
	gcc -c -I/lib/modules/`uname -r`/build/include -D__KERNEL__ -DMODULE -Wall -O dt340.c

dtest2: dtest2.c
	gcc -o dtest2 dtest2.c

counter2: counter2.c
	gcc -o counter2 counter2.c
