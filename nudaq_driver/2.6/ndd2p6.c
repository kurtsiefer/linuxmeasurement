/* Modified driver for a nudaq PC7200 card for a 2.6 kernel. Still work
   in progress: based on a 2.4driver (ndd2.4-3.c) in its  version 3.4.2006.

 Copyright (C) 2000-2007 Christian Kurtsiefer <christian.kurtsiefer@gmail.com>

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


   seems to operate nicely with a co16 card, reads in about 2.8Mevents/sec
   20.9.2006 chk

   has issues with a gcc 4.0.2 compiler on a SuSE 10.0 distribution
   signedness problems?

   patchlevle p1 to take into account that set_page_count has moved to the
   kernel internal commands a user should probably never touch again.
   This implies the removal of the individual page count increases in the
   beginning, but perhaps this is not a problem as get_page automatically
   referrs to the header page of a compound page cluster. Unitl this has
   proven to work reliably, consider this version without the page
   modifications as alpha....chk100307

   Another annoyer for the 10.2 distribution is the omission of the AMCC
   PCI vendor ID code. inserted 
   moved the location of the pci_enable command to make sure that correct
     irqs are issued to the driver 5.9.07chk
   
   fixed syntax with irq flags and irq handler parameters 17.4.09chk
   attempt to fix missing nopage method in newer kernels 11.8.09chk
   STILL NEEDS HEAVY CLEANUP!!!!!!!!

   */


/*

 Driver for the NuDAQ card frm adlink; searches base address of the AMCC
 busmaster chip and offers plain read/write acces with minor devices 0 and 1
 for debugging, and more sophisticated DMA transfer with minor devices 2-4.

 Minor 0:  address space 0...01f, read/writ with 16 bit width
 Minor 1:  address space 0...01f, read/writ with 32 bit width

 Minor 2:  This device is intended to read in external events into memory
           using the DMA busmaster engine in the AMCC chip.

	   When opening the device, the DMA engine gets reserved including
           capture of interrupts. The transfer into user space is done by 
	   memory-mapping the DMA buffer into some virtual memory which can
	   be accessed by the user directly. For that purpose, the kernel
	   allocates a scatter/gather buffer in physical RAM suitable for
	   DMA transfer during the mmap call, and returns an addess to the
	   user where the whole buffer can be accessed as a contiguous piece
	   of memory.

	   The various control operations of the DMA engine and the on-board
	   timer units is accomplished using ioctl commands. Also, a pointer
	   position of the DMA engine can be obtained that way to learn how far
	   information has been used up.

           The following ioctl() functions are implemented; a value may be
	   transferred to the function as an optinal argument in the ioctl()
	   call:

	     timer_set <value> :        sets internal clock source
	     NUDAQ_2_START_DMA :        starts DMA engine
	     NUDAQ_2_STOP_DMA :         stops DMA engine
	     NUDAQ_2_READ_ERR :         returns error status
	     NUDAQ_2_OUTWORD <value> :  writes a word manually on the out port
	     NUDAQ_2_READBACK :         readback output port
	     NUDAQ_2_BYTES_READ :       returns the number of transferred
	                                bytes. The value is trucated to
					31 bits; a negative number indicates an
					error.
	     NUDAQ_2_DITIMERS <value>:  sets the divider ratio for the clock
	                                generator counters: loword: counter0, 
					highword: counter 2. if hiword is ==0,
					only counter 0 is used. refer to the
					manual for understanding what happens.
	     NUDAQ_2_INMODE <value> :   defines input mode. this is is a
	                                bitfield composed from flags 
					DI_MODE0..DI_MODE3, DI_WAIT_TRIG,
					DI_TRIG_POL, DI_CLEAR_FIFO, DI_IREQ_INT

 Minor 3:  like minor device 2, but intended for usage with the coincidence
           unit. Allows also to set a hardware timer to be set with the ioctl()
	   command NUDAQ_3_SET_TIMEOUT, which resets bit 0 of the output
	   register when it expires. This allows to inhibit the further
	   collection into a FIFO after a hardware timer expired. For this
	   ioctl() command, an option DI_IREQ_INT decides if the mailbox-irq
	   from the hardware timer is allowed to be passed to the irq handler,
	   which then leads to a SIGIO sent to the user program to indicate
	   the end of the integration  time. 

 Minor 4:  analog to device minor 2, but for continuous output of data from
           a crclar buffer. Interaction of the user with this buffer is similar
	   to device 2 via mmap. This device honors the following ioctls:

	     timer_set <value> :        sets internal clock source
	     NUDAQ_4_START_DMA :        starts DMA engine
	     NUDAQ_4_STOP_DMA :         stops DMA engine
	     NUDAQ_4_READ_ERR :         returns error status
	     NUDAQ_2_INWORD :           reads (slowly) from th einput port
	     NUDAQ_2_BYTES_SENT :       returns the number of transferred
	                                bytes
	     
	     NUDAQ_4_DITIMERS <value>:  sets the divider ratio for the clock
	                                generator counters: loword: counter0, 
					highword: counter 2. if hiword is ==0,
					only counter 0 is used. refer to the
					manual for understanding what happens.
	     NUDAQ_4_INMODE <value> :   defines input mode. this is is a
	                                bitfield composed from flags 
					DI_MODE0..DI_MODE3, DI_WAIT_TRIG,
					DI_TRIG_POL, DI_CLEAR_FIFO, DI_IREQ_INT

-----------------

History:
 erste Version   30.06.00  chk

 modifizierte Version, soll zugriff auf dma-engine erm�glichen. noch nicht
 fertig. status: 13.08.00, 20:46 chk, kompiliert fehlerfrei

 experimentell, Umsetzung auf kernel version 2.4; kompiliert fehlerfrei fuer
    minor devices 0, 1.

    2.4 version von dev 2, scheint zu funktionieren auf kernel 2.4.18,
    nicht jedoch auf 2.4.4 !!!!                           chk, 1.8.02
    irq anfrage repariert.                                chk, 2.8.02 
    version, die interrupts zulaesst vom onboard-timer.
    spezialisiert auf den Bedarf bie der 16-fach-
    Koinzidenzeinheit.                                    chk, 17.5.03
    Separation von koinzidenz- und normaler Leseoperation chk, 26.5.03
    Version mit bugfixes & tidying
     in setinmode, irq_handler, start_dma_engine          chk, 27.05.03
    version 2.4-2 mit getestetem minor device 4 zur
    DMA-Ausgabe                                           chk, 29.05.03
    setinmode und setoutmode repariert                    chk, 18.06.03
    kernel tainting repaired, version change ->2.4-3      chk 22.08.03
    removed ambiguity in definition of transferred bytes. chk 5.2.06
    version change 2.4->2.6                               chk 20.9.06
    repaired major device recognition for device unload;
    undo page manipulation on close                       chk 22.9.06

    more todo:
    docu ioctl functions
    remove ambiguous timer commands for device 3
    - heavy code cleanup
    - check minor 4 performance and transfer bytes cleaning
    - support more than one card?

*/

