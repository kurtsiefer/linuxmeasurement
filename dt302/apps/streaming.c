/* program to read in from eight differential input channels with the
   data translation dt302/dt322 card. It acquires a set of data continuously,
   starting immediately after invocation, and then sends the output as a
   formatted text to stdout. 

   If you need a limited number of points or trigger options, see the
   oscilloscope or oscilloscope2 programs.

   usage:

   adcstream [-g gain] [-m maxchannel] [-a outmode] [-s time]

   options: -t output time as well (in seconds)
   
    -g <gain>         : common gain in all channels.  <gain> can be 1,2,4,8; 
                        default is 1. The resulting value does not get rescaled
			with this gain, i.e., an input voltage of e.g. 1.0 Volt
			at a gain of 4 gets reported as 4.0 Volt.
    
    -a <fmt>          : output format. The following values of <fmt> are
                        recognized. Default for <fmt> is 1.
			  0: output in raw hex (unscaled), space separated.
			     Each set gets a new line.
			  1: output in normalized floats, space separated.
			     The values are in volts
			  2: plain binary output. The unscaled values are
			     written as unseparated uint16_t, so separator.

    -m <maxchannel>   : largest channel to be collected. The default value is
                        1, corresponding to channels 0 and 1 to be sampled.

    -s <samplingtime> : sampling time in microseconds. Minimum is 10, default
                        is 1000. Maximum is 838000 (internal card limit?)
    -F                : flushoption. If set, a fflush is forced after each
                        data read cycle.

    If maxchannel is 2, the channel sampling takes place in the sequence
     0-1-2-0-1-2-0-1-2..... and the 0-1-2 sequence repeats <numofsets> times.

     history:
       first working version 30.8.09chk

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
  "Can't open device.", /* 1 */
  "Empty error mesg",
  "channel argument not in range 0..7",
  "Wrong gain value (choose 1,2,4 or 8).",
  "timeout occurred.", /* 5 */
  "Empty error msg",
  "largest channel index out of range (0..7)",
  "Wrong outmode (must be 0, 1, or 2)",
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
    timedout = 1; /* do this with any signal */
    return;
};

#define DEFAULT_GAIN 1
#define DEFAULT_TIMEOUT 1 /* in seconds */
#define DEFAULT_OUTMODE 1 /* ascii text, normalized */
#define DEFAULT_MAXCHANNEL 1    /* samples channels 0 and 1 */
#define DEFAULT_NUMOFPOINTS 1000
#define MAX_NUMOFPOINTS 10000000 /* some limit */
#define DEFAULT_SAMPLINGTIME 1000 /* one millisecond */
#define MIN_SAMPLINGTIME 10
#define MAX_SAMPLINGTIME 838000  /* internal card limit (24 bit) */
#define SLEEP_INTERVAL 20000 /* IN MICROSEC */

/* conversion factors for different cards */
#define conversion_offset -10.0
#define conversion_LSB_12 (20.0/4096)
#define conversion_LSB_16 (20.0/65536)


int main(int argc, char *argv[]) {
  int fh; /* card filehandle */
  int i;
  int gain = DEFAULT_GAIN;
  int gainlist[4]={1,2,4,8};
  int gaintab[4]={SELECT_GAIN_1,SELECT_GAIN_2,SELECT_GAIN_4,SELECT_GAIN_8};
  int gain_code;
  int channel;
  int opt;
  int outmode = DEFAULT_OUTMODE;
  int maxchannel = DEFAULT_MAXCHANNEL;
  int numofpoints = DEFAULT_NUMOFPOINTS;
  int totalpoints;     /*  keeps numofpoints*(maxchannel+1) */
  int samplingtime = DEFAULT_SAMPLINGTIME;
  u_int16_t *data_buffer; /* data collection buffer */
  int device; 
  int flushoption = 0; /* no flush by default */

  /* reading in data */
  int p_already, i0;
  float conversion;
  
  /* open board */
  fh=open(BOARDNAME,O_RDWR);
  if (fh==-1) return -emsg(1);
  
  /* parse command line parameters */
  opterr=0; /* be quiet when there are no options */
  while ((opt=getopt(argc, argv, "g:a:m:s:F")) != EOF) {
      switch (opt) {
	  case 'g': /* default gain */
	      sscanf(optarg,"%d",&gain);
	      break;
	  case 'a': /* output option */
	      sscanf(optarg, "%d", &outmode);
	      if (outmode<0 || outmode>2) return -emsg(8);
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
	  case 'F': /* flush option */
	      flushoption = 1;
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
  signal(SIGTERM, &sighandler); /* for termination */
  alarm(0);

  /* output format */
  device = ioctl(fh, IDENTIFY_DTAX_CARD );
  switch (device) {
      case 322: /* for 16-bit converter */
	  conversion=conversion_LSB_16;
	  break;
      default:  /* for 12-bit converter */
	  conversion=conversion_LSB_12;
	  break;
  }
    
  /* read in data */
  p_already=0;
  do { 
      i0=read(fh, &data_buffer[0], totalpoints*2)/2;
      
      /* send it out */
      switch (outmode) {
	  case 0: /* plain hex */
	      for (i=0; i<i0; i++) {
		  printf(" %04x",data_buffer[i]);
		  p_already++; 
		  if ((p_already % (maxchannel+1))==0)
		      printf("\n");
	      }
	      break;
	  case 1: /* formatted ascii */
	      for (i=0; i<i0; i++) {
		  printf(" %f",conversion_offset + conversion*data_buffer[i]);
		  p_already++; 
		  if ((p_already % (maxchannel+1))==0)
		      printf("\n");
	      }
	      break;
	  case 2: /* plain binary */
	      fwrite(data_buffer, sizeof(u_int16_t), i0, stdout);
	      break;
      }
      
      if (flushoption) fflush(stdout);

      usleep(SLEEP_INTERVAL); /* slow acc */
  } while (!timedout);

  alarm(0); /* disarm watchdog */
  
  /* disarm the card */
  ioctl(fh,STOP_SCAN);
  if (timedout) return -emsg(5); /* timeout */
  
 
  free(data_buffer);
  close(fh);

  return 0;  /* to keep child programs happy... */
}




