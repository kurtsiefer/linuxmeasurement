/* dt340.c
   
   Driver for the data translation DT340 Counter/timer& digIO card. Supplies
   a loadable kernel module for a 2.4 kernel, offering two minor devices.


 Copyright (C) 2002-2009 Christian Kurtsiefer <christian.kurtsiefer@gmail.com>

 This source code is free software; you can redistribute it and/or
 modify it under the terms of the GNU Public License as published
 by the Free Software Foundation; either version 2 of the License,
 or (at your option) any later version.

 This source code is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 Please refer to the GNU Public License for more details.

 You should have received a copy of the GNU Public License along with
 this source code; if not, write to:
 Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

--

   The most basic one is minor device 0, allowing simple read/write access
   to the complete address space of 64k offered by the card via ioctl calls as
   well as some driver-internal control registers. The minor device 1 is
   similar to minor device 0, but acquires also the interrrupt of the card.

   The driver also implements an interrupt handler which is able to both
   count interrupt events for the individual interrupt sources on the card,
   or notify the calling process with a SIGIO signal about an interrrupt
   occured. The behavior of the handler can be controlled via a count control
   register, a notify conrol register, and a last-pending-int register. With
   the latter, an application being signalled by the driver can determine
   what interrupt has caused the signal.

   minor device 0: For simple tests. Implements methods open, close and ioctl.
		   The open method returns a -EBUSY in case the device is in
		   use already. 
		   ioctl commands follow roughly the ideas in asm/ioctl.h:
		   Syntax is ioctl(handle,cmd,value), where cmd is an
		   unsigned long, with the lower 16 bits specifying the
		   address, and bitfield cmd[31:30] contains the direction of
		   the transfer: 0x1 means write to dt340, 0x0 indicates a read
		   access to a given address. For read access, no value has to
		   be supplied, and the register content is the return value
		   of the call. For write access, the unsigned long value
		   is written to the specified register. return value is 0 on
		   success.
		   Adresses have to be word-aligned, i.e., cmd[0:1]=0,
		   otherwise -1 is returned.
		   Read/write is always performed in longwords.
   minor device 1: same as minor 0, but with using the interrupt line (i.e., 
                   acquire it on opening ). The device also implements an
		   fasync method, allowing user notification with SIGIO upon
		   an occured interrupt. For this device, the interrupt
		   handler registers make sense using. Access to them is also
		   achieved using the ioctl method; there is read, write, and
		   read/write accesse possible using the bitfield [30:31] as
		   with the hardware registers. For Read/write access, the
		   operation is guaranteed not to be interrupted.
		   The registers are:
		     20 interrupt count registers, one for each possible irq
		       source. Address range is 0x10000 to 0x10013. For
		       example, a read of interrupt count register 5 can be
		       performed with the command ioctl(handle, 0x80010005).
		     a count control register, address 0x10014. The bitfield
		       [0:19] determines which interrupts should increment the
		       irq count registers. The ordering in the bitfield
		       corresponds to the order in the interrupt pending
		       register at address 0x2000.
		     a notify control register at address 0x10015. If a bit
		       in this register is set, the corresponding interrupt
		       can cause a SIGIO signal to be sent to the owner of
		       the open device. Therefore, the user has to install a
		       signal handler and activate the FASYNC mechanism using
		       the fcntl command.
		     a register on address 0x10016 containing the last iqr
		       pending register content. This allows to determine
		       which events caused the signal.
		     a access control register on address 0x10017 to negotiate
		       the access privileges for the different subsystems.
		       Currently a dummy routine, it should be accessed with
		       both read and write. the value written to the register
		       will contain the claimed access rights of the user, and
		       the return value whatever is granted by the driver to
		       this open call to avoid conflicts with other users of
		       the driver.
		   Note that activating the counting or notification does not
		   automatically enable the irqs on the card.

  The driver is alpha in the sense that the following options still should be
  implememented:
  - support for opening the device several times. It should be possible for
    independent processes to access different subunits, or for processes
    having "read only" access to registers, while only one is allowed to
    write in the device. Changes Therefore would affect the f_async structure,
    and would need a mechanism to declare the access needs of a process to
    allow for clever blocking techniques. An ioctl declaring the "greediness"
    (readonly, sharing, exclusive) of a process to the DIO or CT subsystem
    might work.
  - support for SMP machines. Currently, only little care was spent to avoid
    problems in these architectures.
  - check for other race conditions.
  - support for earlier kernels. Probably 2.2 and 2.0  compatibility is not
    hard to achieve, the irq handler uses bh mechanism, and devfs is not used.
    However, certainly some structure defs have to be changed.

    Thanks to Uwe Kimmerle from Data Translation for his uncomplicated help
    with a hardware register description of the card.
    Further reading: A. Rubini, J. Corbet, Linux device drivers, 2nd ed.,
    O'Reilly, 2001

    Copyright (C) 2002 Christian Kurtsiefer
    (christian.kurtsiefer@physik.uni-muenchen.de)

    First working version:                Christian Kurtsiefer, Mar 16, 2002
  
*/


