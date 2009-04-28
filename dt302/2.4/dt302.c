/* dt302.c    (backported version from 2.6 driver, 24.9.06chk)

 Copyright (C) 2002-2006 Christian Kurtsiefer <christian.kurtsiefer@gmail.com>

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

---

   Driver for the data translation DT302 multi-function card.
   planned to be a loadable kernel module, offering several minor devices for
   different levels of access complexity.

   In its initial version, the driver is capable of handling only one card.

   The most basic minor device 0 allows simple read/write access to the
   IO-space of 64kbyte presented by the card on the PCI bus. No careful
   address checking performed yet on this device. R/W is performed via
   IOCTL.

   Minor device 1 supplies access to the card and the DMA buffer for AD
   conversion results. Main communication of setting up the card is
   performed via IOCTLs, but the buffer RAM can be also accessed via the
   read method, which returns the last results (in 16-bit chuncks) in
   a non-blocking manner. Direct mmap access to the DMA buffer is TBD.

   see dt302.h for the large variety of minor device 1 commands.


   first working code minor dev 0       28.09.2002 christian Kurtsiefer
   all subsystems except AD work


   DMA buffer acquisition works         29.09.02 13:30chk
   AD subsystem works on minor device 0

   minor device 1 DA and DIGIO subsystems work 29.09.02 23:00
   minor device 1 AD subsystem works           13.10.02 15:10
   minor device 1 Timer subsystem works        16.10.02 23:00
   backpropagated aspects from the 2.6 driver to
   use the same dt302.h file: implemented IDENTIFY_DTAX_CARD,
   PRELOAD_CGL, read method could follow
 
   current state:
   docu der min device 0 features steht noch aus.
   docu der min device 1 features steht noch aus.

   ToDo:
   - Implementierung der read-methode fuer DMA-Puffer
   - Implementation von TRIMPOT_WRITE/READ commands
   - Kartenidentifikations- ioctl.
   - verallgemeinerte Funktionalitaet auf alle dt30x Karten
   - Verwaltung mehrerer Karten 
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

#include "dt302.h"

/* solves problem with kernel taint*/
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif


/*
#define DEBUG
*/

#define IOCARD_MAJOR 251
#define IOCARD_NAME "dt302_driver\0"
#define IOCARD_SPACE 0xffff
#define PCI_DEVICE_ID_DT302 0x0041
#define PCI_VENDOR_ID_DATX 0x1116


/* there should be more of those */
#define IOCTL_ADRMASK 0xffff
#define IOCTL_CMDMASK 0xfff3
#define DAC_VALUE_MASK 0xffff
#define DAC_TRANSFER_DONE_REQUEST_ATTEMPTS 100

/* RAM buffer related stuff */
#define BUFFER_RAM_PAGE_ORDER 7
#define BUFFER_ALIGNMENT_MASK 0x3ffff
static int targetorder; /* order of RAM buffer obtained from the OS */
static unsigned char *rawtargetbuffer; /* kernel address of buffer obtained from OS */
static dma_addr_t buf_busaddr; /* aligned buffer; bus address */
static unsigned char *aligned_buffer;   /* aligned buffer pointer; kernel address */
static int already_transferred_values; /* user transfer count */

/* local status variables */
static int IOCARD_IN_USE = 0;
static unsigned int mem_base0;
static char *cardspace; /* holds virtual address base of card mem */


