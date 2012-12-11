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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>
#include <time.h>
#include <stdio.h>
#include "telnetd.h"

static char PATH_UTMP[] = "/etc/utmp";
static char PATH_WTMP[] = "/usr/adm/wtmp";

void wtmp(int type, int linenr, char *line, pid_t pid, char *host);
void report(char *label);

void wtmp(type, linenr, line, pid, host)
int type;			/* type of entry */
int linenr;			/* line number in ttytab */
char *line;			/* tty name (only good on login) */
pid_t pid;			/* pid of process */
char *host;			/* name of the remote host */
{
/* Log an event into the UTMP and WTMP files. */

  struct utmp utmp;		/* UTMP/WTMP User Accounting */
  int fd;

  /* Clear the utmp record. */
  memset((void *) &utmp, 0, sizeof(utmp));

  /* Fill in utmp. */
  switch (type) {
  case LOGIN_PROCESS:
  	/* A new login, fill in line and host name. */
	strncpy(utmp.ut_line, line, sizeof(utmp.ut_line));
	strncpy(utmp.ut_host, host, sizeof(utmp.ut_host));
	break;

  case DEAD_PROCESS:
	/* A logout.  Use the current utmp entry, but make sure it is a
	 * user process exiting, and not getty or login giving up.
	 */
	if ((fd = open(PATH_UTMP, O_RDONLY)) < 0) {
		if (errno != ENOENT) report(PATH_UTMP);
		return;
	}
	if (lseek(fd, (off_t) (linenr+1) * sizeof(utmp), SEEK_SET) == -1
		|| read(fd, &utmp, sizeof(utmp)) == -1
	) {
		report(PATH_UTMP);
		close(fd);
		return;
	}
	close(fd);
	if (utmp.ut_type != USER_PROCESS) return;
	strncpy(utmp.ut_name, "", sizeof(utmp.ut_name));
	break;
  }

  /* Finish new utmp entry. */
  utmp.ut_pid = pid;
  utmp.ut_type = type;
  utmp.ut_time = time((time_t *) 0);

  /* Write new entry to utmp. */
  if ((fd = open(PATH_UTMP, O_WRONLY)) < 0
	|| lseek(fd, (off_t) (linenr+1) * sizeof(utmp), SEEK_SET) == -1
	|| write(fd, &utmp, sizeof(utmp)) == -1
  ) {
	if (errno != ENOENT) report(PATH_UTMP);
  }
  if (fd != -1) close(fd);

  if (type == DEAD_PROCESS) {
	/* Add new wtmp entry. */
	if ((fd = open(PATH_WTMP, O_WRONLY | O_APPEND)) < 0
		  || write(fd, &utmp, sizeof(utmp)) == -1
	) {
		if (errno != ENOENT) report(PATH_WTMP);
	}
	if (fd != -1) close(fd);
  }
}

void report(label)
char *label;
{
  char message[128];

  sprintf(message, "telnetd: %i: %s\r\n", errno, strerror(errno));
  (void) write(1, message, strlen(message));
}
