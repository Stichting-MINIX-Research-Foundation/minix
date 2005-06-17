/* access.c Copyright 1992-2000 by Michael Temari All Rights Reserved
 *
 * This file is part of ftpd.
 *
 * This file handles:
 *
 *      USER PASS QUIT
 *
 *
 * 01/25/96 Initial Release	Michael Temari, <Michael@TemWare.Com>
 */

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>
#include <net/gen/in.h>
#include <net/gen/tcp.h>

#include "ftpd.h"
#include "access.h"

_PROTOTYPE(static int AreWeIn, (char *name, struct passwd *pwd));

static char *msg530 = "530 Not logged in.\r\n";

/* Returns -1 = not logged in, 0 = loggedin */
int ChkLoggedIn()
{
   if(!loggedin) {
	printf(msg530);
	return(-1);
   } else
	return(0);
}

/* what a USER! */
int doUSER(buff)
char *buff;
{
   loggedin = 0;
   gotuser = 0;
   strncpy(username, buff, sizeof(username));
   username[sizeof(username)-1] = '\0';

   if(*username == '\0') {
	printf("501 Bad user name.\r\n");
	return(GOOD);
   }

   gotuser = 1;

   printf("331 Password required for %s.\r\n", username);

   return(GOOD);
}

/* secret, secret, secret */
int doPASS(buff)
char *buff;
{
char *name;
struct passwd *pwd;
int bad=0;

   name = username;

   if(!strcmp(name, "anonymous"))
	name = "ftp";

   if(!gotuser || ((pwd = getpwnam(name)) == (struct passwd *)0))
	bad = 1;
   else
	if(strcmp(name, "ftp")) {
		if(!strcmp(pwd->pw_passwd, crypt("", pwd->pw_passwd)))
			bad = 1;
		if(strcmp(pwd->pw_passwd, crypt(buff, pwd->pw_passwd)))
			bad = 1;
	} else {
		strncpy(anonpass, buff, sizeof(anonpass));
		anonpass[sizeof(anonpass)-1] = '\0';
	}

   if(bad) {
	logit("LOGIN", "FAIL");
	printf(msg530);
	return(GOOD);
   }

   return(AreWeIn(name, pwd));
}

/* bye, bye don't let the door hit you in the butt on the way out */
int doQUIT(buff)
char *buff;
{
   printf("221 Service closing, don't be a stranger.\r\n");

   return(BAD);
}

/* see if this user is okay */
static int AreWeIn(name, pwd)
char *name;
struct passwd *pwd;
{
   if(!strcmp(name, "ftp")) {
	if(chroot(pwd->pw_dir)) {
		logit("LOGIN", "FAIL");
		printf("530 Not logged in, could not chroot.\r\n");
		return(GOOD);
	}
	strncpy(newroot, pwd->pw_dir, sizeof(newroot));
	newroot[sizeof(newroot)-1] = '\0';
	anonymous = 1;
	strcpy(pwd->pw_dir, "/");
   }

   if(setgid(pwd->pw_gid) || setuid(pwd->pw_uid) || chdir(pwd->pw_dir)) {
	logit("LOGIN", "FAIL");
	printf(msg530);
	anonymous = 0;
   } else {
	logit("LOGIN", "PASS");
	showmsg("230", (char *)NULL);
	printf("230 User %s logged in, directory %s.\r\n",
		username, pwd->pw_dir);
	loggedin = 1;
   }

   return(GOOD);
}
