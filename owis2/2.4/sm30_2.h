/* SM30_2.h
 * Adaptiert fuer den Linux-Treiber von der Originalversion SM30.h
 * der Firma OWIS.
 *
 * Contains definitions for communication structures as well as 
 * command definitions for higher level device access.
 *
 * Christian Kurtsiefer, 8/8/99
 *
 */

#ifndef __SM30_H_INCLUDED__
#define __SM30_H_INCLUDED__

/* Siehe auch SM30.C,  BYTEPORT_IN(a)  und  BYTEPORT_OUT(a,b)  */

/*-------------------------------------------------------------------------*/
/* SM30-struct */

struct tagSM30 {
  unsigned int Addr;     /* port address; can't be set, only read. */
  unsigned int MaxWait ; /* Timeout SM30Write,SM30Read [ms] */

  /* Return values from a read command */
  int  Data[3] ; /* Ergebnis von SM30Read           */
  int  State[3]; /* steht:0 , laeuft:!=0            */
  int  Error   ; /* Fehlermeldung                   */
  int  Time    ; /* interne Maschinenzeit [2ms]     */
};
typedef struct tagSM30 tSM30;
typedef struct tagSM30 *pSM30;

/*-------------------------------------------------------------------------*/
/* functions (under Linux realized as ioctl() commands for the minor
 *  device 2)
 * Parameter transfer is done via the parameter structure sm30_cmd_struct, */

struct tagcmdst {
  pSM30 SM30;   /* handle structure containing adresses, errors, states */
  int cmd;        /* contains a command to be executed */
  int sel;        /* motor selector */
  unsigned long data[3];  /* data transfer structure */
};

typedef struct tagcmdst sm30_cmd_struct;

/* general Syntax fo calls: int ioctl(int handle, function_name, 
 *                                      *sm30_cmd_struct);
 * The function_name are defined here: */

/* Init command, fills the sm30 structure and returns an error code or
   1 on success. The hardware address can only be set in the device driver. 
   Only *sm30 is required. */
#define SM30Init 0x1 

/* write command, triggers a command with the specified parameters. Needs
   parameters *sm30, cmd, sel; data0 to data2 depending on cmd.*/
#define SM30Write 0x2
#define SM30WriteFunc 0x2

/* Read command, reads data into the sm30 struct according to the 
   specified command cmd. Returns 0 on success, -1 on fail */
#define SM30Read 0x3

/* write query command, returns 0 if device is busy, -1 otherwise. Needs only
 *sm30 */
#define SM30CanWrite 0x4
/* start a read request. Initiates the read command cmd and returns immediately
   with the result of SM30Read, cmd=mcNone. Returns with  -1 if data was read
   already, or with 0 if device is still busy. NEeds *sm30 and cmd.
   is now identical to SM30Read. (somewhere there is an incconsictency )
*/
#define SM30Request 0x5  
/* read query command. returns 0 if device is busy, -1 other wise */ 
#define SM30Replied 0x6  

/*-------------------------------------------------------------------------*/
/* Kommando-Konstanten  (mc... = Motor Command...) */

        /*-- Ein/Ausschalten --*/
#define mcPower       6  /* RW (Alle aus == Fehler loeschen)       */

        /*-- Grundeinstellungen --*/
#define mcU           5  /* RW Motor-Nennspannung setzen           */
#define mcUhold      12  /* RW Motor-Haltespannung in % setzen     */
#define mcAmax        4  /* RW Maximalbeschleunigung setzen        */
#define mcFmax        2  /* RW Maximalgeschwindigkeit setzen       */
#define mcStepWidth  17  /* RW Schrittweite & Drehrichtung         */
#define mcSwitchMode 11  /* RW Endschalter zuordnen                */

        /*-- Bewegen --*/
