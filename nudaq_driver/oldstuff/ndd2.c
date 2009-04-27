/* Treiber für NuDAQ-Karte; sucht Basisadresse des AMCC-chips und stellt ein
 Gerät zur Verfügung, mit dem auf den Adressraum zugegriffen werden kann.

 stellt folgende minor Geräte zur Verfügung:
 Minor 0:  Addressraum 0...01f, Lese/Schreibzugriffe mit 16 bit Breite
 Minor 1:  Addressraum 0...0x1f, Lese/Schreibzugriffe mit 32 bit Breite

 erste Version   30.06.00  chk

 modifizierte Version, soll zugriff auf dma-engine ermöglichen. noch nicht
 fertig. status: 13.08.00, 20:46 chk, kompiliert fehlerfrei

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

#include <linux/module.h>
#include <asm/segment.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/sched.h>
#include <linux/timer.h>

#include <linux/malloc.h>
#include <linux/ioport.h>
#include <linux/fcntl.h>
#include <linux/pci.h>



#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/system.h>

#define langsamtest 0
/* #define bla1 */

#define IOCARD_MAJOR 252
#define IOCARD_NAME "nudaq-driver\0"
#define IOCARD_SPACE 0x1f
#define DMA_ENGINE_SPACE 0x3f

/* ioctl() command definitions */
#define IOCTL_WRITE_CMD 0x0400
#define DMA_ENGINE_SELECT 0x0800 /* for flat access */
/* for access to minor device 2: */

#include "ioctl_def.h" /* JZ */

/* verschiedne modes für INMODE: */
#define DI_MODE0 0x00 /* internal timer source , only timer 0 */
#define DI_MODE1 0x01 /* external, i_req rising edge */
#define DI_MODE2 0x02 /* external, i_req falling edge */
#define DI_MODE3 0x03 /* i_req & i_ack handshake */
#define DI_MODE0_CASCADE 0x04 /* internal timers 0 and 2 cascaded as source */
#define DI_WAIT_TRIG 0x08 /* Wait for trigger */
#define DI_TRIG_POL 0x10 /* trigger_polarity: set:falling */
#define DI_CLEAR_FIFO 0x20 /* eventually clear FIFO before acquisition */

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
#define S5933_INTCSR_abort_mask 0x00300000 /* abort message */
#define S5933_INTCSR_int_on_wr_complete 0x00004000 /* int enable */
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


/* local status variables */
static int IOCARD_IN_USE = 0; 


static unsigned int iocard_base;
static unsigned int dma_engine_base;
static int irq_number;
unsigned long irq_flags;

/* upper/lower access boundaries for the different minor devices */
static unsigned int IOCARD_lower_bound = {0};
static unsigned int IOCARD_upper_bound = {0x1f};

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
/* structure to set the DIO conttrol and interrupt control register in
   the NUDAQ card */ 
static int diovalsave;
static int setinmode(int val){
  unsigned long ad;
  unsigned long dioval,intval;
  /* create int control value */
  intval = (val & DI_MODE0_CASCADE)?0x0400:0 |
    (val & 0x1)?0:0x1000;
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
  unsigned long order; /* order of mem piece */
  char * buffer; /* pointer to actual buffer (as seen by kernel) */
  unsigned long physbuf; /* bus address of actual buffer, for dma engine */
} dma_page_pointer;
static dma_page_pointer * dma_main_pointer; /* parent pointer to DMA array */
static dma_page_pointer * next_to_use_ptr; /* pointer to next seg for DMA */
static int dma_should_run=0;
static int dma_int_use_counter=0; /* counter to test consistency of dma regs */

/* release code for the DMA buffer and the DMA buffer pointer */
void release_dma_buffer(){
  int i;
  struct dma_page_pointer *currbuf, *tmpbuf;
  /* walk through all buffer pieces */
  currbuf=dma_main_pointer;
  if (currbuf) { /* nothing to release ?*/
    do {
#ifdef bla1
      printk("giving back pages; size: %d\n",(int)currbuf->fullsize);
       for (i=MAP_NR(currbuf->buffer);
	    i<MAP_NR(currbuf->buffer+currbuf->fullsize);i++)
	 printk("  page %06x count: %d\n",i,atomic_read(&mem_map[i].count));   
#endif
      /* repair extra page use counter ....*/
      for (i=MAP_NR(currbuf->buffer);
	   i<MAP_NR(currbuf->buffer+currbuf->fullsize);i++)
	atomic_sub(2,&mem_map[i].count); /* here was atomic_dec */
      /* ..then give pages back to OS */
      free_pages((unsigned long) currbuf->buffer,
		    currbuf->order); /* free buffer */
#ifdef bla1
    printk("Seitenbefreiung, ad=%x,  order=%d\n",(int)currbuf->buffer,(int)currbuf->order);
#endif
      tmpbuf =currbuf->next; kfree(currbuf); currbuf=tmpbuf; /* free pointer */
    } while (currbuf !=dma_main_pointer);
    dma_main_pointer=NULL; /* mark buffer empty */
  };
  return;
}

