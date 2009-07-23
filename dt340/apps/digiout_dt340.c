/* program to send a digital pattern to the dt340 outputs.

   Usage:

   digiout_dt340 [-t time] [-p patt]
   
   docu to be written

   Options:

   -t time    time in milliseconds after which the pattern is set back to
              zero. If not specified, the pattern remains.
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
#define DEFAULT_INTEGTIME  0
#define MAX_INTEGTIME 60000
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
  "Waiting time time out of range.", /* 1 */
  "cannot parse pattern",
  "Card is busy",
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
    int pattern = 0; /* default pattern */
    int integtime = DEFAULT_INTEGTIME; /* no waiting */
    int integtime1;
    int oflags, handle;
    int opt;
    int maxtimesl = DEFAULT_MAXTIMESLOT;

    
    /* try to interpret options */
    opterr=0; /* be quiet when there are no options */
    while ((opt=getopt(argc, argv, "t:p:")) != EOF) {
	switch (opt) {
	    case 'p':
		/* read in pattern */
		if (1!= sscanf(optarg,"%i",&pattern)) return -emsg(2);
		pattern &= 0xff; /* limit outputs */
		break;
	    case 't':
		sscanf(optarg,"%d",&integtime);
		/* integration time ok? */
		if (integtime > MAX_INTEGTIME  || integtime < 0) return emsg(1);
		break;
	    default:
		break;
	};
    };
   
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
    
    /* enable digital outputs */
    ioctl(handle, IO_WRITE | DT340_GEN_CONTROL_0, 
	  DIO_D_OutputEnable  );
    
    /* output desired pattern */
    ioctl(handle, IO_WRITE | DT340_DIO_CD, pattern<<8);


    if (integtime >0) { /* we do some fancy stuff here */
    
	/* install a signal handlers */
	signal(SIGIO, &ionote_handler);
	fcntl(handle, F_SETOWN, getpid());
	oflags=fcntl(handle, F_GETFL);
	fcntl(handle, F_SETFL, oflags | FASYNC);
	signal(SIGALRM, &watchdog_handler);
	/* allow for counter 7 to generate a signal */
	ioctl(handle, IO_WRITE |DT340DRV_IRQ_NOTIFY_CTRL, imask2);
	/* allow overflow interrupts for chan 7 */
	ioctl(handle, IO_WRITE | DT340_GEN_CONTROL_1,
	      CHIP0_Reset | CHIP1_Reset | 0x80);
	
	do {
	    integtime1 = (integtime>maxtimesl?maxtimesl:integtime);
	    integtime -= integtime1;
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
	
	/* clear output pattern */
	ioctl(handle, IO_WRITE | DT340_DIO_CD, 0);
	
    }
    
    /* shutdown irq handlers, card irqs and card itself */
    ioctl(handle, IO_WRITE |DT340_GEN_CONTROL_1,0);
    ioctl(handle, IO_WRITE |DT340DRV_IRQ_COUNT_CTRL, 0);
    ioctl(handle, IO_WRITE |DT340DRV_IRQ_NOTIFY_CTRL, 0);
    
    /* close IO device */
    close(handle);
    
    
    if (watchdog)
	fprintf(stderr,"A timeout occured, which should not happen. Hardware bug?\n");  
    
    return 0;
    
}





