/* command definitions for the DT302 device driver 

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

 HISTORY:
   Christian Kurtsiefer first version 28.09.02
   modified to allow card identification, separated CGL
   setup from arm command   24.0.06chk

 TO DO: needs documentation */

/* ioctl commands for minor device 0 (flat access) */

/* ioctl command definitions for minor device 1 (advanced access) */

/* general card control */
#define IDENTIFY_DTAX_CARD 0x6000

/* analog output mode */
/* writing values into DAC transfer registers, takes 1 argument (dac value) */
#define SET_DAC_0 0x2000
#define SET_DAC_1 0x2001
/* test transfer_status ; no arg, returns status*/
#define DAC_TRANSFER_ACTIVE 0x2008
#define WAIT_DAC_TRANSFER_DONE 0x2009
/* load DAC from transfer register no arg, returns 0 on success*/
#define LOAD_DAC  0x2004
#define LOAD_DAC_0 0x2005
#define LOAD_DAC_1 0x2006
#define LOAD_DAC_0_AND_1 0x2007
/* write to transfer reg and load takes 1 arg (dac value), returns 0 on succ*/
#define SET_AND_LOAD_DAC_0 0x2002
#define SET_AND_LOAD_DAC_1 0x2003

/* digital output controls */
#define DIO_A_OUTPUT_ENABLE 0x1000
#define DIO_A_OUTPUT_DISABLE 0x1001
#define DIO_B_OUTPUT_ENABLE 0x1002
#define DIO_B_OUTPUT_DISABLE 0x1003
#define DIO_C_OUTPUT_ENABLE 0x1004
#define DIO_C_OUTPUT_DISABLE 0x1005
#define DIO_AB_IN 0x1006
#define DIO_C_IN 0x1007
/* takes one arg, does re-read for return */
#define DIO_AB_OUT 0x1008
#define DIO_C_OUT  0x1009


/* user counter controls */
#define SET_USER_PERIODE_0 0x3000
#define SET_USER_PERIODE_1 0x3001
#define SET_USER_PERIODE_2 0x3002
#define SET_USER_PERIODE_3 0x3003
#define SET_USER_PULSE_0 0x3004
#define SET_USER_PULSE_1 0x3005
#define SET_USER_PULSE_2 0x3006
#define SET_USER_PULSE_3 0x3007
#define SET_USER_CONTROL_0 0x3008
#define SET_USER_CONTROL_1 0x3009
#define SET_USER_CONTROL_2 0x300a
#define SET_USER_CONTROL_3 0x300b
#define GET_USER_PERIODE_0 0x3100
#define GET_USER_PERIODE_1 0x3101
#define GET_USER_PERIODE_2 0x3102
#define GET_USER_PERIODE_3 0x3103
#define GET_USER_PULSE_0   0x3104
#define GET_USER_PULSE_1   0x3105
#define GET_USER_PULSE_2   0x3106
#define GET_USER_PULSE_3   0x3107
#define GET_USER_CONTROL_0 0x3108
#define GET_USER_CONTROL_1 0x3109
#define GET_USER_CONTROL_2 0x310a
#define GET_USER_CONTROL_3 0x310b

#define GET_USER_STATUS    0x310c
#define SET_AD_SAMPLE_PERIODE 0x300d
#define SET_AD_TRIG_PERIODE 0x300e
#define ASSERT_TIMER_RESET 0x3010
#define DEASSERT_TIMER_RESET 0x3011
#define TIMER_RESET 0x3012

/* definitions to compose timer control word */
#define SELECT_INTERNAL_CLOCK 0x0
#define SELECT_EXTERNAL_CLOCK 0x1
#define SELECT_CASCADE_CLOCK 0x2
#define USER_GATE_LOW 0x0
#define USER_GATE_HIGH 0x4
#define USER_GATE_EXTERN 0x8
#define USER_GATE_EXT_INVERTED 0xc
#define NONRETRIG_ONESHOT_MODE 0x0
#define RETRIG_ONESHOT_MODE 0x10
#define CONTINUOUS_INCR_MODE 0x20
#define OUTPUT_LOW_ACTIVE 0x0
#define OUTPUT_HIGH_ACTIVE 0x40
#define ONESHOT_TRIGGER_ENABLE 0x80

/* definitions to analyze the counter status word */
#define CNT_0_IS_TRIG_ENABLED 0x1
#define CNT_1_IS_TRIG_ENABLED 0x2
#define CNT_2_IS_TRIG_ENABLED 0x4
#define CNT_3_IS_TRIG_ENABLED 0x8


/* analog input commands */
#define RESET_AD_UNIT 0x400f
/* takes 1 parameter */
#define SELECT_AD_CONFIGURATION 0x4001 
/* macros to compose argument thereof */
#define CGL_COUNT_SHIFT 24
#define IGNORE_AD_ERRORS 0x800000
#define TRIGGERED_SCAN_MODE 0x40000
#define CONTINUOUS_SCAN_MODE 0
#define EXTERNAL_INITIAL_TRIGGER 0x20000
#define SOFTWARE_INITIAL_TRIGGER 0
#define EXTERNAL_RETRIGGER_AD 0x10000
#define INTERNAL_RETRIGGER_AD 0
#define TRIG_EXTERNAL_RISING_EDGE 0x2000
#define TRIG_EXTERNAL_FALLING_EDGE 0
#define EXTERNAL_AD_SAMPLE_CLK 0x800
#define INTERNAL_AD_SAMPLE_CLK 0
#define AD_UNIPOLAR_MODE 0x400
#define AD_BIPOLAR_MODE 0
#define AD_SINGLE_ENDED 0x200
#define AD_DIFFERENTIAL_ENDED 0

