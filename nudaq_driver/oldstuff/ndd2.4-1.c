/* Treiber für NuDAQ-Karte; sucht Basisadresse des AMCC-chips und stellt ein
 Gerät zur Verfügung, mit dem auf den Adressraum zugegriffen werden kann.

 stellt folgende minor Geräte zur Verfügung:
 Minor 0:  Addressraum 0...01f, Lese/Schreibzugriffe mit 16 bit Breite
 Minor 1:  Addressraum 0...0x1f, Lese/Schreibzugriffe mit 32 bit Breite

 erste Version   30.06.00  chk

 modifizierte Version, soll zugriff auf dma-engine ermöglichen. noch nicht
 fertig. status: 13.08.00, 20:46 chk, kompiliert fehlerfrei

 experimentell, Umsetzung auf kernel version 2.4; kompiliert fehlerfrei fuer
    minor devices 0, 1.

    2.4 version von dev 2, scheint zu funktionieren auf kernel 2.4.18,
    nicht jedoch auf 2.4.4 !!!!                           chk, 1.8.02
    irq anfrage repariert.                                chk, 2.8.02 
    version, die interrupts zulaesst vom onboard-timer.
    spezialisiert auf den Bedarf bie der 16-fach-
    Koinzidenzeinheit. ToDo: Separation dieser Funktionen
    von der reinen Transfer-Konfig.                       chk, 17.5.03

    more todo:
    docu ioctl functions
    allow output

 ***********************

 soll liefern: minor 2 mit folgenden Eigenschaften:

 Minor 2:  Beim Öffnen wird die DMA-engine reserviert (Interupt frei?)
           Neuer mechanismus: Zunächst wird mit mmap eine Abbildung des
	   DMA-Pufferspeichers in den user space beantragt. Der Puffer wird
	   dabei erst mit der mmap-Funktion alloziert (geht das auch so, dass
	   ein bestimmter Mindestpuffer bei open alloziert wird, so daß ein
	   zugriff mit normalem read funktioniert? Bis jetzt nicht.)

	   Es gibt eine ioctl()-Funktion, die die Anzahl der bereits
	   von der DMA eingelesenen Bytes und eine, die gerade aktuelle
	   Schreibposition in dem DMA-Puffer angibt. 

           Mit einem Lesezugriff soll ein kernel-Puffer ausgelesen werden,
	   der von der eigentlichen DMA-Engine gefüttert wird. Die genauen
	   Parameter des Transfers können mit ioctl()-Funktionen bestimmt
	   werden. Folgende ioctl()-funktionen sollen implementiert sein:
	   Kommando:  timer_set value (setzt den Taktgeber auf intern)
	              NUDAQ_2_START_DMA           (startet dma-Einleserei)
		      NUDAQ_2_STOP_DMA            (schaltet dma wieder ab)
		      NUDAQ_2_READ_ERR            ( liest fehlerstatus )
		      block_mode (read blockiert)
		      noblock_mode (read blockiert nicht)
		      granular_1, _2, _4 (granularität des Lesetransfers)
		      NUDAQ_2_OUTWORD value      (gibt auf ausgabeport aus)
		      NUDAQ_2_READBACK           (liest ausgaberegister zurück)
		      NUDAQ_2_BYTES_READ          (Anzahl der per DMA
		                                   transferierten bytes)
		      NUDAQ_2_DITIMERS val       (setzt teilerverhältnis;
		              loword: counter0, hiword: counter2. Falls
			      hiword ==0, word nur counter 0 gesetzt.)
		      NUDAQ_2_INMODE val    legt inmode fest; val setzt sich
		         zusammen aus DI_MODE0..DI_MODE3, DI_WAIT_TRIG,
			 DI_TRIG_POL, DI_CLEAR_FIFO 

*/

/* get explicit.. */
/* #define bla1 */

/* debug noise for page allocation */
/* #define bla_pages   */
/* debug noise for nopage calls */
/* #define bla_nopage */

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

/* #include <linux/miscdevice.h> */

#define PCI_DEVICE_ID_NUDAQ7200 0x80d8
  
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/system.h>
#include <asm/uaccess.h>

