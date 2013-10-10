/*	$NetBSD: tee.c,v 1.11 2013/03/06 11:44:11 yamt Exp $	*/

/*
 * Copyright (c) 1988, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1988, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)tee.c	8.1 (Berkeley) 6/6/93";
#endif
__RCSID("$NetBSD: tee.c,v 1.11 2013/03/06 11:44:11 yamt Exp $");
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <err.h>

typedef struct _list {
	struct _list *next;
	int fd;
	const char *name;
} LIST;
LIST *head;

void	add(int, const char *);
int	main(int, char **);

int
main(int argc, char *argv[])
{
	LIST *p;
	ssize_t rval;
	int fd;
	int append, ch, exitval;
	char *buf;
#define	BSIZE (8 * 1024)

	setlocale(LC_ALL, "");

	append = 0;
	while ((ch = getopt(argc, argv, "ai")) != -1)
		switch((char)ch) {
		case 'a':
			append = 1;
			break;
		case 'i':
			(void)signal(SIGINT, SIG_IGN);
			break;
		case '?':
		default:
			(void)fprintf(stderr, "usage: tee [-ai] [file ...]\n");
			exit(1);
		}
	argv += optind;
	argc -= optind;

	if ((buf = malloc(BSIZE)) == NULL)
		err(1, "malloc");

	add(STDOUT_FILENO, "stdout");

	for (exitval = 0; *argv; ++argv)
		if ((fd = open(*argv, append ? O_WRONLY|O_CREAT|O_APPEND :
		    O_WRONLY|O_CREAT|O_TRUNC, DEFFILEMODE)) < 0) {
			warn("%s", *argv);
			exitval = 1;
		} else
			add(fd, *argv);

	while ((rval = read(STDIN_FILENO, buf, BSIZE)) > 0)
		for (p = head; p; p = p->next) {
			const char *bp = buf;
			size_t n = rval;
			ssize_t wval;

			do {
				if ((wval = write(p->fd, bp, n)) == -1) {
					warn("%s", p->name);
					exitval = 1;
					break;
				}
				bp += wval;
			} while (n -= wval);
		}
	if (rval < 0) {
		warn("read");
		exitval = 1;
	}

	for (p = head; p; p = p->next) {
		if (close(p->fd) == -1) {
			warn("%s", p->name);
			exitval = 1;
		}
	}

	exit(exitval);
}

void
add(int fd, const char *name)
{
	LIST *p;

	if ((p = malloc(sizeof(LIST))) == NULL)
		err(1, "malloc");
	p->fd = fd;
	p->name = name;
	p->next = head;
	head = p;
}