#include <linux/module.h>
#include <linux/errno.h> 
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/pci.h> 
#include <asm/io.h>
#include <asm/ioctl.h>
#include <asm/system.h>
#include <linux/interrupt.h>


/* Module parameters */

MODULE_AUTHOR("Christian Kurtsiefer <christian.kurtsiefer@gmx.net>");
MODULE_DESCRIPTION("Data Translation DT340 driver for Linux\n");
MODULE_LICENSE("GPL");  

#define IOCARD_MAJOR 253
#define IOCARD_NAME "dt340_driver\0"
#define IOCARD_SPACE 0xffff
#define PCI_DEVICE_ID_DT340 0x0060
#define PCI_VENDOR_ID_DATX 0x1116

/* card register defs; more of that should be in a header file. */
#define DT340_INT_PENDING_ADR 0x2000
#define DT340_CONTROL_0_ADR 0x0
#define DT340_CONTROL_1_ADR 0x1000
#define INT_PENDING_MASK 0xfffff

/* there should be more of those */
#define IOCTL_ADRMASK 0xffff
#define IOCTL_DIRMASK 0xc0000000


/* local status variables */
static int IOCARD_IN_USE = 0;
static unsigned long access_mode; /* holds access mode; not used yet. */

static unsigned int mem_base0;
static int irq_number;
unsigned long irq_flags;
static char *cardspace; /* holds virtual address base of card mem */

/* interrupt handler and associated data structures */
static unsigned long intcounter[20]; /* tick counters for each interrupt */
static unsigned long countflag=0; /* contains bitfields enabling counting */
static unsigned long notify_flag=0; /* contains fields where to signal user */
static unsigned long last_IRQ_note=0; /* simple irq notification */
static unsigned long pending;
 
static struct fasync_struct *my_async_queue;
  
static void dt340_bh_handler(void *unused) { 
    last_IRQ_note |= (pending & notify_flag);
    /* tell it to the interested.. */
    kill_fasync(&my_async_queue, SIGIO, POLL_IN);
    return;
}
struct tq_struct notify_task = { routine: &dt340_bh_handler,
				 data: NULL,
				 sync: 0,
};

static void dt340_int_handler(int irq, void *dev_id, struct pt_regs *regs) {
    /* arguments are not needed here */
    static unsigned long pm;
    static int i;
    /* get interrupt status register */
    pending = readl(cardspace+DT340_INT_PENDING_ADR);
    mb();
    if (!(pending & INT_PENDING_MASK)) return; /* for other devices sharing the irq */
    pm=pending & countflag;
    if (pm) {/* skip if no counter is to be incremented */
	for (i=0;i<20;i++) {  /* increment all selected counters */
	    if (pm & (1<<i)) intcounter[i]++;
	}
    }
    /* clear all detected pending IRQs */
    writel(pending,cardspace+DT340_INT_PENDING_ADR);
   
    if ( pending & notify_flag ) {
	/* only run bottom half if someone wants a notice */
	queue_task(&notify_task, &tq_immediate);
	mark_bh(IMMEDIATE_BH);
    }
    return;
}

/* working routines for accessing the irq driver elements like counters
 and their controlling flags. The register is referenced by a pointer,
 dir is 1 for write, 2 for read, 3 for RW. 
 return value is either the data read BEFORE the write, 0 for ok or -1
 if an error has occured. */
unsigned int rw_regs(unsigned long *reg, unsigned int value, int dir) {
    unsigned long tmp;
    switch (dir) {
	case 1: /* write access */
	    *reg = value;
	    return 0;
	case 2: /* read access */
	    return *reg;
	case 3: /* read/wrice access */
	    /* Attention, a spinlock would avoid troubles here... */
	    tmp = *reg;*reg = value;
	    return tmp;
    }
    return -1;
}

