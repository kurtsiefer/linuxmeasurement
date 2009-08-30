OPT = -O3 #-g

targets= dacout_dt302 readsinglechan_dt302 dt302_counter readallchannels_dt302 oscilloscope oscilloscope2 streaming

wd=$(PWD)

# makefile for the applications using the analog input card.
all:  $(targets) links


dacout_dt302: dacout_dt302.c
	gcc -lm -o dacout_dt302 dacout_dt302.c

readsinglechan_dt302: readsinglechan_dt302.c
	gcc -lm -Wall -o readsinglechan_dt302 readsinglechan_dt302.c

dt302_counter: dt302_counter.c
	gcc -lm -Wall -o dt302_counter dt302_counter.c

readallchannels_dt302: readallchannels_dt302.c
	gcc -lm -Wall -o readallchannels_dt302 readallchannels_dt302.c

oscilloscope: oscilloscope.c
	gcc -lm -Wall -o oscilloscope oscilloscope.c

oscilloscope2: oscilloscope2.c
	gcc -lm -Wall -o oscilloscope2 oscilloscope2.c

streaming: streaming.c
	gcc -lm -Wall -o streaming streaming.c

clean: 
	rm -f $(targets)
	rm -f ../dacout_dt302
	rm -f ../readsinglechan_dt302
	rm -f ../dt302_counter 
	rm -f ../readallchannels_dt302
	rm -f ../oscilloscope2
	rm -f ../streaming
	rm -f .+
	rm -f *~
links:
	ln -sf apps/dacout_dt302 ../dacout_dt302
	ln -sf apps/readsinglechan_dt302 ../readsinglechan_dt302
	ln -sf apps/dt302_counter ../dt302_counter 
	ln -sf apps/readallchannels_dt302 ../readallchannels_dt302 
	ln -sf apps/streaming ../streaming
	ln -sf apps/oscilloscope2 ../oscilloscope2
