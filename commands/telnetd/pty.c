/*
 * TNET		A server program for MINIX which implements the TCP/IP
 *		suite of networking protocols.  It is based on the
 *		TCP/IP code written by Phil Karn et al, as found in
 *		his NET package for Packet Radio communications.
 *
 *		Handle the allocation of a PTY.
 *
 * Author:	Fred N. van Kempen, <waltje@uwalt.nl.mugnet.org>
 */
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "telnetd.h"


#define DEV_DIR		"/dev"

/*
 * Allocate a PTY, by trying to open one repeatedly,
 * until all PTY channels are done.  If at that point
 * no PTY is found, go into panic mode :-(
 */
int get_pty(pty_fdp, tty_namep)
int *pty_fdp;
char **tty_namep;
{
  char buff[128], temp[128];
  register int i, j;
  int pty_fd;
  static char tty_name[128];

  for(i = 'p'; i < 'w'; i++) {
	j = 0;
	do {
		sprintf(buff, "%s/pty%c%c",
			DEV_DIR, i, (j < 10) ? j + '0' : j + 'a' - 10);

		if (opt_d == 1) {
			(void) write(2, "Testing: ", 9);
			(void) write(2, buff, strlen(buff));
			(void) write(2, "...: ", 5);
		}

		pty_fd = open(buff, O_RDWR);
		if (opt_d == 1) {
			if (pty_fd < 0) sprintf(temp, "error %d\r\n", errno);
			  else sprintf(temp, "OK\r\n");
			(void) write(2, temp, strlen(temp));
		}

		if (pty_fd >= 0) break;

		j++;
		if (j == 16) break;
	} while(1);

	/* Did we find one? */
	if (j < 16) break;
  }
  if (pty_fd < 0) return(-1);

  if (opt_d == 1) {
	sprintf(temp, "File %s, desc %d\n", buff, pty_fd);
	(void) write(1, temp, strlen(temp));
  }

  sprintf(tty_name, "%s/tty%c%c", DEV_DIR,
  					i, (j < 10) ? j + '0' : j + 'a' - 10);

  *pty_fdp = pty_fd;
  *tty_namep = tty_name;
  return(0);
}
