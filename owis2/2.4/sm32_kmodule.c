/* sm32_kmodule.c

   Driver for the OWIS stepper motor driver sm32. Pci version of the older
   sm30 driver, but with better midlevel device.
   minor device 0 should give read/write access to the 0x20 bytes of dual port
   ram on the board, being referenced to by the third address region of the
   plx 9030 bridge.

 * Driver for a OWIS SM32 stepper motor driver card. This is rather clever
 * card, doing most of the tricky stuff on a microcontroller. This driver
 * only tries to give a convenient access to the various interface levels.
 * depending on the level, communication takes place with read/write commands
 * or with ioctl commands for transfering single bytes or pre-defined data
 * blocks.
 * The driver supplies a two minor devices for the different access
 * levels; opening one of the minor devices blocks usage of the others in
 * order to avoid conflicts.
 *
 * Here are the minor device capabilities:
 *
 * minor device 0:   allows the most basic access to the card. Here, the
 *                   physical adress space of the card (0x20 positions)
 *                   is mapped into a simple char device file. Access via
 *                   ioctl in the form 
 *                   int ioctl(int handle, int adr) for reading a byte
 *                   (adr=0..1f), and
 *                   int ioctl(int handle, int adr |0x1000, int value) for
 *                   writing a byte (LSB of value) into the physical IO ports.
 * minor device 3:   implements function calls to the motor similar to the
                     way the original sm32 calls are implemented. Communication
		     is again with ioctl. Sending commands to the motor driver,
		     the following form is used:
		     
		 int ioctl(int handle,  MOT_x | command_name | action, value);
		     where
		       command_name   is defined in sm32_2.h, 
		       MOT_x          is a macro defined in sm32_2.h referring
		                      to one of the motors if applicable, 
		       action         can be SM32Post, SM32Request to send
		                      either a write command or a request for
				      data command to the microcontroller.
				      
		     The call returns 0 on success, or -1 if busy
 
		     To check if data has been provided, use
		 int ioctl(int handle, MOT_x | SM32Replied)
		     The call returns 0 if the microcontroller has not yet
		     replied.

		     To retrieve the requested data (a long int), use
		 int ioctl(int handle, MOT_x | SM32Fetch)
		     where the return value contains the data.

		     To check if the microcontroller can accept new commands,
		 int ioctl(int handle, MOT_x | SM32IsReady)
		     returns !=0 if this is the case.

   Thanks to the better register structure, this driver should be nicer to
   use than the SM30 driver, and can possibly be extended to a
   multiple-process access topology.

   first working version              Christian Kurtsiefer, 24.03.2002
*/ 

#include <linux/module.h>
#include <linux/errno.h> 
#include <linux/kernel.h>
#include <linux/major.h>

#include <linux/init.h>
#include <linux/pci.h> 

#include <linux/ioport.h>

/* keep kernel tainting message quiet */
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
MODULE_AUTHOR("Christian Kurtsiefer <christian.kurtsiefer@gmx.net>");
MODULE_DESCRIPTION("Driver for OWIS SM32 stepper motor driver");


/* to force ugly imlement of old style driver */
/* #define oldstyle */

#ifdef oldstyle
#include "sm30_2.h"
#else
#include "sm32_2.h"
#endif



#define IOCARD_MAJOR 65
#define IOCARD_NAME "owis_sm32\0"
#define IOCARD_SPACE 0x1f

#define PCI_DEVICE_ID_PLX_9030 0x9030
#define PCI_DEVICE_SUB_ID_SM32 0x2337

/* for minor device 0 */
#define IOCARD_lower_bound 0
#define IOCARD_upper_bound 0x1f


static unsigned long IOCARD_BASE;  /* holds io address of dual port ram */
static int IOCARD_IN_USE = 0;


/*
 * faster read/write with a single command for the minor device 0. 
 * cmd=0x1000...0x101f: write register with value,
 * cmd=0x0..0x1f: return content of register
 */
