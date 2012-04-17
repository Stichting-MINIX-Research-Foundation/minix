/*	$NetBSD: chroot.c,v 1.19 2011/09/20 14:28:52 christos Exp $	*/

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
static char sccsid[] = "@(#)chroot.c	8.1 (Berkeley) 6/9/93";
#else
__RCSID("$NetBSD: chroot.c,v 1.19 2011/09/20 14:28:52 christos Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>

#include <err.h>
#include <errno.h>
#include <grp.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>

static void	usage(void) __dead;

static int
getnum(const char *str, uintmax_t *num)
{
	char *ep;

	errno = 0;

	*num = strtoumax(str, &ep, 0);
	if (str[0] == '\0' || *ep != '\0') {
		errno = EINVAL;
		return -1;
	}

	if (errno == ERANGE && *num == UINTMAX_MAX)
		return -1;

	return 0;
}


static gid_t
getgroup(const char *group)
{
	uintmax_t	num;
	struct group	*gp;

	if ((gp = getgrnam(group)) != NULL)
		return gp->gr_gid;

	if (getnum(group, &num) == -1)
	    errx(1, "no such group `%s'", group);

	return (gid_t)num;
}

static uid_t
getuser(const char *user)
{
	uintmax_t	num;
	struct passwd	*pw;

	if ((pw = getpwnam(user)) != NULL)
		return pw->pw_uid;

	if (getnum(user, &num) == -1)
		errx(1, "no such user `%s'", user);

	return (uid_t)num;
}

int
main(int argc, char *argv[])
{
	char	*user;		/* user to switch to before running program */
	char	*group;		/* group to switch to ... */
	char	*grouplist;	/* group list to switch to ... */
	char		*p;
	const char	*shell;
	gid_t		gid, gidlist[NGROUPS_MAX];
	uid_t		uid;
	int		ch, gids;

	user = NULL;
	group = NULL;
	grouplist = NULL;
	gid = 0;
	uid = 0;
	gids = 0;
	while ((ch = getopt(argc, argv, "G:g:u:")) != -1) {
		switch(ch) {
		case 'u':
			user = optarg;
			if (*user == '\0')
				usage();
			break;
		case 'g':
			group = optarg;
			if (*group == '\0')
				usage();
			break;
		case 'G':
			grouplist = optarg;
			if (*grouplist == '\0')
				usage();
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	if (user != NULL)
		uid = getuser(user);

	if (group != NULL)
		gid = getgroup(group);

	if (grouplist != NULL) {
		while ((p = strsep(&grouplist, ",")) != NULL) {
			if (*p == '\0')
				continue;

			if (gids == NGROUPS_MAX)
				errx(1,
				    "too many supplementary groups provided");

			gidlist[gids++] = getgroup(p);
		}
	}

	if (chdir(argv[0]) == -1 || chroot(".") == -1)
		err(1, "%s", argv[0]);

	if (gids && setgroups(gids, gidlist) == -1)
		err(1, "setgroups");
	if (group && setgid(gid) == -1)
		err(1, "setgid");
	if (user && setuid(uid) == -1)
		err(1, "setuid");

	if (argv[1]) {
		execvp(argv[1], &argv[1]);
		err(1, "%s", argv[1]);
	}

	if ((shell = getenv("SHELL")) == NULL)
		shell = _PATH_BSHELL;
	execlp(shell, shell, "-i", NULL);
	err(1, "%s", shell);
	/* NOTREACHED */
}

static void
usage(void)
{

	(void)fprintf(stderr, "Usage: %s [-G group,group,...] [-g group] "
	    "[-u user] newroot [command]\n", getprogname());
	exit(1);
}
