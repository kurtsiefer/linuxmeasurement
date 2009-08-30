/* program to read in all eight differential input channels
   with the dt302 card. It is a simple implementation of the reasonably
   complex data acquisition scheme possible with the DT card.
   
   This is mark 2, trying to implement a trigger.

   It acquires a set of data for a given time, starting immediately after
   invocation, and then sends the output as a formatted text to stdout.

   usage:

   oscilloscope2 [-g gain] [-m maxchannel] [-n sets] [s time] [-t] [-c]
                 [-T trigmode] [-L triglevel] [-P trigpolarity]
		 [-C trigchannel] [-H trighysteresis]
		 [-D trigdelay] [-I triginterpoloption]

   options: -t output time as well (in seconds)
   
    -g <gain>         : common gain in all channels.  <gain> can be 1,2,4,8; 
                        default is 1. The resulting value does not get rescaled
			with this gain, i.e., an input voltage of e.g. 1.0 Volt
			at a gain of 4 gets reported as 4.0 Volt.
    
    -t                : output time as first column (in seconds). By default,
                        no time is sent to the output.
    -c                : comment option. If set, the parameters for the capture
                        will be saved with the output, preceeded with 
			# signs.

    -m <maxchannel>   : largest channel to be collected. The default value is
                        1, corresponding to channels 0 and 1 to be sampled.

    -s <samplingtime> : sampling time in microseconds. Minimum is 10, default
                        is 1000. Maximum is 838000 (internal card limit?)

    -n <numofsets>    : number of sample sets. Each sample set contains
                        a sequence of all sampled channels. Default is 1000.
TRIGGER OPTIONS:
    -T <mode>         : defines Trigger mode.
                        0: no content-dependent trigger, starts immediately
			1: 'normal' trigger; starts capturing only on cond
			2: 'auto' trigger; autotrigs after a delay of ..
    -L <level>        : Trigger level
    -P <polarity>     : >=0: pos slope
    -C <channel>
    -H <hysteresis>
    -D <time>         : from trig to start_acquisition to in seconds. 
                        A positive value of <time> corresponds to a wait from
			trigger time until acquisition starts.
    -I <interpolmode> : Trigger time interpolation. The following modes are
                        observed; default is mode 2:
                         0: no trigger time interpolation
			 1: interpolation within one sample cell
			 2: interpolation within the set of points

    If maxchannel is 2, the channel sampling takes place in the sequence
     0-1-2-0-1-2-0-1-2..... and the 0-1-2 sequence repeats <numofsets> times.

     history:
       first working version; tested with several triggers 30.8.09chk

     TODO:
     more testing...

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
  "Unused error msg",
  "unused error msg",
  "Wrong gain value (choose 1,2,4 or 8).",
  "timeout occured.", /* 5 */
  "Illegal trigger interpolation mode (must be 0, 1 or 2)",
  "largest channel index out of range (0..7)",
  "Number of sampling points out of range",
  "Sampling interval out of range (>10 usec, <1sec)",
  "data buffer malloc failed.", /* 10 */
  "Wrong trigger mode (must be 0, 1 or 2)",
  "Trigger channel out of range (0..7)",
  "Trigger level out of range",
  "Hysteresis makes trigger impossible",
  "Trigger channel not part of scan",   /* 15 */
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
#define MAX_SAMPLINGTIME 838000  /* corresponds to 24 bit */
#define DEFAULT_TRIGGERINTERPOLATION 2 /* complete interpolation */
#define MAX_VOLTAGE 10.0
#define MIN_VOLTAGE -10.0
#define SLEEP_INTERVAL 50000 /* in microseconds, between reads */

