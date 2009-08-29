/* dt302.c - version for kernel version 2.6

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

---  

   Driver module for the data translation DT302 multi-function card.
   Can currently handle also a DT322 card, and can most likely be extended
   to handle similar cards by adding the corresponding PCI product IDs
   to a list. This driver can handle several cards at the same time.

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
   migrated to kenel 2.6   30.7.04 chk 
   inserted multicard feature, read method  24.9.06 chk
   moved pci_enable to better position  5.9.07chk
   changed dma buffer allocation to modern mode  29.8.09chk

   ToDo:
   - docu der min device 0 features steht noch aus.
   -  docu der min device 1 features steht noch aus.
   - translate docu to english
   - Implementierung der read-methode fuer DMA-Puffer -ok?
   - Implementation von TRIMPOT_WRITE/READ commands
   - test read access to the card.
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
#include <asm/uaccess.h>

#include "dt302.h"

/* This is a flag to indicate that the DMA buffer should be allocated with
   dma_alloc_coherent instead of the old by-hand method with _get_free_pages.
   This seems to get the caching right with newer kernels.  In a newer version,
   this probably should be made the default, and the old code should go.
*/ 
#define NEWDMA yes

/* Module parameters*/
MODULE_AUTHOR("Christian Kurtsiefer <christian.kurtsiefer@gmail.com>");
MODULE_DESCRIPTION("Data Translation DT302 driver for Linux for kernel 2.6\n");
MODULE_LICENSE("GPL");  

/*
#define DEBUG
*/

#define IOCARD_MAJOR 0 /* for dynamic allocation; old: 251 */
#define IOCARD_NAME "dt302driver\0"
#define IOCARD_SPACE 0xffff
#define PCI_DEVICE_ID_DT302 0x0041
#define PCI_DEVICE_ID_DT322 0x0051
#define PCI_VENDOR_ID_DATX 0x1116

/* there should be more of those */
#define IOCTL_ADRMASK 0xffff
#define IOCTL_CMDMASK 0xfff3
#define DAC_VALUE_MASK 0xffff
#define DAC_TRANSFER_DONE_REQUEST_ATTEMPTS 100

/* RAM buffer related stuff */
#define BUFFER_RAM_PAGE_ORDER 7
#define BUFFER_ALIGNMENT_MASK 0x3ffff
#define SUB_BUFFER_MASK 0x2000 /* to mask out buffer change */
#define BUFFER_SIZE (PAGE_SIZE << BUFFER_RAM_PAGE_ORDER)

/* local status variables for cards */
#define NUMBER_OF_CARDS 5
static int dev_count = 0;  /* holds number of cards loaded */

typedef struct cardinfo {
    int iocard_opened;
    unsigned int mem_base0;
    char *cardspace;        /* holds virtual address base of card mem */
    int major;              /* major of each card */
    int targetorder;        /* order of RAM buffer obtained from the OS */
    unsigned char *rawtargetbuffer; /* kernel address of buffer from OS */
    dma_addr_t buf_busaddr; /* aligned buffer; bus address */
    unsigned char *aligned_buffer;   /* aligned buffer pointer; kernel adr */
    int card_type; /* contains the numeric ID of the card */
    int already_transferred_values; /* user transfer count */
    
    /* new stuff for direct dma alloc */
    struct device *dev;  /* holds device ID */
    dma_addr_t buf_busaddr_raw; /* holds unaligned bus address */
    unsigned int rawbuffersize; /* for freeing later */ 
} cdi;

static struct cardinfo *cif[NUMBER_OF_CARDS];

/* search cardlists for a particular major number */
static struct cardinfo *search_cardlist(int major) {
    int i;
    for (i=0;i<dev_count;i++) {
	if (cif[i]->major == major) return cif[i];
    }
    /* no success otherwise */
    return NULL;
}

