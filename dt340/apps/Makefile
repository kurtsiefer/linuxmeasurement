OPT = -O3 #-g

targets= counter2 digiout_dt340	

wd=$(PWD)

#makefile for all applications using the dt340 card

all: $(targets) links

counter2: counter2.c
	gcc -Wall -o counter2 counter2.c

digiout_dt340: digiout_dt340.c
	gcc -Wall -o digiout_dt340 digiout_dt340.c

clean:
	rm -f $(targets)
	rm -f ../counter2

links:	
	ln -s -sf apps/counter2 ../counter2