/* conversion factors for different cards */
#define conversion_offset -10.0
#define CONVERSION_SPAN 20.0


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
  int commentoption = 0; /* off by default */
  int maxchannel = DEFAULT_MAXCHANNEL;
  int totalchannels; /* keeps maxchannel+1 */
  int numofpoints = DEFAULT_NUMOFPOINTS;
  int totalpoints;     /*  keeps numofpoints*totalchannels */
  int samplingtime = DEFAULT_SAMPLINGTIME;
  u_int16_t *data_buffer; /* data collection buffer */
  int device; 
  float dt, dt0;
  int timeout = DEFAULT_TIMEOUT;

  /* reading in data */
  int p_already, i0, tmp;
  int fillidx, pts_to_get, goodpoints;

  /* amplitude nomalization stuff */
  float conversion;
  int maxADCvalue;

  /* Trigger stuff */
  float TriggerDelay = 0.0; 
  float TriggerLevel = 0.0;
  float TriggerHysteresis = 0.05;
  int triglev1, triglev2, trigdel;   /* integer versions */
  int trigmode = 0;
  int trigchan = 0;   /* channel */
  int trigpol = 0;
  /* the following is for the trigger status */
#define TRIG_PREACQ 0           /* buffer is empty */
#define TRIG_WAITING_FOR_ARM 1  /* waiting for hysteresis criterium */
#define TRIG_ARMED 2            /* pre-trig ok, waiting for trig */ 
#define TRIG_TRIGGERED 3        /* acq active */
#define TRIG_DONE 4             /* we have everything */
  int trigstatus = TRIG_PREACQ; 
  int trigidx ;    /* contains index of trigger */
  int autotrigdelay = -1;  /* keeps autotrig delay. neg means work */
  int trigval_0, trigval_1; /* keep actual/previous trigger level */
  int triginterpoloption = DEFAULT_TRIGGERINTERPOLATION; /* 0: none,
							    1: intra_cell
							    2: complete */
  /* open board */
  fh=open(BOARDNAME,O_RDWR);
  if (fh==-1) return -emsg(1);
  
  /* parse command line parameters */
  opterr=0; /* be quiet when there are no options */
  while ((opt=getopt(argc, argv, "g:tm:s:n:T:D:L:H:P:C:I:c")) != EOF) {
      switch (opt) {
	  case 'g': /* default gain */
	      sscanf(optarg,"%d",&gain);
	      break;
	  case 't': /* output time option */
	      timeoutputoption=1;
	      break;
	  case 'c': /* generate a comment */
	      commentoption = 1;
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
	  case 'D': /* Trigger delay */
	      sscanf(optarg, "%f", &TriggerDelay);
	      break;
	  case 'L': /* Trigger level */
	      sscanf(optarg, "%f", &TriggerLevel);
	      break;
	  case 'H': /* Trigger hysteresis */
	      sscanf(optarg, "%f", &TriggerHysteresis);
	      break;
	  case 'T': /* trig mode */
	      sscanf(optarg, "%d", &trigmode);
	      if (trigmode<0 || trigmode>2) return -emsg(11);
	      break;
	  case 'C': /* trig channel */
	      sscanf(optarg, "%d", &trigchan);
	      if (trigchan<0 || trigchan>7) return -emsg(12);
	      break;
	  case 'P': /* trig polarity */
	      sscanf(optarg, "%d", &trigpol);
	      trigpol = (trigpol<0)?-1:1;
	      break;
	  case 'I': /* Trigger intperpolation mode */
	      sscanf(optarg, "%d", &triginterpoloption);
	      if (triginterpoloption<0 || triginterpoloption>2)
		  return -emsg(6);
	      break; 
	  default:
	      break;
      };
  };
  
  /* just for convenience */
  totalchannels = maxchannel + 1;
  totalpoints = numofpoints * totalchannels;

  /* Amplitude normalization */
  device = ioctl(fh, IDENTIFY_DTAX_CARD );
  switch (device) {
      case 322: /* for 16-bit converter */
	  maxADCvalue=0xffff;
	  break;
      default:  /* for 12-bit converter */
	  maxADCvalue=0xfff;
	  break;
  }
  conversion = CONVERSION_SPAN/(maxADCvalue+1);

  /* Do discrete versions from Trigger data */
  /* delay from trig to start-of-acq in samplesteps, truncated to first entry */
  trigdel = totalchannels * 
      (int)(TriggerDelay/samplingtime*1000000./totalchannels);
  if ((TriggerLevel > MAX_VOLTAGE) || (TriggerLevel < MIN_VOLTAGE))
      return -emsg(13);
  triglev1 = (int)((TriggerLevel-conversion_offset)/conversion);
  triglev2 = triglev1 - (trigpol<0?-1:1) * (int)(TriggerHysteresis/conversion);
  if ((triglev2<0) || (triglev2>maxADCvalue)) return -emsg(14);
  if (trigchan>maxchannel) return -emsg(15);

  /* generate target buffer for collected values */
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
  for (channel=0;channel<totalchannels;channel++) 
      ioctl(fh,PUSH_CGL_FIFO, channel | gain_code );

  /* preload CGL list */
  ioctl(fh, PRELOAD_CGL);
  
  /* arm the thing... */
  ioctl(fh,ARM_CONVERSION);
  
  /* ... and trigger DT card per software */
  ioctl(fh,SOFTWARE_TRIGGER);

  /* prepare trigger status */
  if (trigmode == 0) {
      trigidx = 0;
      trigstatus=TRIG_TRIGGERED; /* go into acquisition mode directly */
  }
  /* fix autotrig */
  if (autotrigdelay<0) autotrigdelay = totalpoints;


  /* read in with timeout procedure */
  timedout=0;
  signal(SIGALRM, &sighandler); /* install signal handler */
  if (trigmode == 1) { /* we may need an alarm */
      if  (timeout) { /* only if we specified */
	  alarm(samplingtime*totalpoints/1000000 + timeout);
      }
  }
    
  /* read in data */
  p_already = 0; fillidx = 0;
  goodpoints = 0; /* we don't have any useful points yet....*/
  do {	
      /* make decision of how many points we read in maximally */
      pts_to_get = totalpoints- goodpoints; /* start with as much as possible */
      if (pts_to_get > totalpoints - fillidx) 
	  pts_to_get = totalpoints - fillidx; /* limit it to what fits in */

      /* do the read */
      i0=read(fh, &data_buffer[fillidx], pts_to_get*2)/2;

      /* do processing of data to determine new trigger status */
      switch (trigstatus) {
	  case TRIG_PREACQ: /*  check if sth happened */
	      tmp = -trigdel ; /* necessary pretrig */
	      if (tmp > totalpoints) tmp = totalpoints;
	      tmp -= p_already; /* this is what we still need */
	      if (i0 < tmp ) break; /* need to wait more */

	      /* prepare for next stage */
	      p_already += tmp;
	      fillidx += tmp;
	      i0 -= tmp;
	      trigstatus = TRIG_WAITING_FOR_ARM;
	  case TRIG_WAITING_FOR_ARM:	      
	      /* prepare starting point */
	      tmp = fillidx - ( fillidx % totalchannels ) + trigchan;
	      if (tmp < fillidx) tmp += totalchannels;

              /* do a search */
	      if (trigpol<0) { /* we need to be above triglev2 */
		  for (i=tmp; i<fillidx+i0; i += totalchannels)
		      if (data_buffer[i] >= triglev2) break;
	      } else {/* we need to be below triglev2 */
		  for (i=tmp; i<fillidx+i0; i += totalchannels) 
		      if (data_buffer[i] <= triglev2) break;}

	      /* check for no success */
	      tmp = i-fillidx; 
	      if (tmp >= i0) break; /* no pre-trig found !!!check equal!! */
	      /* prepare next stage */
	      p_already += tmp;
	      fillidx += tmp;
	      i0 -= tmp;
	      trigstatus = TRIG_ARMED;
	  case TRIG_ARMED:
	      /* prepare starting point */
	      tmp = fillidx - ( fillidx % totalchannels ) + trigchan;
	      if (tmp < fillidx) tmp += totalchannels;
	      /* do a search */
	      if (trigpol<0) { /* we need to be below triglev1 */
		  for (i=tmp; i<fillidx+i0; i += totalchannels) 
		      if (data_buffer[i] <= triglev1) break;
	      } else {/* we need to be above triglev1 */
		  for (i=tmp; i<fillidx+i0; i += totalchannels)
		      if (data_buffer[i] >= triglev1) break;
	      }
	      /* check for no success */
	      tmp = i-fillidx; 
	      if  (tmp >= i0) break; /* no trig found */
	      /* prepare next stage */
	      p_already += tmp;
	      trigidx = p_already; /* this should be already aligned */
	      trigidx -= (trigidx % totalchannels); /* not necessary? */
	      fillidx += tmp;
	      i0 -= tmp;
	      trigstatus = TRIG_TRIGGERED;
	      /* save trigger values */
	      trigval_0 = data_buffer[i];
	      trigval_1 = data_buffer[(i-totalchannels + totalpoints) % totalpoints];
	      alarm(0);
	  case TRIG_TRIGGERED:
	      /* how many good points do we have? */
	      goodpoints = p_already + i0 - trigidx - trigdel;
	      if (goodpoints < 0) { /* we are still in pretrig */
		  goodpoints = 0; break;
	      }
	      if (goodpoints < totalpoints) break; /* no status change */
	      trigstatus = TRIG_DONE;
	  case TRIG_DONE:
	      break;
      }

      /* fix unconditional accounting */
      p_already += i0;
      fillidx = (fillidx + i0) % totalpoints;
      
      if (trigmode == 2) {/* autotrig */
	  if ((trigstatus == TRIG_WAITING_FOR_ARM) ||
	      (trigstatus == TRIG_ARMED)) { /* we could test */
	      if (p_already > autotrigdelay) { /* we trigger */
		  trigidx = p_already; /* current point  */
		  trigidx -= (trigidx % totalchannels); /* do align */
		  trigstatus = TRIG_TRIGGERED;
		  goodpoints = p_already - trigidx - trigdel;
		  if (goodpoints < 0) { /* we are still in pretrig */
		      goodpoints = 0; 
		  }
		  if (goodpoints >= totalpoints) {
		      trigstatus = TRIG_DONE;
		      break;
		  }
	      }
	  }
      }
      usleep(SLEEP_INTERVAL); /* some sleeping */
  } while ((trigstatus < TRIG_DONE ) && !timedout);


  alarm(0); /* disarm watchdog */
  
  /* disarm the card */
  ioctl(fh,STOP_SCAN);
  if (timedout) return -emsg(5); /* timeout */
  
  /* trigger time interpolation */
  dt0 = 0.0;
  switch (triginterpoloption) {
      case 2: dt0 -= (float)trigchan;
      case 1: /* within one sample */
	  dt0 += (((float)(triglev1-trigval_0)) / (float)(trigval_1-trigval_0)
		  * (float)totalchannels);
  }

  /* output data */
  dt=samplingtime*1E-6;
  i0=trigidx+trigdel; /* where to start in array */
  for (i=0; i<totalpoints; i+=totalchannels) {
      if (timeoutputoption) printf("%f ",(i+trigdel+dt0)*dt);
      for (j=0;j<totalchannels;j++)
	  printf(" %f",conversion_offset + 
		 conversion*data_buffer[(i+j+i0) % totalpoints]);
      printf("\n");
  }
  
  /* set print some comments */
  if (commentoption) {
      printf("#\n# generated by the oscilloscope2 program with the following parameters:\n");
      printf("#  Sampling time interval: %d usec per point (not set!)\n",
	     samplingtime);
      printf("#  Number of channels: %d\n", totalchannels);
      printf("#  Number of sample sets: %d\n", numofpoints);
      printf("#  Overall input gain: %d\n", gain);
      printf("#  Card type: DT%d with %d steps\n", device, maxADCvalue+1);
      printf("#  Amplitude resolution: %f volt\n", conversion);
      printf("#  Trigger delay: %f sec\n", TriggerDelay);
      printf("#  Trigger mode: %d\n#  Trigger polarity: %+d\n",
	     trigmode, trigpol);
      printf("#  Trigger source channel: %d\n", trigchan);
      printf("#  Trigger level: %f Volt\n#  Trigger hysteresis: %f Volt\n",
	     TriggerLevel, TriggerHysteresis);
      printf("#  Trigger interpolation mode: %d\n", triginterpoloption);
      printf("#  Trigger time interpolation correction: %f usec\n#\n", 
	     dt0*samplingtime);
  }

  
  free(data_buffer);
  close(fh);

  return 0;  /* to keep child programs happy... */
}