#include <linux/module.h>
#include <asm/segment.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/sched.h>
#include <linux/timer.h>

#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/fcntl.h>
#include <linux/init.h>
#include <linux/pci.h> 
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/pagemap.h>

#include <linux/version.h>



/* fix the nopage -> fault method transition */
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,22) )
#define HAS_FAULT_METHOD
#endif

/* Module parameters */
MODULE_AUTHOR("Christian Kurtsiefer <christian.kurtsiefer@gmail.com>");
MODULE_DESCRIPTION("Nudaq PCI7200 driver for Linux for kernel 2.6\n");
MODULE_LICENSE("GPL");


/* this is making up for the missing AMCC definition in the 10.2 distro */
#ifndef PCI_VENDOR_ID_AMCC
#define PCI_VENDOR_ID_AMCC 0x10e8
#endif

#define PCI_DEVICE_ID_NUDAQ7200 0x80d8
  
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/system.h>
#include <asm/uaccess.h>

// #define DONT_USE_SETPAGE
#ifndef set_page_count
static inline void set_page_count(struct page *page, int v)
{
	atomic_set(&page->_count, v);
}
#endif



/* quick and dirty transient definitions 2.2 -> 2.4 */
#define VMA_OFFSET(vma)   (vma->vm_pgoff * PAGE_SIZE)

#define IOCARD_MAJOR 0 /* for dynamic allocation; old value: 252 */
#define FLAT_16_MINOR 0
#define FLAT_32_MINOR 1
#define DMAMODE_MINOR 2
#define IOCARD_NAME "ndd2p6\0"
#define IOCARD_SPACE 0x1f
#define DMA_ENGINE_SPACE 0x3f

/* to share with user programs , TBD!!!!!!!!!!!!!!*/
/* #include "ndd2.4-0.h" */

/* for access to minor device 2: */
#define NUDAQ_2_START_DMA 0x1001
#define NUDAQ_2_STOP_DMA 0x1002
#define NUDAQ_2_READ_ERR 0x1003
#define NUDAQ_2_BYTES_READ 0x1004
#define NUDAQ_2_OUTWORD 0x1005
#define NUDAQ_2_READBACK 0x1006
#define NUDAQ_2_DITIMERS 0x1007
#define NUDAQ_2_INMODE 0x1008
/* verschiedne modes f�r INMODE: */
#define DI_MODE0 0x00 /* internal timer source , only timer 0 */
#define DI_MODE1 0x01 /* external, i_req rising edge */
#define DI_MODE2 0x02 /* external, i_req falling edge */
#define DI_MODE3 0x03 /* i_req & i_ack handshake */
#define DI_MODE0_CASCADE 0x04 /* internal timers 0 and 2 cascaded as source */
#define DI_WAIT_TRIG 0x08 /* Wait for trigger */
#define DI_TRIG_POL 0x10 /* trigger_polarity: set:falling */
#define DI_CLEAR_FIFO 0x20 /* eventually clear FIFO before acquisition */
#define DI_IREQ_INT 0x100 /* allow for extra IRQs on I_REQ */

/* device 3 specific commands */
#define NUDAQ_3_SET_TIMEOUT 0x1009 /* set timeout counter in millisec */

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

/* minor device 2, 3 and 4 error codes */
#define E_NU_nopointer 1
#define E_NU_already_running 2
#define E_NU_not_running 3



/* register definitions for the dma engine in the S5933 chip */
#define DMA_ENGINE_MWAR 0x24  /* bus master write addr register */
#define DMA_ENGINE_MWTC 0x28  /* bus master write count register */
#define DMA_ENGINE_MRAR 0x2c  /* bus master read addr register */
#define DMA_ENGINE_MRTC 0x30  /* bus master read count register */
#define DMA_ENGINE_INTCSR 0x38 /* bus master interrupt/status register */
#define DMA_ENGINE_MCSR 0x3c  /* S5933 master control/status reg */
#define S5933_INTCSR_int_asserted 0x00800000 /* interrupt asserted bit */
#define S5933_INTCSR_write_complete 0x00040000 /* write complete bit */
#define S5933_INTCSR_read_complete  0x00080000 /* read complete bit */
#define S5933_INTCSR_incoming_mail 0x00020000 /* int on incoming mailbox */
#define S5933_INTCSR_abort_mask 0x00300000 /* abort message */
#define S5933_INTCSR_int_on_wr_complete 0x00004000 /* int enable write */
#define S5933_INTCSR_int_on_rd_complete 0x00008000 /* int enable read */
#define S5933_INTCSR_int_on_inmail_box4_byte3 0x00001f00
#define S5933_MCSR_write_transf_enable  0x00000400
#define S5933_MCSR_read_transf_enable   0x00004000
#define S5933_MCSR_write_FIFO_scheme    0x00000200
#define S5933_MCSR_read_FIFO_scheme     0x00002000
#define S5933_MCSR_write_priority       0x00000100
#define S5933_MCSR_read_priority        0x00001000
#define S5933_MCSR_write_FIFO_reset  0x04000000
#define S5933_MCSR_read_FIFO_reset   0x02000000
#define S5933_MCSR_Add_on_reset      0x01000000 /* do we need that? */
#define IOCARD_COUNTER_0 0x0 /* address of counter 0 */
#define IOCARD_COUNTER_1 0x4 /* address of counter 1 */
#define IOCARD_COUNTER_2 0x8 /* address of counter 2 */
#define IOCARD_COUNTER_CTRL 0xc /* address of counter 2 */
#define IOCARD_DIGIOUT 0x14
#define IOCARD_DIO_STATUS 0x18
#define IOCARD_INT_STATUS 0x1C
#define IOCARD_INT_T1_EN 0x0008 /* enable bit for timer 1 interrupts */
#define IOCARD_INT_T0_EN 0x0004 /* enable bit for timer 0 interrupts */
#define IOCARD_INT_II_REQ 0x0002 /* enable bit for I_REQ interrupts */
#define IOCARD_INT_IO_ACK 0x0001 /* enable bit for O_ACK interrupts */
#define IOCARD_INT_SI_REQ 0x0040 /* status bit for I_REQ interrupts */
#define IOCARD_INT_SO_ACK 0x0020 /* status bit for O_ACK interrupts */
#define IOCARD_INT_SI_T0 0x0080 /* status bit for timer 0 interrupts */
#define IOCARD_INT_SI_T1 0x0100 /* status bit for timer 0 interrupts */
#define IOCARD_INT_T0_T2 0x0400 /* cascade timer 0 - timer 2 */
#define IOCARD_INT_T1_T2 0x0800 /* cascade timer 1 - timer 2 */
#define IOCARD_INT_REQ_NEG 0x1000 /* I_req pol is neg */
#define IOCARD_INT_STATMASK 0x03e0 /* mask for all int status bits */