/* routine to allocate DMA buffer RAM of size (in bytes).
   Returns 0 on success, and <0 on fail */ 
static int get_dma_buffer(ssize_t size) {
  ssize_t bytes_to_get = size & ~0x3; /* get long-aligned */
  ssize_t usedbytes;
  unsigned long page_order;
  void * bufferpiece;
  struct dma_page_pointer * currbuf;
  struct dma_page_pointer * tmpbuf;
  int i;

  /* reset dma pointer buffer */
  currbuf=dma_main_pointer; /* NULL if no buffer exists */
#ifdef bla1
  printk("I should get %d bytes now.\n",bytes_to_get);
#endif
  /* find out starting order (i.e. largest piece to get ; was order 5)*/
  for (i=0;(i>=0) && ((PAGE_SIZE <<i)>=bytes_to_get) ;i--);page_order=i;
  /* try to get largest possible pieces of memory until size is reached */
  while (bytes_to_get>0) {
    /* shrink size if possible */
    while((page_order>0) && (PAGE_SIZE<<(page_order-1))>=bytes_to_get)
      page_order--;
    /* get one piece of mem with dma posssible */
#ifdef bla1
    printk("hole mir gerade seiten, order: %d\n",(int)page_order);
#endif
    bufferpiece=(void *)__get_free_pages(GFP_KERNEL,page_order);
    if (bufferpiece) {
#ifdef bla1
      printk("habe seite bekommen! ad=%x\n",(int)bufferpiece);
#endif
      /* success: make new entry in chain */
      tmpbuf = (dma_page_pointer *) kmalloc(sizeof(dma_page_pointer),
					   GFP_KERNEL); /* first, get buffer */
      if (!tmpbuf) {
	/* release buffer in trouble */
	free_pages((unsigned long)bufferpiece,page_order); 
	printk("Problem: freed buffer %x, order %d\n",(int)bufferpiece,(int)page_order);
	break; /* can kmalloc fail? */
      };
      if (currbuf) { /* there is already a structure */
	/* fill new structure; currbuf points to last structure filled  */
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
      currbuf->physbuf=virt_to_bus(bufferpiece); /* PCI bus address */
      /* pre-increment page counter to avoid swapping */
       for (i=MAP_NR(bufferpiece);i<MAP_NR(bufferpiece+currbuf->fullsize);i++)
	 atomic_add(2,&mem_map[i].count); /* here was atomic_inc */

#ifdef bla1
      printk("table entry:\n fullsize: %d\n size: %d\n order: %d\n kernelad: %x\n physad: %x\n next: %x\n previous: %x\n",(int)currbuf->fullsize,(int)usedbytes,(int)page_order,(int)bufferpiece,(int)currbuf->physbuf,(int)currbuf->next,(int)currbuf->previous); 

      for (i=MAP_NR(bufferpiece);i<MAP_NR(bufferpiece+currbuf->fullsize);i++)
	printk("   page num: %06x count: %d\n",i,atomic_read(&mem_map[i].count)); 
#endif
      /* less work to do.. */
      bytes_to_get -= usedbytes;
    } else {
      /* could not get the large mem piece. try smaller ones */
      if (page_order>0) {
	page_order--; continue;
      } else {
	break; /* stop and clean up in case of problems */
      };
    };
  };
  if (bytes_to_get<=0) return 0; /* everything went fine.... */
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

/* interrupt handler für minor device 2 */
static void interrupt_handler(int irq, void *dev_id, struct pt_regs *regs) {
  static unsigned long ad,intreg;
  ad=dma_engine_base+DMA_ENGINE_INTCSR;

  intreg=inl(ad); /* interrupt status reg */
  if (!(intreg & S5933_INTCSR_int_asserted)) return; /* no int - but what ?? */
  
  if (intreg & S5933_INTCSR_write_complete) {
    /* check if dma should continue to run */
    if (dma_should_run) {
      outl(intreg | S5933_INTCSR_write_complete, ad); /* clear ints */
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
    return;
  };
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
	S5933_INTCSR_int_on_wr_complete ),ad);

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

#ifdef oldvers
/* do memory mapping for all allocated pages - does that work with real RAM??*/
static int do_nudaq_mmap(struct vm_area_struct *vma){
  struct dma_page_pointer *pgindex;
  unsigned long currentadr;
  int retval;
  /* walk through all mem pieces */
  pgindex = dma_main_pointer; /* start with first mem piece */
  currentadr=vma->vm_start;
  do {
    retval=remap_page_range(currentadr,pgindex->physbuf,
			    PAGE_SIZE <<(pgindex->order), 
			    vma->vm_page_prot);  /* is that ok with caching? */
    currentadr += PAGE_SIZE <<(pgindex->order);
    pgindex=pgindex->next;
  } while ((pgindex !=dma_main_pointer) && !retval);
  return retval; /* !=0 if remapping occured ? */
}
#endif

#ifndef oldvers 
/* vm_operations, die das eigentliche mmap über die nopage-methode vornehmen */
static unsigned long dmar_vm_nopage(struct vm_area_struct * area, unsigned long address, int write_access) {
  int adrmap; /* for debug, gives mempage */
  /* address relative to dma memory */
  unsigned long ofs = address - area->vm_start+area->vm_offset;
  unsigned long intofs;
  struct dma_page_pointer *pgindex;
  /* find right page */
  pgindex = dma_main_pointer; /* start with first mem piece */
  intofs=0; /* linear offset of current mempiece start */
  while (intofs+pgindex->fullsize<=ofs) {
    intofs +=pgindex->fullsize;
    pgindex=pgindex->next;
    if (pgindex == dma_main_pointer) return 0; /* ofs not mapped ->SIGBUS */
  }; /* pgindex contains now the relevant page index */
  
  /* do remap by hand */
  adrmap=MAP_NR(&pgindex->buffer[ofs-intofs]); /* physical page table index */
  atomic_inc(&mem_map[adrmap].count); /* use counter */
#ifdef bla1
  printk("nopage-Zugriff; adr=%x, phys=%x, adrmap=%x\n ofs=%x, intofs=%x, pgindex=%x\n",(int)address, (int)pgindex->physbuf,adrmap,(int)ofs,(int)intofs,(int)pgindex);
#endif
  return ((unsigned long) pgindex->buffer)+ofs-intofs;
}
static struct vm_operations_struct dmar_vm_ops = {
  NULL, /* open */ NULL, /* close */ NULL, /* unmap */ NULL, /* protect */
  NULL, /* sync */ NULL, /* advise */ dmar_vm_nopage, /* nopage */
  NULL, /* wppage */ NULL, /*swapout */ NULL /* swapin */};
#endif

static int dma_r_open(struct inode * inode, struct file * filp) {
  if (IOCARD_IN_USE != 0)
    return -EBUSY;
  
  /* Interrupt reservieren, vorher Steuervariablen setzen */
  dma_should_run=0; dma_int_use_counter=0;
  dma_main_pointer=NULL; next_to_use_ptr = NULL; /* avoid trouble */
  request_irq(irq_number,interrupt_handler, irq_flags, IOCARD_NAME, NULL);

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
  free_irq(irq_number, NULL);

  IOCARD_IN_USE &= ~1;
  MOD_DEC_USE_COUNT;
  return 0;
};

static int nu2err;
static int adr2;
static int dma_r_ioctl(struct inode * inode, struct file * file, unsigned int cmd, unsigned long value) {
  char * tmp;
  unsigned long w4,adr,tmpval;
  switch(cmd) {
  case NUDAQ_2_START_DMA:
    if (!dma_main_pointer){ nu2err=E_NU2_nopointer; return -EINVAL; }
    if (dma_should_run) { nu2err=E_NU2_already_running;  return -EINVAL;}

    start_dma_engine();
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
    return (int)tmp[(value>>8)&0xff];
    break;
  case 0x2002:
    /* read buffer mot offset */
    tmp=dma_main_pointer->buffer;
    return (int)tmp[(value>>8)&0xff];
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

  if (vma->vm_offset!=0) return -ENXIO; /* alignment error */
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
#endif  
  return 0;
}

static struct file_operations dma_r_fops = {
  NULL,           /* lseek for positioning read/write head */
  dma_r_read,      /* read */
  NULL,          /* write */
  NULL,		/* port_readdir */
  NULL,		/* port_poll */
  dma_r_ioctl,	/* port_ioctl */
  dma_r_mmap,		/* port_mmap */
  dma_r_open,	/* open code */
  NULL,           /* flush */
  dma_r_close,	/* release code */
  NULL,		/* fsync */
  NULL,
  NULL,
  NULL,
  NULL
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
  /* zum debuggen: */
  if (cmd==0x31415926) {
    /* mod counter zurücksetzen */
    while (MOD_IN_USE) MOD_DEC_USE_COUNT;
    MOD_INC_USE_COUNT;
    return 0x12345678;
  };
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
  NULL,           /* lseek for positioning read/write head */
  NULL,           /* read */
  NULL,           /* write */
  NULL,		/* port_readdir */
  NULL,		/* port_poll */
  ioctl_flat,	/* port_ioctl */
  NULL,		/* port_mmap */
  flat_open,	/* open code */
  NULL,           /* flush */
  flat_close,	/* release code */
  NULL,		/* fsync */
  NULL,
  NULL,
  NULL,
  NULL
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

/* open routines */
static struct file_operations all_fops = {
  NULL,		/* lseek */
  NULL,		/* read */
  NULL,		/* write */
  NULL,		/* readdir */
  NULL,		/* select */
  NULL,		/* ioctl */
  NULL,		/* mmap */
  nudaq_open,	/* iocard_open just a selector for the real open */
  NULL,           /* flush */
  nudaq_close,	/* release */
  NULL,     	/* fsync */
  NULL,
  NULL,
  NULL
};

#define DEBUG

int init_module(void) { 
  unsigned char pci_bus,pci_device_fn;
  unsigned int pci_ioaddr = 0;  /* pass-through base address */
  unsigned int pci_ioaddr2 = 0; /* AMCC control register */
  unsigned int pci_interrupt = 0; /* reserved interrupt */
  struct pci_dev *pdev;
  
  dma_main_pointer = NULL; /* no pointer exists at beginning */

  if (register_chrdev(IOCARD_MAJOR,IOCARD_NAME,&all_fops)) {
    printk("unable to get major %d for %s devs\n", IOCARD_MAJOR,IOCARD_NAME);
    return -EIO;
  };

  /* find NUDAQ PCI card */
  if (pcibios_present()) {
    int pci_index;
    for (pci_index=0;pci_index<8;pci_index++) {

      /* suche nudaq device (genauer: AMCC chip ) */
      if (pcibios_find_device(0x10e8, 0x80d8, pci_index, &pci_bus, &pci_device_fn) != 0 )
	break;
      
      /* hat geraet gefunden; lese IO-Basisadresse ein */
#ifdef DEBUG
      printk(" pci-bus: %d, pci_device: %d\n",pci_bus, pci_device_fn);
#endif

      pdev = pci_find_slot(pci_bus, pci_device_fn);
      pci_ioaddr=pdev->base_address[1];
      pci_ioaddr2=pdev->base_address[0];
      pci_interrupt=pdev->irq;

      
#ifdef DEBUG
      printk("rohe io-adresse: %x\n",pci_ioaddr);
#endif
     
      pci_ioaddr &= PCI_BASE_ADDRESS_IO_MASK;
      pci_ioaddr2 &= PCI_BASE_ADDRESS_IO_MASK;

    };
    
    
    
    /* kein device gefunden */
    if (pci_ioaddr == 0) {
      printk("nudaq device not found\n");
      unregister_chrdev(IOCARD_MAJOR,IOCARD_NAME);
      return -ENXIO;
    };

    
    /* sollte gültige Adresse haben */
    iocard_base = pci_ioaddr;
    dma_engine_base = pci_ioaddr2;
    printk("NuDAQ-Treiber installiert; Basisadresse: %x, DMA-engine: %x\n",iocard_base,dma_engine_base);
    irq_number = pci_interrupt;
  } else {
    unregister_chrdev(IOCARD_MAJOR,IOCARD_NAME);
    return -ENODEV; /* kein PCIBIOS */
  };
  /* check if IO regions are ok (is that necessary with a PCI device ??) */
  if (check_region(iocard_base,IOCARD_SPACE+1) | 
      check_region(dma_engine_base,DMA_ENGINE_SPACE+1)) {
    printk("IO regions already occupied. Should not happen?!?!\n");
    unregister_chrdev(IOCARD_MAJOR,IOCARD_NAME);
    return -EIO;
  } 

  /* Ask for IO region */
  request_region(iocard_base,IOCARD_SPACE+1, IOCARD_NAME);
  request_region(dma_engine_base,DMA_ENGINE_SPACE+1, IOCARD_NAME);

  return 0;
}


void cleanup_module(void) {
  /* release IO region */
  release_region(iocard_base,IOCARD_SPACE+1);
  release_region(dma_engine_base,DMA_ENGINE_SPACE+1);


  unregister_chrdev(IOCARD_MAJOR,IOCARD_NAME);
}