/* minor device 0 (simple access) structures */
static int dt302_flat_open(struct inode *inode, struct file *filp) {
    if (IOCARD_IN_USE) 
	return -EBUSY;
    IOCARD_IN_USE |= 1;
    MOD_INC_USE_COUNT;
    return 0;
}
static int dt302_flat_close(struct inode *inode, struct file *filp) {
    MOD_DEC_USE_COUNT;
    IOCARD_IN_USE &= ~1;
    return 0;
}
static int dt302_flat_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg) {
    int dir = _IOC_DIR(cmd);
    unsigned long adr = cmd & IOCTL_ADRMASK;
/* printk("dt302: ioctl attemt to dev 0, cmd: %x  vaule: %x \n",cmd, arg); */

    switch ((adr >>12) & 0xf ) {/* check for gross address selection */
	case 0:case 1:case 2:case 3:case 8:case 0xa:case 0xc:case 0xe:case 0xf:
	    break;
	    /* only for testing :*/
	case 4:
	    /* transfer buffer start to busmaster control register on write*/
	    switch (dir) {
		case 1: /* write attempt */
		    writel(buf_busaddr,cardspace+0x4000);
		    return 0;
		case 2: /* read attempt */
		    rmb(); /*  read in all previous stuff NOW */
		    return readl(cardspace+0x4000);
		default:
		    printk("dt302: illegal direction in ioctl.\n");
		    return -1;
	    }  
	case 5: /* read/write DMA buffer bytewise, adr covers first 4k */
	    switch (dir) {
		case 1: /* write */
		    aligned_buffer[adr & 0xfff] = (unsigned char)(arg & 0xff);
		    return 0;
		case 2: /* read */
		    return (unsigned int) aligned_buffer[adr & 0xfff];
		default:
		    return -1;
	    }
	default:
	    printk("dt302: illegal address range \n");
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
	    printk("dt302: illegal direction %d for ioctl call.\n",dir);
	    return -1;
    }
   
    return 0;
}