/* minor device 0 (simple access) structures */
static int dt302_flat_open(struct inode *inode, struct file *filp) {
    struct cardinfo *cp;
    cp= search_cardlist(imajor(inode));
    if (!cp) return -ENODEV;
    if (cp->iocard_opened) 
	return -EBUSY;
    cp->iocard_opened = 1;
    filp->private_data = (void *)cp; /* store card info in file structure */
    return 0;
}
static int dt302_flat_close(struct inode *inode, struct file *filp) {
    struct cardinfo *cp = (struct cardinfo *)filp->private_data;
    if (cp) cp->iocard_opened = 0;
    return 0;
}
static int dt302_flat_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg) {
    struct cardinfo *cp = (struct cardinfo *)filp->private_data;
    int dir = _IOC_DIR(cmd);
    unsigned long adr = cmd & IOCTL_ADRMASK;

    switch ((adr >>12) & 0xf ) {/* check for gross address selection */
	case 0:case 1:case 2:case 3:case 8:case 0xa:case 0xc:case 0xe:case 0xf:
	    break;
	case 4:
	    /* transfer buffer start to busmaster control register on write*/
	    switch (dir) {
		case 1: /* write attempt */
		    writel(cp->buf_busaddr,cp->cardspace+0x4000);
		    return 0;
		case 2: /* read attempt */
		    rmb(); /*  read in all previous stuff NOW */
		    return readl(cp->cardspace+0x4000);
		default:
		    printk("dt302: illegal direction in ioctl.\n");
		    return -1;
	    }  
	case 5: /* read/write DMA buffer bytewise, adr covers first 4k */
	    switch (dir) {
		case 1: /* write */
		    cp->aligned_buffer[adr & 0xfff] = 
			(unsigned char)(arg & 0xff);
		    return 0;
		case 2: /* read */
		    return (unsigned int) cp->aligned_buffer[adr & 0xfff];
		default:
		    return -1;
	    }
	default:
	    printk("dt302: illegal address range \n");
	    return -1;
    }
    switch(dir) {
	case 1: /* write attempt */
	    writel(arg,cp->cardspace+adr);
	    return 0;
	case 2: /* read attempt */
	    rmb(); /* compiler directive: read in all previous stuff NOW */
	    return readl(cp->cardspace+adr);
	default:
	    printk("dt302: illegal direction: %d.\n",dir);
	    return -1;
    }
   
    return 0;
}



/* minor device 1 (advanced access) structures */
static int dt302_advanced_open(struct inode *inode, struct file *filp) {
   struct cardinfo *cp;
    cp= search_cardlist(imajor(inode));
    if (!cp) return -ENODEV;
    if (cp->iocard_opened) 
	return -EBUSY;
    cp->iocard_opened = 1;
    filp->private_data = (void *)cp; /* store card info in file structure */
    cp->already_transferred_values = 0;
    return 0;
}

static int dt302_advanced_close(struct inode *inode, struct file *filp) {
    struct cardinfo *cp = (struct cardinfo *)filp->private_data;
    unsigned long tmp1;
    char * card = cp->cardspace;  /* shortend code..*/

    /* disarm and stop scan */
    tmp1=readl(card + GEN_CONTROL_REG_1);rmb();
    writel((tmp1 &  ~AD_SYSTEM_ARM_BIT) | AD_SYSTEM_SCANSTOP_BIT,
	   card +  GEN_CONTROL_REG_1); mb();

    if (cp) cp->iocard_opened = 0;
    return 0;
}

