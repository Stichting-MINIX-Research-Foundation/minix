/*	$NetBSD: from.c,v 1.17 2008/07/21 14:19:22 lukem Exp $	*/

/*
 * Copyright (c) 1980, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
__COPYRIGHT("@(#) Copyright (c) 1980, 1988, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)from.c	8.1 (Berkeley) 6/6/93";
#endif
__RCSID("$NetBSD: from.c,v 1.17 2008/07/21 14:19:22 lukem Exp $");
#endif /* not lint */

#include <sys/types.h>
#include <ctype.h>
#include <err.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int	main (int, char **);
int	match (const char *, const char *);

int
main(int argc, char **argv)
{
	struct passwd *pwd;
	int ch, newline;
	char *file, *sender, *p;
#if MAXPATHLEN > BUFSIZ
	char buf[MAXPATHLEN];
#else
	char buf[BUFSIZ];
#endif

	file = sender = NULL;
	while ((ch = getopt(argc, argv, "f:s:")) != -1)
		switch((char)ch) {
		case 'f':
			file = optarg;
			break;
		case 's':
			sender = optarg;
			for (p = sender; *p; ++p)
				*p = tolower((unsigned char)*p);
			break;
		default:
			fprintf(stderr, "usage: from [-f file] [-s sender] [user]\n");
			exit(1);
		}
	argv += optind;

	/*
	 * We find the mailbox by:
	 *	1 -f flag
	 *	2 user
	 *	2 MAIL environment variable
	 *	3 _PATH_MAILDIR/file
	 */
	if (!file) {
		if (!(file = *argv)) {
			if (!(file = getenv("MAIL"))) {
				if (!(pwd = getpwuid(getuid())))
					errx(1, "no password file entry for you");
				if ((file = getenv("LOGNAME")) != NULL ||
				    (file = getenv("USER")) != NULL) {
					(void)snprintf(buf, sizeof(buf), 
					    "%s/%s", _PATH_MAILDIR, file);
					file = buf;
				} else
					(void)snprintf(file = buf, sizeof(buf),
					    "%s/%s",
					    _PATH_MAILDIR, pwd->pw_name);
			}
		} else {
			(void)snprintf(buf, sizeof(buf), "%s/%s",
				_PATH_MAILDIR, file);
			file = buf;
		}
	}
	if (!freopen(file, "r", stdin))
		err(1, "can't read %s", file);

	for (newline = 1; fgets(buf, sizeof(buf), stdin);) {
		if (*buf == '\n') {
			newline = 1;
			continue;
		}
		if (newline && !strncmp(buf, "From ", 5) &&
		    (!sender || match(buf + 5, sender)))
			printf("%s", buf);
		newline = 0;
	}
	exit(0);
}

int
match(const char *line, const char *sender)
{
	char ch, pch, first;
	const char *p, *t;

	for (first = *sender++;;) {
		if (isspace((unsigned char)(ch = *line)))
			return(0);
		++line;
		ch = tolower((unsigned char)ch);
		if (ch != first)
			continue;
		for (p = sender, t = line;;) {
			if (!(pch = *p++))
				return(1);
			ch = tolower((unsigned char)*t++);
			if (ch != pch)
				break;
		}
	}
	/* NOTREACHED */
}
