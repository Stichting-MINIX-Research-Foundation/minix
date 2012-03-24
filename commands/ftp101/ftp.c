/* ftp.c Copyright 1992-2000 by Michael Temari All Rights Reserved
 *
 * ftp          An ftp client program for use with TNET.
 *
 * Usage:       ftp [host]
 *
 * Version:     0.10    06/21/92 (pre-release not yet completed)
 *              0.20    07/01/92
 *              0.30    01/15/96 (Minix 1.7.1 initial release)
 *              0.40    08/27/96
 *              0.50    03/08/00
 *              1.00    12/12/03 (added ver command)
 *              1.01    02/07/05
 *
 * Author:      Michael Temari, <Michael@TemWare.Com>
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

#include "ftp.h"
#include "local.h"
#include "file.h"
#include "other.h"
#include "net.h"

char *FtpVersion = "1.01 02/07/05";

int linkopen;
int loggedin;
int type;
int format;
int mode;
int structure;
int passive;
int atty;

int cmdargc;
char *cmdargv[NUMARGS];

int printreply = 1;
char reply[1024];

#ifdef __NBSD_LIBC
/* Already declared in stdio.h */
#define getline ftp_getline
#endif

static void makeargs(char *buff);
int DOver(void);
int DOhelp(void);
static int getline(char *line, int len);
int main(int argc, char *argv[]);


static void makeargs(buff)
char *buff;
{
int i;
char *p;

   for(i = 0; i < NUMARGS; i++)
	cmdargv[i] = (char *)0;

   p = buff + strlen(buff) - 1;
   while(p >= buff)
	if(*p == '\r' || *p == '\n' || isspace(*p))
		*p-- = '\0';
	else
		break;

   p = buff;
   cmdargc = 0;
   while(cmdargc < NUMARGS) {
	while(*p && isspace(*p))
		p++;
	if(*p == '\0')
		break;
	cmdargv[cmdargc++] = p;
	while(*p && !isspace(*p)) {
		if(cmdargc == 1)
			*p = tolower(*p);
		p++;
	}
	if(*p == '\0')
		break;
	*p = '\0';
	p++;
   }
}

