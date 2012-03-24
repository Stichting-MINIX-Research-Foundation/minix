/* ftp.h Copyright 1992-2000 by Michael Temari All Rights Reserved
 *
 * This file is part of ftp.
 *
 *
 * 01/25/96 Initial Release	Michael Temari, <Michael@TemWare.Com>
 */

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

extern int printreply;
extern char reply[1024];

#define	RETR	0
#define	STOR	1

#define	TYPE_A	0
#define	TYPE_I	1

#define	MODE_S	0
#define	MODE_B	1

#define	MODE_B_EOF	64

int readline(char *prompt, char *buff, int len);
int DOgetreply(void);
int DOcmdcheck(void);
int DOcommand(char *ftpcommand, char *ftparg);
