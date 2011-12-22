/*	$NetBSD: fsck.c,v 1.49 2010/02/24 13:56:07 hannken Exp $	*/

/*
 * Copyright (c) 1996 Christos Zoulas. All rights reserved.
 * Copyright (c) 1980, 1989, 1993, 1994
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
 *
 * From: @(#)mount.c	8.19 (Berkeley) 4/19/94
 * From: NetBSD: mount.c,v 1.24 1995/11/18 03:34:29 cgd Exp 
 *
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: fsck.c,v 1.49 2010/02/24 13:56:07 hannken Exp $");
#endif /* not lint */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/queue.h>
#include <sys/wait.h>
#define FSTYPENAMES
#define FSCKNAMES
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>

#include <err.h>
#include <errno.h>
#include <fstab.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "pathnames.h"
#include "fsutil.h"
#include "exitvalues.h"

static enum { IN_LIST, NOT_IN_LIST } which = NOT_IN_LIST;

TAILQ_HEAD(fstypelist, entry) opthead, selhead, omhead;

struct entry {
	char *type;
	char *options;
	TAILQ_ENTRY(entry) entries;
};

static int maxrun = 0;
static char *options = NULL;
static int flags = 0;

static int checkfs(const char *, const char *, const char *, void *, pid_t *);
static int selected(const char *);
static int omitted(const char *);
static void addoption(char *);
static const char *getoptions(const char *);
static void addentry(struct fstypelist *, const char *, const char *);
static void maketypelist(char *);
static void catopt(char **, const char *);
static void mangle(char *, int *, const char ** volatile *, int *);
static const char *getfslab(const char *);
static void usage(void);
static void *isok(struct fstab *);

int
main(int argc, char *argv[])
{
	struct fstab *fs;
	int i, rval;
	const char *vfstype = NULL;
	char globopt[3];
	int ret = FSCK_EXIT_OK;

	globopt[0] = '-';
	globopt[2] = '\0';

	TAILQ_INIT(&selhead);
	TAILQ_INIT(&opthead);
	TAILQ_INIT(&omhead);

	while ((i = getopt(argc, argv, "dfl:nPpqT:t:vx:y")) != -1) {
		switch (i) {
		case 'd':
			flags |= CHECK_DEBUG;
			continue;

		case 'f':
			flags |= CHECK_FORCE;
			break;

		case 'n':
			flags |= CHECK_NOFIX;
			break;

		case 'p':
			flags |= CHECK_PREEN;
			break;

		case 'P':
			flags |= CHECK_PROGRESS;
			break;

		case 'q':
			break;

		case 'l':
			maxrun = atoi(optarg);
			continue;

		case 'T':
			if (*optarg)
				addoption(optarg);
			continue;

		case 't':
			if (TAILQ_FIRST(&selhead) != NULL)
				errx(1, "only one -t option may be specified.");

			maketypelist(optarg);
			vfstype = optarg;
			continue;

		case 'v':
			flags |= CHECK_VERBOSE;
			continue;

		case 'x':
			addentry(&omhead, optarg, "");
			continue;

		case 'y':
			break;

		case '?':
		default:
			usage();
			/* NOTREACHED */
		}

		/* Pass option to fsck_xxxfs */
		globopt[1] = i;
		catopt(&options, globopt);
	}

	/* Don't do progress meters if we're debugging. */
	if (flags & CHECK_DEBUG)
		flags &= ~CHECK_PROGRESS;

	/*
	 * If progress meters are being used, force max parallel to 1
	 * so the progress meter outputs don't interfere with one another.
	 */
	if (flags & CHECK_PROGRESS)
		maxrun = 1;

#ifdef __minix
	/* parallel checking heuristic doesn't work for minix currently */
	maxrun = 1;
#endif

	argc -= optind;
	argv += optind;

	if (argc == 0)
		return checkfstab(flags, maxrun, isok, checkfs);

#define	BADTYPE(type)							\
	(strcmp(type, FSTAB_RO) &&					\
	    strcmp(type, FSTAB_RW) && strcmp(type, FSTAB_RQ))


	for (; argc--; argv++) {
		const char *spec, *type, *cp;
		char	device[MAXPATHLEN];

		spec = *argv;
		cp = strrchr(spec, '/');
		if (cp == 0) {
			(void)snprintf(device, sizeof(device), "%s%s",
				_PATH_DEV, spec);
			spec = device;
		}
		if ((fs = getfsfile(spec)) == NULL &&
		    (fs = getfsspec(spec)) == NULL) {
			if (vfstype == NULL)
				vfstype = getfslab(spec);
			type = vfstype;
		}
		else {
			spec = fs->fs_spec;
			type = fs->fs_vfstype;
			if (BADTYPE(fs->fs_type))
				errx(FSCK_EXIT_CHECK_FAILED,
				    "%s has unknown file system type.",
				    spec);
		}

		rval = checkfs(type, blockcheck(spec), *argv, NULL, NULL);
		if (rval > ret) 
			ret = rval;
	}

	return ret;
}


