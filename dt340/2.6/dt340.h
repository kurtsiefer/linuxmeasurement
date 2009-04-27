/*
 * file: dt340.h
 *
 * Contains definitions for dt340 driver.
 *
 * Access to the card is managed via ioctl commands to the driver module.
 * After having opened the device driver file, a typical read od a register
 * looks like:
 *
 *    returnvalue = ioctl(handle, IO_READ | register_name);
 *
 * A typical write-to-register looks like
 *    ioctl(handle, IO_WRITE | register_name, value);
 * 
 * This file contains macros for the register_names in the DT340 card as
 * well as some content helper macros.
 *
 * For a more detailled description of the driver capabilities, please refer
 * to the driver source.
 *
 * Copyright (C) 2002 Christian Kurtsiefer       (first version)
 */ 

/* general ioctl definitions */
#define IO_WRITE 0x40000000
#define IO_READ  0x80000000


/* DT340 register definitions */
#define DT340_GEN_CONTROL_0 0x0
#define DT340_GEN_CONTROL_1 0x1000
#define DT340_IRQ_PENDING 0x2000
#define DT340_DIO_AB 0xc000
#define DT340_DIO_CD 0xd000

#define DT340_COUNTER_0 0x8000
#define DT340_COUNTER_1 0x8004
#define DT340_COUNTER_2 0x8008
#define DT340_COUNTER_3 0x800c
#define DT340_COUNTER_4 0x9000
#define DT340_COUNTER_5 0x9004
#define DT340_COUNTER_6 0x9008
#define DT340_COUNTER_7 0x900c
#define DT340_CNT0 0x8000 /* shorter versions */
#define DT340_CNT1 0x8004
#define DT340_CNT2 0x8008
#define DT340_CNT3 0x800c
#define DT340_CNT4 0x9000
#define DT340_CNT5 0x9004
#define DT340_CNT6 0x9008
#define DT340_CNT7 0x900c

#define DT340_U_PULSE_0 0x8010
#define DT340_U_PULSE_1 0x8014
#define DT340_U_PULSE_2 0x8018
#define DT340_U_PULSE_3 0x801c
#define DT340_U_PULSE_4 0x9010
#define DT340_U_PULSE_5 0x9014
#define DT340_U_PULSE_6 0x9018
#define DT340_U_PULSE_7 0x901c

#define DT340_U_CONTROL_0 0x8020
#define DT340_U_CONTROL_1 0x8024
#define DT340_U_CONTROL_2 0x8028
#define DT340_U_CONTROL_3 0x802c
#define DT340_U_CONTROL_4 0x9020
#define DT340_U_CONTROL_5 0x9024
#define DT340_U_CONTROL_6 0x9028
#define DT340_U_CONTROL_7 0x902c

#define DT340_I_TIMER_0_LS 0x8048
#define DT340_I_TIMER_0_MS 0x804c
#define DT340_I_TIMER_1_LS 0x8040
#define DT340_I_TIMER_1_MS 0x8044
#define DT340_I_TIMER_2_LS 0x9048
#define DT340_I_TIMER_2_MS 0x904c
#define DT340_I_TIMER_3_LS 0x9040
#define DT340_I_TIMER_3_MS 0x9044
#define DT340_ITIM0_LS 0x8048 /* shorter versions */
#define DT340_ITIM0_MS 0x804c
#define DT340_ITIM1_LS 0x8040
#define DT340_ITIM1_MS 0x8044
#define DT340_ITIM2_LS 0x9048
#define DT340_ITIM2_MS 0x904c
#define DT340_ITIM3_LS 0x9040
#define DT340_ITIM3_MS 0x9044

#define DT340_STATUS_CHIP_0 0x8030
#define DT340_STATUS_CHIP_1 0x9030


/* Driver register definitions */
#define DT340DRV_IRQ_COUNT_CTRL 0x10014   /* bitmask enables irq counters */
#define DT340DRV_IRQ_NOTIFY_CTRL 0x10015  /* bitmask which irq causes sigio */
#define DT340DRV_IRQ_LAST_PENDING 0x10016 /* last iqr pending reg content */
#define DT340DRV_ACCESS_MODE 0x10017      /* register containing access mode */

