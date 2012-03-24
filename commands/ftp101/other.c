/* other.c Copyright 1992-2000 by Michael Temari All Rights Reserved
 *
 * ftp          An ftp client program for use with TNET.
 *
 * Author:      Michael Temari, <Michael@TemWare.Com>
 */

#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "ftp.h"
#include "other.h"

static int docmdargs(char *cmd, int fa);

void FTPinit()
{
   linkopen = 0;
   loggedin = 0;
   type = TYPE_A;
   format = 0;
   mode = MODE_S;
   structure = 0;
   passive = 0;
   atty = isatty(0);
}

int DOpass()
{
int s;
struct termios oldtty, newtty;
char *pass;
char password[64];

   if(!linkopen) {
	printf("You must \"OPEN\" a connection first.\n");
	return(0);
   }

   pass = cmdargv[1];

   s = 0;

   if(cmdargc < 2) {
   	if(atty) {
		tcgetattr(fileno(stdout), &oldtty);
		newtty = oldtty;
		newtty.c_lflag &= ~ECHO;
		tcsetattr(fileno(stdout), TCSANOW, &newtty);
	}
	s = readline("Password: ", password, sizeof(password));
	if(atty) {
		tcsetattr(fileno(stdout), TCSANOW, &oldtty);
		printf("\n");
	}
	pass = password;
   }

   if(s < 0)
   	return(-1);

   s = DOcommand("PASS", pass);

   if(s == 230)
	loggedin = 1;

   return(s);
}

int DOuser()
{
char *user;
int s;
char username[32];

   if(!linkopen) {
	printf("You must \"OPEN\" a connection first.\n");
	return(0);
   }

   loggedin = 0;

   user = cmdargv[1];

   s = 0;

   if(cmdargc < 2) {
	if(readline("Username: ", username, sizeof(username)) < 0)
		return(-1);
	user = username;
   }

   s = DOcommand("USER", user);

   if(atty && s == 331) {
   	cmdargv[0] = "password";
   	cmdargc = 1;
	return(DOpass());
   }

   if(s == 230)
	loggedin = 1;

   return(s);
}

int DOnoop()
{
   if(DOcmdcheck())
	return(0);

   return(DOcommand("NOOP", ""));
}

int DOpassive()
{
   passive = 1 - passive;

   printf("Passive mode is now %s\n", (passive ? "ON" : "OFF"));

   return(0);
}

int DOsyst()
{
   if(DOcmdcheck())
	return(0);

   return(DOcommand("SYST", ""));
}

int DOremotehelp()
{
   if(!linkopen) {
	printf("You must \"OPEN\" a connection first.\n");
	return(0);
   }

   return(DOcommand("HELP", ""));
}

static int docmdargs(cmd, fa)
char *cmd;
int fa;
{
int i;
static char args[512];

   args[0] = '\0';

   for(i = fa; i < cmdargc; i++) {
	if(i != fa)
		strcat(args, " ");
	strcat(args, cmdargv[i]);
   }

   return(DOcommand(cmd, args));
}

int DOquote()
{
   return(docmdargs(cmdargv[1], 2));
}

int DOsite()
{
   return(docmdargs("SITE", 1));
}
