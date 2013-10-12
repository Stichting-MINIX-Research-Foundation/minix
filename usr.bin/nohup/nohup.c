/*	$NetBSD: nohup.c,v 1.15 2011/09/06 18:24:15 joerg Exp $	*/

/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
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
__COPYRIGHT("@(#) Copyright (c) 1989\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)nohup.c	5.4 (Berkeley) 6/1/90";
#endif
__RCSID("$NetBSD: nohup.c,v 1.15 2011/09/06 18:24:15 joerg Exp $");
#endif /* not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static void dofile(void);
__dead static void usage(void);

/* nohup shall exit with one of the following values:
   126 - The utility was found but could not be invoked.
   127 - An error occurred in the nohup utility, or the utility could
         not be found. */
#define EXIT_NOEXEC	126
#define EXIT_NOTFOUND	127
#define EXIT_MISC	127

int
main(int argc, char **argv)
{
	int exit_status;

	while (getopt(argc, argv, "") != -1) {
		usage();
	}
	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	if (isatty(STDOUT_FILENO))
		dofile();
	if (isatty(STDERR_FILENO) && dup2(STDOUT_FILENO, STDERR_FILENO) == -1) {
		/* may have just closed stderr */
		(void)fprintf(stdin, "nohup: %s\n", strerror(errno));
		exit(EXIT_MISC);
	}

	/* The nohup utility shall take the standard action for all signals
	   except that SIGHUP shall be ignored. */
	(void)signal(SIGHUP, SIG_IGN);

	execvp(argv[0], &argv[0]);
	exit_status = (errno == ENOENT) ? EXIT_NOTFOUND : EXIT_NOEXEC;
	(void)fprintf(stderr, "nohup: %s: %s\n", argv[0], strerror(errno));
	exit(exit_status);
}

static void
dofile(void)
{
	int fd;
	char path[MAXPATHLEN];
	const char *p;

	/* If the standard output is a terminal, all output written to 
	   its standard output shall be appended to the end of the file
	   nohup.out in the current directory.  If nohup.out cannot be
	   created or opened for appending, the output shall be appended
	   to the end of the file nohup.out in the directory specified 
	   by the HOME environment variable.

	   If a file is created, the file's permission bits shall be
	   set to S_IRUSR | S_IWUSR. */
#define	FILENAME	"nohup.out"
	p = FILENAME;
	if ((fd = open(p, O_RDWR|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR)) >= 0)
		goto dupit;
	if ((p = getenv("HOME")) != NULL) {
		(void)strlcpy(path, p, sizeof(path));
		(void)strlcat(path, "/", sizeof(path));
		(void)strlcat(path, FILENAME, sizeof(path));
		if ((fd = open(p = path, O_RDWR|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR)) >= 0)
			goto dupit;
	}
	(void)fprintf(stderr, "nohup: can't open a nohup.out file.\n");
	exit(EXIT_MISC);

dupit:	(void)lseek(fd, 0L, SEEK_END);
	if (dup2(fd, STDOUT_FILENO) == -1) {
		(void)fprintf(stderr, "nohup: %s\n", strerror(errno));
		exit(EXIT_MISC);
	}
	(void)fprintf(stderr, "sending output to %s\n", p);
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: nohup utility [argument ...]\n");
	exit(EXIT_MISC);
}