/* for sending the calling process a signal */
static int dt340_fasync(int fd, struct file *filp, int mode) {
    return fasync_helper(fd, filp, mode, &my_async_queue);
}

/* open/release functions for getting the IRQ; minor device 1 */
static int dt340_flat_open_irq(struct inode *inode, struct file *filp) {
    int retval;
    if (IOCARD_IN_USE) 
	return -EBUSY;
    /* get irq */
    retval=request_irq(irq_number, dt340_int_handler, 
		       SA_INTERRUPT|SA_SHIRQ, IOCARD_NAME, &countflag);
    if (retval) return retval; /* no success in getting irq */

    IOCARD_IN_USE |= 1;
    MOD_INC_USE_COUNT;
    return 0;
}
static int dt340_flat_close_irq(struct inode *inode, struct file *filp) {
    unsigned long intstat;
    /* make sure no IRQ is issued */
    intstat=readl(cardspace+DT340_CONTROL_0_ADR) & ~0x0ff0; /* DIO IRQs */
    mb();writel(intstat,cardspace+DT340_CONTROL_0_ADR);
    intstat=readl(cardspace+DT340_CONTROL_1_ADR) & ~0x0fff; /* TIMER_IRQs */
    mb();writel(intstat,cardspace+DT340_CONTROL_1_ADR);
    /* remove async_wait_queue */
    dt340_fasync(-1,filp,0);

    /* give back irq */
    free_irq(irq_number, &countflag);

    MOD_DEC_USE_COUNT;
    IOCARD_IN_USE &= ~1;
    return 0;
}

/* minor device 0 (simple access) structures */
static int dt340_flat_open(struct inode *inode, struct file *filp) {
    if (IOCARD_IN_USE) 
	return -EBUSY;
    IOCARD_IN_USE |= 1;
    MOD_INC_USE_COUNT;
    return 0;
}
static int dt340_flat_close(struct inode *inode, struct file *filp) {
    MOD_DEC_USE_COUNT;
    IOCARD_IN_USE &= ~1;
    return 0;
}
static int dt340_flat_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg) {
    int dir = _IOC_DIR(cmd);
    unsigned long adr = cmd & IOCTL_ADRMASK;
/* printk("dt340: ioctl attemt to dev 0, cmd: %x  vaule: %x \n",cmd, arg); */


    /* quick and dirty address check (to avoid hardware damage?) */
    if (cmd & 0x3fff0003) {
	/* check for special ioctls for counters etc. */
	if (((cmd & 0x3fffffe0) == 0x10000) && ((cmd & 0x1f) < 24)) {
	    switch (cmd & 0x1f) {
		case 20:
		    return rw_regs(&countflag, arg, dir);
		case 21:
		    return rw_regs(&notify_flag, arg, dir);
		case 22:
		    return rw_regs(&last_IRQ_note, arg, dir);
		case 23: /* dummy routine for the moment; to control access */
		    return rw_regs(&access_mode, arg, dir);
		    break;
		default: /* accss to the irq event counters */ 
		    return rw_regs(&intcounter[cmd & 0x1f],arg, dir);
	    }
	}
	/* if we arrive here, an illegal address ioctl was given */
	printk("dt340: illegal ioctl command/address range %x\n",cmd);
	return -1;
    }
    switch ((adr >>12) & 0xf ) {/* check for gross address selection */
	case 0:case 1:case 2:case 0xc:case 0xd:
	    break;
	case 8:case 9: /* check for counter chip reserved adresses */
	    if ((adr & 0x7f)<0x34) break;    /* adr range [6:0] 0x00 to 0x30 */
	    if ((adr & 0x70) == 0x40) break; /* adr range [6:0] 0x40..0x4c */
	default:
	    printk("dt340: illegal address range \n");
	    return -1;
    }
    switch(dir) {
	case 1: /* write attempt */
	    writel(arg,cardspace+adr);
	    return 0;
	case 2: /* read attempt */
	    rmb(); /* compiler directive: read in all previous stuff NOW */
	    return readl(cardspace+adr);
	default:
	    printk("dt340: illegal direction %d for ioctl call.\n",dir);
	    return -1;
    }
   
    return 0;
}

