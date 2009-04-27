OPT = -O3 #-g

targets= counter2

wd=$(PWD)

#makefile for all applications using the dt340 card

all: $(targets) links

counter2: counter2.c
	gcc -o counter2 counter2.c

clean:
	rm -f $(targets)
	rm -f ../counter2

links:	
	ln -s -sf apps/counter2 ../counter2
