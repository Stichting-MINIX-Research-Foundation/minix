/* other.c by Michael Temari 06/21/92
 *
 * ftp          An ftp client program for use with TNET.
 *
 * Author:      Michael Temari, <temari@ix.netcom.com>
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

void FTPinit()
{
   linkopen = 0;
   loggedin = 0;
   type = TYPE_A;
   format = 0;
   mode = 0;
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

   if(cmdargc < 2) {
	tcgetattr(fileno(stdout), &oldtty);
	newtty = oldtty;
	newtty.c_lflag &= ~ECHO;
	tcsetattr(fileno(stdout), TCSANOW, &newtty);
	readline("Password: ", password, sizeof(password));
	tcsetattr(fileno(stdout), TCSANOW, &oldtty);
	printf("\n");
	pass = password;
   }

   s = DOcommand("PASS", pass);

   if(s == 230)
	loggedin = 1;

   return(s);
}

int DOuser()
{
char *user;
int s;
char username[64];

   if(!linkopen) {
	printf("You must \"OPEN\" a connection first.\n");
	return(0);
   }

   loggedin = 0;

   user = cmdargv[1];

   if(cmdargc < 2) {
	readline("Username: ", username, sizeof(username));
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

int DOquote()
{
int i;
static char args[512];

   args[0] = '\0';

   for(i = 2; i < cmdargc; i++) {
	if(i != 2)
		strcat(args, " ");
	strcat(args, cmdargv[i]);
   }

   return(DOcommand(cmdargv[1], args));
}

int DOsite()
{
int i;
static char args[512];

   args[0] = '\0';

   for(i = 1; i < cmdargc; i++) {
   	if(i != 1)
		strcat(args, " ");
	strcat(args, cmdargv[i]);
   }

   return(DOcommand("SITE", args));
}
