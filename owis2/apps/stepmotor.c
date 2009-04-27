/* program to drive to a given position with a stepper motor.
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
*/

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include "sm30_2.h"
#include <time.h>

/* #define DEBUG */
/* default values and global variables */
#define DEFAULT_VOLT 6.0
#define DEFAULT_SPEED 200
#define DEFAULT_MOVEMODE 0  /* 0: pos, 1: vel */

#define DEVICE  "/dev/ioboards/stepper2"
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

/* io structures to communicate with the motor */
tSM30 SM30;  /* OWIS data struct */
sm30_cmd_struct sm30cmd  = {&SM30,0,0}; /* initialize command struct */


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

/* some basic motor commands */
struct timespec twenty_millisec = {0,20000000};
struct timespec time_left;
int is_motor_ready(){
  int tc=timeout_motor_ready/20;
  while (ioctl(handle,SM30CanWrite,sm30cmd)==0 && tc>0) {
    /* wait if device is busy */
    nanosleep(&twenty_millisec,&time_left);
    tc--;
  };
  return tc; /* zero if timeout */
};
int get_mot_status(int num){
  sm30cmd.cmd=mcNone;sm30cmd.sel=(1<<num);
  ioctl(handle,SM30Read,sm30cmd);
  return SM30.State[num];
}
int is_motor_there(motnum){
  int tc=timeout_motor_travel/20;
  sm30cmd.cmd=mcNone;
  while ((get_mot_status(motnum)==3) && tc>0) {
    /* wait if device is busy */
    nanosleep(&twenty_millisec,&time_left);
    tc--;
  };
  return tc; /* zero if timeout */
};


/* command interpreter */
char cmd[MAXINLEN+1];

#define NUMCOMMANDS 13
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
};



