/*-
 * Copyright (c) 1991, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1991, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)id.c	8.3 (Berkeley) 4/28/95";
#else
__RCSID("$NetBSD: id.c,v 1.32 2011/09/16 15:39:26 joerg Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>

#include <err.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void current(void);
static void pretty(struct passwd *);
static void group(struct passwd *, int);
__dead static void usage(void);
static void user(struct passwd *);
static struct passwd *who(char *);

static int maxgroups;
static gid_t *groups;

int
main(int argc, char *argv[])
{
	struct group *gr;
	struct passwd *pw;
	int ch, id;
	int Gflag, gflag, nflag, pflag, rflag, uflag;
	const char *opts;

	Gflag = gflag = nflag = pflag = rflag = uflag = 0;

	if (strcmp(getprogname(), "groups") == 0) {
		Gflag = 1;
		nflag = 1;
		opts = "";
		if (argc > 2)
			usage();
	} else if (strcmp(getprogname(), "whoami") == 0) {
		uflag = 1;
		nflag = 1;
		opts = "";
		if (argc > 1)
			usage();
	} else
		opts = "Ggnpru";

	while ((ch = getopt(argc, argv, opts)) != -1)
		switch (ch) {
		case 'G':
			Gflag = 1;
			break;
		case 'g':
			gflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'p':
			pflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		case 'u':
			uflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	switch (Gflag + gflag + pflag + uflag) {
	case 1:
		break;
	case 0:
		if (!nflag && !rflag)
			break;
		/* FALLTHROUGH */
	default:
		usage();
	}

	if (strcmp(opts, "") != 0 && argc > 1)
		usage();

	pw = *argv ? who(*argv) : NULL;

	maxgroups = sysconf(_SC_NGROUPS_MAX);
	if ((groups = malloc((maxgroups + 1) * sizeof(gid_t))) == NULL)
		err(1, NULL);

	if (gflag) {
		id = pw ? pw->pw_gid : rflag ? getgid() : getegid();
		if (nflag && (gr = getgrgid(id)))
			(void)printf("%s\n", gr->gr_name);
		else
			(void)printf("%u\n", id);
		goto done;
	}

	if (uflag) {
		id = pw ? pw->pw_uid : rflag ? getuid() : geteuid();
		if (nflag && (pw = getpwuid(id)))
			(void)printf("%s\n", pw->pw_name);
		else
			(void)printf("%u\n", id);
		goto done;
	}

	if (Gflag) {
		group(pw, nflag);
		goto done;
	}

	if (pflag) {
		pretty(pw);
		goto done;
	}

	if (pw)
		user(pw);
	else
		current();
done:
	free(groups);

	return 0;
}

static void
pretty(struct passwd *pw)
{
	struct group *gr;
	u_int eid, rid;
	char *login;

	if (pw) {
		(void)printf("uid\t%s\n", pw->pw_name);
		(void)printf("groups\t");
		group(pw, 1);
	} else {
		if ((login = getlogin()) == NULL)
			err(1, "getlogin");

		pw = getpwuid(rid = getuid());
		if (pw == NULL || strcmp(login, pw->pw_name))
			(void)printf("login\t%s\n", login);
		if (pw)
			(void)printf("uid\t%s\n", pw->pw_name);
		else
			(void)printf("uid\t%u\n", rid);

		if ((eid = geteuid()) != rid) {
			if ((pw = getpwuid(eid)) != NULL)
				(void)printf("euid\t%s\n", pw->pw_name);
			else
				(void)printf("euid\t%u\n", eid);
		}
		if ((rid = getgid()) != (eid = getegid())) {
			if ((gr = getgrgid(rid)) != NULL)
				(void)printf("rgid\t%s\n", gr->gr_name);
			else
				(void)printf("rgid\t%u\n", rid);
		}
		(void)printf("groups\t");
		group(NULL, 1);
	}
}