/* minor device 0 (simple access) file options */
struct file_operations dt340_simple_fops = {
    open:    dt340_flat_open,
    release: dt340_flat_close,
    ioctl:   dt340_flat_ioctl,
};
/* minor device 1 (simple access with IRQ ) file options */
struct file_operations dt340_irq_fops = {
    open:    dt340_flat_open_irq,
    release: dt340_flat_close_irq,
    ioctl:   dt340_flat_ioctl,
    fasync: dt340_fasync,
};
/* the open method dispatcher for the different minor devices */
static int dt340_open(struct inode *inode, struct file *filp) {
    int mindevice = MINOR(inode->i_rdev);
    switch(mindevice) {
	case 0:
	    filp->f_op = &dt340_simple_fops;
	    break;
	case 1:
	    filp->f_op = &dt340_irq_fops;
	    break;
	default:
	    printk("dt340: no minor device %d supported.\n",mindevice);
	    return -ENODEV;
    }
    if (filp->f_op && filp->f_op->open) {
	return filp->f_op->open(inode,filp);
    }
    return 0; /* to keep compiler happy */
}
/* release dispatcher; maybe this could look simpler. */
static int dt340_release(struct inode *inode, struct file *filp) {
    int mindevice = MINOR(inode->i_rdev);
    switch (mindevice) {
	/* I am not sure I really need that; is the open dispatch enough? */
	case 0:
	    filp->f_op = &dt340_simple_fops;
	    break;
	case 1:
	    filp->f_op = &dt340_irq_fops;
	    break;
	default:
	    printk("dt340: no minor device %d supported.\n",mindevice);
	    return -ENODEV;
    }
    if (filp->f_op && filp->f_op->release) {
	return filp->f_op->release(inode,filp);
    }
    return 0; /* to keep compiler happy */
}

/* generic file operations */
struct file_operations dt340_fops = {
    open:    dt340_open,
    release: dt340_release,
};

/* initialisation of the driver: getting resources etc. */
static int __init dt340_init_one(struct pci_dev *dev, const struct pci_device_id *ent) {
    static int dev_count = 0;

    /* make sure there is only one card */
    dev_count++;
    if (dev_count > 1) {
	printk ("this driver only supports 1 board\n");
	return -ENODEV;
    }

    /* register resources with kernel */    
    if (pci_enable_device (dev)) 
	return -EIO;

    /* get resources */
    irq_number = dev->irq;
    mem_base0 = pci_resource_start(dev, 0);
  
    if (check_mem_region(mem_base0,IOCARD_SPACE+1)) {
	printk("dt340: memory at %x already in use\n",mem_base0);
	goto out1;
    }
    request_mem_region(mem_base0,IOCARD_SPACE+1, IOCARD_NAME);
    /* get virtual address for the pci mem */
    cardspace = (char *) ioremap_nocache(mem_base0,IOCARD_SPACE+1);
    printk("dt340: got new mem pointer: %p\n",cardspace);    
    /* register char devices with kernel */
    if (register_chrdev(IOCARD_MAJOR, IOCARD_NAME, &dt340_fops)<0) {
	printk("Error in registering major device %d\n",IOCARD_MAJOR);
	goto out2;
    }

    return 0; /* everything is fine */
 out2:
    iounmap(cardspace);
    release_mem_region(mem_base0,IOCARD_SPACE+1);  
 out1:
    return -EBUSY;
}
static void __exit dt340_remove_one(struct pci_dev *pdev) {
    unregister_chrdev(IOCARD_MAJOR, IOCARD_NAME);
    iounmap(cardspace);
    release_mem_region(mem_base0,IOCARD_SPACE+1);
}

/* driver description info for registration */
static struct pci_device_id dt340_pci_tbl[] = __initdata {
    {PCI_VENDOR_ID_DATX, PCI_DEVICE_ID_DT340, PCI_ANY_ID, PCI_ANY_ID, },
    {0,},
};

MODULE_DEVICE_TABLE(pci, dt340_pci_tbl);

static struct pci_driver dt340_driver = { 
    name:       "dt340-driver",
    id_table:   dt340_pci_tbl,
    probe:      dt340_init_one,
    remove:     dt340_remove_one,
};

static void  __exit dt340_clean(void) {
    pci_unregister_driver( &dt340_driver );
}

static int __init dt340_init(void) {
    int rc;
    rc = pci_module_init( &dt340_driver );
    if (rc < 0) return -ENODEV;
    return 0;
}

module_init(dt340_init);
module_exit(dt340_clean);