/* irq counters */
#define DT340DRV_IRQCNT_DIO0 0x10000
#define DT340DRV_IRQCNT_DIO1 0x10001
#define DT340DRV_IRQCNT_DIO2 0x10002
#define DT340DRV_IRQCNT_DIO3 0x10003
#define DT340DRV_IRQCNT_DIO4 0x10004
#define DT340DRV_IRQCNT_DIO5 0x10005
#define DT340DRV_IRQCNT_DIO6 0x10006
#define DT340DRV_IRQCNT_DIO7 0x10007
#define DT340DRV_IRQCNT_CNT0 0x10008
#define DT340DRV_IRQCNT_CNT1 0x10009
#define DT340DRV_IRQCNT_CNT2 0x1000a
#define DT340DRV_IRQCNT_CNT3 0x1000b
#define DT340DRV_IRQCNT_CNT4 0x1000c
#define DT340DRV_IRQCNT_CNT5 0x1000d
#define DT340DRV_IRQCNT_CNT6 0x1000e
#define DT340DRV_IRQCNT_CNT7 0x1000f
#define DT340DRV_IRQCNT_ITIM0 0x10010
#define DT340DRV_IRQCNT_ITIM1 0x10011
#define DT340DRV_IRQCNT_ITIM2 0x10012
#define DT340DRV_IRQCNT_ITIM3 0x10013

/* content helpers for  DT340_GEN_CONTROL_0 */
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

/* content helpers for  DT340_GEN_CONTROL_1 */
#define CNT0_IrqEnable  0x00001
#define CNT1_IrqEnable  0x00002
#define CNT2_IrqEnable  0x00004
#define CNT3_IrqEnable  0x00008
#define CNT4_IrqEnable  0x00010
#define CNT5_IrqEnable  0x00020
#define CNT6_IrqEnable  0x00040
#define CNT7_IrqEnable  0x00080
#define ITIM0_IrqEnable  0x00100
#define ITIM1_IrqEnable  0x00200
#define ITIM2_IrqEnable  0x00400
#define ITIM3_IrqEnable  0x00800
#define CHIP0_Reset         0x04000
#define CHIP1_Reset         0x08000
#define ITIM0_CntEnable  0x10000
#define ITIM1_CntEnable  0x20000
#define ITIM2_CntEnable  0x40000
#define ITIM3_CntEnable  0x80000

/* content helpers for  DT340_IRQ_PENDING interpretation
   (Also indicates drier counters) */
#define DIO0_IrqPending 0x00001
#define DIO1_IrqPending 0x00002
#define DIO2_IrqPending 0x00004
#define DIO3_IrqPending 0x00008
#define DIO4_IrqPending 0x00010
#define DIO5_IrqPending 0x00020
#define DIO6_IrqPending 0x00040
#define DIO7_IrqPending 0x00080
#define CNT0_IrqPending 0x00100
#define CNT1_IrqPending 0x00200
#define CNT2_IrqPending 0x00400
#define CNT3_IrqPending 0x00800
#define CNT4_IrqPending 0x01000
#define CNT5_IrqPending 0x02000
#define CNT6_IrqPending 0x04000
#define CNT7_IrqPending 0x08000
#define ITIM0_IrqPending 0x10000
#define ITIM1_IrqPending 0x20000
#define ITIM2_IrqPending 0x40000
#define ITIM3_IrqPending 0x80000

/* content helpers for setting DT340_U_CONTROL_x */
#define OneShot_Trigger_Cmd  0x80
#define Output_Pol_Low 0x00
#define Output_Pol_High 0x40
#define Nonretrig_Oneshot_Mode 0x00
#define Retrig_Oneshot_Mode 0x10
#define Continuous_Mode 0x20
#define Gate_Low 0x00
#define Gate_High 0x04
#define Gate_Extern 0x08
#define Gate_Extern_Invert 0x0c
#define Internal_Clock 0x00
#define External_Clock 0x01
#define Cascaded_Clock 0x02

/* content helpers for  interpreting DT340_STATUS_CHIP_0/1 content */
#define CNT0_Oneshot_Trig_Enable 0x1
#define CNT1_Oneshot_Trig_Enable 0x2
#define CNT2_Oneshot_Trig_Enable 0x4
#define CNT3_Oneshot_Trig_Enable 0x8
#define CNT4_Oneshot_Trig_Enable 0x1
#define CNT5_Oneshot_Trig_Enable 0x2
#define CNT6_Oneshot_Trig_Enable 0x4
#define CNT7_Oneshot_Trig_Enable 0x8

/* content helper for DT340DRV_ACCESS_MODE register */
#define DIO_exclusive 0x8
#define DIO_shared 0x4
#define DIO_readonly 0
#define CT_exclusive 0x2
#define CT_shared 0x1
#define CT_readonly 0








