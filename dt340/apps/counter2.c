/* program to read in counters for a given time with the DT340 counter card.
   To replace counter program for NI card.

   Usage:

   counter [-t time] [-d nnnn] [-m time]
   
   The program starts the nine counters, integrates for a time specified
   in the -t option and returns the value of all (or the specified counters
   in the -d option) to stdout, or an error condition to stderr. Return value
   is 0 normally, or -1 in case of an error.

   Options:

   -t time    Integrates for time, given in milliseconds. Default is
              1000 milliseconds.
   
   -d nnnn    Returns only the counters specified in the string nnnn.
              Each character of nnnn may be one of the digits from 1 to 6,
	      in an arbitrary sequence.

   -m time    Maximum time per integration slot in milliseconds. Usually
              set to 60000. Just kept for test and compatibility to older
	      version, the DT340 card is not susceptible to the underlying
	      problem.

   -n         If this option is given, no newline is sent after the results.
              Seems to be helpful for newer scanning scripts.


   Hardware usage:  counter 6 serves as 1 kHz clock in continuous counting
                    mode. counter 7 uses counter 6 output as clock and serves
		    as gating one-shot for counters 0-5. After counter 7 is
		    over, the software is notified by an interrupt.

   prelim version (laeuft im Test)            22.3.02, Christian Kurtsiefer
   fixed strings dependency...     6.11.2006 chk

*/

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <strings.h>

#include "dt340.h"

/* all system and default parameters */
#define IODEVICE  "/dev/ioboards/dt340test1"
#define DEFAULT_INTEGTIME  1000
#define DEFAULT_SEQUENCE  "123456"
#define SEQ_MAXLEN 20

#define CR_OPTION_DEFAULT 1  /* send newline after values */

/* some check ranges */
#define MAX_INTEGTIME  600000 /* ten minute integration time */
#define INITIAL_DELAY 10      /* initial time delay before active gate */
#define DEFAULT_MAXTIMESLOT 60000    /* time to be integrated per run */ 

/* masks for contoling irq enables */
#define imask1 (CNT0_IrqPending | CNT1_IrqPending | CNT2_IrqPending | CNT3_IrqPending | CNT4_IrqPending | CNT5_IrqPending )
#define imask2 CNT7_IrqPending

/* adress tables */
unsigned int counter_adr[] = {
  DT340_COUNTER_0,DT340_COUNTER_1,DT340_COUNTER_2,
  DT340_COUNTER_3,DT340_COUNTER_4,DT340_COUNTER_5,
};
unsigned int overflow_reg[] = {
  DT340DRV_IRQCNT_CNT0, DT340DRV_IRQCNT_CNT1, DT340DRV_IRQCNT_CNT2,
  DT340DRV_IRQCNT_CNT3, DT340DRV_IRQCNT_CNT4, DT340DRV_IRQCNT_CNT5,
};
unsigned int control_adr[] = {
  DT340_U_CONTROL_0, DT340_U_CONTROL_1, DT340_U_CONTROL_2,
  DT340_U_CONTROL_3, DT340_U_CONTROL_4, DT340_U_CONTROL_5,
};

extern char *optarg;
extern int optind, opterr, optopt;

struct timespec twenty_millisec = {0,20000000};
struct timespec ten_millisec = {0,10000000};
struct timespec time_left;

/* error handling */
char *errormessage[] = {
  "No error.",
  "Integration time out of range.",
  "Illegal element in output specifier.",
  "IO device busy.",
  "Error opening IO device.",
  "??", /* 5 */
  "??",
  "Maximum timeslot size out of range.",
};
int emsg(int code) {
  fprintf(stderr,"%s\n",errormessage[code]);
  return code;
};

