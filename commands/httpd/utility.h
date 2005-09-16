/* utility.h
 *
 * This file is part of httpd.
 *
 * 02/17/1996 			Michael Temari <Michael@TemWare.Com>
 * 07/07/1996 Initial Release	Michael Temari <Michael@TemWare.Com>
 * 12/29/2002			Michael Temari <Michael@TemWare.Com>
 *
 */

#define	LWS(c)	((c == ' ') || (c == '\t') || (c == '\r') || (c == '\n'))

_PROTOTYPE(char *logdate, (time_t *t));
_PROTOTYPE(char *httpdate, (time_t *t));
_PROTOTYPE(time_t httptime, (char *p));
_PROTOTYPE(char *mimetype, (char *url));
_PROTOTYPE(char *decode64, (char *p));
_PROTOTYPE(int getparms, (char *p, char *parms[], int maxparms));
_PROTOTYPE(int mkurlaccess, (char *p));
