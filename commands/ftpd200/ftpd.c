/* ftpd.c Copyright 1992-2000 by Michael Temari All Rights Reserved
 *
 * ftpd         An FTP server program for use with Minix.
 *
 * Usage:       Minix usage: tcpd ftp ftpd
 *
 * 06/14/92 Tnet Release	Michael Temari
 * 01/15/96 0.30		Michael Temari
 * 01/25/96 0.90		Michael Temari
 * 03/17/96 0.91		Michael Temari
 * 06/27/96 0.92		Michael Temari
 * 07/02/96 0.93		Michael Temari
 * 07/15/96 0.94		Michael Temari
 * 08/27/96 0.95		Michael Temari
 * 02/09/97 0.96		Michael Temari
 * 02/10/97 0.97		Michael Temari
 * 09/25/97 0.98		Michael Temari
 * 03/10/00 0.99		Michael Temari, <Michael@TemWare.Com>
 * 12/12/03 1.00		Michael Temari, <Michael@TemWare.Com>
 * 02/06/05 1.01		Michael Temari, <Michael@TemWare.Com>
 * 02/12/05 2.00		Michael Temari, <Michael@TemWare.Com>
 */

char *FtpdVersion = "2.00";

#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <net/gen/in.h>
#include <net/gen/tcp.h>

#include "ftpd.h"
#include "access.h"
#include "file.h"
#include "net.h"

_PROTOTYPE(static void init, (void));
_PROTOTYPE(static int doHELP, (char *buff));
_PROTOTYPE(static int doNOOP, (char *buff));
_PROTOTYPE(static int doUNIMP, (char *buff));
_PROTOTYPE(static int getline, (char *line, int len));

FILE *msgfile = (FILE *)NULL;

/* The following defines the inactivity timeout in seconds */
#define	INACTIVITY_TIMEOUT	60*5

char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

char line[512];

int type, format, mode, structure;
int ftpdata_fd = -1;
int loggedin, gotuser, anonymous;
char username[80];
char anonpass[128];
char newroot[128];

ipaddr_t myipaddr, rmtipaddr, dataaddr;
tcpport_t myport, rmtport, dataport;

char myhostname[256], rmthostname[256];

#define	FTPD_LOG	"/usr/adm/ftpd.log"
#define	FTPD_MSG	"/etc/ftpd_msg"

FILE *logfile;

int timeout = 0;

_PROTOTYPE(static int doHELP, (char *buff));
_PROTOTYPE(int readline, (char **args));
_PROTOTYPE(void Timeout, (int sig));
_PROTOTYPE(int main, (int argc, char *argv[]));

struct commands {
	char *name;
	_PROTOTYPE(int (*func), (char *buff));
};

struct commands commands[] = {
	"ABOR", doUNIMP,
	"ACCT", doUNIMP,
	"ALLO", doALLO,
	"APPE", doAPPE,
	"CDUP", doCDUP,
	"CWD",  doCWD,
	"DELE", doDELE,
	"HELP", doHELP,
	"LIST", doLIST,
	"MDTM", doMDTM,
	"MKD",  doMKD,
	"MODE", doMODE,
	"NLST", doNLST,
	"NOOP", doNOOP,
	"PASS", doPASS,
	"PASV", doPASV,
	"PORT", doPORT,
	"PWD",  doPWD,
	"QUIT", doQUIT,
	"REIN", doUNIMP,
	"REST", doREST,
	"RETR", doRETR,
	"RMD",  doRMD,
	"RNFR", doRNFR,
	"RNTO", doRNTO,
	"SITE", doSITE,
	"SIZE", doSIZE,
	"SMNT", doUNIMP,
	"STAT", doSTAT,
	"STOR", doSTOR,
	"STOU", doSTOU,
	"STRU", doSTRU,
	"SYST", doSYST,
	"TYPE", doTYPE,
	"USER", doUSER,
	"XCUP", doCDUP,
	"XCWD", doCWD,
	"XMKD", doMKD,
	"XPWD", doPWD,
	"XRMD", doRMD,
	"",     (int (*)())0
};

static void init()
{
   loggedin = 0;
   gotuser = 0;
   anonymous = 0;
   newroot[0] = '\0';
   type = TYPE_A;
   format = 0;
   mode = MODE_S;
   structure = 0;
   ftpdata_fd = -1;
   username[0] = '\0';
   anonpass[0] = '\0';
}

/* nothing, nada, zilch... */
static int doNOOP(buff)
char *buff;
{
   printf("200 NOOP to you too!\r\n");

   return(GOOD);
}

/* giv'em help, what a USER! */
static int doHELP(buff)
char *buff;
{
struct commands *cmd;
char star;
int i;
char *space = "    ";

   printf("214-Here is a list of available ftp commands\r\n");
   printf("    Those with '*' are not yet implemented.\r\n");

   i = 0;
   for(cmd = commands; *cmd->name != '\0'; cmd++) {
	if(cmd->func == doUNIMP)
		star = '*';
	else
		star = ' ';
	printf("     %s%c%s", cmd->name, star, space + strlen(cmd->name));
	if(++i == 6) {
		printf("\r\n");
		i = 0;
	}
   }

   if(i)
	printf("\r\n");

   printf("214 That's all the help you get.\r\n");

   return(GOOD);
}