int readline(prompt, buff, len)
char *prompt;
char *buff;
int len;
{
char *p;

   printf(prompt); fflush(stdout);

   if(fgets(buff, len, stdin) == (char *)NULL) {
	printf("\nEnd of file on input!\n");
	return(-1);
   }

   p = buff + strlen(buff) - 1;
   while(p >= buff)
	if(*p == '\r' || *p == '\n' || isspace(*p))
		*p-- = '\0';
	else
		break;

   if(!atty) {
	printf("%s\n", buff);
	fflush(stdout);
   }

   return(0);
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
   	s = read(ftpcomm_fd, line, 1);
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

int DOgetreply()
{
int firsttime;
int s;
char code[4];

   do {
	firsttime = 1;
	do {
		if((s = getline(reply, sizeof(reply))) < 0)
			return(s);
		if(printreply) {
			printf("%s\n", reply);
			fflush(stdout);
		}
		if(firsttime) {
			firsttime = 0;
			strncpy(code, reply, 3);
			code[3] = '\0';
		}
	   } while(strncmp(reply, code, 3) || reply[3] == '-');
	   s = atoi(code);
   } while(s < 200 && s != 125 && s != 150);

   return(s);
}

int DOcmdcheck()
{
   if(!linkopen) {
	printf("You must \"OPEN\" a connection first.\n");
	return(1);
   }

   if(!loggedin) {
	printf("You must login first.\n");
	return(1);
   }

   return(0);
}

int DOcommand(ftpcommand, ftparg)
char *ftpcommand;
char *ftparg;
{
int s;
#if 1
static char ss[64];
   if(*ftparg)
   	sprintf(ss, "%s %s\r\n", ftpcommand, ftparg);
   else
   	sprintf(ss, "%s\r\n", ftpcommand);

   s = write(ftpcomm_fd, ss, strlen(ss));
   if(s != strlen(ss))
   	return(-1);

#else
   s = write(ftpcomm_fd, ftpcommand, strlen(ftpcommand));
   if(s != strlen(ftpcommand))
   	return(-1);

   if(*ftparg) {
	s = write(ftpcomm_fd, " ", 1);
	if(s != 1)
		return(-1);

	s = write(ftpcomm_fd, ftparg, strlen(ftparg));
	if(s != strlen(ftparg))
		return(-1);
   }

   s = write(ftpcomm_fd, "\r\n", 2);
   if(s != 2)
   	return(-1);
#endif

   return(DOgetreply());
}

int DOver()
{
   printf("FTP Version %s\n", FtpVersion);
   return(0);
}

int DOhelp()
{
char junk[10];

   printf("Command:      Description\n");
   printf("!             Escape to a shell\n");
   printf("append        Append a file to remote host\n");
   printf("ascii         Set file transfer type to ascii\n");
   printf("binary        Set file transfer type to binary\n");
   printf("block         Set file transfer mode to block\n");
   printf("bye           Close connection and exit\n");
   printf("cd            Change directory on remote host\n");
   printf("close         Close connection\n");
   printf("clone         Clone a file\n");
   printf("del           Remove file on remote host\n");
   printf("dir           Display long form remote host directory listing\n");
   printf("exit          Close connection and exit\n");
   printf("get           Retrieve a file from remote host\n");
   printf("help          Display this text\n");

   if(readline("Press ENTER to continue... ", junk, sizeof(junk)))
   	return(-1);

   printf("lcd           Change directory on local host\n");
   printf("ldir          Display long form local host directory listing\n");
   printf("lls           Display local host directory listing\n");
   printf("lmkdir        Create directory on local host\n");
   printf("lpwd          Display current directory on local host\n");
   printf("lrmdir        Remove directory on local host\n");
   printf("ls            Display remote host directory listing\n");
   printf("mget          Retrieve multiple files from remote host\n");
   printf("mkdir         Create directory on remote host\n");
   printf("mod           Get file modification time\n");
   printf("mput          Send multiple files to remote host\n");
   printf("noop          Send the ftp NOOP command\n");

   if(readline("Press ENTER to continue... ", junk, sizeof(junk)))
   	return(-1);

   printf("open          Open connection to remote host\n");
   printf("pass          Enter remote user password\n");
   printf("passive       Toggle passive mode\n");
   printf("put           Send a file to remote host\n");
   printf("putu          Send a file to remote host(unique)\n");
   printf("pwd           Display current directory on remote host\n");
   printf("quit          Close connection and exit\n");
   printf("quote         Send raw ftp command to remote host\n");
   printf("reget         Restart a partial file retrieve from remote host\n");
   printf("remotehelp    Display ftp commands implemented on remote host\n");
   printf("reput         Restart a partial file send to remote host\n");
   printf("rm            Remove file on remote host\n");
   printf("rmdir         Remove directory on remote host\n");

   if(readline("Press ENTER to continue... ", junk, sizeof(junk)))
   	return(-1);

   printf("site          Send a site specific command\n");
   printf("size          Get file size information\n");
   printf("status        Get connection/file status information\n");
   printf("stream        Set file transfer mode to stream\n");
   printf("system        Get remote system type information\n");
   printf("user          Enter remote user information\n");
   printf("ver           Display client version information\n");

   return(0);
}

struct commands {
	char *name;
 int(*func) (void);
};

static struct commands commands[] = {
        "!",            DOlshell,
	"append",	DOappe,
	"ascii",        DOascii,
	"binary",       DObinary,
	"block",        DOblock,
	"bye",          DOquit,
	"cd",           DOcd,
	"close",        DOclose,
	"clone",        DOclone,
	"del",          DOdelete,
	"dir",          DOlist,
	"exit",         DOquit,
	"get",          DOretr,
	"help",         DOhelp,
	"lcd",          DOlcd,
        "ldir",         DOllist,
        "lls",          DOlnlst,
	"lmkdir",       DOlmkdir,
	"lpwd",         DOlpwd,
	"lrmdir",       DOlrmdir,
	"ls",           DOnlst,
	"mget",         DOMretr,
	"mkdir",        DOmkdir,
	"mod",		DOmdtm,
	"mput",         DOMstor,
	"noop",         DOnoop,
	"open",         DOopen,
	"pass",		DOpass,
	"passive",      DOpassive,
	"put",          DOstor,
	"putu",		DOstou,
	"pwd",          DOpwd,
	"quit",         DOquit,
	"quote",        DOquote,
	"reget",	DOrretr,
	"remotehelp",   DOremotehelp,
	"reput",	DOrstor,
	"rm",           DOdelete,
	"rmdir",        DOrmdir,
	"site",		DOsite,
	"size",		DOsize,
	"status",	DOstat,
	"stream",	DOstream,
	"system",	DOsyst,
	"user",         DOuser,
	"ver",          DOver,
	"",     (int (*)())0
};

int main(argc, argv)
int argc;
char *argv[];
{
int s;
struct commands *cmd;
static char buffer[128];

   if(NETinit())
   	return(-1);

   FTPinit();

   s = 0;

   if(argc > 1) {
	sprintf(buffer, "open %s ", argv[1]);
	makeargs(buffer);
	s = DOopen();
	if(atty && s > 0) {
		sprintf(buffer, "user");
		makeargs(buffer);
		s = DOuser();
	}
   }

   while(s >= 0) {
	s = readline("ftp>", buffer, sizeof(buffer));
	if(s < 0) break;
	makeargs(buffer);
	if(cmdargc == 0) continue;
	for(cmd = commands; *cmd->name != '\0'; cmd++)
		if(!strcmp(cmdargv[0], cmd->name))
			break;
	if(*cmd->name != '\0')
		s = (*cmd->func)();
	else {
		s = 0;
		printf("Command \"%s\" not recognized.\n", cmdargv[0]);
	}
   }

   return(s);
}
