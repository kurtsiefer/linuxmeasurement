/*
  Stepper Motor Controller SM32 v1.03 :

  SM32 interface  v1.03
  Tested with Turbo C 2.01

  (c)2001 OWIS GmbH, 79219 Staufen i.Br, Germany
*/

#include <string.h>  /* need stricmp */

#include "sm32.h"

/*----------------------------------------------------------------------*/
/* system specific ; might need changes for compiler */

typedef void           VOID0 ;
typedef unsigned char  BYTE8 ;
typedef unsigned char  BOOL8 ;
typedef unsigned short WORD16;
typedef long           LONG32;

/* TC 2.01 */
#include <dos.h>

#define BYTEPORT_IN(a)    inp(a)
#define BYTEPORT_OUT(a,b) outp(a,b)
/**/

/*#########################################################################*/
/*  CONTENTS:
 *  0.0: Requirements.
 *
 *  1.0: Communication basics: Dual Ported RAM with tChannel structure array.
 *  1.1: Code: Field Offsets. GetByte, PutByte, GetLong, PutLong.
 *  1.2: Internal structure of tMotor variables.
 *  1.3: Code: SM32Post,SM32Request,SM32Replied,SM32Fetch,SM32Write,SM32Read.
 *
 *  2.0: How to find the SM32.
 *  2.1: Code for PCI bus low level access
 *  2.2: Code: SM32Enum, SM32Init, SM32Done
 *
 *  3.0: Special functions: SM32CmdToName, SM32NameToCmd, SM32CmdFlags,
 *  3.1: Special functions: SM32GetTimeout, SM32SetTimeout
 */

/*#########################################################################*/
/*> 0.0: Requirements
 *
 * Communication with the SM32 is based on
 * - I/O port access to perform data transfers,
 * - BIOS calls to find the SM32.
 */






/*#########################################################################*/
/*> 1.0: Communication basics: Dual Ported RAM with tChannel structure.
 *
 * The SM32 has a Dual Ported RAM which contains three tChannel records
 * (one for each motor) of the structure shown here:
 *
 *   type tChannel = record
 *                     Flags :byte;    {R}
 *                     fill  :byte;    {-}
 *                     WCmd  :byte;    {W}
 *                     RCmd  :byte;    {R/W}
 *                     Data  :longint; {R/W}
 *                   end;
 *
 *   type tSM32DPR = array[0..2] of tChannel;
 *
 * Flags are:
 */

#define  cWFlag   1  /*if( Flags  &  cWFlag )   then command not processed yet*/
#define  cRFlag   2  /*if( Flags  &  cRFlag )   then data not available yet   */

/*
 * WCmd  is used to send data to the SM32:
 *       After putting command data into Data one writes the
 *       command number into WCmd which sets the bit cWFlag in Flags,
 *       telling the SM32 that a command has to be processed.
 *
 *       When the SM32 is done with the command it resets the cWFlag bit,
 *       telling the PC that another command can be sent.
 *
 * RCmd  is used to request data from the SM32:
 *       Putting the command number of the desired data into RCmd
 *       sets the bit cRFlag in Flags, telling the SM32 that data
 *       should be deliverd.
 *       The SM32 places the data into Data and resets the cRFlag bit,
 *       telling the PC that the requested data are available.
 *
 * Data  are actually two variables, one for writing and one for reading.
 *       Data written by the PC can only be readen by the SM32, and vice versa.
 */

/*
 * On the PC's side, the DPR is mapped into i/o space, therefor
 * we cannot simply define something like  tSM32DPR SM32DPR;
 * Instead, chapter 1.1 defines some low level code to access the
 * SM32DPR fields through i/o ports.
 *
 * Chapter 1.2 defines the internal structure of tMotor variables,
 * which is required for..
 *
 * Chapter 1.3: here, the functions
 * SM32Post, SM32Request, SM32Replied, SM32Fetch, SM32Write, SM32Read
 * are defined.
 *
 * Chapters 2.x show how to find the SM32 and define SM32Enum,SM32Init,SM32Done
 */

/*#########################################################################*/
/*> 1.1: Code: Field Offsets. GetByte, PutByte, GetLong, PutLong. */

#define oFlags   0        /* offset of tChannel.Cmd  */
#define oWCmd    2        /* offset of tChannel.WCmd */
#define oRCmd    3        /* offset of tChannel.RCmd */
#define oData    4        /* offset of tChannel.Data */

