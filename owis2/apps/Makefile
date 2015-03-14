OPT = -O3 #-g

targets= stepmotor2

wd=$(PWD)

#makefile for all applications using the dt340 card

all: $(targets) links

stepmotor2: stepmotor2.c
	gcc -Wall  -o stepmotor2 stepmotor2.c

clean:
	rm $(targets)
	rm -f ../stepmotor2

links:	
	ln -s -sf apps/stepmotor2 ../stepmotor2
