/* ftpd.h Copyright 1992-2000 by Michael Temari All Rights Reserved
 *
 * This file is part of ftpd.
 *
 *
 * 01/25/96 Initial Release	Michael Temari, <Michael@TemWare.Com>
 */

#define	GOOD	0
#define	BAD	1

#define	TYPE_A	0
#define	TYPE_I	1

#define	MODE_S	0
#define	MODE_B	1

#define	MODE_B_EOF	64

extern char *FtpdVersion;
extern int type, format, mode, structure;
extern ipaddr_t myipaddr, rmtipaddr, dataaddr;
extern tcpport_t myport, rmtport, dataport;
extern int ftpdata_fd;
extern int loggedin, gotuser, anonymous;
extern char newroot[128];
extern char *days[], *months[];
extern char username[80];
extern char anonpass[128];
extern char myhostname[256], rmthostname[256];
extern char line[512];

extern FILE *logfile;

_PROTOTYPE(void cvtline, (char **args));
_PROTOTYPE(void logit, (char *type, char *parm));
_PROTOTYPE(void showmsg, (char *reply, char *filename));
