/* net.h Copyright 1992-2000 by Michael Temari All Rights Reserved
 *
 * This file is part of ftpd.
 *
 *
 * 01/25/96 Initial Release	Michael Temari, <Michael@TemWare.Com>
 */

_PROTOTYPE(int doPASV, (char *buff));
_PROTOTYPE(int doPORT, (char *buff));
_PROTOTYPE(int DataConnect, (void));
_PROTOTYPE(int CleanUpPasv, (void));
_PROTOTYPE(void GetNetInfo, (void));
