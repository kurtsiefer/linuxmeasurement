/* program to test the driver, minor device 1 */
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>

void the_handler(int sig) {
    printf("** BING! **\n");
    return;
}


int main(int argc, char *argv[]) {
    int fh,oflags;
    unsigned int cmd;
    int retval;
    unsigned long arg;
    fh=open("/dev/ioboards/dt302one", O_RDWR);
    if (fh<0) {
	printf("open failed.\n");
	return -1;
    }
    printf("open worked.\n");
    /* install signal handler */
    /* signal(SIGIO, &the_handler);
    fcntl(fh, F_SETOWN, getpid());
    oflags=fcntl(fh, F_GETFL);
    printf("found oflags: %x\n",oflags);
    fcntl(fh, F_SETFL, oflags | FASYNC); */


    do {
	printf("enter cmd and arg:");
	scanf("%x%x",&cmd,&arg);
	printf("received: cmd: %x, val: %x\n",cmd,arg);
	retval=ioctl(fh,cmd,arg);
	printf("return value: %d (hex: %x)\n",retval,retval);
    } while (cmd);
    close(fh);
}



