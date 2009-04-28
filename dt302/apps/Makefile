OPT = -O3 #-g

targets= stst te2 dacout_dt302 readsinglechan_dt302 dt302_counter readallchannels_dt302

wd=$(PWD)

# makefile for the applications using the analog input card.
all:  $(targets) links

stst: stst.c
	gcc -lm -o stst stst.c

te2: te2.c
	gcc -lm -o te2 te2.c

dacout_dt302: dacout_dt302.c
	gcc -lm -o dacout_dt302 dacout_dt302.c

readsinglechan_dt302: readsinglechan_dt302.c
	gcc -lm -Wall -o readsinglechan_dt302 readsinglechan_dt302.c

dt302_counter: dt302_counter.c
	gcc -lm -Wall -o dt302_counter dt302_counter.c

readallchannels_dt302: readallchannels_dt302.c
	gcc -lm -Wall -o readallchannels_dt302 readallchannels_dt302.c

clean: 
	rm -f $(targets)
	rm -f ../dacout_dt302
	rm -f ../readsinglechan_dt302
	rm -f ../dt302_counter 
	rm -f ../readallchannels_dt302 
	rm -f .+
	rm -f *~
links:
	ln -sf apps/dacout_dt302 ../dacout_dt302
	ln -sf apps/readsinglechan_dt302 ../readsinglechan_dt302
	ln -sf apps/dt302_counter ../dt302_counter 
	ln -sf apps/readallchannels_dt302 ../readallchannels_dt302 

