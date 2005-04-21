/* ftp.h
 *
 * This file is part of ftp.
 *
 *
 * 01/25/96 Initial Release	Michael Temari, <temari@ix.netcom.com>
 */

extern FILE *fpcommin;
extern FILE *fpcommout;

extern int linkopen;
extern int loggedin;
extern int type;
extern int format;
extern int mode;
extern int structure;
extern int passive;
extern int atty;

#define	NUMARGS	10
extern int cmdargc;
extern char *cmdargv[NUMARGS];

extern char reply[1024];

#define	RETR	0
#define	STOR	1

#define	TYPE_A	0
#define	TYPE_I	1

_PROTOTYPE(int readline, (char *prompt, char *buff, int len));
_PROTOTYPE(int DOgetreply, (void));
_PROTOTYPE(int DOcmdcheck, (void));
_PROTOTYPE(int DOcommand, (char *ftpcommand, char *ftparg));