/* ioctl() command definitions */
#define IOCTL_WRITE_CMD 0x0400
#define DMA_ENGINE_SELECT 0x0800 /* for flat access */


/* local status variables */
static int IOCARD_IN_USE = 0;  
static int IOCARD_USAGE_MODE = 0; /* stores minor device */

static unsigned int iocard_base;
static unsigned int dma_engine_base;
static int irq_number;

/* not sure if these both options work together at the same time */
/* #define irq_flags (SA_SHIRQ | SA_INTERRUPT) old version */
#define irq_flags (IRQF_DISABLED | IRQF_SHARED)

/* upper/lower access boundaries for the different minor devices */
static unsigned int IOCARD_lower_bound = {0};
static unsigned int IOCARD_upper_bound = {0x1f};

/* used to point to the PCI device for buffer allocation */
static struct pci_dev *this_pci_device = NULL;



/* routines for minor device 2 */

/* set 8254 counter to a given value in mode 3 (square wave generator)
   fails if something went wrong with !=0 */
static int set_counter(int counter, int value){
  unsigned long lsb, msb, countad, mode, modead;
  if (counter<0 || counter>2) return -1;

  /* prepare lsb/msb of counter */
  lsb=value & 0xff; msb=(value >>8) & 0xff;
  countad=iocard_base+((counter & 0x03)<<2);

  /* set mode: load lsb, msb sequentially, mode 3, binary */
  mode=((counter &0x3)<<6) |0x36;
  modead=iocard_base+IOCARD_COUNTER_CTRL;

  /* send stuff to counter */
  outl(mode,modead);
  outl(lsb,countad);
  outl(msb,countad);

  return 0;
}

/* set 8254 counter to a given value in mode 0 (countdown mode)
   fails if something went wrong with !=0 */
static int set_counter_countdown(int counter, int value){
  unsigned long lsb, msb, countad, mode, modead;
  if (counter<0 || counter>2) return -1;

  /* prepare lsb/msb of counter */
  lsb=value & 0xff; msb=(value >>8) & 0xff;
  countad=iocard_base+((counter & 0x03)<<2);

  /* set mode: load lsb, msb sequentially, mode 4, binary */
  mode=((counter &0x3)<<6) |0x38;
  modead=iocard_base+IOCARD_COUNTER_CTRL;

  /* send stuff to counter */
  outl(mode,modead);
  outl(lsb,countad);
  outl(msb,countad);

  return 0;
}



/* structure to set the DIO conttrol and interrupt control register in
   the NUDAQ card */ 
static int diovalsave;
static int setinmode(int val){
  unsigned long ad;
  unsigned long dioval,intval;
  /* create int control value */
  intval = (val & DI_MODE0_CASCADE)?0x0400:0 |
           (val & 0x1)?0:0x1000 |
           (val & DI_IREQ_INT)?0x2:0;
  /* create dio control val */
  dioval = (((val & 0x03)==3)?0x1:0) |
      (((val & 0x3)!=DI_MODE0)?0x2:0x4 ) |  0x08 |  
           ((val & DI_TRIG_POL)?0:0x10 )| 
           ((val & DI_WAIT_TRIG)?0x20:0);
  ad=iocard_base+IOCARD_INT_STATUS; outl(intval,ad);

  ad=iocard_base+IOCARD_DIO_STATUS; outl(dioval,ad);
  dioval |=0x40; outl(dioval,ad);
  diovalsave=dioval; /* for debugging */
  return 0;
}

static int setoutmode(int val){
  unsigned long ad;
  unsigned long dioval,intval;
  /* create int control value */
  intval = (val & DO_MODE1_CASCADE)?0x0800:0 |
           ((val & DO_O_ACK_INT))?0x1:0;
  /* create dio control val */
  dioval = ((val & 0x03)?0x180:0x200) |
           ((val & DO_TRIG)?0x800:0x0 ) |
           ((val & DO_UND)?0x10000:0) |
           ((val & DO_CLEAR_FIFO)?0x400:0);
  ad=iocard_base+IOCARD_INT_STATUS; outl_p(intval,ad);
  ad=iocard_base+IOCARD_DIO_STATUS; outl_p(dioval,ad);
  diovalsave=dioval; 
  return 0;
}

/* Struktur, die pages f�r DMA-Buffer enthalten soll. Soll effizienten
   Zugriff auf Speicher w�hrend Seitenwechsel im DMA erlauben. */
typedef struct dma_page_pointer {
    struct dma_page_pointer * next;  /* pointer to next DMA page cluster */
    struct dma_page_pointer * previous; /* pointer to previous page cluster */
    unsigned long size; /* size in bytes of current cluster */
    unsigned long fullsize; /* full size of allocated cluster, = 4k<<order */
    unsigned long order;   /*  order of mem piece */
    char * buffer; /* pointer to actual buffer (as seen by kernel) */
    dma_addr_t physbuf; /* bus address of actual buffer, for dma engine */
} dma_page_pointer;
static dma_page_pointer * dma_main_pointer; /* parent pointer to DMA array */
static dma_page_pointer * next_to_use_ptr; /* pointer to next seg for DMA */
static int dma_should_run=0;
static int dma_int_use_counter=0; /* counter to test consistency of dma regs */

#ifndef DONT_USE_SETPAGE
/* code to manipulate page counters of page blocks */
void  add_individual_page_counts(void *buffer, unsigned long order) {
    int i,orig;
    struct page *spa;
    if (order) {
	orig=page_count(virt_to_page(buffer)); /* first page setting */
	for (i=1;i<(1 << order);i++) {
	    spa=virt_to_page(buffer+i*PAGE_SIZE);
	    set_page_count(spa,orig);
	}
    }	  
}
void release_individual_page_counts(void *buffer, unsigned long order) {
    int i;
    struct page *spa;
    /* UnlockPage(virt_to_page(buffer)); */
    if (order) {
	for (i=1;i<(1 << order);i++) {
	    spa=virt_to_page(buffer+i*PAGE_SIZE);
	    set_page_count(spa,0);
	}
    }	  
}
#endif

