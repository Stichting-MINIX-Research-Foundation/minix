/* local.c
 *
 * This file is part of ftp.
 *
 *
 * 01/25/96 Initial Release	Michael Temari, <temari@ix.netcom.com>
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "ftp.h"
#include "local.h"

static char line2[512];

_PROTOTYPE(static void dodir, (char *path, int full));

int DOlpwd()
{
   if(getcwd(line2, sizeof(line2)) == (char *)NULL)
	printf("Could not determine local directory. %s\n", strerror(errno));
   else
	printf("Current local directory: %s\n", line2);

   return(0);
}

int DOlcd()
{
char *path;
int s;

   path = cmdargv[1];

   if(cmdargc < 2) {
	readline("Path: ", line2, sizeof(line2));
	path = line2;
   }

   if(chdir(path))
	printf("Could not change local directory. %s\n", strerror(errno));
   else
	DOlpwd();
   
   return(0);
}

int DOlmkdir()
{
char *path;
int s;

   path = cmdargv[1];

   if(cmdargc < 2) {
	readline("Directory: ", line2, sizeof(line2));
	path = line2;
   }

   if(mkdir(path, 0777))
	printf("Could not make directory %s. %s\n", path, strerror(errno));
   else
	printf("Directory created.\n");
   
   return(0);
}

int DOlrmdir()
{
char *path;
int s;

   path = cmdargv[1];

   if(cmdargc < 2) {
	readline("Directory: ", line2, sizeof(line2));
	path = line2;
   }

   if(rmdir(path))
	printf("Could not remove directory %s. %s\n", path, strerror(errno));
   else
	printf("Directory removed.\n");
   
   return(0);
}

int DOllist(void)
{
   dodir(".", 1);
}

int DOlnlst(void)
{
   dodir(".", 0);
}

int DOlshell(void)
{
   system("$SHELL");
}

static void dodir(path, full)
char *path;
int full;
{
char cmd[128];
static char name[32];

   tmpnam(name);

   if(full)
	sprintf(cmd, "ls -l %s > %s", path, name);
   else
	sprintf(cmd, "ls %s > %s", path, name);

   system(cmd);
   sprintf(cmd, "more %s", name);
   system(cmd);
   sprintf(cmd, "rm %s", name);
   system(cmd);
}