static int ioctl_flat(struct inode * inode, struct file * file, unsigned int cmd, unsigned long value)
{      unsigned char w,v;
       unsigned int ad = (cmd & IOCARD_SPACE);
       
       if (ad >= IOCARD_lower_bound &&
	   ad <= IOCARD_upper_bound )
	 { ad += IOCARD_BASE;
	 if (cmd &0x1000)
	   { w=(unsigned char) value;
	     outb_p(w,ad);
	     return 0;
	   }
	 else 
	   { v=inb_p(ad);
	     return (int) v;
	   };
	 }
       else 
	 { return (int) -EFAULT;
	 };
}
static int flat_open(struct inode * inode, struct file * filp){
  /* check if someone else is using the card */
  if (IOCARD_IN_USE  != 0)
    return -EBUSY;
  IOCARD_IN_USE |= 1;
  MOD_INC_USE_COUNT;
  return 0;
};
static int flat_close(struct inode * inode, struct file * filp){ 
  /* release card */
  IOCARD_IN_USE &= !1;
  MOD_DEC_USE_COUNT;
  return 0;
};
static struct file_operations flat_fops = {
    ioctl:  ioctl_flat,	/* port_ioctl */
    open:   flat_open,	/* open code */
    release: flat_close,	/* release code */
};


/* here starts  midlevel device 2; kept for compatibility with existing 
   motor driver software. The current hardware access would allow a much
   smarter access of the motors, but this drivers allows to reuse code. 

   contains code from sm30_komodule, minor dev 2 */
/* routines for compatibility with main ioctl code, att: no adr check! */ 
int read_virtual_byte(int vad) {
  return (int) inb(IOCARD_BASE + vad); /* get byte */
}
void write_virtual_byte(int vad, int data){
   outb(data & 0xff, (IOCARD_BASE + vad));       /* put byte */
}
void write_virtual_bytes(int vad, char *data, int cnt) {
  int i;
  for (i=0;i<cnt;i++) 
    outb(data[i] & 0xff ,(vad+i+IOCARD_BASE));       /* put byte */
}
void read_virtual_bytes(int vad, char *data, int cnt) {
  int i;
  for (i=0;i<cnt;i++) 
    data[i]=inb(vad+i+IOCARD_BASE);       /* get byte */
}
static unsigned long tmplong;
unsigned int read_virtual_long(int vad) {
    read_virtual_bytes(vad,(char *) &tmplong,4);
    return tmplong;
}
void write_virtual_long(int vad,unsigned long value) {
    tmplong=value;
    write_virtual_bytes(vad,(char *) &tmplong,4);
}

#ifdef oldstyle
static int ioctl_midlevel(struct inode * inode, struct file * file, unsigned int cmd, unsigned long w1) {
  int retval;
  sm30_cmd_struct *args;
  int i;
  
  args = (sm30_cmd_struct *) w1;
  switch (cmd) {
      case SM30Init: /* read back some minor important values */
	  /* read back address */
	  args->SM30->Addr=IOCARD_BASE;
	  retval= 1;
	  break;
      case SM30CanWrite: /* NOW: CHECK ALL THREE MOTORS */
	  retval= !(1 & (read_virtual_byte(0) | read_virtual_byte(8) |
			 read_virtual_byte(0x10)));
	  break;
      case SM30Write:
	  if (1 & (read_virtual_byte(0) | read_virtual_byte(8) |
		   read_virtual_byte(0x10))) {/* kernel does not wait. */
	      retval = 0; break;};    /* can't write yet */
	  for (i=0;i<=2;i++) { /* go through all three motors */
	      if (args->sel & (1>>i)) {
		  write_virtual_long(i*8+4,args->data[i]);
		  write_virtual_byte(i*8+2,args->cmd); /* write cmd */
	      }}
	  retval=1;break;   /* everything is ok */
      case SM30Request:
	  if (args->cmd != mcNone) {
	      for (i=0;i<3;i++) 
		  write_virtual_byte(i*8+3,args->cmd);
	  };
	  retval=0;break;
      case SM30Replied:
	  /* see if there is something */
	  retval=0; for (i=0;i<3;i++) retval |= read_virtual_byte(i*8+0);
	  retval= !(retval & 02 );break;
      case SM30Read: /*modified; no fancy status/error things */
	  retval=0; for (i=0;i<3;i++) retval |= read_virtual_byte(i*8+0);
	  if (!(retval= !(retval & 02 )));break; /* kernel does not wait */
	  /* read in data */
	  for (i=0;i<3;i++) args->data[i] = read_virtual_long(i*8+4);
	  retval=0;break;
      default:
	  retval = (int)  -EFAULT;
	  break;
  };
  return  retval;
}

