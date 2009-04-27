/* sm32_kmodule.c for kernel version 2.6

   Driver for the OWIS stepper motor driver sm32. 
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
   transition to kernel version 2.6;removed minor 2 precautions; made driver
   able to handle multiple cards (yet to be tested, works fine with one)
   loads/unloads cleanly.  7.11.2006 chk
*/ 

#include <linux/module.h>
#include <linux/errno.h> 
#include <linux/kernel.h>
#include <linux/major.h>

#include <linux/init.h>
#include <linux/pci.h> 

#include <linux/ioport.h>

/* keep kernel tainting message quiet */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Christian Kurtsiefer <christian.kurtsiefer@gmx.net>");
MODULE_DESCRIPTION("Driver for OWIS SM32 stepper motor driver for kernel 2.6");

#include "sm32_2.h"

#define IOCARD_MAJOR 0 /* old style: 65; dynamical access: 0 */
#define IOCARD_NAME "sm32_kmodule\0"
#define IOCARD_SPACE 0x1f

#define PCI_DEVICE_ID_PLX_9030 0x9030
#define PCI_DEVICE_SUB_ID_SM32 0x2337

/* for minor device 0 */
#define IOCARD_lower_bound 0
#define IOCARD_upper_bound 0x1f

/* local status variables for cards */
#define NUMBER_OF_CARDS 5
static int dev_count = 0;  /* holds number of cards loaded */

/* structure to allow for multiple cards */
typedef struct cardinfo {
    int iocard_opened;
    unsigned int iocard_base; /* holds io address of dual port ram */
    int major;                /* major of each card */
} cdi;

/* holds several card pointers */
static struct cardinfo *cif[NUMBER_OF_CARDS];

/*
 * faster read/write with a single command for the minor device 0. 
 * cmd=0x1000...0x101f: write register with value,
 * cmd=0x0..0x1f: return content of register
 */