static ssize_t dt_302_advanced_read(struct file *filp,
				    char __user *targetbuffer,
				    size_t num_of_bytes, loff_t *offset) {
    struct cardinfo *cp = (struct cardinfo *)filp->private_data;
    char * card = cp->cardspace;  /* shortend code..*/

    int start_idx, end_idx; /* indexes into DMA buffer */
    int transferred; /* how many bytes have been transferred */
    int bytes_to_transfer, tmp1, retval; /* working varibles */

    if (!targetbuffer) return -EINVAL; /* according to driver book */

    end_idx = (readl(card + PCI_BUSMASTER_REG)) &
	PCI_BUSMASTER_OFFSET_MASK;
    start_idx = (cp->already_transferred_values % WORDS_IN_BUFFER)*2;

    transferred=0; /* to keep track in split buffer situatons */
    if (start_idx > end_idx) {
	/* overwrap buffer */
	bytes_to_transfer = WORDS_IN_BUFFER * 2 - start_idx;
	if (bytes_to_transfer > (num_of_bytes&~1)) 
	    bytes_to_transfer = num_of_bytes&~1; /* limit to user buffer */
	retval = copy_to_user(targetbuffer, &(cp->aligned_buffer[start_idx]),
			      bytes_to_transfer);
	if (retval) return -EFAULT; /* cannot copy to user space */
	/* clear host block done bit; retreive DAC write bit according
	   to DTAX specs?? */
	tmp1= readl(card+GEN_CONTROL_REG_0) & DA_SHIFT_IN_PROGRESS; rmb();
	writel(tmp1 | HOST_BLOCK_DONE, card+GEN_CONTROL_REG_0);
	
	transferred = bytes_to_transfer; /* for second round */
	start_idx = 0;
    }

    bytes_to_transfer = end_idx - start_idx;
    if (bytes_to_transfer > ((num_of_bytes-transferred)&~1)) 
	bytes_to_transfer = (num_of_bytes-transferred)&~1; /* limit.. */
    if (bytes_to_transfer) {
	retval = copy_to_user(&targetbuffer[transferred],
			      &(cp->aligned_buffer[start_idx]),
			      bytes_to_transfer);
	if (retval) return -EFAULT; /* cannot copy to user space */
    }
    /* probe for sub-buffer boundary change */
    if ((start_idx ^ (start_idx + bytes_to_transfer)) & SUB_BUFFER_MASK) {
	/* clear host block done bit; retreive DAC write bit according
	   to DTAX specs?? */
	tmp1= readl(card+GEN_CONTROL_REG_0) & DA_SHIFT_IN_PROGRESS; rmb();
	writel(tmp1 | HOST_BLOCK_DONE, card+GEN_CONTROL_REG_0);
	}
    transferred += bytes_to_transfer; /* for second round */
    cp->already_transferred_values += transferred >>1; /* words transferred */

    *offset += transferred;
    return transferred; /* bytes transferred */
}

