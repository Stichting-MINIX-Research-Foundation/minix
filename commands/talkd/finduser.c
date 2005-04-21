/* finduser.c Copyright Michael Temari 07/22/1996 All Rights Reserved */

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <utmp.h>
#include <net/gen/in.h>

#include "talk.h"
#include "finduser.h"

int find_user(name, tty)
char *name;
char *tty;
{
int fd;
int ret;
struct utmp utmp;

   /* Now find out if the requested user is logged in. */
   if((fd = open(UTMP, O_RDONLY)) < 0) {
	perror("talkd: opening UTMP file");
	return(FAILED);
   }

   ret = NOT_HERE;

   while(read(fd, &utmp, sizeof(struct utmp)) == sizeof(struct utmp)) {
	if(utmp.ut_type != USER_PROCESS) continue;
	if(strncmp(utmp.ut_user, name, sizeof(utmp.ut_user))) continue;
	if(*tty && strncmp(utmp.ut_line, tty, sizeof(utmp.ut_line))) continue;
	strcpy(tty, utmp.ut_line);
	ret = SUCCESS;
	break;
   }

   close(fd);

   return(ret);
}
