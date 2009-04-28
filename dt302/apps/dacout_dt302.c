/* program to set the DACs on a data translation dt302 card to  given voltage.
   Arguments are the channel number (0 or 1), and a voltage as a
   floating point value.

   usage:

   dacout_dt302 [channel] voltage

   if no channel is specified, channel 0 is assumed as a default.

   works. 13.10.02 chk

 */

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "dt302.h"

/* using definitions of minor device 1 */
#define BOARDNAME "/dev/ioboards/dt302one"

#define NUMBER_OF_BITS 16
#define MAX_OUT_PATTERN ((1<<NUMBER_OF_BITS) -1)

/* error handling */
char *errormessage[] = {
  "No error.",
  "Can't open devcice.", /* 1 */
  "Wrong number of args. Usage: dacout [kanal] spannung\n",
  "Voltage out of range (-10.0...+10.0 V).",
};

int emsg(int code) {
  fprintf(stderr,"%s\n",errormessage[code]);
  return code;
};

int main(int argc, char *argv[]) {
  int fh;
  float wert;
  int outword;
  int channel;

  fh=open(BOARDNAME,O_RDWR);
  if (fh==-1) return -emsg(1);

  switch (argc) {
  case 2:
    channel = 0;
    sscanf(argv[1],"%f",&wert);
    break;
  case 3:
    sscanf(argv[1],"%d",&channel);
    sscanf(argv[2],"%f",&wert);
    channel=(channel?1:0);
    break;
  default:
    return -emsg(2);
  };

  if (wert>10. || wert < -10.0) return -emsg(3);
  outword = (wert+10.0)*MAX_OUT_PATTERN/20.;
  if (outword <0) {
      outword = 0;
  }  else if (outword>MAX_OUT_PATTERN)   outword = MAX_OUT_PATTERN;

  ioctl(fh,channel?SET_AND_LOAD_DAC_1:SET_AND_LOAD_DAC_0, outword);

  close(fh);

}