int parse_command(char * cmd) { /* returns  0 on success, or error code */
  char cmdi2[MAXINLEN+1];
  int idx, retval, converted,arg2;
  int motnum, steps, inspeed;
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
      if (!is_motor_ready()) {retval=-emsg(4);break;};
      sm30cmd.cmd=mcGo;sm30cmd.sel=(1<<motnum);sm30cmd.data[motnum]=steps;
      ioctl(handle,SM30Write,&sm30cmd);
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
      if (inspeed<0.0 || inspeed >MAXSPEED) {retval = -emsg(6);break;};
      speed[motnum]=inspeed;
    case 3:
      if (involts<0.0 || involts >MAXVOLT) {retval = -emsg(5);break;};
      volts[motnum]=involts;
    case 2:
      if (!is_motor_ready()) {retval=-emsg(4);break;};
      sm30cmd.cmd=mcU;sm30cmd.sel=(1<<motnum); /* set voltage */
      sm30cmd.data[motnum]=(int)(volts[motnum]*10);
      ioctl(handle,SM30Write,&sm30cmd);
      nanosleep(&twenty_millisec,&time_left);
      sm30cmd.cmd=mcPosMode;sm30cmd.sel=(1<<motnum);
      sm30cmd.data[motnum]=(movemode[motnum]?mmMove:mmPos);
      ioctl(handle,SM30Write,&sm30cmd); /* set position/move mode */
      nanosleep(&twenty_millisec,&time_left);
      if (!is_motor_ready()) {retval=-emsg(4);break;};
      sm30cmd.cmd=mcF;sm30cmd.sel=(1<<motnum); /* set speed */
      sm30cmd.data[motnum]=(int)(movemode[motnum]?0:speed[motnum]);
      ioctl(handle,SM30Write,&sm30cmd);
      nanosleep(&twenty_millisec,&time_left);
      if (!is_motor_ready()) {retval=-emsg(4);break;};
      sm30cmd.cmd=mcPower;sm30cmd.sel=(1<<motnum);sm30cmd.data[motnum]=1;
      ioctl(handle,SM30Write,&sm30cmd);/* switch on */
      nanosleep(&twenty_millisec,&time_left);
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
      if (!is_motor_ready()) {retval=-emsg(4);break;};
      sm30cmd.cmd=mcPower;sm30cmd.sel=(1<<motnum);
      sm30cmd.data[motnum]=(idx==4?1:-1);
      ioctl(handle,SM30Write,&sm30cmd);/* switch on */
      break;
    default:
      retval=-emsg(2);
    };
    break;
  case 6: /* set pos command */
    steps=0;
    switch (sscanf(cmd,"%s%d%d",cmdi2,&motnum,&steps)) {
    case 3:case 2:
      if (!is_motor_ready()) {retval=-emsg(4);break;};
      sm30cmd.cmd=mcPosition;sm30cmd.sel=(1<<motnum);
      sm30cmd.data[motnum]=steps;
      ioctl(handle,SM30Write,&sm30cmd);/* switch on */
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
      if (involts<0.0 || involts >MAXVOLT) {retval = -emsg(5);break; };
      if (motnum<0 || motnum>2) {retval=-emsg(3);break;};
      break;
    default:
      retval = -emsg(2);
    };
    if (retval==0) {
      volts[motnum]=involts;
      if (!is_motor_ready()) {retval=-emsg(4);break;};
      sm30cmd.cmd=mcU;sm30cmd.sel=(1<<motnum);
      sm30cmd.data[motnum]=(int)(10.0*volts[motnum]);
      ioctl(handle,SM30Write,&sm30cmd);/* set voltage */
    };
    break;
  case 9: /* set actual and default speed */
    switch (sscanf(cmd,"%s%d%d",cmdi2,&motnum,&inspeed)) {
    case 2:
      inspeed=DEFAULT_SPEED;
      break;
    case 3:
      if (!movemode)
	if (inspeed<0.0 || inspeed >MAXSPEED) {retval = -emsg(6);break; };
      if (motnum<0 || motnum>2) {retval=-emsg(3);break;};
      break;
    default:
      retval = -emsg(2);
    };
    if (retval==0) {
      speed[motnum]=inspeed;
      if (!is_motor_ready()) {retval=-emsg(4);break;};
      sm30cmd.cmd=mcF;sm30cmd.sel=(1<<motnum);
      sm30cmd.data[motnum]=(int)speed[motnum];
      ioctl(handle,SM30Write,&sm30cmd);/* set speed */
    };
    break;
  case 10: /* reset command */
    if (!is_motor_ready()) {retval=-emsg(4);break;};
    sm30cmd.cmd=mcReset;
    ioctl(handle,SM30Write,&sm30cmd);
    break;
  case 11: /* break command */
    switch (sscanf(cmd,"%s%d",cmdi2,&motnum)) {
    case 2:
      if (motnum<0 || motnum>2) {retval=-emsg(3);break;};
      if (!is_motor_ready()) {retval=-emsg(4);break;};
      sm30cmd.cmd=mcBreak;sm30cmd.sel=(1<<motnum);
      ioctl(handle,SM30Write,&sm30cmd);
      break;
    default:
      retval = -emsg(2);
    };
    break;
  case 12:case 13: /* vmode/pmode command */
    switch (sscanf(cmd,"%s%d",cmdi2,&motnum)) {
    case 2:
      if (motnum<0 || motnum>2) {retval=-emsg(3);break;};
      if (!is_motor_ready()) {retval=-emsg(4);break;};
      movemode[motnum]=(idx=12?1:0);;
      sm30cmd.cmd=mcPosMode;sm30cmd.sel=(1<<motnum);
      sm30cmd.data[motnum]=(movemode[motnum]?mmMove:mmPos);
      ioctl(handle,SM30Write,&sm30cmd);
      break;
    default:
      retval = -emsg(2);
    }; 
      
  default:case 0: /* no command */
  };
  return retval;
}

int main(int argc, char * argv[]) {
  int idx, ir, cmo;
  int inh=0; /* stdin */
  /* open stepper motor device */
  handle=open(DEVICE,O_RDWR);
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





