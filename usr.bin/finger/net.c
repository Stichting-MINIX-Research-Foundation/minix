/*	$NetBSD: net.c,v 1.23 2009/04/12 06:18:54 lukem Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Tony Nardo of the Johns Hopkins University/Applied Physics Lab.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)net.c	8.4 (Berkeley) 4/28/95";
#else
__RCSID("$NetBSD: net.c,v 1.23 2009/04/12 06:18:54 lukem Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <netdb.h>
#include <time.h>
#include <db.h>
#include <unistd.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <err.h>

#include "utmpentry.h"

#include "finger.h"
#include "extern.h"

void
netfinger(char *name)
{
	FILE *fp;
	int c, lastc;
	int s;
	char *host;
	struct addrinfo hints, *res, *res0;
	int error;
	const char *emsg = NULL;

	lastc = 0;
	if (!(host = strrchr(name, '@')))
		return;
	*host++ = '\0';
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_CANONNAME;
	error = getaddrinfo(host, "finger", &hints, &res0);
	if (error) {
		warnx("%s: %s", gai_strerror(error), host);
		return;
	}

	s = -1;
	for (res = res0; res; res = res->ai_next) {
		s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (s < 0) {
			emsg = "socket";
			continue;
		}

		if (connect(s, res->ai_addr, res->ai_addrlen)) {
			close(s);
			s = -1;
			emsg = "connect";
			continue;
		}

		break;
	}
	if (s < 0) {
		if (emsg != NULL)
			warn("%s", emsg);
		return;
	}

	/* have network connection; identify the host connected with */
	(void)printf("[%s]\n", res0->ai_canonname ? res0->ai_canonname : host);

	/* -l flag for remote fingerd  */
	if (lflag)
		write(s, "/W ", 3);
	/* send the name followed by <CR><LF> */
	(void)write(s, name, strlen(name));
	(void)write(s, "\r\n", 2);

	/*
	 * Read from the remote system; once we're connected, we assume some
	 * data.  If none arrives, we hang until the user interrupts.
	 *
	 * If we see a <CR> followed by a newline character, only output
	 * one newline.
	 *
	 * If a character isn't printable and it isn't a space, we strip the
	 * 8th bit and set the 7th bit.  Every ASCII character with bit 7 set
	 * is printable.
	 */
	if ((fp = fdopen(s, "r")) != NULL)
		while ((c = getc(fp)) != EOF) {
			if (c == '\r') {
				if (lastc == '\r')	/* ^M^M - skip dupes */
					continue;
				c = '\n';
				lastc = '\r';
			} else {
				if (!(eightflag || isprint(c) || isspace(c))) {
					c &= 0x7f;
					c |= 0x40;
				}
				if (lastc != '\r' || c != '\n')
					lastc = c;
				else {
					lastc = '\n';
					continue;
				}
			}
			putchar(c);
		}
	if (lastc != '\n')
		putchar('\n');
	(void)fclose(fp);
}