/* release code for the DMA buffer and the DMA buffer pointer */
void release_dma_buffer(void){
  struct dma_page_pointer *currbuf, *tmpbuf;

  /* only one buffer to free... */
  currbuf=dma_main_pointer;
  if (currbuf) { /* nothing to release ?*/
      do {
#ifndef DONT_USE_SETPAGE
	  /* undo page count manipulation thing?? */
	  release_individual_page_counts(currbuf->buffer, currbuf->order);
#endif
	  /* ..then give pages back to OS */
	  free_pages((unsigned long) currbuf->buffer,
		     currbuf->order); /* free buffer */
	  tmpbuf =currbuf->next; kfree(currbuf); 
	  currbuf=tmpbuf; /* free pointer */
      } while (currbuf !=dma_main_pointer);
      dma_main_pointer=NULL; /* mark buffer empty */
  };
}

/* routine to allocate DMA buffer RAM of size (in bytes).
   Returns 0 on success, and <0 on fail */ 
static int get_dma_buffer(ssize_t size) {
  ssize_t bytes_to_get = size & ~0x3; /* get long-aligned */
  ssize_t usedbytes;
  unsigned long page_order;
  void * bufferpiece;

  dma_addr_t busaddress; /* bus address of DMA buffer */
  struct dma_page_pointer * currbuf;
  struct dma_page_pointer * tmpbuf;
  /* check multi pages */
  struct page *spa;
  unsigned long pflags;
  int pcnt,i;  

  /* reset dma pointer buffer */
  currbuf=NULL; /* dma_main_pointer; */ /* NULL if no buffer exists */
 
  /* still have to get only small pieces.... */
  page_order = 4;
    
  /* page_order = get_order(bytes_to_get); */
  if (page_order >= MAX_ORDER) page_order=MAX_ORDER;

  while (bytes_to_get>0) {
      /* shrink size if possible */
      while((page_order>0) && (PAGE_SIZE<<(page_order-1))>=bytes_to_get)
	  page_order--;
       
      bufferpiece = (void *)__get_free_pages(GFP_KERNEL,page_order);
      
      if (bufferpiece) {

#ifndef DONT_USE_SETPAGE
	  /* repair missing page counts */
	  add_individual_page_counts(bufferpiece, page_order);
#endif
	  
	  /* get block structure */
	  for (i=0;i<(1 << page_order);i++) {
	      spa=virt_to_page(bufferpiece+i*PAGE_SIZE);
	      pcnt=page_count(spa);
	      pflags = spa->flags;
      }	  
	  busaddress = virt_to_bus(bufferpiece);
	  /* success: make new entry in chain */
	  tmpbuf = (dma_page_pointer *) kmalloc(sizeof(dma_page_pointer),
						GFP_KERNEL); /* first, get buffer */
	  if (!tmpbuf) {
	      printk(" Wruagh - kmalloc failed for buffer pointer....\n");
	      free_pages((unsigned long)bufferpiece,page_order); /* give it back */
	      printk("kmalloc failed during DMA buffer alloc. better reboot.\n");
	      return -ENOMEM;
	  }
	  
	  
	  if (currbuf) { /* there is already a structure */
	      /* fill new struct; currbuf points to last structure filled  */
	      tmpbuf->next=currbuf->next; tmpbuf->previous=currbuf;
	      /* insert in chain */
	      currbuf->next->previous=tmpbuf;currbuf->next=tmpbuf;
	      currbuf=tmpbuf;
	  } else {
	      tmpbuf->previous=tmpbuf; tmpbuf->next=tmpbuf; /* fill new struct */
	      currbuf=tmpbuf; dma_main_pointer=currbuf; /* set main pointer */
	  };
	  
	  /* fill structure with actual buffer info */
	  usedbytes = PAGE_SIZE<<page_order;
	  currbuf->fullsize = usedbytes; /* all allocated bytes */
	  usedbytes = (usedbytes>bytes_to_get?bytes_to_get:usedbytes);
	  currbuf->size=usedbytes; /* get useful size into buffer */
	  currbuf->order=page_order; /* needed for free_pages */
	  currbuf->buffer=bufferpiece;  /* kernel address of buffer */
	  currbuf->physbuf=busaddress; /* PCI bus address */
	  
      /* less work to do.. */
	  bytes_to_get -= usedbytes;
      } else {
	  /* could not get the large mem piece. try smaller ones */
	  if (page_order>0) {
	      page_order--; continue;
	  } else {
	      break; /* stop and clean up in case of problems */
	  };
      }
  }
  if (bytes_to_get <=0)  return 0; /* everything went fine.... */
  /* cleanup of unused buffers and pointers with standard release code */
  release_dma_buffer();
  return -ENOMEM;
}
/* how many bytes are transferred already ? 
   modified inconsistency: the negative indicator (MSB) in the return value
   indicates an error, but we need to avoid count rollover to cause an error
   to be detected. Therefore, true counts are given as pos numbers truncated
   to 31 bit, and a neg number means an error.
*/
static unsigned long already_transfered_bytes=0;
static int get_bytes_transfered(void){
    int irq_problem; /* get a consistent value */
    unsigned long  ad;
    int i=5; /* max. mumber of attempts to read */
    unsigned long cnts;

    /* try to find out if DMA is running */
    if (!dma_should_run) return (already_transfered_bytes & 0x7fffffff);
    
    ad=dma_engine_base+(IOCARD_USAGE_MODE>3?DMA_ENGINE_MRTC:DMA_ENGINE_MWTC);
    /* check if there has been a mempiece change only if there 
       is already  memory assigned. */
    if (!next_to_use_ptr) return 0;
    do {
	irq_problem=dma_int_use_counter; /* consistency check */
	cnts=(next_to_use_ptr->previous->size - inl(ad)) + 
	    already_transfered_bytes;
	i--;
    } while ((irq_problem!=dma_int_use_counter) && i);
    
    if (i==0) return -EFAULT; /* no consistent number to get; too many irq ? */
    return (cnts & 0x7fffffff); /* mask to 31 bit if correct */
}

/* interrupt handler and associated data structures */
static struct fasync_struct *my_async_queue;
static spinlock_t irq_lock = SPIN_LOCK_UNLOCKED; /* for avoiding trouble ? */

