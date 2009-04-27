OPT = -O3 #-g

all: dt330.o dtest dtest2

dt330.o: dt330.c dt330.h
	gcc -c -I/lib/modules/`uname -r`/build/include -D__KERNEL__ -DMODULE -Wall -O dt330.c

dtest: dtest.c
	gcc -o dtest dtest.c

dtest2: dtest2.c
	gcc -o dtest2 dtest2.c