#define CHANNELSIZE   8   /* sizeof(tChannel)        */

static WORD16  /* offsets of each channel within SM32DPR: */
        oChannel[3] = {
          0,
          CHANNELSIZE,
          CHANNELSIZE*2
        };
                          /* The only thing missing here is 'Baseport',
                           * the port address where SM32DPR starts.
                           * More about this in Chapter 2.x
                           */

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Put a byte into SM32DPR */

#define PutByte( Baseport, Fieldoffset, Data ) \
   BYTEPORT_OUT( Baseport+ Fieldoffset, Data )

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Get a byte from SM32DPR */

#define GetByte( Baseport, Fieldoffset ) \
    BYTEPORT_IN( Baseport+ Fieldoffset )

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Put a longint into SM32DPR */

static void PutLong( WORD16 Baseport, WORD16 Fieldoffset, LONG32 Data )
{
  PutByte( Baseport, Fieldoffset  , ((BYTE8*)&Data)[0] );
  PutByte( Baseport, Fieldoffset+1, ((BYTE8*)&Data)[1] );
  PutByte( Baseport, Fieldoffset+2, ((BYTE8*)&Data)[2] );
  PutByte( Baseport, Fieldoffset+3, ((BYTE8*)&Data)[3] );
}

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Get a longint from SM32DPR */

static LONG32 GetLong( WORD16 Baseport, WORD16 Fieldoffset )
{
LONG32 Data;

  ((BYTE8*)&Data)[0] = GetByte( Baseport, Fieldoffset   );
  ((BYTE8*)&Data)[1] = GetByte( Baseport, Fieldoffset+1 );
  ((BYTE8*)&Data)[2] = GetByte( Baseport, Fieldoffset+2 );
  ((BYTE8*)&Data)[3] = GetByte( Baseport, Fieldoffset+3 );

  return Data;
}

/*#########################################################################*/
/*> 1.2: Internal structure of tMotor variables.
 *
 * Seen from the outside tMotor is a simple 32bit value, but
 * internally it is devided into three fields:
 *
 *   Baseport: SM32Init() stores here where the SM32 can be found in i/o space.
 *             Baseport can have two special values:
 *               mhSoftwareTest tells that no real sM32 should be accessed,
 *               mhUnknown      tells that the variable has been invalidated
 *                              by SM32Done.
 *
 *   Number: holds the motor number.
 *
 *   RCmd: stores the latest requested read command; this is used to check
 *         if the data delivered by the SM32 are of the right type.
 *
 * The M_... definitions will be used to typecast tMotor *M variables for
 * internal field access:
 */

#define M_BASEPORT ((WORD16*) M) [0]
#define M_NUMBER   ((BYTE8 *) M) [2]
#define M_RCMD     ((BYTE8 *) M) [3]

#define mhSoftwareTest  0xCDEF
#define mhUnknown            0

/*#########################################################################*/
/* 1.3: Code: SM32Post,SM32Request,SM32Replied,SM32Fetch,SM32Write,SM32Read. */

/* Notes about safety checks:
 *   Baseport < $100 would be in system area and hence is illegal.
 *   Baseport must be a multiple of 4.
 */

/*-------------------------------------------------------------------------*/
long _CALLSTYLE_ SM32Post( tSM32Motor* M, long Cmd, long Data )
{
WORD16 MyOffset;

  if( M_BASEPORT==mhSoftwareTest ) return mcrOk;

  /* M valid ? */
  if(    (M_BASEPORT < 0x100)
      || (M_BASEPORT & 3    )
      || (((BYTE8)(M_NUMBER-1)) >2) )                 return mcrNotInitialised;

  /* Calc channel offset  */
  MyOffset = oChannel[ M_NUMBER-1 ];

  /* Previous command not processed yet? */
  if( GetByte(M_BASEPORT, MyOffset+oFlags) & cWFlag ) return mcrBusy;

  /* Put Data and Command. */
  PutLong( M_BASEPORT, MyOffset+oData, Data );
  PutByte( M_BASEPORT, MyOffset+oWCmd, Cmd  );        return mcrOk;
}

