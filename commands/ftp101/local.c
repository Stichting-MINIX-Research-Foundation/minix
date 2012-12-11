/* local.c Copyright 1992-2000 by Michael Temari All Rights Reserved
 *
 * This file is part of ftp.
 *
 *
 * 01/25/96 Initial Release	Michael Temari, <Michael@TemWare.Com>
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

static void dodir(char *path, int full);

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

   path = cmdargv[1];

   if(cmdargc < 2) {
	if(readline("Path: ", line2, sizeof(line2)) < 0)
		return(-1);
	path = line2;
   }

   if(chdir(path))
	printf("Could not change local directory. %s\n", strerror(errno));
   else
	return(DOlpwd());
   
   return(0);
}

int DOlmkdir()
{
char *path;

   path = cmdargv[1];

   if(cmdargc < 2) {
	if(readline("Path: ", line2, sizeof(line2)) < 0)
		return(-1);
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

   path = cmdargv[1];

   if(cmdargc < 2) {
	if(readline("Path: ", line2, sizeof(line2)) < 0)
		return(-1);
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

   return(0);
}

int DOlnlst(void)
{
   dodir(".", 0);

   return(0);
}

int DOlshell(void)
{
   (void) system("$SHELL");

   return(0);
}

static void dodir(path, full)
char *path;
int full;
{
static char cmd[128];
static char name[32];

   (void) tmpnam(name);

   if(full)
	sprintf(cmd, "ls -l %s > %s", path, name);
   else
	sprintf(cmd, "ls %s > %s", path, name);

   (void) system(cmd);
   sprintf(cmd, "more %s", name);
   (void) system(cmd);
   sprintf(cmd, "rm %s", name);
   (void) system(cmd);
}