/* quick and dirty transient definitions 2.2 -> 2.4 */
#define VMA_OFFSET(vma)   (vma->vm_pgoff * PAGE_SIZE)


#define IOCARD_MAJOR 252
#define FLAT_16_MINOR 0
#define FLAT_32_MINOR 1
#define DMAMODE_MINOR 2
#define IOCARD_NAME "nudaq-driver\0"
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
/* verschiedne modes für INMODE: */
#define DI_MODE0 0x00 /* internal timer source , only timer 0 */
#define DI_MODE1 0x01 /* external, i_req rising edge */
#define DI_MODE2 0x02 /* external, i_req falling edge */
#define DI_MODE3 0x03 /* i_req & i_ack handshake */
#define DI_MODE0_CASCADE 0x04 /* internal timers 0 and 2 cascaded as source */
#define DI_WAIT_TRIG 0x08 /* Wait for trigger */
#define DI_TRIG_POL 0x10 /* trigger_polarity: set:falling */
#define DI_CLEAR_FIFO 0x20 /* eventually clear FIFO before acquisition */
#define DI_IREQ_INT 0x100 /* allow for extra IRQs on I_REQ */


/* minor device 2 error codes */
#define E_NU2_nopointer 1
#define E_NU2_already_running 2
#define E_NU2_not_running 3

/* register definitions for the dma engine in the S5933 chip */
#define DMA_ENGINE_MWAR 0x24  /* bus master write addr register */
#define DMA_ENGINE_MWTC 0x28  /* bus master write count register */
#define DMA_ENGINE_INTCSR 0x38 /* bus master interrupt/status register */
#define DMA_ENGINE_MCSR 0x3c  /* S5933 master control/status reg */
#define S5933_INTCSR_int_asserted 0x00800000 /* interrupt asserted bit */
#define S5933_INTCSR_write_complete 0x00040000 /* write complete bit */
#define S5933_INTCSR_incoming_mail 0x00020000 /* int on incoming mailbox */
#define S5933_INTCSR_abort_mask 0x00300000 /* abort message */
#define S5933_INTCSR_int_on_wr_complete 0x00004000 /* int enable */
#define S5933_INTCSR_int_on_inmail_box4_byte3 0x00001f00
#define S5933_MCSR_write_transf_enable  0x00000400
#define S5933_MCSR_write_FIFO_scheme    0x00000200
#define S5933_MCSR_write_priority    0x00000100
#define S5933_MCSR_write_FIFO_reset  0x04000000
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
#define IOCARD_INT_SI_REQ 0x0040 /* status bit for I_REQ interrupts */
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

static unsigned int iocard_base;
static unsigned int dma_engine_base;
static int irq_number;
#define irq_flags (SA_SHIRQ | SA_INTERRUPT)

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
  dioval = ((val & 0x03)?0x1:0) |
           (((val & 0x3)!=DI_MODE0)?0x2:0x4 ) | 0x08 |
           ((val & DI_TRIG_POL)?0:0x10 )| 
           ((val & DI_WAIT_TRIG)?0x20:0);
  ad=iocard_base+IOCARD_INT_STATUS; outl(intval,ad);

  ad=iocard_base+IOCARD_DIO_STATUS; outl(dioval,ad);
  dioval |=0x40; outl(dioval,ad);
  diovalsave=dioval; /* for debugging */
  return 0;
}

