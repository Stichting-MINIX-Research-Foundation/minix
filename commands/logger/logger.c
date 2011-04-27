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
 * #ifndef lint
 * char copyright[] =
 * "@(#) Copyright (c) 1983 Regents of the University of California.\n\
 *  All rights reserved.\n";
 * #endif
 *
 * #ifndef lint
 * static char sccsid[] = "@(#)logger.c	6.8 (Berkeley) 6/29/88";
 * #endif
 *
 * Porting to Minix by G. Falzoni <gfalzoni@inwind.it>
 */

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/*
** 	LOGGER -- read and log utility
**
**	This program reads from an input and arranges to write the
**	result on the system log, along with a useful tag.
*/

#define  SYSLOG_NAMES
#include <syslog.h>

/*
**	Name:		void bailout(char *msg, char *arg);
**	Function:	Handles error exit.
*/
void bailout(const char *msg, const char *arg)
{

  fprintf(stderr, "logger: %s %s\n", msg, arg);
  exit(EXIT_FAILURE);
}

/*
**	Name:		int decode(char *name, struct code * codetab);
**	Function:	Decodes a name to the equivalent priority/facility.
*/
int decode(char *name, const struct _code * codetab)
{
  const struct _code *c;

  if (isdigit(*name)) return(atoi(name));

  for (c = codetab; c->c_name; c++)
	if (!strcasecmp(name, c->c_name)) return(c->c_val);

  return(-1);
}

/*
**	Name:		int pencode(char *s);
**	Function:	Decode a symbolic name (facility/priority)
**			to a numeric value.
*/
int pencode(char *s)
{
  char *save;
  int fac, lev;

  for (save = s; *s && *s != '.'; ++s);
  if (*s) {
	*s = '\0';
#ifdef __NBSD_LIBC
	fac = decode(save, facilitynames);
#else
	fac = decode(save, FacNames);
#endif
	if (fac < 0) bailout("unknown facility name:", save);
	*s++ = '.';
  } else {
	fac = 0;
	s = save;
  }
#ifdef __NBSD_LIBC
  lev = decode(s, prioritynames);
#else
  lev = decode(s, PriNames);
#endif
  if (lev < 0) bailout("unknown priority name:", save);
  return((lev & LOG_PRIMASK) | (fac & LOG_FACMASK));
}

/*
**	Name:		int main(int argc, char **argv);
**	Function:	Main entry for logger.
*/
int main(int argc, char **argv)
{
  int pri = LOG_NOTICE;
  int ch, logflags = 0;
  char *tag, buf[200];
  static const char usage[] =
  "[-i] [-f file] [-p pri] [-t tag] [ message ... ]";

  tag = NULL;
  while ((ch = getopt(argc, argv, "f:ip:t:")) != EOF) {
	switch ((char) ch) {
	    case 'f':		/* file to log */
		if (freopen(optarg, "r", stdin) == NULL) {
			bailout(strerror(errno), optarg);
		}
		break;
	    case 'i':		/* log process id also */
		logflags |= LOG_PID;
		break;
	    case 'p':		/* priority */
		pri = pencode(optarg);
		break;
	    case 't':		/* tag */
		tag = optarg;
		break;
	    case '?':
	    default:	bailout(usage, "");	break;
	}
  }
  argc -= optind;
  argv += optind;

  /* Setup for logging */
  openlog(tag ? tag : getlogin(), logflags, 0);
  fclose(stdout);

  if (argc > 0) {		/* Log input line if appropriate */
	char *p, *endp;
	int len;

	for (p = buf, endp = buf + sizeof(buf) - 1;;) {
		len = strlen(*argv);
		if (p + len < endp && p > buf) {
			*--p = '\0';
			syslog(pri, buf);
			p = buf;
		}
		if (len > sizeof(buf) - 1) {
			syslog(pri, *argv++);
			if (!--argc) break;
		} else {
			memcpy(p, *argv++, len);
			p += len;
			if (!--argc) break;
			*p++ = ' ';
			*--p = '\0';
		}
	}
	if (p != buf) {
		*p = '\0';
		syslog(pri, buf);
	}
  } else			/* Main loop */
	while (fgets(buf, sizeof(buf), stdin) != NULL) syslog(pri, buf);

  return EXIT_SUCCESS;
}

/** logger.c **/