static void
current(void)
{
	struct group *gr;
	struct passwd *pw;
	gid_t gid, egid, lastid;
	uid_t uid, euid;
	int cnt, ngroups;
	const char *fmt;

	uid = getuid();
	(void)printf("uid=%ju", (uintmax_t)uid);
	if ((pw = getpwuid(uid)) != NULL)
		(void)printf("(%s)", pw->pw_name);
	gid = getgid();
	(void)printf(" gid=%ju", (uintmax_t)gid);
	if ((gr = getgrgid(gid)) != NULL)
		(void)printf("(%s)", gr->gr_name);
	if ((euid = geteuid()) != uid) {
		(void)printf(" euid=%ju", (uintmax_t)euid);
		if ((pw = getpwuid(euid)) != NULL)
			(void)printf("(%s)", pw->pw_name);
	}
	if ((egid = getegid()) != gid) {
		(void)printf(" egid=%ju", (uintmax_t)egid);
		if ((gr = getgrgid(egid)) != NULL)
			(void)printf("(%s)", gr->gr_name);
	}
	if ((ngroups = getgroups(maxgroups, groups)) != 0) {
		for (fmt = " groups=%ju", lastid = -1, cnt = 0; cnt < ngroups;
		    fmt = ",%ju", lastid = gid, cnt++) {
			gid = groups[cnt];
			if (lastid == gid)
				continue;
			(void)printf(fmt, (uintmax_t)gid);
			if ((gr = getgrgid(gid)) != NULL)
				(void)printf("(%s)", gr->gr_name);
		}
	}
	(void)printf("\n");
}

static void
user(struct passwd *pw)
{
	struct group *gr;
	const char *fmt;
	int cnt, id, lastid, ngroups;
	gid_t *glist = groups;

	id = pw->pw_uid;
	(void)printf("uid=%u(%s)", id, pw->pw_name);
	(void)printf(" gid=%lu", (u_long)pw->pw_gid);
	if ((gr = getgrgid(pw->pw_gid)) != NULL)
		(void)printf("(%s)", gr->gr_name);
	ngroups = maxgroups + 1;
	if (getgrouplist(pw->pw_name, pw->pw_gid, glist, &ngroups) == -1) {
		glist = malloc(ngroups * sizeof(gid_t));
		(void) getgrouplist(pw->pw_name, pw->pw_gid, glist, &ngroups);
	}
	for (fmt = " groups=%u", lastid = -1, cnt = 0; cnt < ngroups;
	    fmt=",%u", lastid = id, cnt++) {
		id = glist[cnt];
		if (lastid == id)
			continue;
		(void)printf(fmt, id);
		if ((gr = getgrgid(id)) != NULL)
			(void)printf("(%s)", gr->gr_name);
	}
	(void)printf("\n");
	if (glist != groups)
		free(glist);
}

static void
group(struct passwd *pw, int nflag)
{
	struct group *gr;
	int cnt, ngroups;
	gid_t id, lastid;
	const char *fmt;
	gid_t *glist = groups;

	if (pw) {
		ngroups = maxgroups;
		if (getgrouplist(pw->pw_name, pw->pw_gid, glist, &ngroups)
		    == -1) {
			glist = malloc(ngroups * sizeof(gid_t));
			(void) getgrouplist(pw->pw_name, pw->pw_gid, glist,
					    &ngroups);
		}
	} else {
		glist[0] = getgid();
		ngroups = getgroups(maxgroups, glist + 1) + 1;
	}
	fmt = nflag ? "%s" : "%u";
	for (lastid = -1, cnt = 0; cnt < ngroups; ++cnt) {
		if (lastid == (id = glist[cnt]) || (cnt && id == glist[0]))
			continue;
		if (nflag) {
			if ((gr = getgrgid(id)) != NULL)
				(void)printf(fmt, gr->gr_name);
			else
				(void)printf(*fmt == ' ' ? " %u" : "%u",
				    id);
			fmt = " %s";
		} else {
			(void)printf(fmt, id);
			fmt = " %u";
		}
		lastid = id;
	}
	(void)printf("\n");
	if (glist != groups)
		free(glist);
}

static struct passwd *
who(char *u)
{
	struct passwd *pw;
	long id;
	char *ep;

	/*
	 * Translate user argument into a pw pointer.  First, try to
	 * get it as specified.  If that fails, try it as a number.
	 */
	if ((pw = getpwnam(u)) != NULL)
		return pw;
	id = strtol(u, &ep, 10);
	if (*u && !*ep && (pw = getpwuid(id)))
		return pw;
	errx(1, "%s: No such user", u);
	/* NOTREACHED */
	return NULL;
}

static void
usage(void)
{

	if (strcmp(getprogname(), "groups") == 0) {
		(void)fprintf(stderr, "usage: groups [user]\n");
	} else if (strcmp(getprogname(), "whoami") == 0) {
		(void)fprintf(stderr, "usage: whoami\n");
	} else {
		(void)fprintf(stderr, "usage: id [user]\n");
		(void)fprintf(stderr, "       id -G [-n] [user]\n");
		(void)fprintf(stderr, "       id -g [-nr] [user]\n");
		(void)fprintf(stderr, "       id -p [user]\n");
		(void)fprintf(stderr, "       id -u [-nr] [user]\n");
	}
	exit(1);
}