/*-------------------------------------------------------------------------*/
long _CALLSTYLE_ SM32Request( tSM32Motor* M, long Cmd )
{
WORD16 MyOffset;

  M_RCMD = (BYTE8)Cmd;

  if( M_BASEPORT==mhSoftwareTest )             return mcrOk;

  /* M valid ? */
  if(    (M_BASEPORT < 0x100)
      || (M_BASEPORT & 3    )
      || (((BYTE8)(M_NUMBER-1)) > 2) )         return mcrNotInitialised;

  /* Calc offset of SM32DPR.WData[(M.Number-1)] */
  MyOffset = oChannel[ M_NUMBER-1 ];

  /* Put Request */
  PutByte( M_BASEPORT, MyOffset+oRCmd, Cmd );  return mcrOk;
}

/*-------------------------------------------------------------------------*/
long _CALLSTYLE_ SM32Replied( tSM32Motor* M )
{
BYTE8 MyOffset;

  if( M_RCMD==mcNone )                                return mcrNoRequest;

  if( M_BASEPORT==mhSoftwareTest )                    return mcrOk;

  if(    (M_BASEPORT < 0x100)
      || (M_BASEPORT & 3    )
      || (((BYTE8)(M_NUMBER-1)) > 2) )                return mcrNotInitialised;

  /* Calc channel offset  */
  MyOffset = oChannel[ M_NUMBER-1 ];

  /* Replied := (RData.Cmd=0); */
  if( GetByte(M_BASEPORT,MyOffset+oFlags ) & cRFlag ) return mcrBusy;

                                                      return mcrOk;
}

/*-------------------------------------------------------------------------*/
long _CALLSTYLE_ SM32Fetch( tSM32Motor* M, long* Data )
{
BYTE8 MyOffset;
long result;

  if( M_RCMD==mcNone )                               return mcrNoRequest;

  if( M_BASEPORT==mhSoftwareTest ) {
    if( M_RCMD==mcError )  *Data = meNotFound;
    else                   *Data = 0;

    M_RCMD=mcNone;                                   return mcrOk;
  };

  /* Baseport and Motor number valid ? */
  if(    (M_BASEPORT < 0x100)
      || (M_BASEPORT & 3    )
      || (((BYTE8)(M_NUMBER-1)) > 2) )               return mcrNotInitialised;

  /* Calc channel offset  */
  MyOffset = oChannel[ M_NUMBER-1 ];

  /* Replied ? */
  if( GetByte(M_BASEPORT, MyOffset+oFlags) &cRFlag ) return mcrBusy;

  /* Fetch Data */
  result = mcrOk;
  *Data = GetLong( M_BASEPORT, MyOffset+oData );

  /* Check if data delivered by the SM32 are of the same type as requested,
     otherwise another program on the same computer must have requested
     different data in the mean time.
   */
  if( M_RCMD != GetByte( M_BASEPORT, MyOffset+oRCmd ) )
    result = mcrInterference;

  M_RCMD = mcNone;

  return result;
}

/*#########################################################################*/
/* Unsuccessful SM32Post/SM32Request/SM32Fetch take about 0.1 microseconds */

/* Max time SM32Write/SM32Read wait for the SM32 to react*/
static LONG32 SM32Timeout = 10000000; /* *0.1 usec */

/*-------------------------------------------------------------------------*/
long _CALLSTYLE_ SM32Write( tSM32Motor* M, long Cmd, long Data )
{
long t, r;

  for( t=SM32Timeout; t>=0; t-- ) {
    r = SM32Post( M, Cmd, Data );
    if( r!=mcrBusy ) break;        /* mcrOk or error */
    /* mcrBusy. wait. */
    if( t==0 )  return mcrTimeout;
  };

  return r;
}

/*-------------------------------------------------------------------------*/
long _CALLSTYLE_ SM32Read( tSM32Motor* M, long Cmd, long* Data )
{
long r, t;

  for( t=SM32Timeout ; t>=0 ; t-- ) {
    r = SM32Request( M, Cmd );
    if( r==mcrOk   ) break;    /* ok, go to Fetch data */
    if( r!=mcrBusy ) return r; /* error */
    /* mcrBusy. wait. */
    if( t==0 )  return mcrTimeout;
  };

  for( t=SM32Timeout ; t>=0 ; t-- ) {
    r = SM32Fetch( M, Data );
    if( r==mcrOk   ) break;    /* ok. */
    if( r!=mcrBusy ) return r; /* error */
    /* mcrBusy. wait. */
    if( t==0 )  return mcrTimeout;
  };

  return mcrOk;
}

/*#########################################################################*/
/*> 2.0: How to find the SM32. */

/*#########################################################################*/
/*> 2.1: Low level search code */

#define cVendorID  0x10B5       /* PLX */
#define cDeviceID  0x9030