/* interrupt handler for minor device 2, 3 and 4 */
static irqreturn_t interrupt_handler(int irq, void *dev_id ){
    /*, struct pt_regs *regs) { old style */
 static unsigned long ad,intreg,ad1,ad2,intreg2,tmpval;

  ad1=dma_engine_base+DMA_ENGINE_INTCSR;
  intreg=inl(ad1); /* interrupt status reg */

  if (intreg & S5933_INTCSR_int_asserted) {
      spin_lock(&irq_lock);

      /* check for extrnal input */
      if ((intreg & S5933_INTCSR_write_complete) && (IOCARD_USAGE_MODE<4)) {
	  /* check if dma should continue to run */
	  if (dma_should_run) {
	      dma_int_use_counter++; /* once is enough since int handler is atomic */
	      /* set new physical address: */
	      ad=dma_engine_base+DMA_ENGINE_MWAR;
	      outl(next_to_use_ptr->physbuf,ad);
	      /* set size of dma transfer */
	      ad=dma_engine_base+DMA_ENGINE_MWTC;
	      outl(next_to_use_ptr->size,ad);
	    
	      /* restart dma engine (nothing to do, was done writing size?? */
	      already_transfered_bytes+=next_to_use_ptr->previous->size;
	      next_to_use_ptr=next_to_use_ptr->next; /* prepare next buffer */
	  }
      }
      if ((intreg & S5933_INTCSR_read_complete)&&(IOCARD_USAGE_MODE>3)) {
	  /* check if dma should continue to run */
	  if (dma_should_run) {
	      dma_int_use_counter++; /* once is enough since int handler is atomic */
	      /* set new physical address: */
	      ad=dma_engine_base+DMA_ENGINE_MRAR;
	      outl(next_to_use_ptr->physbuf,ad);
	      /* set size of dma transfer */
	      ad=dma_engine_base+DMA_ENGINE_MRTC;
	      outl(next_to_use_ptr->size,ad);
	    
	      /* restart dma engine (nothing to do, was done writing size?? */
	      already_transfered_bytes+=next_to_use_ptr->previous->size;
	      next_to_use_ptr=next_to_use_ptr->next; /* prepare next buffer */
	  }
      }

      /* irqs from the nudaq proprietary circuitry for minor device 3 */
      if (intreg & S5933_INTCSR_incoming_mail) {
	  ad2=iocard_base+IOCARD_INT_STATUS; /* read in this irqreg */
	  intreg2=inl(ad2);

	  if (intreg2 & IOCARD_INT_STATMASK) { /* came from nudaq */
	      /*  nudaq fpga interupt handler  */
	      outl(intreg2 | IOCARD_INT_STATMASK, ad2);
	      
	      /* disable data acquisition */
	      if (IOCARD_USAGE_MODE==3) {
		  tmpval=inl(iocard_base+IOCARD_DIGIOUT) & ~0x2;
		  outl(tmpval,iocard_base+IOCARD_DIGIOUT);
	      }

	      /* tell what happened */
	      kill_fasync(&my_async_queue, SIGIO, POLL_IN);

	      /* clear Nudaq proprietary irq source */
	      outl(intreg2,ad2);
	  }
      }
     /* now clear all irqs from the AMCC source */
     outl_p(intreg, ad1); 
     spin_unlock(&irq_lock);
     return IRQ_HANDLED;
  }
  return IRQ_NONE;
};


/* routine to set dma to running */
static void start_dma_engine(void){ 
  unsigned long ad;
  unsigned long value;
  /* set read pointer to base address and reset byte count */
  next_to_use_ptr=dma_main_pointer; already_transfered_bytes=0;
  
  /* fill 5933 dma engine registers */
  ad=dma_engine_base+(IOCARD_USAGE_MODE<4?DMA_ENGINE_MWAR:DMA_ENGINE_MRAR);
  outl(next_to_use_ptr->physbuf,ad); /* base addr */
  ad=dma_engine_base+(IOCARD_USAGE_MODE<4?DMA_ENGINE_MWTC:DMA_ENGINE_MRTC);
  outl(next_to_use_ptr->size,ad); /* bytes to transfer now */

  /* prepare next-to-use pointer for interrupt routine */
  next_to_use_ptr=next_to_use_ptr->next;

  /* switch on interrupt handler */
  dma_should_run=1;
  
  /* set interrupt control flags in S5933, and reset pending interrupts */
  ad=dma_engine_base+DMA_ENGINE_INTCSR;
  if (IOCARD_USAGE_MODE<4) {
      value=(S5933_INTCSR_abort_mask | S5933_INTCSR_write_complete |
	     S5933_INTCSR_int_on_wr_complete);
  } else {
      value=(S5933_INTCSR_abort_mask |  S5933_INTCSR_read_complete |
	     S5933_INTCSR_int_on_rd_complete);
  }
  
  if (IOCARD_USAGE_MODE==3) {/* for nudaq circuitry in minor device 3 */
      value |= S5933_INTCSR_int_on_inmail_box4_byte3;
  }

  outl(value,ad);

  /* for debug */
  
  /* turn on transfer bit, and set FIFO policy */
  ad=dma_engine_base+DMA_ENGINE_MCSR;
  if (IOCARD_USAGE_MODE<4){
      outl((S5933_MCSR_write_FIFO_reset | S5933_MCSR_write_priority) , ad);
      outl((S5933_MCSR_write_transf_enable | S5933_MCSR_write_priority), ad);
  } else {
      outl((S5933_MCSR_read_FIFO_reset | S5933_MCSR_read_priority) , ad);
      outl((S5933_MCSR_read_transf_enable | S5933_MCSR_read_priority), ad);
  }
  /* printk("start DMA finished - mode: %d\n",IOCARD_USAGE_MODE); */
}
/* routine to switch off dma unit */
static void shutoff_dma_engine(void) {
  unsigned long ad,val;
  int trf;
  /* turn off dma engine */
  ad=dma_engine_base+DMA_ENGINE_MCSR;
  /* avoid switching on things */
  val=S5933_MCSR_write_priority;outl(val,ad);
  /* update transfer count */
  trf=get_bytes_transfered();
  if (trf>=0) already_transfered_bytes=trf;
  dma_should_run=0;

  /* set count to zero - is that necesary ?*/
  /* ad=dma_engine_base+DMA_ENGINE_MWTC; */
  /* outl(0,ad); */
  /* switch off interrupts, and eventually clear a pending one */
  ad=dma_engine_base+DMA_ENGINE_INTCSR;
  outl(S5933_INTCSR_write_complete| S5933_INTCSR_read_complete,ad);
  return;
}

static void dmar_vm_open(struct  vm_area_struct * area) {
    /* printk("vm open called.\n"); */
}
static void dmar_vm_close(struct  vm_area_struct * area) {
    /* printk("vm close called.\n"); */
}


/* vm_operations to do the core mmap */

