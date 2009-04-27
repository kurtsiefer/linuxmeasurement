/* definitions for dt330 driver */


/* general ioctl definitions */
#define IO_WRITE 0x40000000
#define IO_READ  0x80000000


/* DT330 register definitions */
#define DT330_STATUS_REG 0x1000
#define DT330_CONTROL_0_ADR 0x0000
#define DT330_DIO_PORT_AB 0x2000
#define DT330_DIO_PORT_CD 0x3000
#define DT330_TRIMPOT_REG 0x4000
#define DT330_DA0 0x8010
#define DT330_DA1 0x8020
#define DT330_DA2 0x8040
#define DT330_DA3 0x8080
#define DT330_DA4 0x8100
#define DT330_DA5 0x8200
#define DT330_DA6 0x8400
#define DT330_DA7 0x8800


/* Driver register definitions */
#define DT330DRV_IRQ_COUNT_CTRL 0x10009
#define DT330DRV_IRQ_NOTIFY_CTRL 0x1000a
#define DT330DRV_IRQ_LAST_PENDING 0x1000b

/* irq counters */
#define DT330DRV_IRQCNT_0 0x10000
#define DT330DRV_IRQCNT_1 0x10001
#define DT330DRV_IRQCNT_2 0x10002
#define DT330DRV_IRQCNT_3 0x10003
#define DT330DRV_IRQCNT_4 0x10004
#define DT330DRV_IRQCNT_5 0x10005
#define DT330DRV_IRQCNT_6 0x10006
#define DT330DRV_IRQCNT_7 0x10007

#define DT330DRV_IRQCNT_DIO0 0x10000 /* more meaningful names */
#define DT330DRV_IRQCNT_DIO1 0x10001
#define DT330DRV_IRQCNT_DIO2 0x10002
#define DT330DRV_IRQCNT_DIO3 0x10003
#define DT330DRV_IRQCNT_DIO4 0x10004
#define DT330DRV_IRQCNT_DIO5 0x10005
#define DT330DRV_IRQCNT_DIO6 0x10006
#define DT330DRV_IRQCNT_DIO7 0x10007

/* content helpers for  DT330_CONTROL_0*/
#define DIO_A_OutputEnable 0x001
#define DIO_B_OutputEnable 0x002
#define DIO_C_OutputEnable 0x004
#define DIO_D_OutputEnable 0x008
#define DIO_D_0_IrqEnable  0x010
#define DIO_D_1_IrqEnable  0x020
#define DIO_D_2_IrqEnable  0x040
#define DIO_D_3_IrqEnable  0x080
#define DIO_D_4_IrqEnable  0x100
#define DIO_D_5_IrqEnable  0x200
#define DIO_D_6_IrqEnable  0x400
#define DIO_D_7_IrqEnable  0x800
#define DAC_0_SAMPLE      0x010000
#define DAC_1_SAMPLE      0x020000
#define DAC_2_SAMPLE      0x040000
#define DAC_3_SAMPLE      0x080000
#define DAC_4_SAMPLE      0x100000
#define DAC_5_SAMPLE      0x200000
#define DAC_6_SAMPLE      0x400000
#define DAC_7_SAMPLE      0x800000

/* content helpers for  DT330_IRQ_PENDING interpretation
   (Also indicates driver counters and notify_enable) */
#define DIO0_IrqPending 0x00001
#define DIO1_IrqPending 0x00002
#define DIO2_IrqPending 0x00004
#define DIO3_IrqPending 0x00008
#define DIO4_IrqPending 0x00010
#define DIO5_IrqPending 0x00020
#define DIO6_IrqPending 0x00040
#define DIO7_IrqPending 0x00080

#define DAC_SHIFT_IN_PROG 0x10000









