/* ftp.c by Michael Temari 06/21/92
 *
 * ftp          An ftp client program for use with TNET.
 *
 * Usage:       ftp [[host] [port]]
 *
 * Version:     0.10    06/21/92 (pre-release not yet completed)
 *              0.20    07/01/92
 *              0.30    01/15/96 (Minix 1.7.1 initial release)
 *              0.40    08/27/96
 *
 * Author:      Michael Temari, <temari@ix.netcom.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "ftp.h"
#include "local.h"
#include "file.h"
#include "other.h"
#include "net.h"

FILE *fpcommin;
FILE *fpcommout;

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

char reply[1024];

_PROTOTYPE(static int makeargs, (char *buff));
_PROTOTYPE(int DOhelp, (void));
_PROTOTYPE(int main, (int argc, char *argv[]));

static int makeargs(buff)
char *buff;
{
char *p;
int i;

   for(i = 0; i < NUMARGS; i++)
	cmdargv[i] = (char *)0;

   p = buff + strlen(buff) - 1;
   while(p != buff)
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
   printf(prompt); fflush(stdout);

   if(fgets(buff, len, stdin) == (char *)NULL) {
	printf("\nEnd of file on input!\n");
	exit(1);
   }

   *strchr(buff, '\n') = 0;

   if(!atty) {
	printf("%s\n", buff);
	fflush(stdout);
   }

   return(0);
}

int DOgetreply()
{
char *p;
char buff[6];
int s;
int firsttime;

   do {
	firsttime = 1;
	do {
		if(fgets(reply, sizeof(reply), fpcommin) == (char *)0)
			return(-1);
		p = reply + strlen(reply) - 1;
		while(p != reply)
			if(*p == '\r' || *p == '\n' || isspace(*p))
				*p-- = '\0';
			else
				break;
		printf("%s\n", reply); fflush(stdout);
		if(firsttime) {
			firsttime = 0;
			strncpy(buff, reply, 4);
			buff[3] = ' ';
		}
	   } while(strncmp(reply, buff, 3) || reply[3] == '-');
	   s = atoi(buff);
   } while(s < 200 && s != 125 & s != 150);

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
   if(*ftparg)
	fprintf(fpcommout, "%s %s\r\n", ftpcommand, ftparg);
   else
   	fprintf(fpcommout, "%s\r\n", ftpcommand);

   fflush(fpcommout);

   return(DOgetreply());
}

int DOhelp()
{
char junk[10];

   printf("Command:      Description\n");
   printf("!             Escape to a shell\n");
   printf("append        Append a file to remote host\n");
   printf("ascii         Set file transfer mode to ascii\n");
   printf("binary        Set file transfer mode to binary\n");
   printf("bye           Close connection and exit\n");
   printf("cd            Change directory on remote host\n");
   printf("close         Close connection\n");
   printf("del           Remove file on remote host\n");
   printf("dir           Display long form remote host directory listing\n");
   printf("exit          Close connection and exit\n");
   printf("get           Retrieve a file from remote host\n");
   printf("help          Display this text\n");
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

   readline("Press ENTER to continue... ", junk, sizeof(junk));

   printf("mput          Send multiple files to remote host\n");
   printf("noop          Send the ftp NOOP command\n");
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
   printf("site          Send a site specific command\n");
   printf("size          Get file size information\n");
   printf("status        Get connection/file status information\n");
   printf("system        Get remote system type information\n");
   printf("user          Enter remote user information\n");

   return(0);
}

struct commands {
	char *name;
	_PROTOTYPE(int (*func), (void));
};

static struct commands commands[] = {
        "!",            DOlshell,
	"append",	DOappe,
	"ascii",        DOascii,
	"binary",       DObinary,
	"bin",          DObinary,
	"bye",          DOquit,
	"cd",           DOcd,
	"close",        DOclose,
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
	"system",	DOsyst,
	"user",         DOuser,
	"",     (int (*)())0
};

int main(argc, argv)
int argc;
char *argv[];
{
int s;
struct commands *cmd;
static char buffer[128];

   NETinit();

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
	readline("ftp>", buffer, sizeof(buffer));
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

   return(0);
}