/* handler & variable  to indicate a SIGIO */
int sigionote=0;
void ionote_handler(int sig) {
/*     printf("got a sigio!\n"); */
  if (sig==SIGIO) sigionote=-1;
}
/* handler & variable  to indicate a SIGIO */
int watchdog=0;
void watchdog_handler(int sig) {
/*    printf("got a timeout!\n"); */
   if (sig==SIGALRM) watchdog =-1;
}
int main(int argc, char *argv[]) {
  unsigned long integtime = DEFAULT_INTEGTIME;
  unsigned long integtime1;
  char out_sequence[SEQ_MAXLEN+1] = DEFAULT_SEQUENCE;
  int opt, i, r1, r2;
  unsigned long counts[6];          /* counting results */
  int handle;     /* iocard handle */
  int maxtimesl = DEFAULT_MAXTIMESLOT;
  int cr_option = CR_OPTION_DEFAULT;  /* if true, append newline to output */ 
  int oflags; /* to store output flags for fasync procedure */

  /* try to interpret options */
  opterr=0; /* be quiet when there are no options */
  while ((opt=getopt(argc, argv, "nm:t:d:")) != EOF) {
    switch (opt) {
    case 'n':
      /* clear newline flag */
      cr_option=0;
      break;
    case 't':
      sscanf(optarg,"%d",&integtime);
      /* integration time ok? */
      if (integtime > MAX_INTEGTIME || integtime < 0) return emsg(1);
      break;
    case 'd':
      sscanf(optarg,"%19s",out_sequence);
      for (i=0; i<SEQ_MAXLEN && out_sequence[i] !=0; i++) 
	if (!index("123456",out_sequence[i])) return emsg(2);
      break;
    case 'm':
      sscanf(optarg,"%d",&maxtimesl);
      if (maxtimesl <1 || maxtimesl > DEFAULT_MAXTIMESLOT) return emsg(7);
      break;
    default:
      break;
    };
  };

  /* initialize output vector */
  for (i=0;i<6;i++) counts[i]=0;

  /* open counter card driver */
  if ((handle=open(IODEVICE,O_RDWR)) == -1) {
    if (errno == EBUSY) { return -emsg(3);}
    else { return -emsg(4);};
  };


  /* reset/unreset chip */
  ioctl(handle,IO_WRITE | DT340_GEN_CONTROL_1, 0);
  ioctl(handle,IO_WRITE | DT340_GEN_CONTROL_1, CHIP0_Reset | CHIP1_Reset);

  /* initialize 1 khz clock */
  ioctl(handle,IO_WRITE | DT340_COUNTER_6, 0xffff+2-40000);
 ioctl(handle,IO_WRITE| DT340_U_PULSE_6, 0xffff+2-20000);
  ioctl(handle,IO_WRITE | DT340_U_CONTROL_6,
	Continuous_Mode | Output_Pol_High | Gate_High | Internal_Clock);
  /* initialize event counters 0..5 */
  for (i=0;i<6;i++) {
    ioctl(handle,IO_WRITE | control_adr[i],
	  Continuous_Mode | Gate_Extern_Invert | External_Clock );
    ioctl(handle, IO_WRITE | counter_adr[i],0);
    ioctl(handle, IO_WRITE | overflow_reg[i],0);
  }

  /* allow for counters 0..5 to increment overflow counters */
  ioctl(handle, IO_WRITE | DT340DRV_IRQ_COUNT_CTRL, imask1);
  /* install a signal handlers */
  signal(SIGIO, &ionote_handler);
  fcntl(handle, F_SETOWN, getpid());
  oflags=fcntl(handle, F_GETFL);
  fcntl(handle, F_SETFL, oflags | FASYNC);
  signal(SIGALRM, &watchdog_handler);
  /* allow for counter 7 to generate a signal */
  ioctl(handle, IO_WRITE |DT340DRV_IRQ_NOTIFY_CTRL, imask2);
  /* allow overflow interrupts for chan 0..5 and 7 */
  ioctl(handle, IO_WRITE | DT340_GEN_CONTROL_1,
	CHIP0_Reset | CHIP1_Reset | 0xbf);

  do {
    integtime1 = (integtime>maxtimesl?maxtimesl:integtime);
    integtime -= integtime1;
/*    printf("this round: integtime = %d\n",integtime1); */
    /* initialize gate counter */
    ioctl(handle, IO_WRITE | DT340_U_PULSE_7, 0x10000-integtime1);
    ioctl(handle, IO_WRITE | DT340_COUNTER_7, 0x10000-integtime1);
    ioctl(handle,IO_WRITE | DT340_U_CONTROL_7,
	  Output_Pol_Low |  Gate_Low | Cascaded_Clock | OneShot_Trigger_Cmd );
    ioctl(handle,IO_WRITE | DT340_U_CONTROL_7,
	  Output_Pol_Low | Gate_High | Cascaded_Clock );
    alarm((integtime1 / 1000) +2); /* activate/update watchdog */

    /* wait for interrupt or timeout (integr time + 1..2 seconds)*/
    pause();

    if (watchdog || !sigionote) break; /* timeout or other signal */ 
    sigionote=0;
    signal(SIGIO, &ionote_handler);
  } while (integtime >0);
  alarm(0); /* switch off alarm */

  /* read in all six counters */
  for (i=5;i>=0;i--) {
    counts[i]=(ioctl(handle, IO_READ | overflow_reg[i])*0x10001) +
      ioctl(handle, IO_READ | counter_adr[i]);
  };
  /* shutdown irq handlers, card irqs and card itself */
  ioctl(handle, IO_WRITE |DT340_GEN_CONTROL_1,0);
  ioctl(handle, IO_WRITE |DT340DRV_IRQ_COUNT_CTRL, 0);
  ioctl(handle, IO_WRITE |DT340DRV_IRQ_NOTIFY_CTRL, 0);

  /* close IO device */
  close(handle);
    
  /* output count results */

  if (watchdog)
      fprintf(stderr,"A timeout occured, which should not happen. Hardware bug?\n");  
  for (i=0; i<SEQ_MAXLEN && out_sequence[i] !=0; i++) 
    fprintf(stdout," %d",counts[out_sequence[i]-'1']);
  if (cr_option) fprintf(stdout,"\n");
  return 0;

}





