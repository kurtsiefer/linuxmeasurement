/* program to read in all eight differential input channels
   with the dt302 card. It is a simple implementation of the reasonably
   complex data accquisition scheme possible with the DT card.

   It acquires a set of data for a given time, starting immediately after
   invocation, and then sends the output as a formatted text to stdout.

   Works reasonably well, but does not have any fancy trigger yet.

   usage:

   oscilloscope [-g gain] [-m maxchannel] [-n sets] [s time] [-t]

   options: -t output time as well (in seconds)
   
    -g <gain>         : common gain in all channels.  <gain> can be 1,2,4,8; 
                        default is 1. The resulting value does not get rescaled
			with this gain, i.e., an input voltage of e.g. 1.0 Volt
			at a gain of 4 gets reported as 4.0 Volt.
    
    -t                : output time as first column (in seconds). By default,
                        no time is sent to the output.

    -m <maxchannel>   : largest channel to be collected. The default value is
                        1, corresponding to channels 0 and 1 to be sampled.

    -s <samplingtime> : sampling time in microseconds. Minimum is 10, default
                        is 1000.

    -n <numofsets>    : number of sample sets. Each sample set contains
                        a sequence of all sampled channels. Default is 1000.

    If maxchannel is 2, the channel sampling takes place in the sequence
     0-1-2-0-1-2-0-1-2..... and the 0-1-2 sequence repeats <numofsets> times.

     history:
       first working version 29.8.09chk

*/

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "dt302.h"
#include <time.h>
#include <signal.h>
#include <stdlib.h>


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
  "largest channel index out of range (0..7)",
  "Number of sampling points out of range",
  "Sampling interval out of range (>10 usec, <1sec)",
  "data buffer malloc failed.", /* 10 */
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
#define DEFAULT_TIMEPUTPUTOPTION 0 /* no time output */
#define DEFAULT_MAXCHANNEL 1    /* samples channels 0 and 1 */
#define DEFAULT_NUMOFPOINTS 1000
#define MAX_NUMOFPOINTS 10000000 /* some limit */
#define DEFAULT_SAMPLINGTIME 1000 /* one millisecond */
#define MIN_SAMPLINGTIME 10
#define MAX_SAMPLINGTIME 1000000  /* one second */

/* conversion factors for different cards */
#define conversion_offset -10.0
#define conversion_LSB_12 (20.0/4096)
#define conversion_LSB_16 (20.0/65536)