/* minor device 1 (advanced access) structures */
static int dt302_advanced_open(struct inode *inode, struct file *filp) {
    if (IOCARD_IN_USE) 
	return -EBUSY;
    IOCARD_IN_USE |= 1;
    MOD_INC_USE_COUNT;
    return 0;
}
static int dt302_advanced_close(struct inode *inode, struct file *filp) {
    MOD_DEC_USE_COUNT;
    IOCARD_IN_USE &= ~1;
    return 0;
}
static int dt302_advanced_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg) {
    int dir = _IOC_DIR(cmd);
/*    unsigned long cmd2 = cmd & IOCTL_CMDMASK; */
    unsigned long tmp1,tmp2;
    int tmp3;
    
    /* just for debug */
    if (dir) return dt302_flat_ioctl(inode, filp, cmd, arg);

#ifdef DEBUG
    printk("ioctl command: %x\n",cmd);
#endif

    switch (cmd) {/* parse different ioctls */
	case SET_DAC_0:case SET_DAC_1:
	    if (readl(cardspace+GEN_STATUS_REG) & DA_SHIFT_IN_PROGRESS) 
		return -1; /* shift in progress error */
	    writel(arg & DAC_VALUE_MASK, 
		   cardspace+((cmd == SET_DAC_1)?DAC_1_REG:DAC_0_REG));
	    break;
	case LOAD_DAC_0:case LOAD_DAC_1:case LOAD_DAC_0_AND_1:
	    if (readl(cardspace+GEN_STATUS_REG) & DA_SHIFT_IN_PROGRESS) 
		return -1; /* shift in progress error */
	    tmp1=readl(cardspace+GEN_CONTROL_REG_1);mb();
	    tmp2=(tmp1 & ~DAC_SAMPLE_CLK_MASK) | 
		((cmd & 0x3)<<DAC_SAMPLE_CLK_SHIFT);
	    writel(tmp2, cardspace + GEN_CONTROL_REG_1);
	    break;
	case DAC_TRANSFER_ACTIVE:
	    mb();
	    tmp1=readl(cardspace+GEN_STATUS_REG) & DA_SHIFT_IN_PROGRESS;
	    return tmp1;
	case WAIT_DAC_TRANSFER_DONE:
	    tmp1=DAC_TRANSFER_DONE_REQUEST_ATTEMPTS;
	    while (tmp1) {
		mb();
		if (!(readl(cardspace+GEN_STATUS_REG) & DA_SHIFT_IN_PROGRESS)) 
		    break;
		tmp1--;
	    };
	    return (!tmp1);
	case SET_AND_LOAD_DAC_0:case SET_AND_LOAD_DAC_1:
	    if (readl(cardspace+GEN_STATUS_REG) & DA_SHIFT_IN_PROGRESS) 
		return -1; /* shift in progress error */
	    writel(arg & DAC_VALUE_MASK, cardspace+
		   ((cmd == SET_AND_LOAD_DAC_1)?DAC_1_REG:DAC_0_REG));
    	    tmp1=DAC_TRANSFER_DONE_REQUEST_ATTEMPTS;
	    while (tmp1) {
		mb();
		if (!(readl(cardspace+GEN_STATUS_REG) & DA_SHIFT_IN_PROGRESS)) 
		    break;
		tmp1--;
	    };
	    if (!tmp1) return -1; /* transfer took too long */
	    tmp1=readl(cardspace+GEN_CONTROL_REG_1);mb();
	    tmp2=(tmp1 & ~DAC_SAMPLE_CLK_MASK) | 
		(((cmd==SET_AND_LOAD_DAC_0)?1:2)<<DAC_SAMPLE_CLK_SHIFT);
	    writel(tmp2, cardspace + GEN_CONTROL_REG_1);
	    break;
	    /* digital IO commands */
	case DIO_A_OUTPUT_ENABLE:case DIO_A_OUTPUT_DISABLE:
	    tmp1=(readl(cardspace+GEN_CONTROL_REG_0)& ~DIO_A_OUT_MASK);mb();
	    writel(tmp1 | (cmd==DIO_A_OUTPUT_ENABLE?DIO_A_OUT_MASK:0),
		   cardspace + GEN_CONTROL_REG_0);
	    break;
	case DIO_B_OUTPUT_ENABLE:case DIO_B_OUTPUT_DISABLE:
	    tmp1=(readl(cardspace+GEN_CONTROL_REG_0)& ~DIO_B_OUT_MASK);mb();
	    writel(tmp1 | (cmd==DIO_B_OUTPUT_ENABLE?DIO_B_OUT_MASK:0),
		   cardspace + GEN_CONTROL_REG_0);
	    break;
	case DIO_C_OUTPUT_ENABLE:case DIO_C_OUTPUT_DISABLE:
	    tmp1=(readl(cardspace+GEN_CONTROL_REG_0)& ~DIO_C_OUT_MASK);mb();
	    writel(tmp1 | (cmd==DIO_C_OUTPUT_ENABLE?DIO_C_OUT_MASK:0),
		   cardspace + GEN_CONTROL_REG_0);
	    break;
	case DIO_AB_OUT:
	    writel(arg & DIO_AB_VALUE_MASK, cardspace + DIGITAL_PORTS_AB);
	    mb();
	case DIO_AB_IN:
	    return (readl(cardspace + DIGITAL_PORTS_AB) & DIO_AB_VALUE_MASK);
    	case DIO_C_OUT:
	    tmp1=readl(cardspace + GEN_CONTROL_REG_1);mb();
	    writel((tmp1 & ~DIO_C_VALUE_MASK_1) | 
		   ((arg & DIO_C_VALUE_MASK_0)<<DIO_C_VALUE_SHIFT),
		   cardspace + GEN_CONTROL_REG_1);mb();
	case DIO_C_IN:
	    tmp1=readl(cardspace + GEN_CONTROL_REG_1);mb();
	    return (tmp1 >>DIO_C_VALUE_SHIFT) & DIO_C_VALUE_MASK_0;
      /* timer unit commands */
	case SET_USER_PERIODE_0:case SET_USER_PERIODE_1:
	case SET_USER_PERIODE_2:case SET_USER_PERIODE_3: 
	    writel(arg & BITMASK_WORD, cardspace + USER_PERIODE_REG_0 + 
		   ((cmd & CNTR_INDX_MASK)<< CNTR_INDX_SHIFT));mb();
	    return 0;
	case SET_USER_PULSE_0:case SET_USER_PULSE_1:
	case SET_USER_PULSE_2:case SET_USER_PULSE_3: 
	    writel(arg & BITMASK_WORD, cardspace + USER_PULSE_REG_0 + 
		   ((cmd & CNTR_INDX_MASK)<< CNTR_INDX_SHIFT));mb();
	    return 0;
	case SET_USER_CONTROL_0:case SET_USER_CONTROL_1:
	case SET_USER_CONTROL_2:case SET_USER_CONTROL_3: 
	    writel(arg & BITMASK_CHAR, cardspace + USER_CONTROL_REG_0 + 
		   ((cmd & CNTR_INDX_MASK)<< CNTR_INDX_SHIFT));mb();
	    return 0;
	case GET_USER_PERIODE_0:case GET_USER_PERIODE_1:
	case GET_USER_PERIODE_2:case GET_USER_PERIODE_3: 
	    tmp1=readl(cardspace + USER_PERIODE_REG_0 + 
		   ((cmd & CNTR_INDX_MASK)<< CNTR_INDX_SHIFT));mb();
	    return tmp1 & BITMASK_WORD;
	case GET_USER_PULSE_0:case GET_USER_PULSE_1:
	case GET_USER_PULSE_2:case GET_USER_PULSE_3: 
	    tmp1=readl(cardspace + USER_PULSE_REG_0 + 
		   ((cmd & CNTR_INDX_MASK)<< CNTR_INDX_SHIFT));mb();
	    return tmp1 & BITMASK_WORD;
	case GET_USER_CONTROL_0:case GET_USER_CONTROL_1:
	case GET_USER_CONTROL_2:case GET_USER_CONTROL_3: 
	    tmp1=readl(cardspace + USER_CONTROL_REG_0 + 
		   ((cmd & CNTR_INDX_MASK)<< CNTR_INDX_SHIFT));mb();
	    return tmp1 & BITMASK_CHAR;
	case GET_USER_STATUS:
	    tmp1=readl(cardspace + USER_STATUS_REG );mb();
	    return tmp1 & 0xf;
	case SET_AD_SAMPLE_PERIODE:
	    writel(arg & 0xffff, cardspace+AD_SAMPLE_PERIODE_LS);
	    writel((arg >> 16) & 0xff,  cardspace+AD_SAMPLE_PERIODE_MS);
	    break;
	case SET_AD_TRIG_PERIODE:
	    writel(arg & 0xffff, cardspace + TRIGGERED_SCAN_PERIODE_LS);
	    writel((arg >> 16) & 0xff,cardspace + TRIGGERED_SCAN_PERIODE_MS);
	    break;
	case ASSERT_TIMER_RESET:case DEASSERT_TIMER_RESET:
	    tmp1=readl(cardspace + GEN_CONTROL_REG_1);mb();
	    writel((tmp1 & ~TIMER_RESET_MASK) | 
		   (cmd==DEASSERT_TIMER_RESET?TIMER_RESET_MASK:0),
		   cardspace +  GEN_CONTROL_REG_1);
	    break;
	case TIMER_RESET:
	    tmp1=readl(cardspace + GEN_CONTROL_REG_1);mb();
	    writel(tmp1 & ~TIMER_RESET_MASK, cardspace +  GEN_CONTROL_REG_1); 
	    mb();
	    writel(tmp1 | TIMER_RESET_MASK, cardspace +  GEN_CONTROL_REG_1); 
	    mb();
	    break;
         /* AD conversion unit commands */
	case RESET_AD_UNIT:
	    tmp1=readl(cardspace + GEN_CONTROL_REG_1) & ~ADUNIT_RESET_MASK_1;
	    mb();
	    writel(tmp1 , cardspace + GEN_CONTROL_REG_1); 
	    mb();
	    writel(tmp1 | ADUNIT_RESET_MASK_3, cardspace + GEN_CONTROL_REG_1);
	    already_transferred_values = 0;
	    break;
	case SELECT_AD_CONFIGURATION:
	    tmp1=(readl(cardspace + GEN_CONTROL_REG_0) & ~AD_CONFIG_MASK_1);
	    mb();
	    writel(tmp1 | (arg & AD_CONFIG_MASK_1),
		   cardspace + GEN_CONTROL_REG_0); 
	    mb();
	    break;
	case LOAD_PCI_MASTER:
	    writel(buf_busaddr,cardspace + PCI_BUSMASTER_REG);mb();
	    break;
	case PREPARE_CGL_FIFO:
	    tmp1=readl(cardspace + GEN_CONTROL_REG_1) & ~ADUNIT_RESET_MASK_1;
	    mb();
	    writel(tmp1 | ADUNIT_RESET_MASK_3, cardspace + GEN_CONTROL_REG_1);
	    mb();
	    break;
	case PUSH_CGL_FIFO:
	    writel(arg & CGL_ENTRY_MASK ,cardspace + CGL_FIFO_REG);mb();
	    break;

	case PRELOAD_CGL: /* have this as a separate command to wait longer */
	    /* initiate CGL */
	    tmp1=readl(card + GEN_CONTROL_REG_1);rmb();
	    writel(tmp1 | CHANNEL_PARAM_REGISTER_LOAD,
		   cardspace +  GEN_CONTROL_REG_1); mb();
	    break;
	case ARM_CONVERSION:
	    tmp1=readl(card + GEN_CONTROL_REG_1);rmb();
	    /* clear error bits */
	    writel(CLEAR_GENERAL_STATUS_BITS, cardspace +GEN_STATUS_REG);mb();
	    /* Arm converter */
	    writel(tmp1 | AD_SYSTEM_ARM_BIT, cardspace +  GEN_CONTROL_REG_1);
	    mb();
	    break;
	case SOFTWARE_TRIGGER:
	    tmp1=readl(cardspace + GEN_CONTROL_REG_1);mb();
	    writel(tmp1 | AD_SYSTEM_TRIG_BIT,
		   cardspace +  GEN_CONTROL_REG_1); mb();
	    break;
	case STOP_SCAN:case CONTINUE_SCAN:
	    tmp1=readl(cardspace + GEN_CONTROL_REG_1);mb();
	    writel((tmp1 &  AD_SYSTEM_SCANSTOP_BIT) | 
		   (cmd==STOP_SCAN?AD_SYSTEM_SCANSTOP_BIT:0),
		   cardspace +  GEN_CONTROL_REG_1); mb();
	    break;
	case DISARM_CONVERSION:
	    tmp1=readl(cardspace + GEN_CONTROL_REG_1);mb();
	    writel((tmp1 &  ~AD_SYSTEM_ARM_BIT),
		   cardspace +  GEN_CONTROL_REG_1); mb();
	    break;
	case SET_AD_STATUS:
	    writel(arg & SET_STATUS_MASK, cardspace +GEN_STATUS_REG);mb();
	    break;
	case GET_AD_STATUS:
	    tmp1=readl(cardspace +GEN_STATUS_REG);mb();
	    return tmp1;
	    /* readout commands for the DMA buffer */
	case NEXT_INDEX_TO_WRITE:
	    tmp1=readl(cardspace + PCI_BUSMASTER_REG);mb();
	    return (tmp1 & PCI_BUSMASTER_OFFSET_MASK) >> 1; /* in words */
	case HOW_MANY_FRESH_VALUES:
	    tmp1=readl(cardspace + PCI_BUSMASTER_REG);mb();
	    tmp2=(tmp1 & PCI_BUSMASTER_OFFSET_MASK) >> 1; /* in words */
	    tmp3=already_transferred_values % WORDS_IN_BUFFER;
	    return (((unsigned int)tmp3)>tmp2?tmp2+WORDS_IN_BUFFER-tmp3:tmp2-tmp3);
	case GET_NEXT_VALUE:
	    tmp1=readl(cardspace + PCI_BUSMASTER_REG);mb();
	    tmp2=(tmp1 & PCI_BUSMASTER_OFFSET_MASK) >> 1; /* in words */
	    tmp3=tmp2-(already_transferred_values % WORDS_IN_BUFFER);
	    tmp3=(tmp3<0?tmp3+WORDS_IN_BUFFER:tmp3); /* new wrds */
	    if (tmp3>0) {
		tmp3=already_transferred_values % WORDS_IN_BUFFER;
		tmp2 = aligned_buffer[tmp3*2]+(aligned_buffer[tmp3*2+1]<<8);
		already_transferred_values++;
		if (!(already_transferred_values & 0xffff)) {
		    /* clear block done bit if end-of-block is reached */ 
		    writel(HOST_BLOCK_DONE, cardspace+GEN_STATUS_REG);
		}
		return tmp2;
	    } else {
		return -1;
	    }
	    break;
	case GET_ALREADY_TRANSFERRED_VALUES:
	    return already_transferred_values;
	case GET_JIFFIES:
	    return jiffies;
	case IDENTIFY_DTAX_CARD:  /* what card do we have */
	    return 302; /* simple mock-up */

	default:
	    printk("dt302: illegal address range \n");
	    return -1;
    }
    return 0;
}