static int midlevel_open(struct inode * inode, struct file * filp){
  /* check if someone else is using the card */
  if (IOCARD_IN_USE  != 0)
    return -EBUSY;
 
  IOCARD_IN_USE |= 1;
  MOD_INC_USE_COUNT;
  return 0;
};
static int midlevel_close(struct inode * inode, struct file * filp){ 
  /* release card */
  IOCARD_IN_USE &= !1;
  MOD_DEC_USE_COUNT;
  return 0;
};

static struct file_operations midlevel_fops = {
	ioctl:    ioctl_midlevel,	/* port_ioctl */
	open:     midlevel_open,	/* open code */
	release:  midlevel_close,	/* release code */
};
#endif


/* Implementation of a less chaotic driver interface, just exchanging
   the commands for each motor individually */

#define oFlags   0        /* offset of tChannel.Cmd  */
#define oWCmd    2        /* offset of tChannel.WCmd */
#define oRCmd    3        /* offset of tChannel.RCmd */
#define oData    4        /* offset of tChannel.Data */
#define CHANNELSIZE   8   /* sizeof(tChannel)        */ 
#define  cWFlag   1  /*if( Flags  &  cWFlag ) then command not processed yet*/
#define  cRFlag   2  /*if( Flags  &  cRFlag ) then data not available yet   */

static int ioctl_direct(struct inode * inode, struct file * file, unsigned int cmd, unsigned long value) {
    int retval=0;
    int mot = (cmd & SM32_MOTOR_MASK)>>SM32_MOTOR_SHIFT;
    int cmd2 = cmd & SM32_COMMAND_MASK;
    
    switch (cmd & SM32_CALL_MASK) {
	case SM32Post:
	    if (read_virtual_byte(mot*CHANNELSIZE) &  cWFlag ) return 1;
	    write_virtual_long(mot*CHANNELSIZE+ oData, value);
	    write_virtual_byte(mot*CHANNELSIZE+oWCmd, cmd2);
	    retval=0;break;
	case SM32Request:
	    write_virtual_byte(mot*CHANNELSIZE+oRCmd, cmd2);
	    retval=0;break;
	case SM32Replied:
	    retval= !(read_virtual_byte(mot*CHANNELSIZE) &  cRFlag); break;
	case SM32Fetch:
	    retval=read_virtual_long(mot*CHANNELSIZE+oData);break;
	case SM32IsReady:
	    retval = !(read_virtual_byte(mot*CHANNELSIZE) &  cWFlag);break;
	default:
	    retval= (int) -EFAULT;
    }
  return  retval;
}

static int open_direct(struct inode * inode, struct file * filp){
  /* check if someone else is using the card */
  if (IOCARD_IN_USE  != 0)
    return -EBUSY;
 
  IOCARD_IN_USE |= 1;
  MOD_INC_USE_COUNT;
  return 0;
};
static int close_direct(struct inode * inode, struct file * filp){ 
  /* release card */
  IOCARD_IN_USE &= !1;
  MOD_DEC_USE_COUNT;
  return 0;
};

static struct file_operations direct_fops = {
	ioctl:    ioctl_direct,	/* port_ioctl */
	open:     open_direct,	/* open code */
	release:  close_direct,	/* release code */
};




