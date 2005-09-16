/* pass.h
 *
 * This file is part of httpd.
 *
 * 07/07/1996 Initial Release	Michael Temari <Michael@TemWare.Com>
 * 12/29/2002 Initial Release	Michael Temari <Michael@TemWare.Com>
 *
 */

_PROTOTYPE(int passfile, (char *pwdfile));
_PROTOTYPE(int passuser, (char *pwdfile, char *user));
_PROTOTYPE(int passnone, (char *pwdfile, char *user));
_PROTOTYPE(int passpass, (char *pwdfile, char *user, char *pass));
_PROTOTYPE(int passadd,  (char *pwdfile, char *user, char *pass, char *e1, char *e2, char *e3, char *e4));

#define	PASS_GOOD	0
#define	PASS_USEREXISTS	1
#define	PASS_ERROR	-1	
