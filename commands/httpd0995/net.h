/* net.h
 *
 * This file is part of httpd.
 *
 *
 * 01/25/1996 			Michael Temari <Michael@TemWare.Com>
 * 07/07/1996 Initial Release	Michael Temari <Michael@TemWare.Com>
 * 12/29/2002			Michael Temari <Michael@TemWare.Com>
 *
 */

_PROTOTYPE(void GetNetInfo, (void));
_PROTOTYPE(void daemonloop, (char *service));

extern char myhostname[256];
extern char rmthostname[256];
extern char rmthostaddr[3+1+3+1+3+1+3+1];