int main(int argc, char *argv[]) {
  int fh; /* card filehandle */
  int i,j;
  int gain = DEFAULT_GAIN;
  int gainlist[4]={1,2,4,8};
  int gaintab[4]={SELECT_GAIN_1,SELECT_GAIN_2,SELECT_GAIN_4,SELECT_GAIN_8};
  int gain_code;
  int channel;
  int opt;
  int timeoutputoption = DEFAULT_TIMEPUTPUTOPTION;
  int maxchannel = DEFAULT_MAXCHANNEL;
  int numofpoints = DEFAULT_NUMOFPOINTS;
  int totalpoints;     /*  keeps numofpoints*(maxchannel+1) */
  int samplingtime = DEFAULT_SAMPLINGTIME;
  u_int16_t *data_buffer; /* data collection buffer */
  int device; 
  float dt;

  /* reading in data */
  int p_already, i0;
  float conversion;
  
  /* open board */
  fh=open(BOARDNAME,O_RDWR);
  if (fh==-1) return -emsg(1);
  
  /* parse command line parameters */
  opterr=0; /* be quiet when there are no options */
  while ((opt=getopt(argc, argv, "g:tm:s:n:")) != EOF) {
      switch (opt) {
	  case 'g': /* default gain */
	      sscanf(optarg,"%d",&gain);
	      break;
	  case 't': /* output time option */
	      timeoutputoption=1;
	      break;
	  case 'm': /* max channel */
	      sscanf(optarg, "%d", &maxchannel);
	      if (maxchannel<0 || maxchannel>7) return -emsg(7);
	      break;
	  case 's': /* specify sampling time in usec */
	      sscanf(optarg,"%d",&samplingtime);
	      if (samplingtime < MIN_SAMPLINGTIME ||
		  samplingtime > MAX_SAMPLINGTIME) return -emsg(9);
	      break;
	  case 'n': /* number of points */
	      sscanf(optarg, "%d", &numofpoints);
	      if (numofpoints<0 || numofpoints>MAX_NUMOFPOINTS) return -emsg(8);
	      break;
	  default:
	      break;
      };
  };


  /* generate target buffer for collected values */
  totalpoints=numofpoints * (maxchannel+1);
  data_buffer=calloc(totalpoints, sizeof(u_int16_t));
  if (!data_buffer) return -emsg(10); /* malloc failed */
  
  
  /* create gain code */
  for (i=0;((i<4) && (gain != gainlist[i])) ;i++);
  if (i>=4) return -emsg(4); /* wrong gain */
  gain_code = gaintab[i];
  
  
  /* initialize TIMER unit to create a scan clock corresponding to the specified
     sampling rate. The retrigger is switched off. The sample timer is driven
     by a 20 MHz master clock. */
  ioctl(fh,TIMER_RESET);
  ioctl(fh,SET_AD_SAMPLE_PERIODE,0xffffff+2-(20*samplingtime)); 
  
  /* initialize ADC card */
  ioctl(fh,RESET_AD_UNIT);
	/* set AD operation mode : internal triggered scan */
  ioctl(fh,SELECT_AD_CONFIGURATION,
	CONTINUOUS_SCAN_MODE 
	| SOFTWARE_INITIAL_TRIGGER | INTERNAL_RETRIGGER_AD
	| INTERNAL_AD_SAMPLE_CLK | AD_BIPOLAR_MODE | AD_DIFFERENTIAL_ENDED);
	
  /* initialize hardware buffer */
  ioctl(fh,LOAD_PCI_MASTER);
  
  /* install channel-gain list */
  for (channel=0;channel<=maxchannel;channel++) 
      ioctl(fh,PUSH_CGL_FIFO, channel | gain_code );

  /* preload CGL list */
  ioctl(fh, PRELOAD_CGL);
  
  /* arm the thing... */
  ioctl(fh,ARM_CONVERSION);
  
  /* ... and trigger it per software */
  ioctl(fh,SOFTWARE_TRIGGER);
  
  /* read in with timeout procedure */
  timedout=0;
  signal(SIGALRM, &sighandler); /* install signal handler */
  alarm(samplingtime*totalpoints/1000000 + DEFAULT_TIMEOUT);
    
  /* read in data */
  p_already = 0;
  do {	 
      i0=read(fh, &data_buffer[p_already],
	      (totalpoints-p_already)*2)/2;
      p_already += i0;
      usleep(20000); /* slow acc */
  } while ((p_already < totalpoints ) && !timedout);

  alarm(0); /* disarm watchdog */
  
  /* disarm the card */
  ioctl(fh,STOP_SCAN);
  if (timedout) return -emsg(5); /* timeout */
  
  device = ioctl(fh, IDENTIFY_DTAX_CARD );
  switch (device) {
      case 322: /* for 16-bit converter */
	  conversion=conversion_LSB_16;
	  break;
      default:  /* for 12-bit converter */
	  conversion=conversion_LSB_12;
	  break;
  }
  
  /* output data, prelim */
  if (timeoutputoption) {
      dt=(maxchannel+1)*samplingtime*1E-6;
      for (i=0; i<numofpoints; i++) {
	  printf("%f ",i*dt);
	  for (j=0;j<=maxchannel;j++)
	      printf(" %f",conversion_offset + 
		     conversion*data_buffer[i*(maxchannel+1)+j]);
	  printf("\n");
      }
  } else {
      for (i=0; i<numofpoints; i++) {
	  for (j=0;j<=maxchannel;j++)
	      printf("%f ",conversion_offset + 
		     conversion*data_buffer[i*(maxchannel+1)+j]);
	  printf("\n");
      }
  }
  free(data_buffer);
  close(fh);

  return 0;  /* to keep child programs happy... */
}