static int ioctl_flat(struct inode * inode, struct file * filp, unsigned int cmd, unsigned long value) {
    struct cardinfo *cp = (struct cardinfo *)filp->private_data;
    unsigned char w,v;
    unsigned int ad = (cmd & IOCARD_SPACE);
       
       if (ad >= IOCARD_lower_bound &&
	   ad <= IOCARD_upper_bound )
	 { ad += cp->iocard_base;
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

/* search cardlists for a particular major number */
static struct cardinfo *search_cardlist(int major) {
    int i;
    for (i=0;i<dev_count;i++) {
	if (cif[i]->major == major) return cif[i];
    }
    /* no success otherwise */
    return NULL;
}

static int flat_open(struct inode * inode, struct file * filp){
    struct cardinfo *cp;
    cp= search_cardlist(imajor(inode));
    /* printk("open flat; cp: %p, maj: %d, opened: %d\n",cp,cp->major, cp->iocard_opened); */
    if (!cp) return -ENODEV;
    if (cp->iocard_opened) 
	return -EBUSY;
    cp->iocard_opened = 1;
    filp->private_data = (void *)cp; /* store card info in file structure */
    return 0;
};
static int flat_close(struct inode * inode, struct file * filp){ 
    struct cardinfo *cp = (struct cardinfo *)filp->private_data;
    if (cp) cp->iocard_opened = 0;
    return 0;
};
static struct file_operations flat_fops = {
    ioctl:  ioctl_flat,	/* port_ioctl */
    open:   flat_open,	/* open code */
    release: flat_close,	/* release code */
};


/* routines for compatibility with main ioctl code, att: no adr check! */ 
__inline__ int read_virtual_byte(struct cardinfo *cp, int vad) {
  return (int) inb(cp->iocard_base + vad); /* get byte */
}
__inline__ void write_virtual_byte(struct cardinfo *cp, int vad, int data){
   outb(data & 0xff, (cp->iocard_base + vad));       /* put byte */
}
__inline__ void write_virtual_bytes(struct cardinfo *cp, int vad,
				    char *data, int cnt) {
  int i;
  for (i=0;i<cnt;i++) 
    outb(data[i] & 0xff ,(vad+i+cp->iocard_base));       /* put byte */
}
__inline__ void read_virtual_bytes(struct cardinfo *cp, int vad,
				   char *data, int cnt) {
  int i;
  for (i=0;i<cnt;i++) 
    data[i]=inb(vad+i+cp->iocard_base);       /* get byte */
}
static unsigned long tmplong;
unsigned int read_virtual_long(struct cardinfo *cp, int vad) {
    read_virtual_bytes(cp, vad,(char *) &tmplong,4);
    return tmplong;
}
void write_virtual_long(struct cardinfo *cp, int vad,unsigned long value) {
    tmplong=value;
    write_virtual_bytes(cp, vad,(char *) &tmplong,4);
}


/* Implementation of a less chaotic driver interface, just exchanging
   the commands for each motor individually */

#define oFlags   0        /* offset of tChannel.Cmd  */
#define oWCmd    2        /* offset of tChannel.WCmd */
#define oRCmd    3        /* offset of tChannel.RCmd */
#define oData    4        /* offset of tChannel.Data */
#define CHANNELSIZE   8   /* sizeof(tChannel)        */ 
#define  cWFlag   1  /*if( Flags  &  cWFlag ) then command not processed yet*/
#define  cRFlag   2  /*if( Flags  &  cRFlag ) then data not available yet   */

static int ioctl_direct(struct inode * inode, struct file * filp, unsigned int cmd, unsigned long value) {
    struct cardinfo *cp = (struct cardinfo *)filp->private_data;
    int retval=0;
    int mot = (cmd & SM32_MOTOR_MASK)>>SM32_MOTOR_SHIFT;
    int cmd2 = cmd & SM32_COMMAND_MASK;
    
    switch (cmd & SM32_CALL_MASK) {
	case SM32Post:
	    if (read_virtual_byte(cp, mot*CHANNELSIZE) &  cWFlag ) return 1;
	    write_virtual_long(cp, mot*CHANNELSIZE+ oData, value);
	    write_virtual_byte(cp, mot*CHANNELSIZE+oWCmd, cmd2);
	    retval=0;break;
	case SM32Request:
	    write_virtual_byte(cp, mot*CHANNELSIZE+oRCmd, cmd2);
	    retval=0;break;
	case SM32Replied:
	    retval= !(read_virtual_byte(cp, mot*CHANNELSIZE) &  cRFlag); break;
	case SM32Fetch:
	    retval=read_virtual_long(cp, mot*CHANNELSIZE+oData);break;
	case SM32IsReady:
	    retval = !(read_virtual_byte(cp, mot*CHANNELSIZE) &  cWFlag);break;
	default:
	    retval= (int) -EFAULT;
    }
  return  retval;
}

static int open_direct(struct inode * inode, struct file * filp){
    struct cardinfo *cp;
    cp= search_cardlist(imajor(inode));
    /* printk("open direct; cp: %p, maj: %d, opened: %d\n",cp,cp->major, cp->iocard_opened); */
    if (!cp) return -ENODEV;
    if (cp->iocard_opened) 
	return -EBUSY;
    cp->iocard_opened = 1;
    filp->private_data = (void *)cp; /* store card info in file structure */
    return 0;
};

static int close_direct(struct inode * inode, struct file * filp){ 
    struct cardinfo *cp = (struct cardinfo *)filp->private_data;
    if (cp) cp->iocard_opened = 0;
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
      case 3:
	  /* new-style minor device for higher level commands */
	  filp->f_op = &direct_fops;
	  break;  
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
      case 3:
	  /* new-style minor device for higher level commands */
	  filp->f_op = &direct_fops;
	  break;  
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
    struct cardinfo *cp; /* pointer to this card */

    /* make sure there is enough card space */
    if (dev_count >= NUMBER_OF_CARDS) {
	printk ("sm32: this driver only supports %d boards\n",
		NUMBER_OF_CARDS);
	return -ENODEV;
    }
    cp = (struct cardinfo *)kmalloc(sizeof(struct cardinfo),GFP_KERNEL);
    if (!cp) {
	printk("sm32: Cannot kmalloc device memory\n");
	return -ENOMEM;
    }
    
    cp->iocard_opened=0; /* initialize as closed */

    /* get resources */
    cp->iocard_base =  pci_resource_start(dev, 3);

    /* register resources with kernel */    

    

    /*  depeciated under kernel 2.6 ?? */
    if (!request_region(cp->iocard_base, IOCARD_SPACE+1 , IOCARD_NAME))
	goto out1;

    if (pci_enable_device (dev)) 
	return -EIO;

    /* register char devices with kernel */
    cp->major = register_chrdev(IOCARD_MAJOR, IOCARD_NAME, &sm32_fops);
    if (cp->major<0) {
	printk("Error in registering major device %d\n",IOCARD_MAJOR);
	goto out2;
    }
    if (IOCARD_MAJOR) cp->major=IOCARD_MAJOR;

    cif[dev_count]=cp; /* for shorter reference */
    dev_count++; /* got one more card */

    return 0; /* everything is fine */
 out2:
 out1:
    return -EBUSY;
}

static struct cardinfo * find_card_from_membase(unsigned int iocard_base){
    int i;
    for (i=0;i<dev_count;i++) {
	if (cif[i]->iocard_base == iocard_base) return cif[i];
    }
    /* no success otherwise */
    return NULL;
}

static void __exit sm32_remove_one(struct pci_dev *pdev) {
    struct cardinfo *cp; /* to retreive card data */
    cp = find_card_from_membase(pci_resource_start(pdev, 3));
    if (!cp) {
	printk("sm32: Cannot find device entry for sm32 to unload card\n");
	return;
    }

    unregister_chrdev(cp->major, IOCARD_NAME);
    /* release io space */
    release_region(cp->iocard_base, IOCARD_SPACE+1);
    pci_disable_device(pdev);
    kfree(cp); /* give back card data container structure */
}

/* driver description info for registration */
static struct pci_device_id sm32_pci_tbl[] = {
    {.vendor = PCI_VENDOR_ID_PLX, .device = PCI_DEVICE_ID_PLX_9030,
     .subvendor = PCI_VENDOR_ID_PLX, .subdevice = PCI_DEVICE_SUB_ID_SM32,
     .class = 0, .class_mask = 0, .driver_data = 0},
    {0,0,0,0,0,0,0},
};

MODULE_DEVICE_TABLE(pci, sm32_pci_tbl);

static struct pci_driver sm32_driver = { 
    name:       "sm32_kmodule",
    id_table:   sm32_pci_tbl,
    probe:      sm32_init_one,
    remove:     sm32_remove_one,
};



static void  __exit sm32_clean(void) {
    pci_unregister_driver( &sm32_driver );
}

static int __init sm32_init(void) {
    int rc;
    rc = pci_register_driver( &sm32_driver );
    if (rc < 0) return -ENODEV;
    return 0;
}

module_init(sm32_init);
module_exit(sm32_clean);