/* Struktur, die pages für DMA-Buffer enthalten soll. Soll effizienten
   Zugriff auf Speicher während Seitenwechsel im DMA erlauben. */
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
    if (order) {
	for (i=1;i<(1 << order);i++) {
	    spa=virt_to_page(buffer+i*PAGE_SIZE);
	    set_page_count(spa,0);
	}
    }	  
}
/* release code for the DMA buffer and the DMA buffer pointer */
void release_dma_buffer(){
  struct dma_page_pointer *currbuf, *tmpbuf;

  /* only one buffer to free... */
  currbuf=dma_main_pointer;
  if (currbuf) { /* nothing to release ?*/
      do {
/*      pci_free_consistent(this_pci_device, currbuf->fullsize,
	currbuf->buffer, currbuf->physbuf);  */
	  /* free_pages((unsigned long)currbuf->buffer,0); */
#ifdef bla_pages
      printk("given back buffer at virtad=%x, physad=%x; size: %d\n",
	     (int)currbuf->buffer,(int)currbuf->physbuf,
	     (int)currbuf->fullsize);
#endif
      /* ..then give pages back to OS */
      free_pages((unsigned long) currbuf->buffer,
		 currbuf->order); /* free buffer */
#ifdef bla_pages
      printk("Seitenbefreiung, ad=%x,  order=%d\n",(int)currbuf->buffer,(int)currbuf->order);
#endif
      tmpbuf =currbuf->next; kfree(currbuf); currbuf=tmpbuf; /* free pointer */
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
#ifdef bla_pages
  struct page * page; /* just to check */
#endif
  dma_addr_t busaddress; /* bus address of DMA buffer */
  struct dma_page_pointer * currbuf;
  struct dma_page_pointer * tmpbuf;
  /* check multi pages */
  struct page *spa;
  unsigned long pflags;
  int pcnt,i;  

  /* reset dma pointer buffer */
  currbuf=NULL; /* dma_main_pointer; */ /* NULL if no buffer exists */
#ifdef bla1
  printk("I should get %d bytes now.\n",bytes_to_get);
#endif
 
  /* still have to get only small pieces.... */
  page_order = 4;
    
  /* page_order = get_order(bytes_to_get); */
  if (page_order >= MAX_ORDER) page_order=MAX_ORDER;

  while (bytes_to_get>0) {
      /* shrink size if possible */
      while((page_order>0) && (PAGE_SIZE<<(page_order-1))>=bytes_to_get)
	  page_order--;
       
#ifdef bla_pages
      printk("current page order: %d\n",(int)page_order);
#endif
      /* bufferpiece = pci_alloc_consistent (this_pci_device,
	 bytes_to_get, &busaddress); */ /* does not help... */
      bufferpiece = (void *)__get_free_pages(GFP_KERNEL,page_order);
      
      if (bufferpiece) {

	  /* repair missing page counts */
	  add_individual_page_counts(bufferpiece, page_order);
	  
	  /* get block structure */
	  for (i=0;i<(1 << page_order);i++) {
	      spa=virt_to_page(bufferpiece+i*PAGE_SIZE);
	      pcnt=page_count(spa);
	      pflags = spa->flags;
	      /*      printk("subpage index: %d, count: %d, flags: %x\n",i,pcnt,(int)pflags); */
      }	  
	  busaddress = virt_to_bus(bufferpiece);
	  /* success: make new entry in chain */
	  tmpbuf = (dma_page_pointer *) kmalloc(sizeof(dma_page_pointer),
						GFP_KERNEL); /* first, get buffer */
	  if (!tmpbuf) {
	      printk(" Wruagh - kmalloc failed for buffer pointer....\n");
	      /* release_individual_page_counts(bufferpiece,page_order); */
	      free_pages((unsigned long)bufferpiece,page_order); /* give it back */
	      printk("kmalloc failed during DMA buffer alloc. better reboot.\n");
	      return -ENOMEM;
	  }
	  
#ifdef bla_pages
	  page = virt_to_page(bufferpiece);
	  printk(" page of %x is: %x\n",(int)bufferpiece, (int)page);
#endif
	  
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
/* how many bytes are transferred already ?*/
static unsigned long already_transfered_bytes=0;
static int get_bytes_transfered(){
  unsigned long int_problem, ad;
  int i=5; /* max. mumber of attempts to read */
  unsigned long cnts;

  /* try to find out if DMA is running */
  if (!dma_should_run) return already_transfered_bytes;

  ad=dma_engine_base+DMA_ENGINE_MWTC;
  /* check if there has been a mempiece change only if there is already  memory     assigned. */
  if (!next_to_use_ptr) return 0;
  do {
    int_problem=dma_int_use_counter; /* consistency check */
    cnts=(next_to_use_ptr->previous->size - inl(ad))+already_transfered_bytes;
    /* for debugging: actual setting */
    /* cnts=inl(ad); */

    i--;
  } while ((int_problem!=dma_int_use_counter) && i);

  if (i==0) return -EFAULT; /* no consistent number to get - too often irq ? */
  return cnts;
}
/* interupt handling - notofication queues, bh's, handler ..*/
	    
static struct fasync_struct *my_async_queue;
static void nudaq_bh_handler(void *unused) {
    /* tell what happened */
    kill_fasync(&my_async_queue, SIGIO, POLL_IN);
    return;
}

/* interrupt handling - helper for notifying task on non-bufferchange ints */
struct tq_struct notify_task = { routine: &nudaq_bh_handler,
				 data: NULL,
				 sync: 0,
};

/* interrupt handler für minor device 2 */
static void interrupt_handler(int irq, void *dev_id, struct pt_regs *regs) {
  static unsigned long ad,intreg,ad1,ad2,intreg2,tmpval;

  ad1=dma_engine_base+DMA_ENGINE_INTCSR;

  intreg=inl(ad1); /* interrupt status reg */
  if (intreg & S5933_INTCSR_int_asserted) {
      
      if (intreg & S5933_INTCSR_write_complete) {
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

      /* irqs from the nudaq proprietary circuitry */
      if (intreg & S5933_INTCSR_incoming_mail) {
	  ad2=iocard_base+IOCARD_INT_STATUS; /* read in this irqreg */
	  intreg2=inl(ad2);

	  if (intreg2 & IOCARD_INT_STATMASK) { /* came from nudaq */
	      /*  nudaq fpga interupt handler  */
	      outl(intreg2 | IOCARD_INT_STATMASK, ad2);
	      
	      /* disable data acquisition */
	      tmpval=inl(iocard_base+IOCARD_DIGIOUT) & ~0x2;
	      outl(tmpval,iocard_base+IOCARD_DIGIOUT);

	      /* schedule for notification */
	      queue_task(&notify_task, &tq_immediate);
	      mark_bh(IMMEDIATE_BH);
	      /* clear Nudaq proprietary irq source */
	      outl(intreg2,ad2);
	  }
      }
     /* now clear all irqs from the AMCC source */
     outl_p(intreg, ad1); 
  }
  return;
};


/* routine to set dma to running */
static void start_dma_engine(){ 
  unsigned long ad;
  /* set read pointer to base address and reset byte count */
  next_to_use_ptr=dma_main_pointer; already_transfered_bytes=0;
  
  /* fill 5933 dma engine registers */
  ad=dma_engine_base+DMA_ENGINE_MWAR;
  outl(next_to_use_ptr->physbuf,ad); /* base addr */
  ad=dma_engine_base+DMA_ENGINE_MWTC;
  outl(next_to_use_ptr->size,ad); /* bytes to transfer now */

  /* prepare next-to-use pointer for interrupt routine */
  next_to_use_ptr=next_to_use_ptr->next;

  /* switch on interrupt handler */
  dma_should_run=1;
  
  /* set interrupt control flags in S5933, and reset pending interrupts */
  ad=dma_engine_base+DMA_ENGINE_INTCSR;
  outl((S5933_INTCSR_abort_mask | S5933_INTCSR_write_complete |
	S5933_INTCSR_int_on_wr_complete |
	S5933_INTCSR_int_on_inmail_box4_byte3 /* for nudaq circuitry */
      ),ad);

  /* turn on transfer bit, and set FIFO policy */
  ad=dma_engine_base+DMA_ENGINE_MCSR;
  outl((S5933_MCSR_write_FIFO_reset | S5933_MCSR_write_priority) , ad);
  outl((S5933_MCSR_write_transf_enable | S5933_MCSR_write_priority), ad);
  
}
/* routine to switch off dma unit */
static void shutoff_dma_engine() {
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

  /* set count to zero */
  ad=dma_engine_base+DMA_ENGINE_MWTC;
  /* outl(0,ad); */
  /* switch off interrupts, and eventually clear a pending one */
  ad=dma_engine_base+DMA_ENGINE_INTCSR;
  outl(S5933_INTCSR_write_complete,ad);
  return;
}

static void dmar_vm_open(struct  vm_area_struct * area) {
    printk("vm open called.\n");
}
static void dmar_vm_close(struct  vm_area_struct * area) {
    printk("vm close called.\n");
}


/* vm_operations, die das eigentliche mmap über die nopage-methode vornehmen */
static struct page *dmar_vm_nopage(struct vm_area_struct * area, unsigned long address, int write_access) {
  struct page *page = NOPAGE_SIGBUS; /* for debug, gives mempage */

  /* address relative to dma memory */
  unsigned long ofs = (address - area->vm_start) + VMA_OFFSET(area);
  unsigned long intofs;
  unsigned char * virtad = NULL; /* start ad of page in kernel space */
  struct dma_page_pointer *pgindex;
#ifdef bla_nopage
  void * pga; /* for tests */
#endif
  /* pgd_t *pgd; pmd_t *pmd; pte_t *pte;  for getting mapping to real bufs 
  unsigned long lpage; */

  /* find right page */
  pgindex = dma_main_pointer; /* start with first mem piece */
  intofs=0; /* linear offset of current mempiece start */
  while (intofs+pgindex->fullsize<=ofs) {
    intofs +=pgindex->fullsize;
    pgindex=pgindex->next;
    if (pgindex == dma_main_pointer) return NOPAGE_SIGBUS; /* ofs not mapped */
  }; /* pgindex contains now the relevant page index */
  
  /* do remap by hand */
  virtad = &pgindex->buffer[ofs-intofs];  
  
  /* virtad = dma_main_pointer->buffer;   for tests only */
 
  /* page table index */
  
  page=virt_to_page(virtad); 
  
  get_page(page); /* increment page use counter */
  
#ifdef bla_nopage
  pga = page_address(page); /* for tests */
  printk("nopage-Zugriff; adr=%x, phys=%x, page=%x\n ofs=%x, intofs=%x, pgindex=%x, virtad = %x, pga= %x\n",
	 (int)address, (int)pgindex->physbuf,(int)page,(int)ofs,
 (int)intofs,(int)pgindex, (int)virtad, (int) pga);
#endif

  return page;
}
/* modified from kernel 2.2 to 2.4 to have only 3 entries */
static struct vm_operations_struct dmar_vm_ops = {
    open: dmar_vm_open,
    close: dmar_vm_close, 
    nopage: dmar_vm_nopage, /* nopage */};


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
	printk("Can't get my irq - someone is greedy.\n");
	return -EBUSY;
    }
    IOCARD_IN_USE |= 1;
    MOD_INC_USE_COUNT;
#ifdef bla1
    printk("ndd2: open dev 2\n");
#endif
    return 0;
};
static int dma_r_close(struct inode * inode, struct file * filp) {

  /* DMA gegebenenfalls beenden */
  shutoff_dma_engine();

  /* Speicher freigeben */
  release_dma_buffer();
#ifdef bla1
  printk("ndd2: close dev 2\n");
#endif
  /* Interrupt zurückgeben */
  free_irq(irq_number,this_pci_device);

  IOCARD_IN_USE &= ~1;
  MOD_DEC_USE_COUNT;
  return 0;
};

static int nu2err;
static int adr2;
static int dma_r_ioctl(struct inode * inode, struct file * file, unsigned int cmd, unsigned long value) {
  char * tmp;
  struct dma_page_pointer * tmppnt;
  int ilocal;
  unsigned long w4,adr,tmpval;
  char val;
  switch(cmd) {
  case NUDAQ_2_START_DMA:
    if (!dma_main_pointer){ nu2err=E_NU2_nopointer; return -EINVAL; }
    if (dma_should_run) { nu2err=E_NU2_already_running;  return -EINVAL;}

    start_dma_engine(value); /* 160503 */
    break;
  case NUDAQ_2_STOP_DMA:
    if (!dma_main_pointer){ nu2err=E_NU2_nopointer; return -EINVAL; }
    if (!dma_should_run) { nu2err=E_NU2_not_running;  return -EINVAL;}
    
    shutoff_dma_engine();
    break;
  case NUDAQ_2_READ_ERR:
    return nu2err; break;
  case NUDAQ_2_BYTES_READ:
    return get_bytes_transfered();break;
  case NUDAQ_2_OUTWORD:
    outl(value,iocard_base+0x14);break;
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
  case NUDAQ_2_INMODE:
    return setinmode(value);
    break;
    /* for debugging: */
  case 0x2001:
    /* write to buffer adr mit offset, return kernel adr */
    tmp=dma_main_pointer->buffer;
    tmp[(value>>8)&0xff]=(value & 0xff);
    return (unsigned int)tmp[(value>>8)&0xff];
    break;
  case 0x2002:
    /* read buffer mot offset */
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
  case 0x2010:
      /* allow for nudaq specific irqs */
      outl(value,iocard_base+IOCARD_INT_STATUS);
      return 0;
      break;
    /* irq related stuff + starting of data acq */
  case 0x2012: 
      /* set timer to irq every value millisecs and clear inhibit*/
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
#ifdef bla1
  printk("ger_dma_mem: Will %d bytes haben\n",(int)(vma->vm_end-vma->vm_start));
#endif
  /* get DMA-buffer first */
  erc=get_dma_buffer(vma->vm_end-vma->vm_start); /* offset? page alignment? */
  if (erc) {
    printk("getmem error, code: %d\n",erc);return erc;
  }
  /* do mmap to user space */
#ifdef oldvers
  if (do_nudaq_mmap(vma)) return -EAGAIN;
#else
  vma->vm_ops = &dmar_vm_ops; /* get method */
  vma->vm_flags |= VM_RESERVED; /* avoid swapping , also: VM_SHM?*/
#ifdef bla1
  printk("vm flags found: %x\n",(int)vma->vm_flags);
#endif
#endif  
  return 0;
}

/* for sending the calling process a signal */
static int dma_r_fasync(int fd, struct file *filp, int mode) {
    return fasync_helper(fd, filp, mode, &my_async_queue);
}

static struct file_operations dma_r_fops = {
    read:     dma_r_read,   /* read */
    ioctl:    dma_r_ioctl,  /* port_ioctl */
    mmap:     dma_r_mmap,   /* port_mmap */
    open:     dma_r_open,   /* open code */
    release:  dma_r_close,   /* release code */
    fasync:   dma_r_fasync  /* notification on irq */
};



/* routines for minor devices 0 and 1 */
/*
 * read/write with a single command. 
 * cmd=0x400..0x41f: write register with value,
 * cmd=0..0x01f: return content of register
 * cmd=0xC00..0xC1f: write dma engine register with value,
 * cmd=0x800..0x81f: return dma engine register content
 */

static int ioctl_flat(struct inode * inode, struct file * file, unsigned int cmd, unsigned long value) {
  unsigned short w2;
  unsigned int w4; 
  unsigned int ad = (cmd & IOCARD_SPACE);
  unsigned int mindev = MINOR(inode->i_rdev);
  unsigned int basead;
  
  
  basead = (cmd & DMA_ENGINE_SELECT)?dma_engine_base:iocard_base;
  switch(mindev) {
  case 0: /* 16 bit Zugriff */
    if (ad >= IOCARD_lower_bound &&
	     ad <= IOCARD_upper_bound )
      { ad += basead;
      if (cmd & IOCTL_WRITE_CMD)
	{ w2=(unsigned short) value;
	outw(w2,ad);
	return 0;
	}
      else 
	{ w2=inw(ad);
	return (int) w2;
	};
	   }
    else 
      { return (int) -EFAULT;
      };
    break;
  case 1: /* 32 bit Zugriff */
    if (ad >= IOCARD_lower_bound &&
	ad <= IOCARD_upper_bound )
      { ad += basead;
      if (cmd & IOCTL_WRITE_CMD)
	     { w4=(unsigned int) value;
	     outl(w4,ad);
	     return 0;
	     }
      else 
	{ w4=inl(ad);
	return (int) w4;
	};
      }
    else 
      { return (int) -EFAULT;
      };
    break;
  };
  /* should never be reached */
  return (int) -EFAULT;
}

static int flat_open(struct inode * inode, struct file * filp){
  unsigned int mindev;

  if (IOCARD_IN_USE != 0)
    return -EBUSY;

  mindev = MINOR(inode->i_rdev);
  /* printk("minor device: %d\n",mindev); */
   

  IOCARD_IN_USE |= 1;
  MOD_INC_USE_COUNT;
  return 0;
};
static int flat_close(struct inode * inode, struct file * filp){  
  IOCARD_IN_USE &= !1;
  MOD_DEC_USE_COUNT;
  return 0;
};


static struct file_operations flat_fops = {
    ioctl:   ioctl_flat,	/* port_ioctl */
    open:    flat_open,	        /* open code */
    release: flat_close 	/* release code */
};


/* general administration stuff associated with devive driver and
   module generation 
   */


static int nudaq_open(struct inode * inode, struct file * filp) {
    switch (MINOR(inode->i_rdev)) { 
	case 0:case 1:
	    /* flat adressing of IO module */
	    filp->f_op = &flat_fops;
	    break;
	case 2:
	    /* DMA reading */
	    filp->f_op = &dma_r_fops;
	    break;
	default:
	    return -ENXIO;
    }
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
	case 2:
	    /* dma reading */
	    filp->f_op = &dma_r_fops;
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

/* neu ab kernel 2.4 */
/* 
static struct miscdevice pci7200_flat16 = {
    FLAT_16_MINOR,
    "flat16bit",
    &flat_fops_16
};

static struct miscdevice pci7200_flat32 = {
    FLAT_32_MINOR,
    "flat32bit",
    &flat_fops_32
};
*/ 
/*
static struct miscdevice pci7200_dmamode = {
    DMAMODE_MINOR,
    "dmamode",
    &dma_r_fops
};
*/


static int nudaq7200_init_one(struct pci_dev *dev, const struct pci_device_id *ent) {
    static int dev_count = 0;

    /* make sure there is only one card */
    dev_count++;
    if (dev_count > 1) {
	printk ("this driver only supports 1 device\n");
	return -ENODEV;
    }

    /* get resources */
    irq_number = dev->irq;
    iocard_base = pci_resource_start(dev, 1);
    dma_engine_base =  pci_resource_start(dev, 0);
    printk ("nudaq-7200 card:\n  work regs at at %X\n  dma engine at %X\n  "
	    "(Interrupt %d)\n", iocard_base, dma_engine_base, irq_number);

    /* register resources */
    if (pci_enable_device (dev)) 
	return -EIO;
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
    
    if (register_chrdev (IOCARD_MAJOR, IOCARD_NAME, &all_fops)) {
	printk ("can't register major device %d\n", IOCARD_MAJOR);
	goto prob1;
    }
    /* save pci device handle */
    this_pci_device = dev;
    return 0; /* everything is fine */

 prob1:
    release_region(iocard_base,IOCARD_SPACE+1);
    release_region(dma_engine_base, DMA_ENGINE_SPACE+1); 
    return -ENODEV;
}

static void nudaq7200_remove_one(struct pci_dev *pdev) {
    unregister_chrdev (IOCARD_MAJOR,IOCARD_NAME);

    release_region(iocard_base,IOCARD_SPACE+1);
    release_region(dma_engine_base, DMA_ENGINE_SPACE+1); 
    /* release device handle */
    this_pci_device = NULL;
}

/* driver description info for registration */
static struct pci_device_id nudaq7200_pci_tbl[] __initdata = {
    { PCI_VENDOR_ID_AMCC, PCI_DEVICE_ID_NUDAQ7200, PCI_ANY_ID, PCI_ANY_ID },
    {0,0,0,0},
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
    rc = pci_module_init( &nudaq7200_driver );
    if (rc != 0) return -ENODEV;
    return 0;
}

module_init(nudaq7200_init);
module_exit(nudaq7200_clean);