/* as of kernel 2.6.23 or so the fault()method is the advertised way of 
   carrying out the remapping of DMA memory into user space, but that was
   only establised later.  Earlier kernel versions still need the nopage()
   method.
*/
#ifdef HAS_FAULT_METHOD      /* new fault() code */
static int dmar_vm_fault(struct vm_area_struct *area, struct vm_fault *vmf) {
    unsigned long ofs = (vmf->pgoff << PAGE_SHIFT); /* should be addr_t ? */
    unsigned long intofs;
    unsigned char * virtad = NULL; /* start ad of page in kernel space */
    struct dma_page_pointer *pgindex;

    /* find right page */
    /* TODO: fix references... */
    pgindex = dma_main_pointer; /* start with first mem piece */
    intofs=0; /* linear offset of current mempiece start */
    while (intofs+pgindex->fullsize<=ofs) {
	intofs +=pgindex->fullsize;
	pgindex=pgindex->next;
	if (pgindex == dma_main_pointer) {
            /* offset is not mapped */
	    return VM_FAULT_SIGBUS; /* cannot find a proper page  */
	}
    }; /* pgindex contains now the relevant page index */
  
    /* do remap by hand */
    virtad = &pgindex->buffer[ofs-intofs];
    vmf->page = virt_to_page(virtad); /* return page index */

    return 0;  /* everything went fine */ 
}

#else /* This is the old nopage method for compatibility reasons */
static struct page *dmar_vm_nopage(struct vm_area_struct * area, unsigned long address, int *nopage_type) {
    struct page *page = NOPAGE_SIGBUS; /* for debug, gives mempage */
    
    /* address relative to dma memory */
    unsigned long ofs = (address - area->vm_start) + VMA_OFFSET(area);
    unsigned long intofs;
    unsigned char * virtad = NULL; /* start ad of page in kernel space */
    struct dma_page_pointer *pgindex;
    
    /* find right page */
    pgindex = dma_main_pointer; /* start with first mem piece */
    intofs=0; /* linear offset of current mempiece start */
    while (intofs+pgindex->fullsize<=ofs) {
	intofs +=pgindex->fullsize;
	pgindex=pgindex->next;
	if (pgindex == dma_main_pointer) {
	    *nopage_type = VM_FAULT_SIGBUS; /* new in kernel 2.6 */
	    return NOPAGE_SIGBUS; /* ofs not mapped */
	}
    }; /* pgindex contains now the relevant page index */
    
    /* do remap by hand */
    virtad = &pgindex->buffer[ofs-intofs];  
    
    /* page table index */
    page=virt_to_page(virtad); 
    get_page(page); /* increment page use counter */
    *nopage_type = VM_FAULT_MINOR; /* new in kernel 2.6 */
    return page;
}
#endif

/* modified from kernel 2.2 to 2.4 to have only 3 entries */
static struct vm_operations_struct dmar_vm_ops = {
    open: dmar_vm_open,
    close: dmar_vm_close, 
#ifdef HAS_FAULT_METHOD
    fault:     dmar_vm_fault,  /* introduced in kernel 2.6.23 */
#else
    nopage: dmar_vm_nopage, /* nopage */
#endif
};


static int dma_r_open(struct inode * inode, struct file * filp) {
    int irqret;
    if (IOCARD_IN_USE != 0)
	return -EBUSY;
    
    /* Interrupt reservieren, vorher Steuervariablen setzen */
    dma_should_run=0; dma_int_use_counter=0;
    dma_main_pointer=NULL; next_to_use_ptr = NULL; /* avoid trouble */
    irqret = request_irq(irq_number,interrupt_handler,
			 irq_flags, IOCARD_NAME, this_pci_device);
    if (irqret) {
	printk("Can't get my irq - someone is greedy (err: %d).\n",irqret);
	return -EBUSY;
    }
    IOCARD_IN_USE |= 1;
    return 0;
};
static int dma_r_close(struct inode * inode, struct file * filp) {
  int ad;
    
  /* DMA gegebenenfalls beenden */
  shutoff_dma_engine();

  /* completely shut down both DMA engines */
  ad=dma_engine_base+DMA_ENGINE_MWTC;
  outl(0,ad);
  ad=dma_engine_base+DMA_ENGINE_MRTC;
  outl(0,ad);

  /* Speicher freigeben */
  release_dma_buffer();

  /* Interrupt zur�ckgeben */
  free_irq(irq_number,this_pci_device);

  IOCARD_USAGE_MODE = 0;
  IOCARD_IN_USE &= ~1;
  return 0;
};

