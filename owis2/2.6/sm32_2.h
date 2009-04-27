/* SM32_2.h

 mainly stolen from OWIS header for DOS C drivers.

 * Contains definitions for communication structures as well as 
 * command definitions for higher level device access.
 *
 * Christian Kurtsiefer, 3/24/02
 *
 */

/*------------------------------------------------------------------------- 
   functions (under Linux realized as ioctl() commands for the minor
    device 3)
   Parameter transfer is done via a long argument and the command itself

   general Syntax fo calls for write commands:
   int ioctl(int handle,  MOT_x | command_name | action, value);
   where:
      command_name   are defined below, 
      MOT_x          is a macro referring to one of the motors, 
      action         can be SM32Post, SM32Request to send either a write
                     command or  a request for data command

      returns 0 on success, or -1 if busy

   general syntax for read status check command:
   int ioctl(int handle, MOT_x | SM32Replied)
     returns 0 if not yet replied
   readback command:
   int ioctl(int handle, MOT_x | SM32Fetch)
     returns the data value as int/longint

*/

/* constants for getting IOCTL right: */
#define MOT_0 0
#define MOT_1 0x100
#define MOT_2 0x200

#define SM32Post    0x40000000
#define SM32Request 0x40010000
#define SM32Replied 0x80000000
#define SM32Fetch   0x80010000
#define SM32IsReady 0x80020000

#define SM32_MOTOR_MASK 0x300
#define SM32_MOTOR_SHIFT 0x8
#define SM32_CALL_MASK 0xffff0000
#define SM32_COMMAND_MASK 0xff


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