/* minor device 0 (simple access) file options */
struct file_operations dt302_simple_fops = {
    open:    dt302_flat_open,
    release: dt302_flat_close,
    ioctl:   dt302_flat_ioctl,
};
/* minor device 1 (advanced access) file options */
struct file_operations dt302_advanced_fops = {
    open:    dt302_advanced_open,
    release: dt302_advanced_close,
    ioctl:   dt302_advanced_ioctl,
};

/* the open method dispatcher for the different minor devices */
static int dt302_open(struct inode *inode, struct file *filp) {
    int mindevice = MINOR(inode->i_rdev);
    switch(mindevice) {
	case 0:
	    filp->f_op = &dt302_simple_fops;
	    break;
	case 1:
	    filp->f_op = &dt302_advanced_fops;
	    break;
	default:
	    printk("dt302: no minor device %d supported.\n",mindevice);
	    return -ENODEV;
    }
    if (filp->f_op && filp->f_op->open) {
	return filp->f_op->open(inode,filp);
    }
    return 0; /* to keep compiler happy */
}
/* release dispatcher; maybe this could look simpler. */
static int dt302_release(struct inode *inode, struct file *filp) {
    int mindevice = MINOR(inode->i_rdev);
    switch (mindevice) {
	/* I am not sure I really need that; is the open dispatch enough? */
	case 0:
	    filp->f_op = &dt302_simple_fops;
	    break;
	case 1:
	    filp->f_op = &dt302_advanced_fops;
	    break; 
	default:
	    printk("dt302: no minor device %d supported.\n",mindevice);
	    return -ENODEV;
    }
    if (filp->f_op && filp->f_op->release) {
	return filp->f_op->release(inode,filp);
    }
    return 0; /* to keep compiler happy */
}


