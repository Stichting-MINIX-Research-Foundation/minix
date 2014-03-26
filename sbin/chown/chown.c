/*	$NetBSD: chown.c,v 1.8 2012/10/24 01:12:51 enami Exp $	*/

/*
 * Copyright (c) 1988, 1993, 1994, 2003
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
__COPYRIGHT("@(#) Copyright (c) 1988, 1993, 1994, 2003\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)chown.c	8.8 (Berkeley) 4/4/94";
#else
__RCSID("$NetBSD: chown.c,v 1.8 2012/10/24 01:12:51 enami Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <locale.h>
#include <fts.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

static void	a_gid(const char *);
static void	a_uid(const char *);
static id_t	id(const char *, const char *);
__dead static void	usage(void);

static uid_t uid;
static gid_t gid;
static int ischown;
static const char *myname;

struct option chown_longopts[] = {
	{ "reference",		required_argument,	0,
						1 },
	{ NULL,			0,			0,
						0 },
};

int
main(int argc, char **argv)
{
	FTS *ftsp;
	FTSENT *p;
	int Hflag, Lflag, Rflag, ch, fflag, fts_options, hflag, rval, vflag;
	char *cp, *reference;
	int (*change_owner)(const char *, uid_t, gid_t);

	setprogname(*argv);

	(void)setlocale(LC_ALL, "");

	myname = getprogname();
	ischown = (myname[2] == 'o');
	reference = NULL;

	Hflag = Lflag = Rflag = fflag = hflag = vflag = 0;
	while ((ch = getopt_long(argc, argv, "HLPRfhv",
	    chown_longopts, NULL)) != -1)
		switch (ch) {
		case 1:
			reference = optarg;
			break;
		case 'H':
			Hflag = 1;
			Lflag = 0;
			break;
		case 'L':
			Lflag = 1;
			Hflag = 0;
			break;
		case 'P':
			Hflag = Lflag = 0;
			break;
		case 'R':
			Rflag = 1;
			break;
		case 'f':
			fflag = 1;
			break;
		case 'h':
			/*
			 * In System V the -h option causes chown/chgrp to
			 * change the owner/group of the symbolic link.
			 * 4.4BSD's symbolic links didn't have owners/groups,
			 * so it was an undocumented noop.
			 * In NetBSD 1.3, lchown(2) is introduced.
			 */
			hflag = 1;
			break;
		case 'v':
			vflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

	if (argc == 0 || (argc == 1 && reference == NULL))
		usage();

	fts_options = FTS_PHYSICAL;
	if (Rflag) {
		if (Hflag)
			fts_options |= FTS_COMFOLLOW;
		if (Lflag) {
			if (hflag)
				errx(EXIT_FAILURE,
				    "the -L and -h options "
				    "may not be specified together.");
			fts_options &= ~FTS_PHYSICAL;
			fts_options |= FTS_LOGICAL;
		}
	} else if (!hflag)
		fts_options |= FTS_COMFOLLOW;

	uid = (uid_t)-1;
	gid = (gid_t)-1;
	if (reference == NULL) {
		if (ischown) {
			if ((cp = strchr(*argv, ':')) != NULL) {
				*cp++ = '\0';
				a_gid(cp);
			}
#ifdef SUPPORT_DOT
			else if ((cp = strrchr(*argv, '.')) != NULL) {
				if (uid_from_user(*argv, &uid) == -1) {
					*cp++ = '\0';
					a_gid(cp);
				}
			}
#endif
			a_uid(*argv);
		} else
			a_gid(*argv);
		argv++;
	} else {
		struct stat st;

		if (stat(reference, &st) == -1)
			err(EXIT_FAILURE, "Cannot stat `%s'", reference);
		if (ischown)
			uid = st.st_uid;
		gid = st.st_gid;
	}

	if ((ftsp = fts_open(argv, fts_options, NULL)) == NULL)
		err(EXIT_FAILURE, "fts_open");

	for (rval = EXIT_SUCCESS; (p = fts_read(ftsp)) != NULL;) {
		change_owner = chown;
		switch (p->fts_info) {
		case FTS_D:
			if (!Rflag)		/* Change it at FTS_DP. */
				fts_set(ftsp, p, FTS_SKIP);
			continue;
		case FTS_DNR:			/* Warn, chown, continue. */
			warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			rval = EXIT_FAILURE;
			break;
		case FTS_ERR:			/* Warn, continue. */
		case FTS_NS:
			warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			rval = EXIT_FAILURE;
			continue;
		case FTS_SL:			/* Ignore unless -h. */
			/*
			 * All symlinks we found while doing a physical
			 * walk end up here.
			 */
			if (!hflag)
				continue;
			/*
			 * Note that if we follow a symlink, fts_info is
			 * not FTS_SL but FTS_F or whatever.  And we should
			 * use lchown only for FTS_SL and should use chown
			 * for others.
			 */
			change_owner = lchown;
			break;
		case FTS_SLNONE:		/* Ignore. */
			/*
			 * The only symlinks that end up here are ones that
			 * don't point to anything.  Note that if we are
			 * doing a phisycal walk, we never reach here unless
			 * we asked to follow explicitly.
			 */
			continue;
		default:
			break;
		}

		if ((*change_owner)(p->fts_accpath, uid, gid) && !fflag) {
			warn("%s", p->fts_path);
			rval = EXIT_FAILURE;
		} else {
			if (vflag)
				printf("%s\n", p->fts_path);
		}
	}
	if (errno)
		err(EXIT_FAILURE, "fts_read");
	exit(rval);
	/* NOTREACHED */
}

static void
a_gid(const char *s)
{
	struct group *gr;

	if (*s == '\0')			/* Argument was "uid[:.]". */
		return;
	gr = *s == '#' ? NULL : getgrnam(s);
	if (gr == NULL)
		gid = id(s, "group");
	else
		gid = gr->gr_gid;
	return;
}

static void
a_uid(const char *s)
{
	if (*s == '\0')			/* Argument was "[:.]gid". */
		return;
	if (*s == '#' || uid_from_user(s, &uid) == -1) {
		uid = id(s, "user");
	}
	return;
}

static id_t
id(const char *name, const char *type)
{
	id_t val;
	char *ep;

	errno = 0;
	if (*name == '#')
		name++;
	val = (id_t)strtoul(name, &ep, 10);
	if (errno)
		err(EXIT_FAILURE, "%s", name);
	if (*ep != '\0')
		errx(EXIT_FAILURE, "%s: invalid %s name", name, type);
	return (val);
}

static void
usage(void)
{

	(void)fprintf(stderr,
	    "Usage: %s [-R [-H | -L | -P]] [-fhv] %s file ...\n"
	    "\t%s [-R [-H | -L | -P]] [-fhv] --reference=rfile file ...\n",
	    myname, ischown ? "owner:group|owner|:group" : "group",
	    myname);
	exit(EXIT_FAILURE);
}
