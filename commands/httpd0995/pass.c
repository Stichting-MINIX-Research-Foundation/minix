/* pass.c
 *
 * This file is part of httpd.
 *
 * 07/07/1996 Initial Release	Michael Temari <Michael@TemWare.Com>
 * 12/29/2002 			Michael Temari <Michael@TemWare.Com>
 *
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pwd.h>
#ifdef _MINIX
#include <minix/minlib.h>
#endif

#define	STD_PASSWD_FILE	"/etc/passwd"

#include "pass.h"

static char buffer[1024];
static char *pwduser;
static char *pwdpass;
static char *pwde[4];

_PROTOTYPE(static int getuser, (char *pwdfile, char *user));

static int getuser(pwdfile, user)
char *pwdfile;
char *user;
{
FILE *fp;
char *p;
int i;

   if((fp = fopen(pwdfile, "r")) == (FILE *)NULL)
	return(-1);

   for(i = 0; i < 4; i ++) pwde[i] = "";

   while(1) {
	if(fgets(buffer, sizeof(buffer), fp) == (char *)NULL) {
		fclose(fp);
		return(-1);
	}
	p = buffer;
	pwduser = p;
	while(*p && *p != ':') p++;
	if(*p != ':') continue;
	*p++ = '\0';
	if(strcmp(pwduser, user)) continue;
	pwdpass = p;
	while(*p && *p != ':' && *p != '\r' && *p != '\n') p++;
	if(*p == ':')
		*p++ = '\0';
	else {
		if(*p) *p = '\0';
		fclose(fp);
	}
	for(i = 0; i < 4; i++) {
		pwde[i] = p;
		while(*p && *p != ':' && *p != '\r' && *p != '\n') p++;
		if(*p == ':')
			*p++ = '\0';
		else {
			if(*p) *p = '\0';
			break;
		}
	}
	fclose(fp);

	return(0);
   }
}

int passfile(pwdfile)
char *pwdfile;
{
FILE *fp;

   if(!strcmp(pwdfile, STD_PASSWD_FILE))
	return(0);

   if((fp = fopen(pwdfile, "r")) == (FILE *)NULL)
	return(-1);

   fclose(fp);

   return(0);
}

int passuser(pwdfile, user)
char *pwdfile;
char *user;
{
   if(!strcmp(pwdfile, STD_PASSWD_FILE))
	if(getpwnam(user) == (struct passwd *)NULL)
		return(-1);
	else
		return(0);

   return(getuser(pwdfile, user));
}

int passnone(pwdfile, user)
char *pwdfile;
char *user;
{
struct passwd *pwd;

   if(!strcmp(pwdfile, STD_PASSWD_FILE))
	if((pwd = getpwnam(user)) == (struct passwd *)NULL)
		return(-1);
	else
		if(!strcmp(pwd->pw_passwd, crypt("", pwd->pw_passwd)))
			return(-1);
		else
			return(0);

   if(getuser(pwdfile, user))
	return(-1);

   if(!strcmp(pwdpass, crypt("", pwdpass)))
	return(-1);
   else
	return(0);
}

int passpass(pwdfile, user, pass)
char *pwdfile;
char *user;
char *pass;
{
struct passwd *pwd;

   if(!strcmp(pwdfile, STD_PASSWD_FILE))
	if((pwd = getpwnam(user)) == (struct passwd *)NULL)
		return(-1);
	else
		if(strcmp(pwd->pw_passwd, crypt(pass, pwd->pw_passwd)))
			return(-1);
		else
			return(0);

   if(getuser(pwdfile, user))
	return(-1);

   if(strcmp(pwdpass, crypt(pass, pwdpass)))
	return(-1);
   else
	return(0);
}

int passadd(pwdfile, user, pass, e1, e2, e3, e4)
char *pwdfile;
char *user;
char *pass;
char *e1;
char *e2;
char *e3;
char *e4;
{
FILE *fp;
time_t salt;
char sl[2];
int cn;
char *ee1;
char *ee2;
char *ee3;
char *ee4;


   if(pwdfile == (char *)NULL ||
	 user == (char *)NULL ||
	 pass == (char *)NULL)
	return(PASS_ERROR);

   if(!strcmp(pwdfile, STD_PASSWD_FILE))
	return(PASS_ERROR);

   if(!getuser(pwdfile, user))
	return(PASS_USEREXISTS);

   time(&salt);
   sl[0] = (salt & 077) + '.';
   sl[1] = ((salt >> 6) & 077) + '.';
   for (cn = 0; cn < 2; cn++) {
	if (sl[cn] > '9') sl[cn] += 7;
	if (sl[cn] > 'Z') sl[cn] += 6;
   }

   if(e1 == (char *)NULL) ee1 = ""; else ee1 = e1;
   if(e2 == (char *)NULL) ee2 = ""; else ee2 = e2;
   if(e3 == (char *)NULL) ee3 = ""; else ee3 = e3;
   if(e4 == (char *)NULL) ee4 = ""; else ee4 = e4;

   /* XXX need to add locking mechanics to add new user */

   if((fp = fopen(pwdfile, "a")) == (FILE *)NULL)
	return(PASS_ERROR);

   fprintf(fp, "%s:%s:%s:%s:%s:%s\n", user, crypt(pass, sl), ee1, ee2, ee3, ee4);

   fclose(fp);

   /* XXX need to add unlocking mechanics to add new user */

   return(PASS_GOOD);
}