/* generic file operations */
struct file_operations dt302_fops = {
    open:    dt302_open,
    release: dt302_release,
};



/* initialisation of the driver: getting resources etc. */
static int __init dt302_init_one(struct pci_dev *dev, const struct pci_device_id *ent) {
    static int dev_count = 0;

    /* make sure there is only one card */
    dev_count++;
    if (dev_count > 1) {
	printk ("this driver only supports 1 board\n");
	return -ENODEV;
    }

    /* get resources */
    mem_base0 = pci_resource_start(dev, 0);

    /* try to get 256 k contiguous RAM buffer for busmaster DMA */
    targetorder = BUFFER_RAM_PAGE_ORDER;
    rawtargetbuffer = (void *)__get_free_pages(GFP_KERNEL,targetorder);
    if (!rawtargetbuffer) { /* cannot get small memory */
	goto out3;
    }
#ifdef DEBUG
    printk("got free pages at %x (order %d).\n",(unsigned int)rawtargetbuffer,targetorder);
#endif
    buf_busaddr= virt_to_bus(rawtargetbuffer);
    if (buf_busaddr & BUFFER_ALIGNMENT_MASK) {
	/* buffer was not aligned correctly; try larger buffer */
	free_pages((unsigned long) rawtargetbuffer,targetorder);
	targetorder++;
	rawtargetbuffer = (void *)__get_free_pages(GFP_KERNEL,targetorder);
	if (!rawtargetbuffer) { /* cannot get large memory */
	    goto out3;
	}
#ifdef DEBUG
    printk("got free pages at %x (order %d).\n",(unsigned int)rawtargetbuffer,targetorder);
#endif
	buf_busaddr= virt_to_bus(rawtargetbuffer);
	if (buf_busaddr & BUFFER_ALIGNMENT_MASK) { /* do alignment */
	    buf_busaddr = (buf_busaddr & BUFFER_ALIGNMENT_MASK) + 
		(BUFFER_ALIGNMENT_MASK +1);
	}
    }
    aligned_buffer = bus_to_virt(buf_busaddr); /* start of aligned buffer */
  
    /* register resources with kernel */    
    if (pci_enable_device (dev)) 
	return -EIO;
    if (check_mem_region(mem_base0,IOCARD_SPACE+1)) {
	printk("dt302: memory at %x already in use\n",mem_base0);
	goto out1;
    }
    request_mem_region(mem_base0,IOCARD_SPACE+1, IOCARD_NAME);
    /* get virtual address for the pci mem */
    cardspace = (char *) ioremap_nocache(mem_base0,IOCARD_SPACE+1);
    printk("dt302: got new mem pointer: %p\n",cardspace);    
    /* register char devices with kernel */
    if (register_chrdev(IOCARD_MAJOR, IOCARD_NAME, &dt302_fops)<0) {
	printk("Error in registering major device %d\n",IOCARD_MAJOR);
	goto out2;
    }



    return 0; /* everything is fine */
 out2:
    iounmap(cardspace);
    release_mem_region(mem_base0,IOCARD_SPACE+1);  
 out1:
    /* first give back DMA buffer */
    free_pages((unsigned long) rawtargetbuffer,targetorder);
    return -EBUSY;
 out3:
    printk("Cannot allocate DMA buffer of order %d; driver not installed.\n",
	   targetorder);
    return -ENOMEM;
}