static void *
isok(struct fstab *fs)
{

	if (fs->fs_passno == 0)
		return NULL;

	if (BADTYPE(fs->fs_type))
		return NULL;

	if (!selected(fs->fs_vfstype))
		return NULL;

	if (omitted(fs->fs_file))
		return NULL;

	return fs;
}


static int
checkfs(const char *vfst, const char *spec, const char *mntpt, void *auxarg,
    pid_t *pidp)
{
	/* List of directories containing fsck_xxx subcommands. */
	static const char *edirs[] = {
#ifdef RESCUEDIR
		RESCUEDIR,
#endif
		_PATH_SBIN,
		_PATH_USRSBIN,
#ifdef __minix
		"/usr/pkg/sbin/",
#endif
		NULL
	};
	const char ** volatile argv, **edir;
	const char * volatile vfstype = vfst;
	pid_t pid;
	int argc, i, status, maxargc;
	char *optb;
	char *volatile optbuf;
	char execname[MAXPATHLEN + 1], execbase1[MAXPATHLEN], execbase2[MAXPATHLEN];
	const char *extra = getoptions(vfstype);

	if (!strcmp(vfstype, "ufs"))
		vfstype = MOUNT_UFS;

	optb = NULL;
	if (options)
		catopt(&optb, options);
	if (extra)
		catopt(&optb, extra);
	optbuf = optb;

	maxargc = 64;
	argv = emalloc(sizeof(char *) * maxargc);

	(void) snprintf(execbase1, sizeof(execbase1), "fsck_%s", vfstype);
	(void) snprintf(execbase2, sizeof(execbase2), "fsck.%s", vfstype);
	argc = 0;
	argv[argc++] = execbase1;
	if (optbuf)
		mangle(optbuf, &argc, &argv, &maxargc);
	argv[argc++] = spec;
	argv[argc] = NULL;

	if (flags & (CHECK_DEBUG|CHECK_VERBOSE)) {
		(void)printf("start %s %swait", mntpt, 
			pidp ? "no" : "");
		for (i = 0; i < argc; i++)
			(void)printf(" %s", argv[i]);
		(void)printf("\n");
	}

	switch (pid = vfork()) {
	case -1:				/* Error. */
		warn("vfork");
		if (optbuf)
			free(optbuf);
		free(argv);
		return FSCK_EXIT_CHECK_FAILED;

	case 0:					/* Child. */
#ifndef __minix
		if ((flags & CHECK_FORCE) == 0) {
			struct statvfs	sfs;

				/*
				 * if mntpt is a mountpoint of a mounted file
				 * system and it's mounted read-write, skip it
				 * unless -f is given.
				 */
			if ((statvfs(mntpt, &sfs) == 0) &&
			    (strcmp(mntpt, sfs.f_mntonname) == 0) &&
			    ((sfs.f_flag & MNT_RDONLY) == 0)) {
				printf(
		"%s: file system is mounted read-write on %s; not checking\n",
				    spec, mntpt);
				if ((flags & CHECK_PREEN) && auxarg != NULL)
					_exit(FSCK_EXIT_OK);	/* fsck -p */
				else
					_exit(FSCK_EXIT_CHECK_FAILED);	/* fsck [[-p] ...] */
			}
		}
#endif

		if (flags & CHECK_DEBUG)
			_exit(FSCK_EXIT_OK);

		/* Go find an executable. */
		edir = edirs;
		do {
			(void)snprintf(execname,
			    sizeof(execname), "%s/%s", *edir, execbase1);
			execv(execname, (char * const *)__UNCONST(argv));
			if (errno != ENOENT) {
				if (spec)
					warn("exec %s for %s", execname, spec);
				else
					warn("exec %s", execname);
			}
			(void)snprintf(execname,
			    sizeof(execname), "%s/%s", *edir, execbase2);
			execv(execname, (char * const *)__UNCONST(argv));
			if (errno != ENOENT) {
				if (spec)
					warn("exec %s for %s", execname, spec);
				else
					warn("exec %s", execname);
			}
		} while (*++edir != NULL);

		if (errno == ENOENT) {
			if (spec)
				warn("exec %s for %s", execname, spec);
			else
				warn("exec %s", execname);
		}
		_exit(FSCK_EXIT_CHECK_FAILED);
		/* NOTREACHED */

	default:				/* Parent. */
		if (optbuf)
			free(optbuf);
		free(argv);

		if (pidp) {
			*pidp = pid;
			return FSCK_EXIT_OK;
		}

		if (waitpid(pid, &status, 0) < 0) {
			warn("waitpid");
			return FSCK_EXIT_CHECK_FAILED;
		}

		if (WIFEXITED(status)) {
			if (WEXITSTATUS(status) != 0)
				return WEXITSTATUS(status);
		}
		else if (WIFSIGNALED(status)) {
			warnx("%s: %s", spec, strsignal(WTERMSIG(status)));
			return FSCK_EXIT_CHECK_FAILED;
		}
		break;
	}

	return FSCK_EXIT_OK;
}


