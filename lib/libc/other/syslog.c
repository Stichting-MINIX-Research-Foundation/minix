/* Copyright (c) 1983, 1988, 1993
 * The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * #if defined(LIBC_SCCS) && !defined(lint)
 * static char sccsid[] = "@(#)syslog.c    8.4 (Berkeley) 3/18/94";
 * #endif
 *
 * Author: Eric Allman
 * Modified to use UNIX domain IPC by Ralph Campbell
 * Patched March 12, 1996 by A. Ian Vogelesang <vogelesang@hdshq.com>
 * Rewritten by Martin Mares <mj@atrey.karlin.mff.cuni.cz> on May 14, 1997
 * Rewritten by G. Falzoni <gfalzoni@inwind.it> for porting to Minix
 */
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <net/netlib.h>
#include <net/hton.h>
#include <net/gen/in.h>
#include <net/gen/udp.h>
#include <net/gen/udp_io.h>
#include <net/gen/netdb.h>
#include <errno.h>
#include <net/gen/inet.h>

static int LogPid = (-1);
static int nfd = (-1);
static int LogFacility = LOG_USER;
static int LogFlags = 0;
static char TagBuffer[40] = "syslog";

/*
** OPENLOG -- open system log
** 	- establishes a channel to syslogd using UDP device
**	  (port 514 is used _ syslog/udp)
**	- stores program tag (if not NULL) and other options
**	  for use by syslog
*/
void openlog(const char *ident, int option, int facility)
{
  struct nwio_udpopt udpopt;

  /* Stores logging flags */
  LogFlags = option & (LOG_PID | LOG_PERROR | LOG_CONS);
  /* Stores process id. if LOG_PID was specified */
  if (option & LOG_PID) LogPid = getpid();
  /* Stores the requested facility */
  LogFacility = facility;
  /* Stores log tag if supplied */
  if (ident != NULL && *ident != '0' && ident != TagBuffer) {
	strncpy(TagBuffer, ident, sizeof(TagBuffer));
	TagBuffer[sizeof(TagBuffer) - 1] = '0';
  }

  /* Opens channel to syslog daemon via UDP device */
  /* Static values used to minimize code */
  if (option & LOG_NDELAY) {
	/* Opens UDP device */
	if ((nfd = open(UDP_DEVICE, O_RDWR)) < 0) {
		 /* Report error */ ;
	}
	/* Sets options for UDP device */
	udpopt.nwuo_flags = NWUO_SHARED | NWUO_LP_SET | NWUO_DI_LOC |
		NWUO_DI_BROAD | NWUO_RP_SET | NWUO_RA_SET |
		NWUO_RWDATONLY | NWUO_DI_IPOPT;
	udpopt.nwuo_locaddr = udpopt.nwuo_remaddr = htonl(0x7F000001L);
	udpopt.nwuo_locport = udpopt.nwuo_remport = htons(514);
	if (ioctl(nfd, NWIOSUDPOPT, &udpopt) < 0 ||
	    ioctl(nfd, NWIOGUDPOPT, &udpopt) < 0) {
		 /* Report error */ ;
	}
  }
  return;
}

/*
**  SYSLOG -- print message on log file
**
**  This routine looks a lot like printf, except that it outputs to the
**  log file instead of the standard output.  Also:
**	- adds a timestamp,
**	- prints the module name in front of the message,
**	- has some other formatting types (or will sometime),
**	- adds a newline on the end of the message.
**
** The output of this routine is intended to be read by syslogd(8).
*/
void syslog(int lprty, const char *msg,...)
{
  time_t now;
  char buff[512];
  int len, rc;
  va_list ap;

  /* First log message open chnnel to syslog */
  if (nfd < 0) openlog(TagBuffer, LogFlags | LOG_NDELAY, LogFacility);
  time(&now);
  len = sprintf(buff, "<%d>%.15s %s: ",
		LogFacility | lprty, ctime(&now) + 4, TagBuffer);
  if (LogFlags & LOG_PID) {
	len -= 2;
	len += sprintf(buff + len, "[%d]: ", LogPid);
  }
  va_start(ap, msg);
  len += vsnprintf(buff + len, sizeof(buff) - len, msg, ap);
  va_end(ap);
  rc = write(nfd, buff, len);
  if ((rc != len && LogFlags & LOG_CONS) || LogFlags & LOG_PERROR) {
	write(STDERR_FILENO, buff, len);
	write(STDERR_FILENO, "\n", 1);
  }
  return;
}

/*
**  CLOSELOG -- close access to syslogd
**	- closes UDP channel
**	- restores default values
*/
void closelog(void)
{

  close(nfd);
  LogPid = nfd = -1;
  LogFacility = LOG_USER;
  LogFlags = 0;
  return;
}

/** syslog.c **/
