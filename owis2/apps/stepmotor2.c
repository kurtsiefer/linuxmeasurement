/* driver for motor using the SM32 card; uses new (as of PCI) driver structure

   program to drive to a given position with a stepper motor.
   usage: stepmotor [options]
   
   waits for commands from stdin
   possible commands: 
   go num pos                  travels motor num to a given position
                               number of steps 
   set num [pos]               sets absolute pos  of motor num
   init num [volts [maxspeed]] initialize, and switch on 
   [on|off] num                switch motor num on
   exit                        leave program
   setvolt num [volt]          set voltage of motor num
   setspeed num [speed]        set speed of motor num
   reset
   break
   vmode num                   set in move mode
   pmode num                   set in pos mode
   limit num [limitmode]       toggle limit switches; 0 is default which
                               means limit swithches are ignored


   status:
   first written around 2002chk
   limit switches introduced 2004alexling
   fixed pmode bug 7.11.06chk
*/

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "sm32_2.h"
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>


/* #define DEBUG */
/* default values and global variables */
#define DEFAULT_VOLT 6.0
#define DEFAULT_SPEED 200
#define DEFAULT_MOVEMODE 0  /* 0: pos, 1: vel */

// #define SDEVICE  "/dev/ioboards/stepper3_0"
#define SDEVICE  "/dev/stepper3_0"
#define MAXINLEN 200
int handle;
float volts[3]={DEFAULT_VOLT,DEFAULT_VOLT,DEFAULT_VOLT};
int speed[3] = {DEFAULT_SPEED,DEFAULT_SPEED,DEFAULT_SPEED};
int movemode[3] ={DEFAULT_MOVEMODE,DEFAULT_MOVEMODE,DEFAULT_MOVEMODE};

/* some timeouts (all in ms) and limits*/
#define timeout_motor_ready 2000
#define waitmode 0   /* o: don't wait after write command */
#define timeout_motor_travel 15000
#define MAXVOLT 12.0
#define MAXSPEED 1000


/* error handling */
char * errormessage[] ={
  "No error.",
  "device not found.",
  "Not enough parameters.",
  "Wrong motor number. Must be 0, 1 or 2.",
  "Motor write timeout.",
  "Voltage out of range (0 to 12V).", /* 5 */
  "Speed out of range (0 to 1000).",
};

int emsg(int a){
  fprintf(stderr, "%s\n",errormessage[a]);
  return a;
}
static int MOT[] = {MOT_0,MOT_1,MOT_2,};

/* some basic motor commands */
struct timespec twenty_millisec = {0,20000000};
struct timespec time_left;
int is_motor_ready(int num){
    int tc=timeout_motor_ready/20;
    while (!ioctl(handle,SM32IsReady| MOT[num]) && tc>0) {
	/* wait if device is busy */
	nanosleep(&twenty_millisec,&time_left);
	tc--;
  };
    return tc; /* zero if timeout */
};
int get_mot_status(int num){
    int tc=timeout_motor_ready/20;
    ioctl(handle,SM32Request | mcState | MOT[num]);
    while (!ioctl(handle,SM32Replied| MOT[num]) && tc>0) {
	/* wait if device is busy */
	nanosleep(&twenty_millisec,&time_left);
	tc--;
    };
    if (tc) return ioctl(handle, SM32Fetch | MOT[num]);
    return 0; /* timeout */
}
int is_motor_there(motnum){
  int tc=timeout_motor_travel/20;
  while ((get_mot_status(motnum) == mstPos) && tc>0) {
    /* wait if device is busy */
    nanosleep(&twenty_millisec,&time_left);
    tc--;
  };
  return tc; /* zero if timeout */
};


/* command interpreter */
char cmd[MAXINLEN+1];

#define NUMCOMMANDS 14
char * commands[]={
  "",  /* 0: do nothing */
  ".", /* terminate */
  "go",/* go to a certain value */
  "init",
  "on",
  "off", /* 5 */
  "set",
  "exit",
  "setvolt",
  "setspeed",
  "reset", /* 10 */
  "break",
  "vmode",
  "pmode",
  "limit",
};