static int
selected(const char *type)
{
	struct entry *e;

	/* If no type specified, it's always selected. */
	TAILQ_FOREACH(e, &selhead, entries)
		if (!strcmp(e->type, type))
			return which == IN_LIST ? 1 : 0;

	return which == IN_LIST ? 0 : 1;
}


static int
omitted(const char *mountedon)
{
	struct entry *e;

	/* If no type specified, it's always selected. */
	TAILQ_FOREACH(e, &omhead, entries)
		if (!strcmp(e->type, mountedon))
			return 1;

	return 0;
}


static const char *
getoptions(const char *type)
{
	struct entry *e;

	TAILQ_FOREACH(e, &opthead, entries)
		if (!strcmp(e->type, type))
			return e->options;
	return "";
}


static void
addoption(char *optstr)
{
	char *newoptions;
	struct entry *e;

	if ((newoptions = strchr(optstr, ':')) == NULL)
		errx(1, "Invalid option string");

	*newoptions++ = '\0';

	TAILQ_FOREACH(e, &opthead, entries)
		if (!strcmp(e->type, optstr)) {
			catopt(&e->options, newoptions);
			return;
		}
	addentry(&opthead, optstr, newoptions);
}


static void
addentry(struct fstypelist *list, const char *type, const char *opts)
{
	struct entry *e;

	e = emalloc(sizeof(struct entry));
	e->type = estrdup(type);
	e->options = estrdup(opts);
	TAILQ_INSERT_TAIL(list, e, entries);
}


static void
maketypelist(char *fslist)
{
	char *ptr;

	if ((fslist == NULL) || (fslist[0] == '\0'))
		errx(1, "empty type list");

	if (fslist[0] == 'n' && fslist[1] == 'o') {
		fslist += 2;
		which = NOT_IN_LIST;
	}
	else
		which = IN_LIST;

	while ((ptr = strsep(&fslist, ",")) != NULL)
		addentry(&selhead, ptr, "");

}


static void
catopt(char **sp, const char *o)
{
	char *s;
	size_t i, j;

	s = *sp;
	if (s) {
		i = strlen(s);
		j = i + 1 + strlen(o) + 1;
		s = erealloc(s, j);
		(void)snprintf(s + i, j, ",%s", o);
	} else
		s = estrdup(o);
	*sp = s;
}


static void
mangle(char *opts, int *argcp, const char ** volatile *argvp, int *maxargcp)
{
	char *p, *s;
	int argc, maxargc;
	const char **argv;

	argc = *argcp;
	argv = *argvp;
	maxargc = *maxargcp;

	for (s = opts; (p = strsep(&s, ",")) != NULL;) {
		/* Always leave space for one more argument and the NULL. */
		if (argc >= maxargc - 3) {
			maxargc <<= 1;
			argv = erealloc(argv, maxargc * sizeof(char *));
		}
		if (*p != '\0')  {
			if (*p == '-') {
				argv[argc++] = p;
				p = strchr(p, '=');
				if (p) {
					*p = '\0';
					argv[argc++] = p+1;
				}
			} else {
				argv[argc++] = "-o";
				argv[argc++] = p;
			}
		}
	}

	*argcp = argc;
	*argvp = argv;
	*maxargcp = maxargc;
}

static const char *
getfslab(const char *str)
{
#ifdef __minix
	errx(1, "cannot determine vfstype under minix");
#else
	static struct dkwedge_info dkw;
	struct disklabel dl;
	int fd;
	char p;
	const char *vfstype;
	u_char t;

	/* deduce the file system type from the disk label */
	if ((fd = open(str, O_RDONLY)) == -1)
		err(1, "cannot open `%s'", str);

	/* First check to see if it's a wedge. */
	if (ioctl(fd, DIOCGWEDGEINFO, &dkw) == 0) {
		/* Yup, this is easy. */
		(void) close(fd);
		return (dkw.dkw_ptype);
	}

	if (ioctl(fd, DIOCGDINFO, &dl) == -1)
		err(1, "cannot get disklabel for `%s'", str);

	(void) close(fd);

	p = str[strlen(str) - 1];

	if ((p - 'a') >= dl.d_npartitions)
		errx(1, "partition `%s' is not defined on disk", str);

	if ((t = dl.d_partitions[p - 'a'].p_fstype) >= FSMAXTYPES) 
		errx(1, "partition `%s' is not of a legal vfstype",
		    str);

	if ((vfstype = fscknames[t]) == NULL)
		errx(1, "vfstype `%s' on partition `%s' is not supported",
		    fstypenames[t], str);

	return vfstype;
#endif
}


static void
usage(void)
{
	static const char common[] =
	    "[-dfnPpqvy] [-x excludemount] [-l maxparallel] [-T fstype:fsoptions]\n\t\t[-t fstype]";

	(void)fprintf(stderr, "usage: %s %s [special|node]...\n",
	    getprogname(), common);
	exit(FSCK_EXIT_USAGE);
}
