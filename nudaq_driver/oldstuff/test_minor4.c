/* program to test the minor 4 device of the nudaq driver to output data

 */
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/time.h>

#define nudaq_device "/dev/ioboards/nudaq_dmawrite" /* nudaq device name */
#define size_dma  (1<<18) /* kernel buffer size, should be multiple of 4k */
#define POLLING_INTERVAL 1000 /* in milliseconds */

/* device 4 specific commands */
#define NUDAQ_4_START_DMA  0x1011  /* start write dma */
#define NUDAQ_4_STOP_DMA   0x1012  /* stop write dma */
#define NUDAQ_4_READ_ERR   0x1013  /* read error */
#define NUDAQ_4_INWORD     0x1015  /* read word from input port */
#define NUDAQ_4_BYTES_SENT 0x1014  /* number of transfered bytes */
#define NUDAQ_4_DITIMERS   0x1017  /* set internal timers 1 and 2 */
#define NUDAQ_4_OUTMODE    0x1018 
/* values for outmode: */
#define DO_MODE0 0x00 /* internal timer source , only timer 1, no o_ack */
#define DO_MODE3 0x03 /* o_req & o_ack handshake, PCI bus timing */
#define DO_MODE1_CASCADE 0x04 /* internal timers 1 and 2 cascaded as source */
#define DO_TRIG 0x10 /* trigger outpot set:1 */
#define DO_CLEAR_FIFO 0x20 /* eventually clear FIFO before acquisition */
#define DO_O_ACK_INT 0x100 /* allow for irqs in O_ack */
#define DO_UND 0x200 /* clear underflow flag */


/* structures for timer interrupts */
struct itimerval newtimer = {{0,0},{0,POLLING_INTERVAL*1000}};
struct itimerval stoptime = {{0,0},{0,0}}; 


/* handler for itimer signal SIGALRM */
int alarmcount,maxalarms;
void timer_handler(int sig) {
    /* static int quadsread; */
    if (sig==SIGALRM) {
	setitimer(ITIMER_REAL, &newtimer,NULL); /* restart timer */
	alarmcount++;
    }
}

/* table for translating a diode pattern in a RAM pattern */
int transtable[16] = { 0, 0x10000000, 0x1000000, 0x11000000,
		       0x100000, 0x10100000, 0x1100000, 0x11100000,
		       0x10000, 0x10010000, 0x1010000, 0x11010000,
		       0x110000, 0x10110000, 0x1110000, 0x11110000};
int pattern;

int main(int argc, char * argv[]){
    int retval;
    /* stuff for dma buffer */
    unsigned char *startad=NULL;
    int fh;
    int i;
    unsigned long * datafield;
    
    if ((fh=open(nudaq_device,O_RDWR))==-1) {
	printf("error opening device.\n");
	return -1;
    };
    printf("Open succeeded.\n");

    /* organize buffer */
    /* try to reserve memory for DMA transfer */
    startad=mmap(NULL,size_dma,PROT_READ|PROT_WRITE, MAP_SHARED,fh,0);
    if (!startad) {
	printf("Error getting buffer.\n");
	return -1;
    }
    printf("Got buffer of size 0x%x at 0x%p\n",size_dma,startad);

    /* fill buffer, start wit clearing */
    datafield = (unsigned long *) startad;
    for (i=0;i<size_dma;i++) startad[i]=0; 
    /* go through index of bit patterns */
    for (i=0;i<size_dma;i+=1) {
	/* index in data field */
	pattern=(((i%54321)<500)?0xf:0);
	datafield[i>>2]|=transtable[pattern & 0xf]<<(i&3);
	} 
    /* for (i=0;i<(size_dma/4);i++) {
	datafield[i]=(((i & 0x1)==0)?0xffff:0)<<16; 
	}*/ 
    printf("startad: %p, dataf: %p\n",startad, datafield);
    printf("longsiz: %d\n",sizeof(unsigned long));
    printf("dataf(4): %p, content: %li\n",&datafield[4],datafield[4]);
    printf(" filled buffer.\n");

    for (i=0;i<200;i++) {
	printf("%02x ",startad[i]);
	if ((i & 0x7)==7) printf(" ");
	if ((i & 0xf) == 0xf) printf("\n");
    }

    /* set up dma mode */
     retval=ioctl(fh,NUDAQ_4_OUTMODE, DO_CLEAR_FIFO | DO_MODE0 ); 
    retval=ioctl(fh,NUDAQ_4_OUTMODE, DO_CLEAR_FIFO | DO_MODE3 );
    printf("set up dma mode, retval: %d\n",retval);
    /*   retval=ioctl(fh,NUDAQ_4_DITIMERS,0x28); */ /* 10 ms */
    printf("set up timers, retval: %d\n",retval); 
    
    
    /* install timeout routine */
    /* install signal handler for polling timer clicks */
    signal(SIGALRM, &timer_handler);


    /* preparation of timer signals */
    alarmcount=0;
    setitimer(ITIMER_REAL, &newtimer,NULL);

    /* start DMA */
    retval=ioctl(fh,NUDAQ_4_START_DMA);
    printf(" started DMA, retval: %d\n",retval);

    do {
	pause();
	printf("hit number %d\n",alarmcount);
	retval=ioctl(fh,NUDAQ_4_BYTES_SENT);
	printf(" bytes sent: %d\n",retval);
	retval=ioctl(fh,0x200f);
	printf("int use ccounter: %d\n",retval);
	retval=ioctl(fh,0x200a);
	printf("MCSR: %x\n",retval);
	retval=ioctl(fh,0x200b);
	printf("INTCSR: %x\n",retval);

    } while (alarmcount < 10 );
    /* stop dma */
    retval=ioctl(fh,NUDAQ_4_STOP_DMA);
    printf(" stopped DMA, retval: %d\n",retval);
   

    /* switch off polling timer */
    setitimer(ITIMER_REAL, &stoptime, NULL);


    close(fh);
    return 0;
}