static int nu2err;
static int adr2;
static long dma_r_ioctl(struct file * file, unsigned int cmd, unsigned long value) {
  char * tmp;
  struct dma_page_pointer * tmppnt;
  int ilocal;
  unsigned long w4,adr,tmpval;
  char val;
  switch(cmd) {
  case NUDAQ_2_START_DMA: case NUDAQ_4_START_DMA:
    if (!dma_main_pointer){ nu2err=E_NU_nopointer; return -EINVAL; }
    if (dma_should_run) { nu2err=E_NU_already_running;  return -EINVAL;}
	  
    start_dma_engine(); /* 160503 */
    break;
  case NUDAQ_2_STOP_DMA: case  NUDAQ_4_STOP_DMA:
    if (!dma_main_pointer){ nu2err=E_NU_nopointer; return -EINVAL; }
    if (!dma_should_run) { nu2err=E_NU_not_running;  return -EINVAL;}
    
    shutoff_dma_engine();
    break;
  case NUDAQ_2_READ_ERR: case NUDAQ_4_READ_ERR:
    return nu2err; break;
  case NUDAQ_2_BYTES_READ: case NUDAQ_4_BYTES_SENT:
    return get_bytes_transfered();break;
  case NUDAQ_2_OUTWORD:
    outl(value,iocard_base+0x14);break;
  case NUDAQ_4_INWORD:
    return inl(iocard_base+0x10);break;
  case NUDAQ_2_READBACK:
    return inl(iocard_base+0x14);break;
  case NUDAQ_2_DITIMERS: /* set internal timers and corresp. cascade mode */
    set_counter(0, value & 0x0ffff);
    tmpval=inl(iocard_base+0x1c) & ~0x0400; /* T0_T2 bit isolation */
    if (value & 0xffff0000) {
      set_counter(2,(value>>16));
      tmpval |= 0x0400; /* set T0_T2 cascade enable bit */
    };
    outl(tmpval,iocard_base+0x1c);
   break;
  case NUDAQ_4_DITIMERS: /* set internal timers and corresp. cascade mode */
    set_counter(1, value & 0x0ffff);
    tmpval=inl(iocard_base+0x1c) & ~0x0800; /* T1_T2 bit isolation */
    if (value & 0xffff0000) {
      set_counter(2,(value>>16));
      tmpval |= 0x0800; /* set T1_T2 cascade enable bit */
    };
    outl(tmpval,iocard_base+0x1c);
   break;
  case NUDAQ_2_INMODE:
    return setinmode(value);
    break;
  case NUDAQ_4_OUTMODE:
    return setoutmode(value);
    break;
    /* for debugging: */
  case 0x2001:
    /* write to buffer adr mit offset, return kernel adr */
    tmp=dma_main_pointer->buffer;
    tmp[(value>>8)&0xff]=(value & 0xff);
    return (unsigned int)tmp[(value>>8)&0xff];
    break;
  case 0x2002:
    /* read buffer with offset */
    tmp=dma_main_pointer->buffer;
    val=tmp[(value>>8)&0xff];
    return (unsigned int)val;
    break;
  case 0x2003:
    return (int)value;
    break;
  case 0x2004:
    /* set adr register */
    adr2 = value;
    return 0;
    break;
  case 0x2005:
    /* re-read adr register */
    return (int) adr2;
    break;
  case 0x2006:case 0x2008:
    /* set register in adr space 1 directly */
    w4=value;adr=(adr2 & 0x3c)+(cmd==0x2006?iocard_base:dma_engine_base);
    outl(w4,adr);
    break;
  case 0x2007:case 0x2009:
    /* read register in adr space 1 directly */
    adr=(adr2 & 0x3c)+(cmd==0x2007?iocard_base:dma_engine_base);
    return (int)inl(adr);
    break;
  case 0x200a:
    /* read bus master control status reg */
    return (int) inl(dma_engine_base+0x3c);
    break;
  case 0x200b:
    /* read interrupt control register */
    return (int) inl(dma_engine_base+0x38);
    break;
  case 0x200c:
    /* read saved value diovalsave */
    return (int)diovalsave;
    break;
  case 0x200d:
    /* read MWTC register */
    return (int)inl(dma_engine_base+0x28);
    break;
  case 0x200e:
    /* transfer the adr2-th entry in the dma buffer chain to userspace */
    tmppnt=dma_main_pointer;
    for (ilocal=adr2;ilocal>0;ilocal--) tmppnt=tmppnt->next;
    if (copy_to_user((dma_page_pointer *)value, tmppnt, sizeof(dma_page_pointer)))
	return -EFAULT;
    break;
  case 0x200f:
      /* read in dma_use_cnt */
      return (int) dma_int_use_counter;
      break;
  case 0x2010:
      /* allow for nudaq specific irqs */
      outl(value,iocard_base+IOCARD_INT_STATUS);
      return 0;
      break;
      /* irq related stuff + starting of data acq for minor device 3. 
         It is dirty to do so many things in one call, but that way no
         time is waisted in surfing between OS layers....
         Argument of this ioctl is the time in millisec for the hardware
         timer to switch off data acquisition */
  case NUDAQ_3_SET_TIMEOUT: 
      /* set timer to irq every value millisecs and clear inhibit*/
      /* first, make sure its minor device 3: */
      if (IOCARD_USAGE_MODE !=3) return -EINVAL;

      set_counter(2,4000); /* generate 1 kHz clock from 4 MHz */
      tmpval=inl(iocard_base+0x1c); /* read in int config reg */
      tmpval = (tmpval & 0x3ff) | 0x800; /* disable irq, chain t1-t2 */
      outl(tmpval,iocard_base+0x1c);
      set_counter_countdown(1,(value & 0xffff)); /* millisecs */
      tmpval |= 0x808; /* allow irqs */
      outl(tmpval,iocard_base+0x1c);
      tmpval=inl(iocard_base+0x14) | 0x02; /* value of the digi out register */
      outl(tmpval,iocard_base+0x14); /* clear inhibit */
      return tmpval;
      break;
  default:
    return -EINVAL;
  };
  /* everything was ok */
  return  0;
}

/* should we use that? */
static ssize_t dma_r_read(struct file *file, char *buffer, size_t count, loff_t *ppos) {
  return 0;
}

static int dma_r_mmap(struct file * file, struct vm_area_struct *vma) {
    int erc; /* returned error code in case of trouble */
    /* check if memory exists already */
    if (dma_main_pointer) return -EFAULT;
    
    if (VMA_OFFSET(vma)!=0) return -ENXIO; /* alignment error */
    /* should there be more checks? */
    
    /* get DMA-buffer first */
    erc=get_dma_buffer(vma->vm_end-vma->vm_start); /* offset? page alignment? */
    if (erc) {
	printk("getmem error, code: %d\n",erc);return erc;
    }
    /* do mmap to user space */
    
    vma->vm_ops = &dmar_vm_ops; /* get method */
    vma->vm_flags |= VM_RESERVED; /* avoid swapping , also: VM_SHM?*/
#ifdef HAS_FAULT_METHOD
    vma->vm_flags |= VM_CAN_NONLINEAR; /* has fault method; do we need more? */
#endif
    
    return 0;
}

/* for sending the calling process a signal */
static int dma_r_fasync(int fd, struct file *filp, int mode) {
    return fasync_helper(fd, filp, mode, &my_async_queue);
}

/* file operations for normal read/write device use */
static struct file_operations dma_rw_fops = {
    read:              dma_r_read,   /* read */
    unlocked_ioctl:    dma_r_ioctl,  /* port_ioctl */
    mmap:              dma_r_mmap,   /* port_mmap */
    open:              dma_r_open,   /* open code */
    release:           dma_r_close   /* release code */
};

/* file operations for dma read with the second timer for timeout */
static struct file_operations special_dma_r_fops = {
    read:              dma_r_read,   /* read */
    unlocked_ioctl:    dma_r_ioctl,  /* port_ioctl */
    mmap:              dma_r_mmap,   /* port_mmap */
    open:              dma_r_open,   /* open code */
    release:           dma_r_close,   /* release code */
    fasync:            dma_r_fasync  /* notification on irq */
};



/* routines for minor devices 0 and 1 */
/*
 * read/write with a single command. 
 * cmd=0x400..0x41f: write register with value,
 * cmd=0..0x01f: return content of register
 * cmd=0xC00..0xC1f: write dma engine register with value,
 * cmd=0x800..0x81f: return dma engine register content
 */

