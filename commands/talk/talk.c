/* talk.c Copyright Michael Temari 08/01/1996 All Rights Reserved */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <utmp.h>
#include <termios.h>
#include <net/gen/netdb.h>
#include <net/hton.h>
#include <net/gen/socket.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <net/gen/tcp.h>
#include <net/gen/udp.h>

#include "talk.h"
#include "proto.h"
#include "net.h"
#include "screen.h"

_PROTOTYPE(int main, (int argc, char *argv[]));
_PROTOTYPE(void DoTalk, (void));

int main(argc, argv)
int argc;
char *argv[];
{
char *p;
struct hostent *hp;
struct stat st;
struct utmp utmp;
int slot;
FILE *fp;

   if(argc < 2 || argc > 3) {
   	fprintf(stderr, "Usage: talk user[@host] [tty]\n");
   	return(-1);
   }

   /* get local host name */
   if(gethostname(lhost, HOST_SIZE) < 0) {
   	fprintf(stderr, "talk: Error getting local host name\n");
   	return(-1);
   }

   /* get local user name and tty */
   if((slot = ttyslot()) < 0) {
   	fprintf(stderr, "talk: You are not on a terminal\n");
   	return(-1);
   }
   if((fp = fopen(UTMP, "r")) == (FILE *)NULL) {
   	fprintf(stderr, "talk: Could not open %s\n", UTMP);
   	return(-1);
   }
   if(fseek(fp, (off_t) sizeof(utmp) * slot, SEEK_SET)) {
   	fprintf(stderr, "talk: Could not seek %s\n", UTMP);
   	fclose(fp);
   	return(-1);
   }
   if(fread((char *)&utmp, sizeof(utmp), 1 , fp) != 1) {
   	fprintf(stderr, "talk: Could not read %s\n", UTMP);
   	fclose(fp);
   	return(-1);
   }
   fclose(fp);
   strncpy(luser, utmp.ut_user, USER_SIZE < sizeof(utmp.ut_user) ?
   				USER_SIZE : sizeof(utmp.ut_user));
   luser[USER_SIZE] = '\0';

   /* get local tty */
   if((p = ttyname(0)) == (char *)NULL) {
   	fprintf(stderr, "talk: You are not on a terminal\n");
   	return(-1);
   }
   strncpy(ltty, p+5, TTY_SIZE);
   ltty[TTY_SIZE] = '\0';

   /* check if local tty is going to be writable */
   if(stat(p, &st) < 0) {
   	perror("talk: Could not stat local tty");
   	return(-1);
   }
   if((st.st_mode & S_IWGRP) == 0) {
   	fprintf(stderr, "talk: Your terminal is not writable.  Use: mesg y\n");
   	return(-1);
   }

   /* get remote user and host name */
   if((p = strchr(argv[1], '@')) != (char *)NULL)
   	*p++ = '\0';
   else
   	p = lhost;
   strncpy(ruser, argv[1], USER_SIZE);
   ruser[USER_SIZE] = '\0';
   strncpy(rhost, p, HOST_SIZE);
   rhost[HOST_SIZE] = '\0';

   /* get remote tty */
   if(argc > 2)
   	strncpy(rtty, argv[2], TTY_SIZE);
   else
   	rtty[0] = '\0';
   rtty[TTY_SIZE] = '\0';

   if((hp = gethostbyname(rhost)) == (struct hostent *)NULL) {
   	fprintf(stderr, "talk: Could not determine address of %s\n", rhost);
   	return(-1);
   }
   memcpy((char *)&raddr, (char *)hp->h_addr, hp->h_length);

   if(NetInit()) {
   	fprintf(stderr, "talk: Error in NetInit\n");
   	return(-1);
   }

   if(ScreenInit())
   	return(-1);

   if(!TalkInit())
	DoTalk();

   ScreenEnd();

   return(0);
}

struct pdata {
	int win;
	int len;
	char buffer[64];
} pdata;

void DoTalk()
{
int s;
int s2;
int kid;
int pfd[2];
int win;
int len;
struct termios termios;
char lcc[3];
char rcc[3];

   ScreenMsg("");
   ScreenWho(ruser, rhost);

   /* Get and send edit characters */
   s = tcgetattr(0, &termios);
   if(s < 0) {
   	perror("talk: tcgetattr");
   	return;
   }
   lcc[0] = termios.c_cc[VERASE];
   lcc[1] = termios.c_cc[VKILL];
   lcc[2] = 0x17; /* Control - W */
   s = write(tcp_fd, lcc, sizeof(lcc));
   if(s != sizeof(lcc)) {
   	ScreenMsg("Connection Closing due to error");
   	return;
   }
   s = read(tcp_fd, rcc, sizeof(rcc));
   if(s != sizeof(rcc)) {
   	ScreenMsg("Connection Closing due to error");
   	return;
   }
   ScreenEdit(lcc, rcc);

   s = pipe(pfd);
   if(s < 0) {
   	ScreenMsg("Could not create pipes");
   	return;
   }

   if((kid = fork()) < 0) {
   	ScreenMsg("Could not fork");
   	close(pfd[0]);
   	close(pfd[1]);
   	return;
   }

   if(kid == 0) {
   	close(tcp_fd);
   	close(pfd[1]);
   	while(1) {
   		s = read(pfd[0], &pdata, sizeof(pdata));
   		if(s != sizeof(pdata)) {
   			close(pfd[0]);
   			exit(-1);
   		}
   		ScreenPut(pdata.buffer, pdata.len, pdata.win);
   	}
   }

   close(pfd[0]);

   if((kid = fork()) < 0) {
   	ScreenMsg("Could not fork");
   	close(pfd[1]);
   	return;
   }

   if(kid == 0) {
   	pdata.win = REMOTEWIN;
   	while(!ScreenDone) {
	   	s = read(tcp_fd, pdata.buffer, sizeof(pdata.buffer));
   		if(s <= 0)
   			break;
   		pdata.len = s;
		write(pfd[1], &pdata, sizeof(pdata));
   	}
   	close(pfd[1]);
   	close(tcp_fd);
	kill(getppid(), SIGINT);
   	exit(-1);
   }

   pdata.win = LOCALWIN;
   while(!ScreenDone) {
	s = read(0, pdata.buffer, sizeof(pdata.buffer));
	if(s <= 0)
		break;
	pdata.len = s;
	write(pfd[1], &pdata, sizeof(pdata));
	s2 = write(tcp_fd, pdata.buffer, s);
	if(s2 != s)
		break;
   }
   kill(kid, SIGINT);
   close(pfd[1]);
   close(tcp_fd);
   return;
}