int parse_command(char * cmd) { /* returns  0 on success, or error code */
  char cmdi2[MAXINLEN+1];
  int idx, retval, arg2;
  int motnum, steps, inspeed, limits;
  float involts;
  if (1!=sscanf(cmd,"%s",cmdi2)){
      idx=0;   /* no command */
  } else {
      /* get command number */
      for(idx=NUMCOMMANDS;(idx>0)&&(strcmp(cmdi2,commands[idx]));idx--);
  };
  retval=0;
#ifdef DEBUG  
  printf("command:>%s<, cmd index:%d\n",cmd,idx);
#endif
  switch (idx) {
      case 1:case 7: /* terminate */
	  retval=1;break;
      case 2: /* go to a certain position */
	  switch(sscanf(cmd,"%s%d%d",cmdi2,&motnum,&steps)) {
	      case 2: steps=0;
	      case 3:
		  if (motnum<0 || motnum>2) {retval=-emsg(3);break;};
		  /* do it */
		  if (!is_motor_ready(motnum)) {retval=-emsg(4);break;};
		  ioctl(handle,SM32Post | mcGo | MOT[motnum], steps);
		  if (waitmode)
		      if (!is_motor_there(motnum)) {retval=-emsg(5);break;};
		  nanosleep(&twenty_millisec,&time_left);
		  break;
	      default:
      retval=-emsg(2);
	  };
	  break;

      case 3: /* init command */
	  arg2=sscanf(cmd,"%s%d%f%d",cmdi2,&motnum,&involts,&inspeed);
	  if (motnum<0 || motnum>2) {retval=-emsg(3);break;};
	  switch (arg2) {
	      case 4:
		  if (inspeed<0.0 || inspeed >MAXSPEED) {
		      retval = -emsg(6);break;};
		  speed[motnum]=inspeed;
	      case 3:
		  if (involts<0.0 || involts >MAXVOLT) {
		      retval = -emsg(5);break;};
		  volts[motnum]=involts;
	      case 2:
		  if (!is_motor_ready(motnum)) {retval=-emsg(4);break;};
		  /* set voltage */
		  ioctl(handle,SM32Post | mcU | MOT[motnum],
			(int)(volts[motnum]*10.));
		  /* set pos / move mode */
		  if (!is_motor_ready(motnum)) {retval=-emsg(4);break;};
		  ioctl(handle,SM32Post | mcPosMode | MOT[motnum],
			(movemode[motnum]?mmMove:mmPos));
		  /* set speed */
		  if (!is_motor_ready(motnum)) {retval=-emsg(4);break;};
		  ioctl(handle,SM32Post | mcF | MOT[motnum],
			(int)(movemode[motnum]?0:speed[motnum]));
		  /* switch on */
		  if (!is_motor_ready(motnum)) {retval=-emsg(4);break;};
		  ioctl(handle,SM32Post | mcPower | MOT[motnum], 1);
		  break;
	      default:
		  retval=-emsg(2);
	  };
	  break;
      case 4:case 5: /* on/off command */
	  arg2=sscanf(cmd,"%s%d",cmdi2,&motnum);
	  if (motnum<0 || motnum>2) {retval=-emsg(3);break;};
	  switch (arg2) {
	      case 2:
		  if (!is_motor_ready(motnum)) {retval=-emsg(4);break;};
		  ioctl(handle,SM32Post | mcPower |MOT[motnum],
			(idx==4?1:0));
		  break;
	      default:
		  retval=-emsg(2);
	  };
	  break;
      case 6: /* set pos command */
	  steps=0;
	  switch (sscanf(cmd,"%s%d%d",cmdi2,&motnum,&steps)) {
	      case 3:case 2:
		  if (!is_motor_ready(motnum)) {retval=-emsg(4);break;};
		  ioctl(handle, SM32Post| mcPosition | MOT[motnum], steps);
		  break;
	      default:
		  retval=-emsg(2);
	  };
	  break;
      case 8: /* set actual and default voltage */
	  switch (sscanf(cmd,"%s%d%f",cmdi2,&motnum,&involts)) {
	      case 2:
		  involts=DEFAULT_VOLT;
		  break;
	      case 3:
		  if (involts<0.0 || involts >MAXVOLT) {
		      retval = -emsg(5);break; };
		  if (motnum<0 || motnum>2) {retval=-emsg(3);break;};
		  break;
	      default:
      retval = -emsg(2);
	  };
	  if (retval==0) {
	      volts[motnum]=involts;
	      if (!is_motor_ready(motnum)) {retval=-emsg(4);break;};
	      /* set voltage */
	      ioctl(handle,SM32Post | mcU | MOT[motnum],
		    (int)(10.0*volts[motnum]));
	  };
	  break;
      case 9: /* set actual and default speed */
	  switch (sscanf(cmd,"%s%d%d",cmdi2,&motnum,&inspeed)) {
	      case 2:
		  inspeed=DEFAULT_SPEED;
		  break;
	      case 3:
		if (inspeed<0.0 || inspeed >MAXSPEED) {
		  retval = -emsg(6);break; };
		if (motnum<0 || motnum>2) {retval=-emsg(3);break;};
		break;
	      default:
		  retval = -emsg(2);
	  };
	  if (retval==0) {
	      speed[motnum]=inspeed;
	      if (!is_motor_ready(motnum)) {retval=-emsg(4);break;};
	      /* set speed */
	      ioctl(handle,SM32Post | mcF | MOT[motnum], (int)speed[motnum]);
	  };
	  break;
      case 10: /* reset command */
	  if (!is_motor_ready(0)) {retval=-emsg(4);break;};
	  ioctl(handle, SM32Post | mcReset, 0);
	  break;
      case 11: /* break command */
	  switch (sscanf(cmd,"%s%d",cmdi2,&motnum)) {
	      case 2:
		  if (motnum<0 || motnum>2) {retval=-emsg(3);break;};
		  if (!is_motor_ready(motnum)) {retval=-emsg(4);break;};
		  ioctl(handle,SM32Post | mcBreak | MOT[motnum],0);
		  break;
	      default:
		  retval = -emsg(2);
	  };
	  break;
      case 12:case 13: /* vmode/pmode command */
	  switch (sscanf(cmd,"%s%d",cmdi2,&motnum)) {
	      case 2:
		  if (motnum<0 || motnum>2) {retval=-emsg(3);break;};
		  if (!is_motor_ready(motnum)) {retval=-emsg(4);break;};
		  movemode[motnum]=(idx==12?1:0);
		  ioctl(handle,SM32Post | mcPosMode | MOT[motnum],
			(movemode[motnum]?mmMove:mmPos));
		  break;
	      default:
		  retval = -emsg(2);
	  }; 
          break;
	case 14: /* toggle limit switch */
          switch(sscanf(cmd,"%s%d%d",cmdi2,&motnum,&limits)) {
              case 2: limits=0;
              case 3:
                  if (motnum<0 || motnum>2) {retval=-emsg(3);break;};
                  /* do it */
                  if (!is_motor_ready(motnum)) {retval=-emsg(4);break;};
                  ioctl(handle,SM32Post | mcSwitchMode | MOT[motnum], limits);
                  if (waitmode)
                      if (!is_motor_there(motnum)) {retval=-emsg(5);break;};
                  nanosleep(&twenty_millisec,&time_left);
                  break;
              default:
      retval=-emsg(2);
          };
          break;
      case 0:
      default: /* no command */
	  break;
  };
  return retval;
}

int main(int argc, char * argv[]) {
  int idx, ir, cmo;
  int inh=0; /* stdin */
  /* open stepper motor device */
  handle=open(SDEVICE, O_RDWR);

  printf("return value: %d\n",handle);
  if (handle==-1) return -emsg(1);
  
  do {
    /* read in one line, waiting for newline */
    idx=0;
    while ((ir=read(inh,&cmd[idx],1))==1 && idx<MAXINLEN) {
      if (cmd[idx]=='\n' || cmd[idx]==';') break;
      idx++;
    };
    cmd[idx]='\0';
    if (idx==MAXINLEN)
      while ((ir=read(inh,&cmo,1))==1) if (cmo=='n' || cmo == ',') break; 
    /* command parser */
    cmo=parse_command(cmd);
#ifdef DEBUG
    printf("parser return: %d\n",cmo);
#endif
  } while (!cmo);
  
  close(handle);
  return 0;
}





