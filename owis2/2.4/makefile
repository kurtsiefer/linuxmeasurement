all:	sm32_kmodule.o

sm32_kmodule.o:  sm32_kmodule.c
	gcc -c -I/lib/modules/`uname -r`/build/include -D__KERNEL__ -DMODULE -Wall -O sm32_kmodule.c


