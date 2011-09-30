/*	$NetBSD: vipw.c,v 1.14 2009/04/19 00:44:49 lukem Exp $	*/

/*
 * Copyright (c) 1987, 1993, 1994
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
__COPYRIGHT("@(#) Copyright (c) 1987, 1993, 1994\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)vipw.c	8.3 (Berkeley) 4/2/94";
#else
__RCSID("$NetBSD: vipw.c,v 1.14 2009/04/19 00:44:49 lukem Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <util.h>

int	main __P((int, char **));
static void	copyfile __P((int, int));
static void	usage __P((void));

char mpwd[MAXPATHLEN], mpwdl[MAXPATHLEN];

int
main(int argc, char *argv[])
{
	const char *prefix;
	int pfd, tfd;
	struct stat begin, end;
	int ch;

	prefix = "";
	while ((ch = getopt(argc, argv, "d:")) != -1) {
		switch (ch) {
		case 'd':
			prefix = optarg;
			if (pw_setprefix(prefix) < 0)
				err(1, "%s", prefix);
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage();

	(void)snprintf(mpwd, sizeof(mpwd), "%s%s", prefix, _PATH_MASTERPASSWD);
	(void)snprintf(mpwdl, sizeof(mpwdl), "%s%s", prefix,
		_PATH_MASTERPASSWD_LOCK);

	pw_init();
	tfd = pw_lock(0);
	if (tfd < 0) {
		if (errno == EEXIST)
			errx(1, "the passwd file is busy.");
		else
			err(1, "%s", mpwdl);
	}

	pfd = open(mpwd, O_RDONLY, 0);
	if (pfd < 0)
		pw_error(mpwd, 1, 1);
	copyfile(pfd, tfd);
	(void)close(tfd);

	for (;;) {
		if (stat(mpwdl, &begin))
			pw_error(mpwdl, 1, 1);
		pw_edit(0, NULL);
		if (stat(mpwdl, &end))
			pw_error(mpwdl, 1, 1);
		if (begin.st_mtime == end.st_mtime &&
		    begin.st_mtimensec == end.st_mtimensec) {
			warnx("no changes made");
			pw_error((char *)NULL, 0, 0);
		}
		if (pw_mkdb(NULL, 0) == 0)
			break;
		pw_prompt();
	}
	return 0;
}

static void
copyfile(int from, int to)
{
	int nr, nw, off;
	char buf[8*1024];
	
	while ((nr = read(from, buf, sizeof(buf))) > 0)
		for (off = 0; off < nr; nr -= nw, off += nw)
			if ((nw = write(to, buf + off, nr)) < 0)
				pw_error(mpwdl, 1, 1);
	if (nr < 0)
		pw_error(mpwd, 1, 1);
}

static void
usage(void)
{

	(void)fprintf(stderr, "usage: %s [-d directory]\n", getprogname());
	exit(1);
}