/* generic open/close for all minor subdevices */
static int iocard_open(struct inode *inode, struct file * filp) {
    int mindev =  MINOR(inode->i_rdev);
  switch (mindev) { 
      case 0:
	  /* flat adressing of IO module */
	  filp->f_op = &flat_fops;
	  break;
#ifdef oldstyle
      case 2:
	  /* old-style minor device for higher level commands */
	  filp->f_op = &midlevel_fops;
	  break;  
#else
      case 3:
	  /* new-style minor device for higher level commands */
	  filp->f_op = &direct_fops;
	  break;  
#endif
      default:
	  return -ENXIO;
  };
  if (filp->f_op && filp->f_op->open) 
      return filp->f_op->open(inode,filp);
  return 0;
}

static int iocard_close(struct inode * inode, struct file * filp) {
    int mindev = MINOR(inode->i_rdev);
  switch (mindev) {
      case 0:
	  /* flat adressing of IO module */
	  filp->f_op = &flat_fops;
	  break;
#ifdef oldstyle
      case 2:
	  /* old- style minor device for higher level commands */
	  filp->f_op = &midlevel_fops;
	  break;
#else
      case 3:
	  /* new-style minor device for higher level commands */
	  filp->f_op = &direct_fops;
	  break;  
#endif
      default:
	  return 0;
  };
  if (filp->f_op && filp->f_op->release)
      return filp->f_op->release(inode,filp);
  return 0;
}

/* generic file operations */
struct file_operations sm32_fops = {
    open:    iocard_open,
    release: iocard_close,
};

/* initialisation of the driver: getting resources etc. */
static int __init sm32_init_one(struct pci_dev *dev, const struct pci_device_id *ent) {
    static int dev_count = 0;

    /* make sure there is only one card */
    dev_count++;
    if (dev_count > 1) {
	printk ("this driver only supports 1 board\n");
	return -ENODEV;
    }

    /* get resources */
    IOCARD_BASE =  pci_resource_start(dev, 3);
    if (check_region(IOCARD_BASE, IOCARD_SPACE + 1 ))  goto out1;
    request_region(IOCARD_BASE, IOCARD_SPACE+1 , IOCARD_NAME);
    /* register resources with kernel */    
    if (pci_enable_device (dev)) 
	return -EIO;
    request_region(IOCARD_BASE, IOCARD_SPACE+1 , IOCARD_NAME);
    
    /* register char devices with kernel */
    if (register_chrdev(IOCARD_MAJOR, IOCARD_NAME, &sm32_fops)<0) {
	printk("Error in registering major device %d\n",IOCARD_MAJOR);
	goto out2;
    }

    return 0; /* everything is fine */
 out2:
 out1:
    return -EBUSY;
}

static void __exit sm32_remove_one(struct pci_dev *pdev) {
    release_region(IOCARD_BASE, IOCARD_SPACE+1);
    unregister_chrdev(IOCARD_MAJOR, IOCARD_NAME);
    /* release io space */
}

/* driver description info for registration */
static struct pci_device_id sm32_pci_tbl[] = __initdata {
    {PCI_VENDOR_ID_PLX,PCI_DEVICE_ID_PLX_9030 ,
     PCI_VENDOR_ID_PLX, PCI_DEVICE_SUB_ID_SM32, },
    {0,},
};

MODULE_DEVICE_TABLE(pci, sm32_pci_tbl);

static struct pci_driver sm32_driver = { 
    name:       "sm32-driver",
    id_table:   sm32_pci_tbl,
    probe:      sm32_init_one,
    remove:     sm32_remove_one,
};



static void  __exit sm32_clean(void) {
    pci_unregister_driver( &sm32_driver );
}

static int __init sm32_init(void) {
    int rc;
    rc = pci_module_init( &sm32_driver );
    if (rc < 0) return -ENODEV;
    return 0;
}

module_init(sm32_init);
module_exit(sm32_clean);