#ifdef PROTOTYPE
#define cSubIDs     0x903010b5  /*prototype*/
#else
#define cSubIDs     0x233710b5
#define cSM32Magic  0x534d3332  /* "SM32" */
#endif

/*-------------------------------------------------------------------------*/
/* PCI BUS: FIND PCI DEVICE
 *
 * VendorID, DeviceID: PCI board identifier at Configuration Register 0
 * DeviceIndex: 0=get first board with matching VendorID:DeviceID,
 *              1=second, 2=third...
 *
 * return value : 0xFFFF = not found, else:
 *                high byte = Bus Number
 *                low byte  = bits 7:5 = Function Number, bits 4:0 = Device
 */

WORD16 PciBios_FindDevice( WORD16 VendorID, WORD16 DeviceID, WORD16 DeviceIndex)
{
WORD16 retval = 0xFFFF;

  asm     mov  ax,0xb102
  asm     mov  cx,DeviceID
  asm     mov  dx,VendorID
  asm     mov  si,DeviceIndex
  asm     int  0x1a
  asm     jc   P_FD_0
  asm     cmp  ah,0
  asm     jne  P_FD_0
  asm     ror  bl,1
  asm     ror  bl,1
  asm     ror  bl,1
  asm     mov  retval,bx
P_FD_0:

  return retval;
}

/*-------------------------------------------------------------------------*/
/* PCI BUS: READ CONFIGURATION DWORD
 *
 * PCI Configuration Registers of Interrest:
 * Register Number     Register Content
 *        0            Device ID : Vendor ID
 *     0x2C            Subsystem ID : Subsystem Vendor ID
 *  SM32's Plx9030 specific:
 *     0x1C            PCI Base Address 3 (used for Local Address Space 1)
 *                     This is the I/O address +1 (the 1 only indicates
 *                     "The address lies within IO Space" and should
 *                     be removed before use.
 *
 * BusNum_DevFunNum: as obtained by PciBios_FindDevice
 * RegisterNumber  : number of the config register; 0x00-0xFF, multiple of 4
 * Data            : Variable for the Result.
 * return value: TRUE=ok, FALSE=error
 */

BYTE8 PciBios_ReadCfgDword( WORD16 BusNum_DevFunNum,
                            WORD16 RegisterNumber  ,
                            LONG32 *Data             )
{
BYTE8 retval = 0;
LONG32 _ecx_;        /*store ecx locally first, otherwise dereferencing */
                     /*the pointer to Data would be necessary in asm.   */
  asm      mov ax,0xb10A
  asm      mov bx,BusNum_DevFunNum
  asm      rol bl,1
  asm      rol bl,1
  asm      rol bl,1
  asm      mov di,RegisterNumber
  asm      int 0x1a
  asm      jc  P_RCD_0
  asm      cmp ah,0
  asm      jne P_RCD_0
  asm      db 0x66;      /* 0x66 == operand size prefix to turn cx into ecx */
  asm      mov word ptr _ecx_,cx  /* mov _ecx_,ecx */
  asm      mov byte ptr retval,1
P_RCD_0:

  if( retval ) *Data = _ecx_;
  return retval;
}

/*#########################################################################*/
/* FindSM32 */
/* Return I/O port address of SM32 number SM32Number ; 0xFFFF=not found */

WORD16 Find_SM32( WORD16 SM32Number, WORD16* BusNum_DevFunNum )
{
WORD16 DeviceIndex     ;
WORD16 SM32Count       ;
WORD16 SM32PortAddr    ;
LONG32 RegData         ;

  SM32Count   = 0;
  DeviceIndex = (WORD16) -1;

  for(;;) {
    ++DeviceIndex;  /* check next Device */

    *BusNum_DevFunNum = PciBios_FindDevice( cVendorID, cDeviceID, DeviceIndex );
    if( *BusNum_DevFunNum == 0xFFFF ) /* not found */      return 0xFFFF;
    /* Found one Plx9030. Check if it is an SM32. */
    /* Config Reg $2C = Subsystem ID : Subsystem Vendor ID*/
    if( ! PciBios_ReadCfgDword( *BusNum_DevFunNum, 0x2c, & RegData)) continue;
    if( RegData != cSubIDs )                                         continue;

    /* Get I/O Port Address from PCI Config Reg $1c */
    if( ! PciBios_ReadCfgDword( *BusNum_DevFunNum, 0x1c, & RegData)) continue;
    if( (RegData & 1) == 0 )             /*0=Mem, 1=I/O */           continue;
    if( (RegData & 0xFFFF0000) != 0 )    /*bigger than word?*/       continue;
    SM32PortAddr = (WORD16)(RegData-1);  /* -1 === remove 'I/O' flag  */

        /* Check for cSM32Magic at port adress +28 */
    asm   mov dx,SM32PortAddr
    asm   add dx,28
    asm   db 0x66
    asm   in ax,dx /* data size prefix $66 turns ax into eax */
    asm   db 0x66
    asm   mov word ptr RegData,ax

    if( RegData != cSM32Magic )                                     continue;
    ++SM32Count;                  /* found one SM32     */
    if( SM32Count != SM32Number ) /*not the searched one*/          continue;
                                                          return SM32PortAddr;
  };
}

