/* program to test the driver */
#include <stdio.h>
#include <fcntl.h>


int main(int argc, char *argv[]) {
    int fh;
    unsigned int cmd;
    int retval;
    unsigned long arg;
    fh=open("/dev/ioboards/dt330zero", O_RDWR);
    if (fh<0) {
	printf("open failed.\n");
	return -1;
    }
    printf("open worked.\n");
    do{
	printf("enter cmd and arg:");
	scanf("%x%x",&cmd,&arg);
	printf("received: cmd: %x, val: %x\n",cmd,arg);
	retval=ioctl(fh,cmd,arg);
	printf("return value: %d (hex: %x)\n",retval,retval);
    } while (cmd);
    close(fh);
}



