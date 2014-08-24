/*
 * TNET		A server program for MINIX which implements the TCP/IP
 *		suite of networking protocols.  It is based on the
 *		TCP/IP code written by Phil Karn et al, as found in
 *		his NET package for Packet Radio communications.
 *
 *		This file contains an implementation of the "server"
 *		for the TELNET protocol.  This protocol can be used to
 *		remote-login on other systems, just like a normal TTY
 *		session.
 *
 * Usage:	telnetd [-dv]
 *
 * Version:	@(#)telnetd.c	1.00	07/26/92
 *
 * Author:	Fred N. van Kempen, <waltje@uwalt.nl.mugnet.org>
 *		Michael Temari, <temari@temari.ae.ge.com>
 */
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <ttyent.h>
#include <utmp.h>
#include <net/gen/in.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/gen/socket.h>
#include <netdb.h>
#include <net/gen/inet.h>
#include "telnetd.h"

#if 0
static char *Version = "@(#) telnetd 1.00 (07/26/92)";
#endif

int opt_d = 0;				/* debugging output flag	*/

void usage(void);
int main(int argc, char *argv[]);

void usage()
{
   fprintf(stderr, "Usage: telnetd [-dv]\n");

   exit(-1);
}

int main(argc, argv)
int argc;
char *argv[];
{
char buff[128];
register int c;
int pty_fd;
int tty_fd;
pid_t pid;
int lineno;
char *tty_name;
struct ttyent *ttyp;
nwio_tcpconf_t tcpconf;
struct hostent *hostent;
char *hostname;

   opterr = 0;
   while ((c = getopt(argc, argv, "dv")) != EOF) switch(c) {
	case 'd':
	case 'v':
		opt_d = 1;
		break;
	default:
		usage();
   }

   /* No more arguments allowed. */
   if (optind != argc) usage();

   /* Obtain the name of the remote host. */
   if (ioctl(0, NWIOGTCPCONF, &tcpconf) < 0) {
	sprintf(buff, "Unable to obtain your IP address\r\n");
	(void) write(1, buff, strlen(buff));
	return(-1);
   }
   if ((hostent = gethostbyaddr((char *) &tcpconf.nwtc_remaddr,
			sizeof(tcpconf.nwtc_remaddr), AF_INET)) != NULL) {
	hostname = hostent->h_name;
   } else {
	hostname = inet_ntoa(tcpconf.nwtc_remaddr);
   }

   /* Try allocating a PTY. */
   if (get_pty(&pty_fd, &tty_name) < 0) {
	sprintf(buff, "I am sorry, but there is no free PTY left!\r\n");
	(void) write(1, buff, strlen(buff));
	return(-1);
   }

   /* Find the tty in the tty table. */
   lineno = 0;
   for (;;) {
	if ((ttyp = getttyent()) == NULL) {
		sprintf(buff, "Can't find the tty entry in the tty table\r\n");
		(void) write(1, buff, strlen(buff));
	}
	if (strcmp(ttyp->ty_name, tty_name+5) == 0) break;
	lineno++;
   }
   endttyent();

   /* Initialize the connection to an 8 bit clean channel. */
   term_init();

   /* Fork off a child process and have it execute a getty(8). */
   if ((pid = fork()) == 0) {
	/* Set up a new session. */
	setsid();
	if ((tty_fd = open(tty_name, O_RDWR)) < 0) {
		sprintf(buff, "Can't open %s\r\n", tty_name);
		(void) write(1, buff, strlen(buff));
		return(-1);
	}

	close(pty_fd);
	dup2(tty_fd, 0);
	dup2(tty_fd, 1);
	dup2(tty_fd, 2);
	close(tty_fd);
	(void) execl("/usr/sbin/getty", "getty", (char *)NULL);
	(void) execl("/usr/bin/getty", "getty", (char *)NULL);
	(void) execl("/usr/bin/login", "login", (char *)NULL);
	(void) write(1, "EXEC failed!\r\n", 14);
   } else if (pid < 0) {
	sprintf(buff, "I am sorry, but the fork(2) call failed!\r\n");
	(void) write(1, buff, strlen(buff));
	(void) close(pty_fd);
	return(-1);
   }

   term_inout(pty_fd);

   (void) close(pty_fd);

   chown(tty_name, 0, 0);
   chmod(tty_name, 0666);

   return(0);
}
