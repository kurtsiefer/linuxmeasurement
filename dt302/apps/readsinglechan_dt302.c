/* program to read in a single voltage on a differential input channel
   with the dt302 card.
   adapted such that usage is similar to NI multifunct card.

   usage:

   readsinglechan_dt302 [channel [gain]]

   channel ranges from 0 to 7, 0 is default.
   gain can be 1,2,4,8; default is 1.

   sample time increased to 40 usec to have less noise. 29.8.09chk

*/

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "dt302.h"
#include <time.h>
#include <signal.h>
#include <sys/ioctl.h>


/* using definitions of minor device 1 */
#define BOARDNAME "/dev/ioboards/dt30x_0"

/* error handling */
char *errormessage[] = {
  "No error.",
  "Can't open devcice.", /* 1 */
  "Wrong number of args. Usage: readsinglechannel [kanal [gain]]\n",
  "channel argument not in range 0..7",
  "Wrong gain value (choose 1,2,10 or 100).",
  "timeout occured.", /* 5 */
  "fifo empty!!! (should not occur)",
};


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

#define DEFAULT_CHANNEL 0
#define DEFAULT_GAIN 1
#define DEFAULT_TIMEOUT 1 /* in seconds */

#define conversion_offset -10.0
#define conversion_LSB_12 (20.0/4096) 
#define conversion_LSB_16 (20.0/65536) 

int main(int argc, char *argv[]) {
  int fh; /* card filehandle */
  int raw_value, i;
  float fval;
  int channel = DEFAULT_CHANNEL;
  int gain = DEFAULT_GAIN;
  int gainlist[4]={1,2,4,8};
  int gaintab[4]={SELECT_GAIN_1,SELECT_GAIN_2,SELECT_GAIN_4,SELECT_GAIN_8};
  int gain_code;
  float conversion; /* conversion factor for various converter */
  int device;

  /* board suchen */
  fh=open(BOARDNAME,O_RDWR);
  if (fh==-1) return -emsg(1);
  
  /* eingabeparemater testen */
  switch(argc) {
      case 1: /* no args, take default */
	  break;
      case 3: /* read in gain */
	  sscanf(argv[2],"%d",&gain);
      case 2: /* only channel given */ 
	  sscanf(argv[1],"%d",&channel);
	  if (channel & ~0x7) return -emsg(3);
	  break;
      default:
	  return -emsg(2); /* wrong arg no. */
  };
  

  /* create gain code */
  for (i=0;((i<4) && (gain != gainlist[i])) ;i++);
  if (i>=4) return -emsg(4); /* wrong gain */
  gain_code = gaintab[i];
  
  /* initialize TIMER unit to create a scan clock of 25 kHz and a frame
     periode of 100 msec*/
  ioctl(fh,TIMER_RESET);
  ioctl(fh,SET_AD_SAMPLE_PERIODE,0xffffff+2-200); /* 800 main clk cycles */
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
  ioctl(fh,PUSH_CGL_FIFO, channel | gain_code ); /* allow for setup */
  ioctl(fh,PUSH_CGL_FIFO, channel | gain_code );
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
  while ((ioctl(fh,HOW_MANY_FRESH_VALUES)<3) && !timedout) {
      /* printf("wait for fifo\n"); */
      usleep(20000);
  };
  alarm(0);
  
  /* disarm the card */
  ioctl(fh,STOP_SCAN);

  if (timedout) return -emsg(5); /* timeout */

  /* read in data */
  raw_value = ioctl(fh,GET_NEXT_VALUE);
  raw_value = ioctl(fh,GET_NEXT_VALUE);
  raw_value = ioctl(fh,GET_NEXT_VALUE);
  /*  printf("raw value: %x\n",raw_value);  */

  /* do adaption to different cards */
  device = ioctl(fh, IDENTIFY_DTAX_CARD );
  /* printf("device: %d\n",device); */
  switch (device) {
      case 322: /* for 16-bit converter */
	  conversion=conversion_LSB_16;
	  break;
      default:  /* for 12-bit converter */
	  conversion=conversion_LSB_12;
	  break;
  }
  fval = ((float)raw_value)*conversion+conversion_offset;
  printf("%f\n", fval);

  close(fh);
  return 0; 
}