static void __exit dt302_remove_one(struct pci_dev *pdev) {
    unregister_chrdev(IOCARD_MAJOR, IOCARD_NAME);
    iounmap(cardspace);
    release_mem_region(mem_base0,IOCARD_SPACE+1);
    free_pages((unsigned long) rawtargetbuffer,targetorder);
#ifdef DEBUG
    printk("freed pages at %x (order %d).\n",(unsigned int)rawtargetbuffer,targetorder);
#endif
}

/* driver description info for registration */
static struct pci_device_id dt302_pci_tbl[] = __initdata {
    {PCI_VENDOR_ID_DATX, PCI_DEVICE_ID_DT302, PCI_ANY_ID, PCI_ANY_ID, },
    {0,},
};


MODULE_DEVICE_TABLE(pci, dt302_pci_tbl);

static struct pci_driver dt302_driver = { 
    name:       "dt302-driver",
    id_table:   dt302_pci_tbl,
    probe:      dt302_init_one,
    remove:     dt302_remove_one,
};

static void  __exit dt302_clean(void) {
    pci_unregister_driver( &dt302_driver );
}

static int __init dt302_init(void) {
    int rc;
    rc = pci_module_init( &dt302_driver );
    if (rc < 0) return -ENODEV;
    return 0;
}

module_init(dt302_init);
module_exit(dt302_clean);