static long ioctl_flat(struct file * file, unsigned int cmd, unsigned long value) {
  unsigned short w2;
  unsigned int w4; 
  unsigned int ad = (cmd & IOCARD_SPACE);
  unsigned int mindev = IOCARD_USAGE_MODE;
  unsigned int basead;
  
  
  basead = (cmd & DMA_ENGINE_SELECT)?dma_engine_base:iocard_base;
  switch(mindev) {
      case 0: /* 16 bit Zugriff */
	  if (ad >= IOCARD_lower_bound && ad <= IOCARD_upper_bound ) {
	      ad += basead;
	      if (cmd & IOCTL_WRITE_CMD) {
		  w2=(unsigned short) value;
		  outw(w2,ad);
		  return 0;
	      } else { 
		  w2=inw(ad);
		  return (int) w2;
	      };
	  } else { 
	      return (int) -EFAULT;
	  };
	  break;
      case 1: /* 32 bit Zugriff */
	  if (ad >= IOCARD_lower_bound && ad <= IOCARD_upper_bound ) {
	      ad += basead;
	      if (cmd & IOCTL_WRITE_CMD) {
		  w4=(unsigned int) value;
		  outl(w4,ad);
		  return 0;
	      } else {
		  w4=inl(ad);
		  return (int) w4;
	      };
	  } else {
	      return (int) -EFAULT;
	  };
	  break;
  };
  /* should never be reached */
  return (int) -EFAULT;
}

static int flat_open(struct inode * inode, struct file * filp){
  if (IOCARD_IN_USE != 0)
    return -EBUSY;
  IOCARD_USAGE_MODE = MINOR(inode->i_rdev);
  IOCARD_IN_USE |= 1;
  return 0;
};

static int flat_close(struct inode * inode, struct file * filp){  
  IOCARD_USAGE_MODE=0;
  IOCARD_IN_USE &= !1;
  return 0;
};

static struct file_operations flat_fops = {
    unlocked_ioctl:   ioctl_flat,	/* port_ioctl */
    open:             flat_open,	        /* open code */
    release:          flat_close 	/* release code */
};


/* general administration stuff associated with devive driver and
   module generation  */

static int nudaq_open(struct inode * inode, struct file * filp) {
    switch (MINOR(inode->i_rdev)) { 
	case 0:case 1: /* flat adressing of IO module */
	    filp->f_op = &flat_fops;
	    break;
	case 2: /* DMA reading from external*/
	case 4: /* DMA write-to-external */
	    filp->f_op = &dma_rw_fops;
	    break;
	case 3: /* special dma mode reading with timeout option */
	    filp->f_op = &special_dma_r_fops;
	    break;

	default:
	    return -ENXIO;
    }

    /* store current open mode for later use */
    IOCARD_USAGE_MODE = MINOR(inode->i_rdev);

    if (filp->f_op && filp->f_op->open) 
	return filp->f_op->open(inode,filp);
    return 0;
}

static int nudaq_close(struct inode * inode, struct file * filp) {
    switch (MINOR(inode->i_rdev)) {
	case 0:case 1:
	    /* flat adressing of IO module */
	    filp->f_op = &flat_fops;
	    break;
	case 2: /* dma reading */
	case 4: /* DMA write-to-external */
	    filp->f_op = &dma_rw_fops;
	    break;
	case 3: /* special dma mode reading with timeout option */
	    filp->f_op = &special_dma_r_fops;
	    break;
	default:
	    return -ENODEV;
    }
    if (filp->f_op && filp->f_op->release)
	return filp->f_op->release(inode,filp);
    return 0;
}

/* open routines , kernel version 2.4 */
static struct file_operations all_fops = {
    open:    nudaq_open,   /* iocard_open just a selector for the real open */
    release: nudaq_close,  /* release */
};

#define DEBUG

static int thisdevice_major;

static int nudaq7200_init_one(struct pci_dev *dev, const struct pci_device_id *ent) {
    static int dev_count = 0;
    int response;

    /* make sure there is only one card */
    dev_count++;
    if (dev_count > 1) {
	printk ("this driver only supports 1 device\n");
	return -ENODEV;
    }

    /* moved this before any access to the device resources to avoid ill
       resources - 5.9.07 chk */
    if (pci_enable_device (dev)) 
	return -EIO;

    /* get resources */
    irq_number = dev->irq;
    iocard_base = pci_resource_start(dev, 1);
    dma_engine_base =  pci_resource_start(dev, 0);
    /*
    printk ("nudaq-7200 card:\n  work regs at at %X\n  dma engine at %X\n  "
	    "(Interrupt %d)\n", iocard_base, dma_engine_base, irq_number);
    */
    /* register resources */
    if (request_region (iocard_base, IOCARD_SPACE+1, IOCARD_NAME) == NULL) {
	printk ("base I/O %X not free.\n", iocard_base);
	return -EIO;
    }
    if (request_region (dma_engine_base, DMA_ENGINE_SPACE+1, IOCARD_NAME) 
	== NULL) {
	printk ("DMA engine I/O at %X not free.\n", dma_engine_base);
	release_region(iocard_base,IOCARD_SPACE+1);
	return -EIO;
    }
    
    /* try to handle dynamic device registration properly */
    response=register_chrdev (IOCARD_MAJOR, IOCARD_NAME, &all_fops);
    if (response<0) {
	printk ("can't register major device %d; err: %d\n",
		IOCARD_MAJOR,response);
	goto prob1;
    }
    /* save major device number */
    thisdevice_major = IOCARD_MAJOR ? IOCARD_MAJOR : response;
	
    /* save pci device handle */
    this_pci_device = dev;
    return 0; /* everything is fine */

 prob1:
    release_region(iocard_base,IOCARD_SPACE+1);
    release_region(dma_engine_base, DMA_ENGINE_SPACE+1); 
    return -ENODEV;
}

static void nudaq7200_remove_one(struct pci_dev *pdev) {
    unregister_chrdev (thisdevice_major,IOCARD_NAME);

    release_region(iocard_base,IOCARD_SPACE+1);
    release_region(dma_engine_base, DMA_ENGINE_SPACE+1); 
    /* release device handle */
    this_pci_device = NULL;
}

/* driver description info for registration */
static struct pci_device_id nudaq7200_pci_tbl[] = {
    { PCI_VENDOR_ID_AMCC, PCI_DEVICE_ID_NUDAQ7200, PCI_ANY_ID, PCI_ANY_ID, 0,0,0},
    {0,0,0,0,0,0,0},
};
MODULE_DEVICE_TABLE(pci, nudaq7200_pci_tbl);

static struct pci_driver nudaq7200_driver = { 
    name:       "nudaq-pci7200",
    id_table:   nudaq7200_pci_tbl,
    probe:      nudaq7200_init_one,
    remove:     nudaq7200_remove_one,
};

static void __exit nudaq7200_clean(void) {
    pci_unregister_driver( &nudaq7200_driver );
}

static int __init nudaq7200_init(void) {
    int rc;
    rc = pci_register_driver( &nudaq7200_driver );
    if (rc != 0) return -ENODEV;
    return 0;
}

module_init(nudaq7200_init);
module_exit(nudaq7200_clean);