#define mcPosMode     8  /* RW Positioniermodus setzen             */
#define mcAbsRel     28  /* RW Relativ/Absolut-Modus setzen        */
#define mcPosition    9  /* RW Positionszaehler setzen             */
#define mcA           3  /* RW Sollbeschleunigung setzen           */
#define mcF           1  /* RW Sollgeschwindigkeit setzen          */
#define mcFact       26  /* R  gegenwaertige Geschwindigkeit       */
#define mcGo          7  /*  W Zielfahrt                           */
#define mcGoHome     15  /*  W Heimfahrt                           */
#define mcState      29  /* R  steht:0                             */
#define mcError      30  /* R  Fehlermeldung                       */
#define mcSwitches   27  /* R  gegenwaertige Schalterzustaende     */

        /*-- Sonstiges --*/
#define mcNone        0  /* R  Zuletzt angeforderten Wert einlesen */
#define mcPowerMeter 22  /* R  Motorleistung lesen                 */
#define mcUact       24  /* R  Motor-Istspannung lesen             */
#define mcBreak      16  /*  W Abbruch                             */
#define mcSwiPos     23  /* R  letzte Schaltposition lesen         */
#define mcPhase      25  /* R  Motorphase lesen                    */
#define mcFg         10  /* RW ggf.Motorcharakteristik angeben     */
#define mcWatchdog   18  /* RW Watchdog setzen                     */
#define mc510V       20  /* RW 5/10V-Ausgang einstellen            */
#define mcReset      19  /*  W In Einschalt-Zustand versetzen      */
#define mcVersion    21  /* R  Versionsnummer lesen                */

/*-------------------------------------------------------------------------*/
/* Modus-Konstanten (mm... = Motor Mode...) */

#define mmMove  1        /* Mit angegebener Geschwindigkeit fahren */
#define mmPos   3        /* Zur angegebenen Position fahren        */

#define mmAbs   0        /* Absolut positionieren                  */
#define mmRel   1        /* Relativ positionieren                  */

/*-------------------------------------------------------------------------*/
/* Konstanten zur Motorauswahl (ms... = Motor Select...) */

#define msNone  (0    )  /* Kein Motor      */
#define msM0    (1    )  /* Motor 0         */
#define msM1    (  2  )  /* Motor 1         */
#define msM01   (1+2  )  /* Motoren 0 und 1 */
#define msM2    (    4)  /* Motor 2         */
#define msM02   (1 + 4)  /* Motoren 0 und 2 */
#define msM12   (  2+4)  /* Motoren 1 und 2 */
#define msM012  (1+2+4)  /* Alle Motoren    */
#define msAll   (1+2+4)  /* Alle Motoren    */

/* BYTE8   SM30Sel[3];       = (1,2,4) !!!!!!MUSS NOCH REPARIERT WERDEN */

/*-------------------------------------------------------------------------*/
/* Fehler-Konstanten (me... = Motor Error...) */

#define meNone        0  /* kein Fehler                                */

        /* temporaere Fehler */
#define meI0          1  /* Motor 0 Ueberstrom                         */
#define meI1          2  /* Motor 1 Ueberstrom                         */
#define meI2          4  /* Motor 2 Ueberstrom                         */
#define meIover       8  /* Allgemeine Strombegrenzung                 */
#define meI          15  /* Irgendein Ueberstrom-Fehler                */

        /* dauerhafte Fehler */
#define me510V       16  /* 5/10V-Anschluss wurde kurzgeschlossen      */
#define meSwitch     32  /* Endschalter im Nothaltmodus wurde betaetigt*/
#define meWatchdog   64  /* Watchdog wurde ausgeloest                  */
#define meReset     128  /* Interner Fehler                            */
#define meNotFound  254  /* SM30Init lieferte FALSE                    */
#define meHalted    240  /* Irgendein dauerhafter Fehler               */

/*-------------------------------------------------------------------------*/
/* Status-Konstanten (mst... = Motor State...) */

#define mstStop    0     /* Motor steht                        */
#define mstMove    1     /* Motor laeuft (im Modus mmMove)     */
#define mstPos     3     /* Motor laeuft (im Modus mmPos )     */
#define mstHome    8     /* Motor macht Heimfahrt              */
#define mstBreak   9     /* Motor fuehrt Schnellbremsung durch */

/*-------------------------------------------------------------------------*/



#endif /* __SM30_H_INCLUDED__ */