static int dt302_advanced_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg) {
    struct cardinfo *cp = (struct cardinfo *)filp->private_data;
    int dir = _IOC_DIR(cmd);
    unsigned long tmp1,tmp2;
    int tmp3;
    char * card = cp->cardspace;  /* shortend code..*/

    /* just for debug */
    if (dir) return dt302_flat_ioctl(inode, filp, cmd, arg);

    switch (cmd) {/* parse different ioctls */
	case SET_DAC_0:case SET_DAC_1:
	    if (readl(card+GEN_STATUS_REG) & DA_SHIFT_IN_PROGRESS) 
		return -1; /* shift in progress error */
	    writel(arg & DAC_VALUE_MASK, 
		   card+((cmd == SET_DAC_1)?DAC_1_REG:DAC_0_REG));
	    break;
	case LOAD_DAC_0:case LOAD_DAC_1:case LOAD_DAC_0_AND_1:
	    if (readl(card+GEN_STATUS_REG) & DA_SHIFT_IN_PROGRESS) 
		return -1; /* shift in progress error */
	    tmp1=readl(card+GEN_CONTROL_REG_1);rmb();
	    tmp2=(tmp1 & ~DAC_SAMPLE_CLK_MASK) | 
		((cmd & 0x3)<<DAC_SAMPLE_CLK_SHIFT);
	    writel(tmp2, card + GEN_CONTROL_REG_1);
	    break;
	case DAC_TRANSFER_ACTIVE:
	    mb();
	    tmp1=readl(card+GEN_STATUS_REG) & DA_SHIFT_IN_PROGRESS;
	    return tmp1;
	case WAIT_DAC_TRANSFER_DONE:
	    tmp1=DAC_TRANSFER_DONE_REQUEST_ATTEMPTS;
	    while (tmp1) {
		mb();
		if (!(readl(card+GEN_STATUS_REG) & DA_SHIFT_IN_PROGRESS)) 
		    break;
		tmp1--;
	    };
	    return (!tmp1);
	case SET_AND_LOAD_DAC_0:case SET_AND_LOAD_DAC_1:
	    if (readl(card+GEN_STATUS_REG) & DA_SHIFT_IN_PROGRESS) 
		return -1; /* shift in progress error */
	    writel(arg & DAC_VALUE_MASK, card+
		   ((cmd == SET_AND_LOAD_DAC_1)?DAC_1_REG:DAC_0_REG));
    	    tmp1=DAC_TRANSFER_DONE_REQUEST_ATTEMPTS;
	    while (tmp1) {
		mb();
		if (!(readl(card+GEN_STATUS_REG) & DA_SHIFT_IN_PROGRESS)) 
		    break;
		tmp1--;
	    };
	    if (!tmp1) return -1; /* transfer took too long */
	    tmp1=readl(card+GEN_CONTROL_REG_1);rmb();
	    tmp2=(tmp1 & ~DAC_SAMPLE_CLK_MASK) | 
		(((cmd==SET_AND_LOAD_DAC_0)?1:2)<<DAC_SAMPLE_CLK_SHIFT);
	    writel(tmp2, card + GEN_CONTROL_REG_1);
	    break;
	    /* digital IO commands */
	case DIO_A_OUTPUT_ENABLE:case DIO_A_OUTPUT_DISABLE:
	    tmp1=(readl(card+GEN_CONTROL_REG_0)& ~DIO_A_OUT_MASK);rmb();
	    writel(tmp1 | (cmd==DIO_A_OUTPUT_ENABLE?DIO_A_OUT_MASK:0),
		   card + GEN_CONTROL_REG_0);
	    break;
	case DIO_B_OUTPUT_ENABLE:case DIO_B_OUTPUT_DISABLE:
	    tmp1=(readl(card+GEN_CONTROL_REG_0)& ~DIO_B_OUT_MASK);rmb();
	    writel(tmp1 | (cmd==DIO_B_OUTPUT_ENABLE?DIO_B_OUT_MASK:0),
		   card + GEN_CONTROL_REG_0);
	    break;
	case DIO_C_OUTPUT_ENABLE:case DIO_C_OUTPUT_DISABLE:
	    tmp1=(readl(card+GEN_CONTROL_REG_0)& ~DIO_C_OUT_MASK);rmb();
	    writel(tmp1 | (cmd==DIO_C_OUTPUT_ENABLE?DIO_C_OUT_MASK:0),
		   card + GEN_CONTROL_REG_0);
	    break;
	case DIO_AB_OUT:
	    writel(arg & DIO_AB_VALUE_MASK, card + DIGITAL_PORTS_AB);
	    mb();
	case DIO_AB_IN:
	    return (readl(card + DIGITAL_PORTS_AB) & DIO_AB_VALUE_MASK);
    	case DIO_C_OUT:
	    tmp1=readl(card + GEN_CONTROL_REG_1);rmb();
	    writel((tmp1 & ~DIO_C_VALUE_MASK_1) | 
		   ((arg & DIO_C_VALUE_MASK_0)<<DIO_C_VALUE_SHIFT),
		   card + GEN_CONTROL_REG_1);mb();
	case DIO_C_IN:
	    tmp1=readl(card + GEN_CONTROL_REG_1);rmb();
	    return (tmp1 >>DIO_C_VALUE_SHIFT) & DIO_C_VALUE_MASK_0;
      /* timer unit commands */
	case SET_USER_PERIODE_0:case SET_USER_PERIODE_1:
	case SET_USER_PERIODE_2:case SET_USER_PERIODE_3: 
	    writel(arg & BITMASK_WORD, card + USER_PERIODE_REG_0 + 
		   ((cmd & CNTR_INDX_MASK)<< CNTR_INDX_SHIFT));mb();
	    return 0;
	case SET_USER_PULSE_0:case SET_USER_PULSE_1:
	case SET_USER_PULSE_2:case SET_USER_PULSE_3: 
	    writel(arg & BITMASK_WORD, card + USER_PULSE_REG_0 + 
		   ((cmd & CNTR_INDX_MASK)<< CNTR_INDX_SHIFT));mb();
	    return 0;
	case SET_USER_CONTROL_0:case SET_USER_CONTROL_1:
	case SET_USER_CONTROL_2:case SET_USER_CONTROL_3: 
	    writel(arg & BITMASK_CHAR, card + USER_CONTROL_REG_0 + 
		   ((cmd & CNTR_INDX_MASK)<< CNTR_INDX_SHIFT));mb();
	    return 0;
	case GET_USER_PERIODE_0:case GET_USER_PERIODE_1:
	case GET_USER_PERIODE_2:case GET_USER_PERIODE_3: 
	    tmp1=readl(card + USER_PERIODE_REG_0 + 
		   ((cmd & CNTR_INDX_MASK)<< CNTR_INDX_SHIFT));rmb();
	    return tmp1 & BITMASK_WORD;
	case GET_USER_PULSE_0:case GET_USER_PULSE_1:
	case GET_USER_PULSE_2:case GET_USER_PULSE_3: 
	    tmp1=readl(card + USER_PULSE_REG_0 + 
		   ((cmd & CNTR_INDX_MASK)<< CNTR_INDX_SHIFT));rmb();
	    return tmp1 & BITMASK_WORD;
	case GET_USER_CONTROL_0:case GET_USER_CONTROL_1:
	case GET_USER_CONTROL_2:case GET_USER_CONTROL_3: 
	    tmp1=readl(card + USER_CONTROL_REG_0 + 
		   ((cmd & CNTR_INDX_MASK)<< CNTR_INDX_SHIFT));rmb();
	    return tmp1 & BITMASK_CHAR;
	case GET_USER_STATUS:
	    tmp1=readl(card + USER_STATUS_REG );rmb();
	    return tmp1 & 0xf;
	case SET_AD_SAMPLE_PERIODE:
	    writel(arg & 0xffff, card+AD_SAMPLE_PERIODE_LS);
	    writel((arg >> 16) & 0xff,  card+AD_SAMPLE_PERIODE_MS);
	    break;
	case SET_AD_TRIG_PERIODE:
	    writel(arg & 0xffff, card + TRIGGERED_SCAN_PERIODE_LS);
	    writel((arg >> 16) & 0xff,card + TRIGGERED_SCAN_PERIODE_MS);
	    break;
	case ASSERT_TIMER_RESET:case DEASSERT_TIMER_RESET:
	    tmp1=readl(card + GEN_CONTROL_REG_1);rmb();
	    writel((tmp1 & ~TIMER_RESET_MASK) | 
		   (cmd==DEASSERT_TIMER_RESET?TIMER_RESET_MASK:0),
		   card +  GEN_CONTROL_REG_1);
	    break;
	case TIMER_RESET:
	    tmp1=readl(card + GEN_CONTROL_REG_1);rmb();
	    writel(tmp1 & ~TIMER_RESET_MASK, card +  GEN_CONTROL_REG_1); 
	    mb();
	    writel(tmp1 | TIMER_RESET_MASK, card +  GEN_CONTROL_REG_1); 
	    mb();
	    break;
         /* AD conversion unit commands */
	case RESET_AD_UNIT:
	    tmp1=readl(card + GEN_CONTROL_REG_1) & ~ADUNIT_RESET_MASK_1;
	    rmb();
	    writel(tmp1 , card + GEN_CONTROL_REG_1); 
	    mb();
	    writel(tmp1 | ADUNIT_RESET_MASK_3, card + GEN_CONTROL_REG_1);
	    cp->already_transferred_values = 0;
	    break;
	case SELECT_AD_CONFIGURATION:
	    tmp1=(readl(card + GEN_CONTROL_REG_0) & ~AD_CONFIG_MASK_1);
	    rmb();
	    writel(tmp1 | (arg & AD_CONFIG_MASK_1),
		   card + GEN_CONTROL_REG_0); 
	    mb();
	    break;
	case LOAD_PCI_MASTER: /*** THIS SHOULD GO TO OPEN!!! */
	    writel(cp->buf_busaddr,card + PCI_BUSMASTER_REG);mb();
	    break;
	case PREPARE_CGL_FIFO:
	    tmp1=readl(card + GEN_CONTROL_REG_1) & ~ADUNIT_RESET_MASK_1;
	    rmb();
	    writel(tmp1 | ADUNIT_RESET_MASK_3, card + GEN_CONTROL_REG_1);
	    mb();
	    break;
	case PUSH_CGL_FIFO:
	    writel(arg & CGL_ENTRY_MASK ,card + CGL_FIFO_REG);mb();
	    break;
	case PRELOAD_CGL: /* have this as a separate command to wait longer */
	    /* initiate CGL */
	    tmp1=readl(card + GEN_CONTROL_REG_1);rmb();
	    writel(tmp1 | CHANNEL_PARAM_REGISTER_LOAD,
		   card +  GEN_CONTROL_REG_1); mb();
	    break;
	case ARM_CONVERSION:
	    tmp1=readl(card + GEN_CONTROL_REG_1);rmb();
	    /* clear error bits */
	    writel(CLEAR_GENERAL_STATUS_BITS, card +GEN_STATUS_REG);mb();
	    /* Arm converter */
	    writel(tmp1 | AD_SYSTEM_ARM_BIT, card +  GEN_CONTROL_REG_1);
	    mb();
	    break;
	case SOFTWARE_TRIGGER:
	    tmp1=readl(card + GEN_CONTROL_REG_1);rmb();
	    writel(tmp1 | AD_SYSTEM_TRIG_BIT,
		   card +  GEN_CONTROL_REG_1); mb();
	    break;
	case STOP_SCAN:case CONTINUE_SCAN:
	    tmp1=readl(card + GEN_CONTROL_REG_1);rmb();
	    writel((tmp1 &  AD_SYSTEM_SCANSTOP_BIT) | 
		   (cmd==STOP_SCAN?AD_SYSTEM_SCANSTOP_BIT:0),
		   card +  GEN_CONTROL_REG_1); mb();
	    break;
	case DISARM_CONVERSION:
	    tmp1=readl(card + GEN_CONTROL_REG_1);rmb();
	    writel((tmp1 &  ~AD_SYSTEM_ARM_BIT),
		   card +  GEN_CONTROL_REG_1); mb();
	    break;
	case SET_AD_STATUS:
	    writel(arg & SET_STATUS_MASK, card +GEN_STATUS_REG);mb();
	    break;
	case GET_AD_STATUS:
	    tmp1=readl(card +GEN_STATUS_REG);mb();
	    return tmp1;
	    /* readout commands for the DMA buffer */
	case NEXT_INDEX_TO_WRITE:
	    tmp1=readl(card + PCI_BUSMASTER_REG);rmb();
	    return (tmp1 & PCI_BUSMASTER_OFFSET_MASK) >> 1; /* in words */
	case HOW_MANY_FRESH_VALUES:
	    tmp1=readl(card + PCI_BUSMASTER_REG);rmb();
	    tmp2=(tmp1 & PCI_BUSMASTER_OFFSET_MASK) >> 1; /* in words */
	    tmp3=cp->already_transferred_values % WORDS_IN_BUFFER;
	    return (((unsigned int)tmp3)>tmp2?tmp2+WORDS_IN_BUFFER-tmp3:tmp2-tmp3);
	case GET_NEXT_VALUE:
	    tmp1=readl(card + PCI_BUSMASTER_REG);rmb();
	    tmp2=(tmp1 & PCI_BUSMASTER_OFFSET_MASK) >> 1; /* in words */
	    tmp3=tmp2-(cp->already_transferred_values % WORDS_IN_BUFFER);
	    tmp3=(tmp3<0?tmp3+WORDS_IN_BUFFER:tmp3); /* new wrds */
	    if (tmp3>0) {
		tmp3=cp->already_transferred_values % WORDS_IN_BUFFER;
		tmp2 = cp->aligned_buffer[tmp3*2]
		    +(cp->aligned_buffer[tmp3*2+1]<<8);
		cp->already_transferred_values++;
		if (!(cp->already_transferred_values & 0xffff)) {
		    /* clear block done bit if end-of-block is reached */ 
		    writel(HOST_BLOCK_DONE, card+GEN_STATUS_REG);
		}
		cp->already_transferred_values++; /* update read bytes */
		return tmp2;
	    } else {
		return -1;
	    }
	    break;
	case GET_ALREADY_TRANSFERRED_VALUES:
	    return cp->already_transferred_values;
	case GET_JIFFIES:
	    return jiffies;
        /* new in 2.6 version */
	case IDENTIFY_DTAX_CARD:  /* what card do we have */
	    return cp->card_type;
	
	default:
	    printk("dt302: undefined ioctl: 0x%x \n",cmd);
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
    read:    dt_302_advanced_read,
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
    
    struct cardinfo *cp; /* pointer to this card */

    /* make sure there is enough card space */
    if (dev_count >= NUMBER_OF_CARDS) {
	printk ("dt302: this driver only supports %d boards\n",
		NUMBER_OF_CARDS);
	return -ENODEV;
    }
    cp = (struct cardinfo *)kmalloc(sizeof(struct cardinfo),GFP_KERNEL);
    if (!cp) {
	printk("dt302: Cannot kmalloc device memory\n");
	return -ENOMEM;
    }

    /* make sure that memnase0 has a corrrect value 5.9.07chk */
    if (pci_enable_device (dev)) 
	return -EIO;

    cp->iocard_opened = 0; /* no open */
    /* get resources */
    cp->mem_base0 = pci_resource_start(dev, 0);

#ifdef NEWDMA
    cp->dev = &(dev->dev); /* store device address */
    cp->rawbuffersize = BUFFER_SIZE;
    cp->rawtargetbuffer = dma_alloc_coherent(cp->dev, 
					     cp->rawbuffersize,
					     &cp->buf_busaddr_raw,
					     GFP_KERNEL);
    if (!(cp->rawtargetbuffer)) { /* cannot get small memory */
	goto out3;}
    cp->buf_busaddr = cp-> buf_busaddr_raw;    
    if (cp->buf_busaddr & BUFFER_ALIGNMENT_MASK) { /* not aligned */
	/* free old one */
	dma_free_coherent(cp->dev, cp->rawbuffersize, 
			 cp->rawtargetbuffer, cp->buf_busaddr_raw );
	/* get larger one */
	cp->rawbuffersize *= 2 ;
	cp->rawtargetbuffer = dma_alloc_coherent(cp->dev, 
						 cp->rawbuffersize,
						 &cp->buf_busaddr_raw,
						 GFP_KERNEL);
	if (!(cp->rawtargetbuffer)) { /* cannot get large memory */
	    goto out3;}
	cp->buf_busaddr = (cp-> buf_busaddr_raw & ~BUFFER_ALIGNMENT_MASK)
	    + BUFFER_ALIGNMENT_MASK +1 ;
    }
#else
    /* try to get 256 k contiguous RAM buffer for busmaster DMA */
    cp->targetorder = BUFFER_RAM_PAGE_ORDER;
    cp->rawtargetbuffer = (void *)__get_free_pages(GFP_KERNEL,cp->targetorder);
    if (!(cp->rawtargetbuffer)) { /* cannot get small memory */
	goto out3;
    }
    cp->buf_busaddr= virt_to_bus(cp->rawtargetbuffer);
    if (cp->buf_busaddr & BUFFER_ALIGNMENT_MASK) {
	/* buffer was not aligned correctly; try larger buffer */
	free_pages((unsigned long) cp->rawtargetbuffer,cp->targetorder);
	cp->targetorder++;
	cp->rawtargetbuffer = 
	    (void *)__get_free_pages(GFP_KERNEL,cp->targetorder);
	if (!(cp->rawtargetbuffer)) { /* cannot get large memory */
	    goto out3;
	}
	cp->buf_busaddr= virt_to_bus(cp->rawtargetbuffer);
	if (cp->buf_busaddr & BUFFER_ALIGNMENT_MASK) { /* do alignment */
	    cp->buf_busaddr = (cp->buf_busaddr & BUFFER_ALIGNMENT_MASK) + 
		(BUFFER_ALIGNMENT_MASK +1);
	}
    }
#endif

    cp->aligned_buffer = bus_to_virt(cp->buf_busaddr); /* start of aligned buffer */

    /* register resources with kernel */    
    if (check_mem_region(cp->mem_base0,IOCARD_SPACE+1)) {
	printk("dt302: memory at %x already in use\n",cp->mem_base0);
	goto out1;
    }
    request_mem_region(cp->mem_base0,IOCARD_SPACE+1, IOCARD_NAME); /* need fix? */
    /* get virtual address for the pci mem */
    cp->cardspace = (char *) ioremap_nocache(cp->mem_base0,IOCARD_SPACE+1);
    printk("dt302: got new mem pointer: %p\n",cp->cardspace);    
    /* register char devices with kernel */
    cp->major = register_chrdev(IOCARD_MAJOR, IOCARD_NAME, &dt302_fops);
    if (cp->major<0) {
	printk("Error in registering major device %d\n",IOCARD_MAJOR);
	goto out2;
    }
    if (IOCARD_MAJOR) cp->major=IOCARD_MAJOR;
    cp->card_type = (int) ent->driver_data; /* card identifier */
    cif[dev_count]=cp; /* for shorter reference */
    dev_count++; /* got one more card */
    return 0; /* everything is fine */
 out2:
    /* free pci resources; mem regions */
    iounmap(cp->cardspace);
    release_mem_region(cp->mem_base0,IOCARD_SPACE+1);  
 out1:
    /* first give back DMA buffer */
#ifdef NEWDMA
    dma_free_coherent(cp->dev, cp->rawbuffersize, 
		      cp->rawtargetbuffer, cp->buf_busaddr_raw );
#else
    free_pages((unsigned long) cp->rawtargetbuffer,cp->targetorder);
#endif
    kfree(cp);
    return -EBUSY;
 out3:
    printk("dt302: Cannot allocate DMA buffer of order %d; driver not installed.\n",
	   cp->targetorder);
    kfree(cp);
    return -ENOMEM;
}

static struct cardinfo * find_card_from_membase(unsigned int mem_base0){
    int i;
    for (i=0;i<dev_count;i++) {
	if (cif[i]->mem_base0 == mem_base0) return cif[i];
    }
    /* no success otherwise */
    return NULL;
}

static void __exit dt302_remove_one(struct pci_dev *pdev) {
    struct cardinfo *cp; /* to retreive card data */
    cp = find_card_from_membase(pci_resource_start(pdev, 0));
    if (!cp) {
	printk("dt302: Cannot find device entry for dt30x to unload card\n");
	return;
    }
    unregister_chrdev(cp->major, IOCARD_NAME);
    iounmap(cp->cardspace);
    release_mem_region(cp->mem_base0,IOCARD_SPACE+1);
    free_pages((unsigned long) cp->rawtargetbuffer,cp->targetorder);
    kfree(cp); /* give back card data container structure */
}

/* driver description info for registration; last entry (driver_data) is the 
   reply to a identification call to ioctl for that card. More card
   identifiers should go here.  */
static struct pci_device_id dt302_pci_tbl[] = {
    {PCI_VENDOR_ID_DATX, PCI_DEVICE_ID_DT302, PCI_ANY_ID, PCI_ANY_ID,
     0, 0, 302},
    {PCI_VENDOR_ID_DATX, PCI_DEVICE_ID_DT322, PCI_ANY_ID, PCI_ANY_ID,
     0, 0, 322},
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
    rc = pci_register_driver( &dt302_driver );
    if (rc < 0) return -ENODEV;
    return 0;
}

module_init(dt302_init);
module_exit(dt302_clean);
