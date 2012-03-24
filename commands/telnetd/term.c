/*
 * TNET		A server program for MINIX which implements the TCP/IP
 *		suite of networking protocols.  It is based on the
 *		TCP/IP code written by Phil Karn et al, as found in
 *		his NET package for Packet Radio communications.
 *
 *		Handle the TERMINAL module.
 *
 * Author:	Fred N. van Kempen, <waltje@uwalt.nl.mugnet.org>
 *              Michael Temari, <temari@temari.ae.ge.com>
 *
 * 07/29/92 MT  Telnet options hack which seems to work okay
 * 01/12/93 MT  Better telnet options processing instead of hack
 */
#include <sys/types.h>
#include <errno.h>
#if 0
#include <fcntl.h>
#endif
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include "telnet.h"
#include "telnetd.h"

void sig_done(int sig);

static char buff[4096];

void term_init()
{
  tel_init();

  telopt(1, WILL, TELOPT_SGA);
  telopt(1, DO,   TELOPT_SGA);
  telopt(1, WILL, TELOPT_BINARY);
  telopt(1, DO,   TELOPT_BINARY);
  telopt(1, WILL, TELOPT_ECHO);
  telopt(1, DO,   TELOPT_WINCH);
}

static int io_done = 0;

void term_inout(pty_fd)
int pty_fd;
{
register int i;
pid_t pid;
struct sigaction sa;

  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = sig_done;
  sigaction(SIGALRM, &sa, (struct sigaction *) NULL);

  if ((pid = fork()) == -1) {
	sprintf(buff, "telnetd: fork() failed: %s\r\n", strerror(errno));
	(void) write(1, buff, strlen(buff));
  }

  if (pid != 0) {
	/* network -> login process */
	while (!io_done && (i = read(0, buff, sizeof(buff))) > 0) {
		tel_in(pty_fd, 1, buff, i);
	}
	/* EOF, kill opposite number and exit. */
	(void) kill(pid, SIGKILL);
  } else {
  	/* login process -> network */
	while ((i = read(pty_fd, buff, sizeof(buff))) > 0) {
		tel_out(1, buff, i);
	}
	/* EOF, alert opposite number and exit. */
	(void) kill(getppid(), SIGALRM);
  }
  /* EOF. */
}

void sig_done(sig)
int sig;
{
  io_done = 1;
  alarm(1);			/* there is always a chance... */
}
