/* program to read in all eight differential input channels
   with the dt302 card.
   adapted such that usage is similar to NI multifunct card.

   usage:

   readallchannels_dt302 [-g gain]

  
   gain can be 1,2,4,8; default is 1.


   ????reads in weired values sometimes.

   modified: returns now 0 in case of normal operation; code cleaned up

    10.3.04 chk

*/

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "dt302.h"
#include <time.h>
#include <signal.h>


/* using definitions of minor device 1 */
#define BOARDNAME "/dev/ioboards/dt302one"

/* error handling */
char *errormessage[] = {
  "No error.",
  "Can't open devcice.", /* 1 */
  "Wrong number of args. Usage: readsinglechannel [kanal [gain]]\n",
  "channel argument not in range 0..7",
  "Wrong gain value (choose 1,2,4 or 8).",
  "timeout occured.", /* 5 */
  "fifo empty!!! (should not occur)",
};

extern char *optarg;
extern int optind, opterr, optopt;

int emsg(int code) {
  fprintf(stderr,"%s\n",errormessage[code]);
  return code;
};


/* timeout procedure */
int timedout;
void sighandler(int sig) {
  if (sig==SIGALRM) timedout = 1;
  return;
};

#define DEFAULT_GAIN 1
#define DEFAULT_TIMEOUT 1 /* in seconds */

#define conversion_offset -10.0
#define conversion_LSB (20.0/4096) 

int main(int argc, char *argv[]) {
  int fh; /* card filehandle */
  int raw_value,i;
  float fval;
  int gain = DEFAULT_GAIN;
  int gainlist[4]={1,2,4,8};
  int gaintab[4]={SELECT_GAIN_1,SELECT_GAIN_2,SELECT_GAIN_4,SELECT_GAIN_8};
  int gain_code;
  int channel;
  int opt;

  /* board suchen */
  fh=open(BOARDNAME,O_RDWR);
  if (fh==-1) return -emsg(1);
  
  /* eingabeparemater testen */
  opterr=0; /* be quiet when there are no options */
  while ((opt=getopt(argc, argv, "g:")) != EOF) {
      switch (opt) {
	  case 'g':
	      sscanf(optarg,"%d",&gain);
	      break;
	  default:
	      break;
      };
  };
  

  /* create gain code */
  for (i=0;((i<4) && (gain != gainlist[i])) ;i++);
  if (i>=4) return -emsg(4); /* wrong gain */
  gain_code = gaintab[i];

  /* printf("channel: %d, gain: %d, gaincode: %d\n",channel, gain, gain_code); */
  
  /* initialize TIMER unit to create a scan clock of 100 kHz and a frame
     periode of 100 msec*/
  ioctl(fh,TIMER_RESET);
  ioctl(fh,SET_AD_SAMPLE_PERIODE,0xffffff+2-200); /* 200 main clk cycles */
  ioctl(fh,SET_AD_TRIG_PERIODE, 0xffffff+2-2000000); /* 2E6 main clk cycles */

  /* initialize ADC card */
  ioctl(fh,RESET_AD_UNIT);
  /* set AD operation mode : internal triggered scan */
  ioctl(fh,SELECT_AD_CONFIGURATION,
	CONTINUOUS_SCAN_MODE );
  /*| SOFTWARE_INITIAL_TRIGGER | INTERNAL_RETRIGGER_AD
    | INTERNAL_AD_SAMPLE_CLK | AD_BIPOLAR_MODE | AD_DIFFERENTIAL_ENDED);*/

  /* initialize hardware buffer */
  ioctl(fh,LOAD_PCI_MASTER);
  
  /* install channel-gain list: one setup, one real sample */
  ioctl(fh,PUSH_CGL_FIFO, 0 | gain_code ); /* allow for setup */
  for (channel=0;channel<8;channel++) 
      ioctl(fh,PUSH_CGL_FIFO, channel | gain_code );

  /* preload CGL list */
  ioctl(fh, PRELOAD_CGL);

  /* arm the thing... */
  ioctl(fh,ARM_CONVERSION);
  
  /* ... and trigger int per software */
  ioctl(fh,SOFTWARE_TRIGGER);

  /* read in with timeout procedure */
  timedout=0;
  signal(SIGALRM, &sighandler); /* install signal handler */
  alarm(DEFAULT_TIMEOUT);
  while ((ioctl(fh,HOW_MANY_FRESH_VALUES)<9) && !timedout) {
      /* printf("wait for fifo\n"); */
      usleep(20000);
  };
  alarm(0);
  
  /* disarm the card */
  ioctl(fh,STOP_SCAN);

  if (timedout) return -emsg(5); /* timeout */

  /* read in data */
  raw_value = ioctl(fh,GET_NEXT_VALUE);
  for (channel=0;channel<8;channel++) {
      raw_value = ioctl(fh,GET_NEXT_VALUE);
      /*  printf("raw value: %x\n",raw_value);  */
      fval = ((float)raw_value)*conversion_LSB+conversion_offset;
      printf("%f ", fval);
  };
  printf("\n");
  /* printf("fresh_values: %d\n",ioctl(fh,NEXT_INDEX_TO_WRITE)); */

  close(fh);

  return 0;  /* to keep child programs happy... */
}




