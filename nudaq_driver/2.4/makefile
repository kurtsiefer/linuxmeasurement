OPT = -O3 #-g

all:  ndd2.4-3.o 

ndd2.4-3.o: ndd2.4-3.c
	gcc -c -O2 -I/lib/modules/`uname -r`/build/include -D__KERNEL__ -DMODULE -Wall -O ndd2.4-3.c