/*-------------------------------------------------------------------------*/
#define cApiInvalidDeviceInfo 0
#define cApiSuccess           1

/*-------------------------------------------------------------------------*/
long Enum_SM32( long aNum, long *aBusDev )
{
WORD16 PortAddr;
WORD16 BusNum_DevFunNum;

  PortAddr = Find_SM32(aNum,&BusNum_DevFunNum);
  if( PortAddr != 0xFFFF ) {
    *aBusDev =   0xB00D00
               | (((BusNum_DevFunNum >> 8) & 0xFF) << 12)
               | (((BusNum_DevFunNum     ) & 0xFF)      );
    return cApiSuccess;
  };
  return cApiInvalidDeviceInfo;
}

/*#########################################################################*/
/*> 2.2: Code: SM32Enum, SM32Init, SM32Done */

/*--------------------------------------------------------------------------*/
long _CALLSTYLE_ SM32Enum( long aIn, long *aOut )
{
long i, BusDev;
long AddrIn   ;
long PciResult;

  AddrIn = aIn;
  if( (AddrIn >= 1) && (AddrIn <= 8) ) {
    PciResult = Enum_SM32( AddrIn, aOut );
    if( PciResult==cApiSuccess )     return mcrOk;
    return mcrNoSM32;
  };

  if( (AddrIn & 0xB00D00) == 0xB00D00 ) {
    for( i=1 ; i<=8 ; i++ ) {
      PciResult = Enum_SM32( i, &BusDev );
      if( PciResult != cApiSuccess ) return mcrNoSM32;
      if( BusDev == AddrIn ) {
        *aOut = i;
        return mcrOk;
      };
    };
    return mcrNoSM32;
  };

  return mcrIllegalAddr;
}

/*-------------------------------------------------------------------------*/
long _CALLSTYLE_ SM32Init( tSM32Motor* M, long Mno, long Bno   )
{
WORD16 baseport;
WORD16 BusNum_DevFunNum;
long Rslt;

  M_BASEPORT = mhSoftwareTest;
  M_NUMBER   = (BYTE8)Mno;
  M_RCMD     = 0;

  if( (Bno < 1) || (Bno > 8) ) {
    if( (Bno & 0xB00D00) != 0xB00D00 ) return mcrIllegalAddr;

    Rslt = SM32Enum( Bno, &Bno );
    if( Rslt != mcrOk ) return Rslt; /*mcrIllegalAddr;*/
  };

  if( (Bno<1) || (Bno>10) ) return mcrIllegalBno;
  if( (Mno<1) || (Mno>3)  ) return mcrIllegalMno;

  baseport = Find_SM32(Bno,&BusNum_DevFunNum);
  if( baseport == 0xFFFF ) return mcrNoSM32;

  M_BASEPORT = baseport;
  return mcrOk;
}

/*-------------------------------------------------------------------------*/
long _CALLSTYLE_ SM32Done( tSM32Motor* M )
{
  M_BASEPORT = mhUnknown;       return mcrOk;
}

/*#########################################################################*/
/*> 3.0: Special functions: SM32CmdToName, SM32NameToCmd, SM32CmdFlags, */

