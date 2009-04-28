/* program to use a user counter in the DT302 card for event counting within
   a given time window.

   usage:
   dt302_counter [n] [-t time] [-m naxtimeslot ] 

   The program sets up an interval counter for a time specified in the -t
   option in multiples of milliseconds; if omitted, an integration time of
   1000 ms is assumed.
   For that period, a gate signal is generated on user counter output 3,
   which should be used as a gate signal on user counter 0. Events to be
   counted should be fed to the user clk input 0, and are counted in a 32 bit
   wide hardware counter constructed from user conter 0 and 1.

   Options:
   
   -t time    Integrates for time, given in milliseconds. Default is
              1000 milliseconds.
   
   -n         If this option is given, no newline is sent after the results.
              Seems to be helpful for newer scanning scripts.

   -m time    Maximum time per integration slot in milliseconds. Usually
              set to 60000. Just kept for test and compatibility to older
              version, the DT340 card is not susceptible to the underlying
              problem.

   Upon successful return, the number of events detected is returned to stdout.

   prelim version, seems to work              17.10.02, Christian Kurtsiefer
   fixed some loose variables for 2.6 compiler warnings  2.10.06 chk

 */

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include "dt302.h"

/* using definitions of minor device 1 */
#define BOARDNAME "/dev/ioboards/dt302one"
#define DEFAULT_INTEGTIME  1000
#define CR_OPTION_DEFAULT 1  /* send newline after values */

/* some check ranges */
#define MAX_INTEGTIME  60000 /* one minute integration time */
#define INITIAL_DELAY 10      /* initial time delay before active gate */
#define DEFAULT_MAXTIMESLOT 60000    /* time to be integrated per run */ 


extern char *optarg;
extern int optind, opterr, optopt;

struct timespec ten_millisec = {0,10000000};
struct timespec time_left;


/* error handling */
char *errormessage[] = {
  "No error.",
  "Integration time out of range.",
  "IO device busy.",
  "Can't open devcice.",
  "Option error. Usage: dt302_counter [-t time ] [-n] [-m maxtime]\n",
  "??", /* 5 */
  "??",
  "Maximum timeslot size out of range.",
};

int emsg(int code) {
  fprintf(stderr,"%s\n",errormessage[code]);
  return code;
};



int main(int argc, char *argv[]) {
  unsigned int integtime = DEFAULT_INTEGTIME;
  unsigned int integtime1;
  int opt;
  int val0,val1; /* store intermediate results */
  int fh;     /* iocard handle */
  int maxtimesl = DEFAULT_MAXTIMESLOT;
  int base_mode_3; /* to store mode of one shot counter */
  int uwert;
  int finalcount=0;
  int cr_option = CR_OPTION_DEFAULT;  /* if true, append newline to output */ 


  /* try to interpret options */
  opterr=0; /* be quiet when there are no options */
  while ((opt=getopt(argc, argv, "nm:t:")) != EOF) {
    switch (opt) {
    case 't':
      sscanf(optarg,"%u",&integtime);
      /* integration time ok? */
      if (integtime > MAX_INTEGTIME || integtime < 0) return emsg(1);
      break;
    case 'm':
      sscanf(optarg,"%d",&maxtimesl);
      if (maxtimesl <1 || maxtimesl > DEFAULT_MAXTIMESLOT) return emsg(7);
      break;
    case 'n':
      /* clear newline flag */
      cr_option=0;
      break;
    default:
	return emsg(4);
      break;
    };
  };

  /* open counter card driver */
  if ((fh=open(BOARDNAME,O_RDWR)) == -1) {
    if (errno == EBUSY) { return -emsg(2);}
    else { return -emsg(3);};
  };

  /* calculate real integration time in elementary steps */
  while (integtime >0) {
    integtime1=(integtime>maxtimesl?maxtimesl:integtime);
    integtime-=integtime1;
    
    
    /* reset/unreset counter chip */
    ioctl(fh,TIMER_RESET);
    
    /* prepare counter 2 as 1 kHz heartbeat for the gate output */
    ioctl(fh,SET_USER_PERIODE_2, 0xffff-20000+2);
    ioctl(fh,SET_USER_PULSE_2, 0xffff-10000+1);
    ioctl(fh,SET_USER_CONTROL_2, SELECT_INTERNAL_CLOCK | USER_GATE_HIGH |
	  CONTINUOUS_INCR_MODE | OUTPUT_HIGH_ACTIVE|ONESHOT_TRIGGER_ENABLE);

    /* prepare counter 3 as gating one shot, out high active */
    ioctl(fh,SET_USER_PULSE_3, 0xffff-integtime1+1);
    ioctl(fh,SET_USER_PERIODE_3, 0xffff-integtime1-INITIAL_DELAY+1);
    base_mode_3 = SELECT_CASCADE_CLOCK |  USER_GATE_LOW |
	NONRETRIG_ONESHOT_MODE | OUTPUT_LOW_ACTIVE; 
    ioctl(fh,SET_USER_CONTROL_3,base_mode_3);   

    /* prepare counter 0 for event counting */
    ioctl(fh,SET_USER_PERIODE_0, 1);
    ioctl(fh,SET_USER_PULSE_0, 0x8000);
    ioctl(fh,SET_USER_CONTROL_0,SELECT_EXTERNAL_CLOCK| USER_GATE_EXT_INVERTED |
	  CONTINUOUS_INCR_MODE | OUTPUT_LOW_ACTIVE| ONESHOT_TRIGGER_ENABLE );

    /* prepare counter 1 for overflow detect counting */
    ioctl(fh,SET_USER_CONTROL_1,SELECT_CASCADE_CLOCK | USER_GATE_HIGH |
	  CONTINUOUS_INCR_MODE | OUTPUT_LOW_ACTIVE );
    ioctl(fh,SET_USER_PERIODE_1, 1);

   /* trigger one shot */
    ioctl(fh,SET_USER_CONTROL_3, base_mode_3 | ONESHOT_TRIGGER_ENABLE);
    ioctl(fh,SET_USER_CONTROL_3, base_mode_3 |  USER_GATE_HIGH );

   /* check if counting is done */
    while (ioctl(fh,GET_USER_STATUS) & CNT_3_IS_TRIG_ENABLED) {
	 nanosleep(&ten_millisec, &time_left);
    }
    
    /* read in values in counter0/1 */
    val0=ioctl(fh,GET_USER_PERIODE_0); val1=ioctl(fh,GET_USER_PERIODE_1);
    
    /* add value to total counts */
    uwert = val0-1+((val1-1)<<16);
    finalcount +=uwert;
  }

  /* reset timing chip */
  ioctl(fh,ASSERT_TIMER_RESET);
  close(fh);

  /* send final value to stdout */
  fprintf(stdout," %d",finalcount);
  if (cr_option) fprintf(stdout,"\n");
  return 0;
}