#define LOAD_PCI_MASTER 0x4002
#define PUSH_CGL_FIFO   0x4003
/* macros to construct argument */
#define DISCARD_SAMPLE 0x100
#define SELECT_GAIN_1 0
#define SELECT_GAIN_2 0x40
#define SELECT_GAIN_4 0x80
#define SELECT_GAIN_8 0xc0
#define DIGITAL_CHANNEL 0x20

#define ARM_CONVERSION 0x4004
#define SOFTWARE_TRIGGER 0x4005
#define STOP_SCAN 0x4006
#define CONTINUE_SCAN 0x4007
#define DISARM_CONVERSION 0x4008
#define SET_AD_STATUS 0x4009
#define GET_AD_STATUS 0x400a
#define PRELOAD_CGL 0x4011

/* macros to analyze and reset status bits */
#define AD_OVERSAMPLE_ERROR 0x2000000
#define INPUT_FIFO_OVERFLOW 0x1000000
#define HOST_BLOCK_OVERFLOW 0x0800000
#define HOST_TRANSFER_DONE  0x0200000
#define HOST_BLOCK_DONE     0x0100000
#define POST_TRIG_CONV_SENT 0x0020000

/* query DMA buffer pointer */
#define NEXT_INDEX_TO_WRITE 0x400b
#define HOW_MANY_FRESH_VALUES 0x400c
/* query next value */
#define GET_NEXT_VALUE 0x400d
#define GET_ALREADY_TRANSFERRED_VALUES 0x400e
#define PREPARE_CGL_FIFO 0x4010

/* dirty hack: command to get jiffies */
#define GET_JIFFIES 0x5000

/*****************************************************/
/* here the definitions used driver-internally start */

/* register adresses */
#define GEN_CONTROL_REG_0  0x0000
#define GEN_CONTROL_REG_1  0x1000
#define GEN_STATUS_REG     0x2000
#define TRIMPOT_REG        0x3000
#define PCI_BUSMASTER_REG  0x4000
#define DIGITAL_PORTS_AB   0x8000
#define CGL_FIFO_REG       0xc000
#define DAC_0_REG          0xe000
#define DAC_1_REG          0xf000
#define USER_PERIODE_REG_0 0xa000
#define USER_PERIODE_REG_1 0xa004
#define USER_PERIODE_REG_2 0xa008
#define USER_PERIODE_REG_3 0xa00c
#define USER_PULSE_REG_0   0xa010
#define USER_PULSE_REG_1   0xa014
#define USER_PULSE_REG_2   0xa018
#define USER_PULSE_REG_3   0xa01c
#define USER_CONTROL_REG_0 0xa020
#define USER_CONTROL_REG_1 0xa024
#define USER_CONTROL_REG_2 0xa028
#define USER_CONTROL_REG_3 0xa02c
#define USER_STATUS_REG    0xa030
#define AD_SAMPLE_PERIODE_LS 0xa040
#define AD_SAMPLE_PERIODE_MS 0xa044
#define TRIGGERED_SCAN_PERIODE_LS 0xa050
#define TRIGGERED_SCAN_PERIODE_MS 0xa054

/* useful masks */
#define DA_SHIFT_IN_PROGRESS 0x4000000
#define DAC_SAMPLE_CLK_MASK 0x3000
#define DAC_SAMPLE_CLK_SHIFT 12

#define DIO_A_OUT_MASK 0x1
#define DIO_B_OUT_MASK 0x2
#define DIO_C_OUT_MASK 0x80000
#define DIO_AB_VALUE_MASK 0xffff
#define DIO_C_VALUE_MASK_0 0x3f
#define DIO_C_VALUE_SHIFT 16
#define DIO_C_VALUE_MASK_1 (DIO_C_VALUE_MASK_0 << DIO_C_VALUE_SHIFT)
#define BITMASK_WORD 0xffff
#define BITMASK_CHAR 0xff
#define CNTR_INDX_MASK 0x3
#define CNTR_INDX_SHIFT 2

#define TIMER_RESET_MASK 1
#define ADUNIT_RESET_MASK_1 0x660
#define ADUNIT_RESET_MASK_2 0x000
#define ADUNIT_RESET_MASK_3 0x060
#define AD_CONFIG_MASK_1 0xff872e00
#define CGL_ENTRY_MASK 0x1ef
#define CHANNEL_PARAM_REGISTER_LOAD 0x0080
#define CLEAR_GENERAL_STATUS_BITS 0x3b20000
#define AD_SYSTEM_ARM_BIT 0x400
#define AD_SYSTEM_SCANSTOP_BIT 0x200
#define AD_SYSTEM_TRIG_BIT 0x100
#define SET_STATUS_MASK 0x3b20000
#define PCI_BUSMASTER_OFFSET_MASK 0x3ffff
#define WORDS_IN_BUFFER 0x20000