/* not implemented */
static int doUNIMP(buff)
char *buff;
{
   printf("502 Command \"%s\" not implemented!\r\n", line);

   return(GOOD);
}

/* convert line for use */
void cvtline(args)
char **args;
{
char *p;

   p = line + strlen(line);
   while(--p >= line)
	if(*p == '\r' || *p == '\n' || isspace(*p))
		*p = '\0';
	else
		break;

  p = line;

#ifdef DEBUG
  logit("COMMAND", line);
#endif

  while(*p && !isspace(*p)) {
	*p = toupper(*p);
	p++;
  }

  if(*p) {
	*p = '\0';
	p++;
	while(*p && isspace(*p))
		p++;
   }

   *args = p;

   return;
}

static int getline(line, len)
char *line;
int len;
{
int s;
int gotcr;

   /* leave room for at end for null */
   len--;

   /* got to be able to put in at least 1 character */
   if(len < 1)
	return(-1);

   gotcr = 0;
   while(len-- > 0) {
	s = read(0, line, 1);
	if(s != 1)
		return(-1);
	if(*line == '\n')
		break;
	gotcr = (*line == '\r');
	line++;
   }
   if(gotcr)
	--line;

   *line = '\0';

   return(0);
}

int readline(args)
char **args;
{
   if(getline(line, sizeof(line)))
	return(BAD);

   cvtline(args);

   return(GOOD);
}

/* signal handler for inactivity timeout */
void Timeout(sig)
int sig;
{
   timeout = 1;

   printf("421 Inactivity timer expired.\r\n");
}

/* logit */
void logit(type, parm)
char *type;
char *parm;
{
time_t now;
struct tm *tm;

   if(logfile == (FILE *)NULL)
	return;

   time(&now);
   tm = localtime(&now);
   fprintf(logfile, "%4d%02d%02d%02d%02d%02d ",
	1900+tm->tm_year,
	tm->tm_mon + 1,
	tm->tm_mday,
	tm->tm_hour, tm->tm_min, tm->tm_sec);
   fprintf(logfile, "%s %s %s %s %s\n",
	rmthostname, username, anonymous ? anonpass : username, type, parm);
   fflush(logfile);
}

void showmsg(reply, filename)
char *reply;
char *filename;
{
FILE *mfp;
char *pe;
static char mline[256];

   if(filename == (char *)NULL)
	mfp = msgfile;
   else
	mfp = fopen(filename, "r");

   if(mfp == (FILE *)NULL)
	return;

   while(fgets(mline, sizeof(mline), mfp) != (char *)NULL) {
	pe = mline + strlen(mline);
	while(--pe >= mline)
		if(*pe == '\r' || *pe == '\n')
			*pe = '\0';
		else
			break;
	printf("%s- %s\r\n", reply, mline);
   }

   if(filename != (char *)NULL)
	fclose(mfp);
}

int main(argc, argv)
int argc;
char *argv[];
{
struct commands *cmd;
char *args;
int status;
time_t now;
struct tm *tm;
int s;

   GetNetInfo();

   /* open transfer log file if it exists */
   if((logfile = fopen(FTPD_LOG, "r")) != (FILE *)NULL) {
	fclose(logfile);
	logfile = fopen(FTPD_LOG, "a");
   }

   /* open login msg file */
   msgfile = fopen(FTPD_MSG, "r");

   /* Let's initialize some stuff */
   init();

   /* Log the connection */
   logit("CONNECT", "");

   /* Tell 'em we are ready */
   time(&now);
   tm = localtime(&now);
   printf("220 FTP service (Ftpd %s) ready on %s at ",
   	FtpdVersion, myhostname);
   printf("%s, %02d %s %d %02d:%02d:%02d %s\r\n", days[tm->tm_wday],
   	tm->tm_mday, months[tm->tm_mon], 1900+tm->tm_year,
	tm->tm_hour, tm->tm_min, tm->tm_sec,
	tzname[tm->tm_isdst]);
   fflush(stdout);

   /* Loop here getting commands */
   while(1) {
	signal(SIGALRM, Timeout);
	alarm(INACTIVITY_TIMEOUT);
	if(readline(&args) != GOOD) {
		if(!timeout)
			printf("221 Control connection closing (EOF).\r\n");
		break;
	}
	alarm(0);
	for(cmd = commands; *cmd->name != '\0'; cmd++)
		if(!strcmp(line, cmd->name))
			break;
	if(*cmd->name != '\0')
		status = (*cmd->func)(args);
	else {
		printf("500 Command \"%s\" not recognized.\r\n", line);
		status = GOOD;
	}
	fflush(stdout);
	if(status != GOOD)
		break;
   }

   CleanUpPasv();

   return(-1);
}