/*-------------------------------------------------------------------------*/
static char *SM32CommandNames_[] = {
  /* 0 mcNone       */ ""             ,
  /* 1 mcF          */ "F"            ,
  /* 2 mcFmax       */ "Fmax"         ,
  /* 3 mcA          */ "A"            ,
  /* 4 mcAmax       */ "Amax"         ,
  /* 5 mcU          */ "U"            ,
  /* 6 mcPower      */ "Power"        ,
  /* 7 mcGo         */ "Go"           ,
  /* 8 mcPosMode    */ "PosMode"      ,
  /* 9 mcPosition   */ "Position"     ,
  /*10 mcFg         */ "Fg"           ,
  /*11 mcSwitchMode */ "SwitchMode"   ,
  /*12 mcUhold      */ "Uhold"        ,
  /*13              */ ""             ,
  /*14              */ ""             ,
  /*15 mcGoHome     */ "GoHome"       ,
  /*16 mcBreak      */ "Break"        ,
  /*17 mcStepWidth  */ "StepWidth"    ,
  /*18 mcWatchdog   */ "Watchdog"     ,
  /*19 mcReset      */ "Reset"        ,
  /*20              */ ""             ,
  /*21 mcVersion    */ "Version"      ,
  /*22 mcPowerMeter */ "PowerMeter"   ,
  /*23 mcSwiPos     */ "SwiPos"       ,
  /*24 mcUact       */ "Uact"         ,
  /*25 mcPhase      */ "Phase"        ,
  /*26 mcFact       */ "Fact"         ,
  /*27 mcSwitches   */ "Switches"     ,
  /*28 mcAbsRel     */ "AbsRel"       ,
  /*29 mcState      */ "State"        ,
  /*30 mcError      */ "Error"        ,
  /*31 mcAuxIn      */ "AuxIn"        ,
  /*32 mcStoreCfg   */ "StoreCfg"
};

/*-------------------------------------------------------------------------*/
static const char SM32CommandFlags_[] ={ /* 4=system 2=writeable 1=readable */
  /* 0 mcNone       */ 0          ,
  /* 1 mcF          */      2 +  1,
  /* 2 mcFmax       */      2 +  1,
  /* 3 mcA          */      2 +  1,
  /* 4 mcAmax       */      2 +  1,
  /* 5 mcU          */      2 +  1,
  /* 6 mcPower      */      2 +  1,
  /* 7 mcGo         */      2     ,
  /* 8 mcPosMode    */      2 +  1,
  /* 9 mcPosition   */      2 +  1,
  /*10 mcFg         */      2 +  1,
  /*11 mcSwitchMode */      2 +  1,
  /*12 mcUhold      */      2 +  1,
  /*13              */ 0          ,
  /*14              */ 0          ,
  /*15 mcGoHome     */      2     ,
  /*16 mcBreak      */      2     ,
  /*17 mcStepWidth  */      2 +  1,
  /*18 mcWatchdog   */  4 + 2 +  1,
  /*19 mcReset      */  4 + 2     ,
  /*20              */ 0          ,
  /*21 mcVersion    */  4 +      1,
  /*22 mcPowerMeter */           1,
  /*23 mcSwiPos     */           1,
  /*24 mcUact       */           1,
  /*25 mcPhase      */           1,
  /*26 mcFact       */           1,
  /*27 mcSwitches   */           1,
  /*28 mcAbsRel     */      2 +  1,
  /*29 mcState      */           1,
  /*30 mcError      */  4 +      1,
  /*31 mcAuxIn      */  4     +  1,
  /*32 mcStoreCfg   */  4 + 2 +  1
};

/*-------------------------------------------------------------------------*/
char* _CALLSTYLE_ SM32CmdToName( long Cmd )
{
static char EmptyString = 0;
  if( (mcCmdMin<=Cmd) && (Cmd<=mcCmdMax) ) return SM32CommandNames_[Cmd];
                                           return &EmptyString;
}

/*-------------------------------------------------------------------------*/
long _CALLSTYLE_ SM32NameToCmd( char* Name )
{
int i;

  for( i=mcCmdMin ; i<=mcCmdMax ; i++ ) {
    if( stricmp(Name,SM32CommandNames_[i]) == 0 ) return i;
  };

  return mcNone;
}

/*-------------------------------------------------------------------------*/
long _CALLSTYLE_ SM32CmdFlags( long Cmd )
{
  if( (mcCmdMin<=Cmd) && (Cmd<=mcCmdMax) ) return SM32CommandFlags_[Cmd];
                                           return 0;
}

/*-------------------------------------------------------------------------*/
long _CALLSTYLE_ SM32GetTimeout(void)
{
  return SM32Timeout / 10000;
}

/*-------------------------------------------------------------------------*/
long _CALLSTYLE_ SM32SetTimeout( long t )
{
  SM32Timeout = t * 10000;
  return  mcrOk;
}
