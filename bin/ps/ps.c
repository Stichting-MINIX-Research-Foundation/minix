/*	$NetBSD: ps.c,v 1.83 2015/06/16 22:31:08 christos Exp $	*/

/*
 * Copyright (c) 2000-2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Simon Burge.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1990, 1993, 1994
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
__COPYRIGHT("@(#) Copyright (c) 1990, 1993, 1994\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)ps.c	8.4 (Berkeley) 4/2/94";
#else
__RCSID("$NetBSD: ps.c,v 1.83 2015/06/16 22:31:08 christos Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/lwp.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>

#include <stddef.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <locale.h>
#include <nlist.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ps.h"

/*
 * ARGOPTS must contain all option characters that take arguments
 * (except for 't'!) - it is used in kludge_oldps_options()
 */
#define	GETOPTSTR	"aAcCeghjk:LlM:mN:O:o:p:rSsTt:U:uvW:wx"
#define	ARGOPTS		"kMNOopUW"

struct kinfo_proc2 *kinfo;
struct varlist displaylist = SIMPLEQ_HEAD_INITIALIZER(displaylist);
struct varlist sortlist = SIMPLEQ_HEAD_INITIALIZER(sortlist);

int	eval;			/* exit value */
int	rawcpu;			/* -C */
int	sumrusage;		/* -S */
int	termwidth;		/* width of screen (0 == infinity) */
int	totwidth;		/* calculated width of requested variables */

int	needcomm, needenv, commandonly;
uid_t	myuid;

static struct kinfo_lwp
		*pick_representative_lwp(struct kinfo_proc2 *,
		    struct kinfo_lwp *, int);
static struct kinfo_proc2
		*getkinfo_kvm(kvm_t *, int, int, int *);
static char	*kludge_oldps_options(char *);
static int	 pscomp(const void *, const void *);
static void	 scanvars(void);
__dead static void	 usage(void);
static int	 parsenum(const char *, const char *);
int		 main(int, char *[]);

char dfmt[] = "pid tt state time command";
char jfmt[] = "user pid ppid pgid sess jobc state tt time command";
char lfmt[] = "uid pid ppid cpu pri nice vsz rss wchan state tt time command";
char sfmt[] = "uid pid ppid cpu lid nlwp pri nice vsz rss wchan lstate tt "
		"ltime command";
char ufmt[] = "user pid %cpu %mem vsz rss tt state start time command";
char vfmt[] = "pid state time sl re pagein vsz rss lim tsiz %cpu %mem command";

const char *default_fmt = dfmt;

struct varent *Opos = NULL; /* -O flag inserts after this point */

kvm_t *kd;

static long long
ttyname2dev(const char *ttname, int *xflg, int *what)
{
	struct stat sb;
	const char *ttypath;
	char pathbuf[MAXPATHLEN];

	ttypath = NULL;
	if (strcmp(ttname, "?") == 0) {
		*xflg = 1;
		return KERN_PROC_TTY_NODEV;
	}
	if (strcmp(ttname, "-") == 0)
		return KERN_PROC_TTY_REVOKE;

	if (strcmp(ttname, "co") == 0)
		ttypath = _PATH_CONSOLE;
	else if (strncmp(ttname, "pts/", 4) == 0 ||
		strncmp(ttname, "tty", 3) == 0) {
		(void)snprintf(pathbuf,
		    sizeof(pathbuf), "%s%s", _PATH_DEV, ttname);
		ttypath = pathbuf;
	} else if (*ttname != '/') {
		(void)snprintf(pathbuf,
		    sizeof(pathbuf), "%s%s", _PATH_TTY, ttname);
		ttypath = pathbuf;
	} else
		ttypath = ttname;
	*what = KERN_PROC_TTY;
	if (stat(ttypath, &sb) == -1) {
		devmajor_t pts;
		int serrno;

		serrno = errno;
		pts = getdevmajor("pts", S_IFCHR);
		if (pts != NODEVMAJOR && strncmp(ttname, "pts/", 4) == 0) {
			int ptsminor = atoi(ttname + 4);

			snprintf(pathbuf, sizeof(pathbuf), "pts/%d", ptsminor);
			if (strcmp(pathbuf, ttname) == 0 && ptsminor >= 0)
				return makedev(pts, ptsminor);
		}
		errno = serrno;
		err(1, "%s", ttypath);
	}
	if (!S_ISCHR(sb.st_mode))
		errx(1, "%s: not a terminal", ttypath);
	return sb.st_rdev;
}

int
main(int argc, char *argv[])
{
	struct varent *vent;
	struct winsize ws;
	struct kinfo_lwp *kl, *l;
	int ch, i, j, fmt, lineno, nentries, nlwps;
	long long flag;
	int prtheader, wflag, what, xflg, showlwps;
	char *nlistf, *memf, *swapf, errbuf[_POSIX2_LINE_MAX];
	char *ttname;

	setprogname(argv[0]);
	(void)setlocale(LC_ALL, "");

	if ((ioctl(STDOUT_FILENO, TIOCGWINSZ, (char *)&ws) == -1 &&
	     ioctl(STDERR_FILENO, TIOCGWINSZ, (char *)&ws) == -1 &&
	     ioctl(STDIN_FILENO,  TIOCGWINSZ, (char *)&ws) == -1) ||
	     ws.ws_col == 0)
		termwidth = 79;
	else
		termwidth = ws.ws_col - 1;

	if (argc > 1)
		argv[1] = kludge_oldps_options(argv[1]);

	fmt = prtheader = wflag = xflg = showlwps = 0;
	what = KERN_PROC_UID;
	flag = myuid = getuid();
	memf = nlistf = swapf = NULL;

	while ((ch = getopt(argc, argv, GETOPTSTR)) != -1)
		switch((char)ch) {
		case 'A':
			/* "-A" shows all processes, like "-ax" */
			xflg = 1;
			/*FALLTHROUGH*/
		case 'a':
			what = KERN_PROC_ALL;
			flag = 0;
			break;
		case 'c':
			commandonly = 1;
			break;
		case 'e':			/* XXX set ufmt */
			needenv = 1;
			break;
		case 'C':
			rawcpu = 1;
			break;
		case 'g':
			break;			/* no-op */
		case 'h':
			prtheader = ws.ws_row > 5 ? ws.ws_row : 22;
			break;
		case 'j':
			parsefmt(jfmt);
			fmt = 1;
			jfmt[0] = '\0';
			break;
		case 'k':
			parsesort(optarg);
			break;
		case 'K':
			break;			/* no-op - was dontuseprocfs */
		case 'L':
			showkey();
			exit(0);
			/* NOTREACHED */
		case 'l':
			parsefmt(lfmt);
			fmt = 1;
			lfmt[0] = '\0';
			break;
		case 'M':
			memf = optarg;
			break;
		case 'm':
			parsesort("vsz");
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 'O':
			/*
			 * If this is not the first -O option, insert
			 * just after the previous one.
			 *
			 * If there is no format yet, start with the default
			 * format, and insert after the pid column.
			 *
			 * If there is already a format, insert after
			 * the pid column, or at the end if there's no
			 * pid column.
			 */
			if (!Opos) {
				if (!fmt)
					parsefmt(default_fmt);
				Opos = varlist_find(&displaylist, "pid");
			}
			parsefmt_insert(optarg, &Opos);
			fmt = 1;
			break;
		case 'o':
			parsefmt(optarg);
			fmt = 1;
			break;
		case 'p':
			what = KERN_PROC_PID;
			flag = parsenum(optarg, "process id");
			xflg = 1;
			break;
		case 'r':
			parsesort("%cpu");
			break;
		case 'S':
			sumrusage = 1;
			break;
		case 's':
			/* -L was already taken... */
			showlwps = 1;
			default_fmt = sfmt;
			break;
		case 'T':
			if ((ttname = ttyname(STDIN_FILENO)) == NULL)
				errx(1, "stdin: not a terminal");
			flag = ttyname2dev(ttname, &xflg, &what);
			break;
		case 't':
			flag = ttyname2dev(optarg, &xflg, &what);
			break;
		case 'U':
			if (*optarg != '\0') {
				struct passwd *pw;

				what = KERN_PROC_UID;
				pw = getpwnam(optarg);
				if (pw == NULL) {
					flag = parsenum(optarg, "user name");
				} else
					flag = pw->pw_uid;
			}
			break;
		case 'u':
			parsefmt(ufmt);
			parsesort("%cpu");
			fmt = 1;
			ufmt[0] = '\0';
			break;
		case 'v':
			parsefmt(vfmt);
			parsesort("vsz");
			fmt = 1;
			vfmt[0] = '\0';
			break;
		case 'W':
			swapf = optarg;
			break;
		case 'w':
			if (wflag)
				termwidth = UNLIMITED;
			else if (termwidth < 131)
				termwidth = 131;
			wflag++;
			break;
		case 'x':
			xflg = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

#define	BACKWARD_COMPATIBILITY
#ifdef	BACKWARD_COMPATIBILITY
	if (*argv) {
		nlistf = *argv;
		if (*++argv) {
			memf = *argv;
			if (*++argv)
				swapf = *argv;
		}
	}
#endif

	if (memf == NULL) {
		kd = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf);
		donlist_sysctl();
	} else
		kd = kvm_openfiles(nlistf, memf, swapf, O_RDONLY, errbuf);

	if (kd == 0)
		errx(1, "%s", errbuf);

	if (!fmt)
		parsefmt(default_fmt);

	/* Add default sort criteria */
	parsesort("tdev,pid");
	SIMPLEQ_FOREACH(vent, &sortlist, next) {
		if (vent->var->flag & LWP || vent->var->type == UNSPECIFIED)
			warnx("Cannot sort on %s, sort key ignored",
				vent->var->name);
	}

	/*
	 * scan requested variables, noting what structures are needed.
	 */
	scanvars();

	/*
	 * select procs
	 */
	if (!(kinfo = getkinfo_kvm(kd, what, flag, &nentries)))
		err(1, "%s", kvm_geterr(kd));
	if (nentries == 0) {
		printheader();
		exit(1);
	}
	/*
	 * sort proc list
	 */
	qsort(kinfo, nentries, sizeof(struct kinfo_proc2), pscomp);
	/*
	 * For each proc, call each variable output function in
	 * "setwidth" mode to determine the widest element of
	 * the column.
	 */

	for (i = 0; i < nentries; i++) {
		struct kinfo_proc2 *ki = &kinfo[i];

		if (xflg == 0 && (ki->p_tdev == (uint32_t)NODEV ||
		    (ki->p_flag & P_CONTROLT) == 0))
			continue;

		kl = kvm_getlwps(kd, ki->p_pid, ki->p_paddr,
		    sizeof(struct kinfo_lwp), &nlwps);
		if (kl == 0)
			nlwps = 0;
		if (showlwps == 0) {
			l = pick_representative_lwp(ki, kl, nlwps);
			SIMPLEQ_FOREACH(vent, &displaylist, next)
				OUTPUT(vent, ki, l, WIDTHMODE);
		} else {
			/* The printing is done with the loops
			 * reversed, but here we don't need that,
			 * and this improves the code locality a bit.
			 */
			SIMPLEQ_FOREACH(vent, &displaylist, next)
				for (j = 0; j < nlwps; j++)
					OUTPUT(vent, ki, &kl[j], WIDTHMODE);
		}
	}
	/*
	 * Print header - AFTER determining process field widths.
	 * printheader() also adds up the total width of all
	 * fields the first time it's called.
	 */
	printheader();
	/*
	 * For each proc, call each variable output function in
	 * print mode.
	 */
	for (i = lineno = 0; i < nentries; i++) {
		struct kinfo_proc2 *ki = &kinfo[i];

		if (xflg == 0 && (ki->p_tdev == (uint32_t)NODEV ||
		    (ki->p_flag & P_CONTROLT ) == 0))
			continue;
		kl = kvm_getlwps(kd, ki->p_pid, (u_long)ki->p_paddr,
		    sizeof(struct kinfo_lwp), &nlwps);
		if (kl == 0)
			nlwps = 0;
		if (showlwps == 0) {
			l = pick_representative_lwp(ki, kl, nlwps);
			SIMPLEQ_FOREACH(vent, &displaylist, next) {
				OUTPUT(vent, ki, l, PRINTMODE);
				if (SIMPLEQ_NEXT(vent, next) != NULL)
					(void)putchar(' ');
			}
			(void)putchar('\n');
			if (prtheader && lineno++ == prtheader - 4) {
				(void)putchar('\n');
				printheader();
				lineno = 0;
			}
		} else {
			for (j = 0; j < nlwps; j++) {
				SIMPLEQ_FOREACH(vent, &displaylist, next) {
					OUTPUT(vent, ki, &kl[j], PRINTMODE);
					if (SIMPLEQ_NEXT(vent, next) != NULL)
						(void)putchar(' ');
				}
				(void)putchar('\n');
				if (prtheader && lineno++ == prtheader - 4) {
					(void)putchar('\n');
					printheader();
					lineno = 0;
				}
			}
		}
	}
	exit(eval);
	/* NOTREACHED */
}

static struct kinfo_lwp *
pick_representative_lwp(struct kinfo_proc2 *ki, struct kinfo_lwp *kl, int nlwps)
{
	int i, onproc, running, sleeping, stopped, suspended;
	static struct kinfo_lwp zero_lwp;

	if (kl == 0)
		return &zero_lwp;

	/* Trivial case: only one LWP */
	if (nlwps == 1)
		return kl;

	switch (ki->p_realstat) {
	case SSTOP:
	case SACTIVE:
		/* Pick the most live LWP */
		onproc = running = sleeping = stopped = suspended = -1;
		for (i = 0; i < nlwps; i++) {
			switch (kl[i].l_stat) {
			case LSONPROC:
				onproc = i;
				break;
			case LSRUN:
				running = i;
				break;
			case LSSLEEP:
				sleeping = i;
				break;
			case LSSTOP:
				stopped = i;
				break;
			case LSSUSPENDED:
				suspended = i;
				break;
			}
		}
		if (onproc != -1)
			return &kl[onproc];
		if (running != -1)
			return &kl[running];
		if (sleeping != -1)
			return &kl[sleeping];
		if (stopped != -1)
			return &kl[stopped];
		if (suspended != -1)
			return &kl[suspended];
		break;
	case SZOMB:
		/* First will do */
		return kl;
		break;
	}
	/* Error condition! */
	warnx("Inconsistent LWP state for process %d", ki->p_pid);
	return kl;
}


static struct kinfo_proc2 *
getkinfo_kvm(kvm_t *kdp, int what, int flag, int *nentriesp)
{

	return (kvm_getproc2(kdp, what, flag, sizeof(struct kinfo_proc2),
	    nentriesp));
}

static void
scanvars(void)
{
	struct varent *vent;
	VAR *v;

	SIMPLEQ_FOREACH(vent, &displaylist, next) {
		v = vent->var;
		if (v->flag & COMM) {
			needcomm = 1;
			break;
		}
	}
}

static int
pscomp(const void *a, const void *b)
{
	const struct kinfo_proc2 *ka = (const struct kinfo_proc2 *)a;
	const struct kinfo_proc2 *kb = (const struct kinfo_proc2 *)b;

	int i;
	int64_t i64;
	VAR *v;
	struct varent *ve;
	const sigset_t *sa, *sb;

#define	V_SIZE(k) ((k)->p_vm_msize)
#define	RDIFF_N(t, n) \
	if (((const t *)((const char *)ka + v->off))[n] > ((const t *)((const char *)kb + v->off))[n]) \
		return 1; \
	if (((const t *)((const char *)ka + v->off))[n] < ((const t *)((const char *)kb + v->off))[n]) \
		return -1;

#define	RDIFF(type) RDIFF_N(type, 0); continue

	SIMPLEQ_FOREACH(ve, &sortlist, next) {
		v = ve->var;
		if (v->flag & LWP)
			/* LWP structure not available (yet) */
			continue;
		/* Sort on pvar() fields, + a few others */
		switch (v->type) {
		case CHAR:
			RDIFF(char);
		case UCHAR:
			RDIFF(u_char);
		case SHORT:
			RDIFF(short);
		case USHORT:
			RDIFF(ushort);
		case INT:
			RDIFF(int);
		case UINT:
			RDIFF(uint);
		case LONG:
			RDIFF(long);
		case ULONG:
			RDIFF(ulong);
		case INT32:
			RDIFF(int32_t);
		case UINT32:
			RDIFF(uint32_t);
		case SIGLIST:
			sa = (const void *)((const char *)a + v->off);
			sb = (const void *)((const char *)b + v->off);
			i = 0;
			do {
				if (sa->__bits[i] > sb->__bits[i])
					return 1;
				if (sa->__bits[i] < sb->__bits[i])
					return -1;
				i++;
			} while (i < (int)__arraycount(sa->__bits));
			continue;
		case INT64:
			RDIFF(int64_t);
		case KPTR:
		case KPTR24:
		case UINT64:
			RDIFF(uint64_t);
		case TIMEVAL:
			/* compare xxx_sec then xxx_usec */
			RDIFF_N(uint32_t, 0);
			RDIFF_N(uint32_t, 1);
			continue;
		case CPUTIME:
			i64 = ka->p_rtime_sec * 1000000 + ka->p_rtime_usec;
			i64 -= kb->p_rtime_sec * 1000000 + kb->p_rtime_usec;
			if (sumrusage) {
				i64 += ka->p_uctime_sec * 1000000
				    + ka->p_uctime_usec;
				i64 -= kb->p_uctime_sec * 1000000
				    + kb->p_uctime_usec;
			}
			if (i64 != 0)
				return i64 > 0 ? 1 : -1;
			continue;
		case PCPU:
			i = getpcpu(kb) - getpcpu(ka);
			if (i != 0)
				return i;
			continue;
		case VSIZE:
			i = V_SIZE(kb) - V_SIZE(ka);
			if (i != 0)
				return i;
			continue;

		default:
			/* Ignore everything else */
			break;
		}
	}
	return 0;

#undef VSIZE
}

/*
 * ICK (all for getopt), would rather hide the ugliness
 * here than taint the main code.
 *
 *  ps foo -> ps -foo
 *  ps 34 -> ps -p34
 *
 * The old convention that 't' with no trailing tty arg means the user's
 * tty, is only supported if argv[1] doesn't begin with a '-'.  This same
 * feature is available with the option 'T', which takes no argument.
 */
static char *
kludge_oldps_options(char *s)
{
	size_t len;
	char *newopts, *ns, *cp;

	len = strlen(s);
	if ((newopts = ns = malloc(len + 3)) == NULL)
		err(1, NULL);
	/*
	 * options begin with '-'
	 */
	if (*s != '-')
		*ns++ = '-';	/* add option flag */
	/*
	 * gaze to end of argv[1]
	 */
	cp = s + len - 1;
	/*
	 * if the last letter is a 't' flag and there are no other option
	 * characters that take arguments (eg U, p, o) in the option
	 * string and the option string doesn't start with a '-' then
	 * convert to 'T' (meaning *this* terminal, i.e. ttyname(0)).
	 */
	if (*cp == 't' && *s != '-' && strpbrk(s, ARGOPTS) == NULL)
		*cp = 'T';
	else {
		/*
		 * otherwise check for trailing number, which *may* be a
		 * pid.
		 */
		while (cp >= s && isdigit((unsigned char)*cp))
			--cp;
	}
	cp++;
	memmove(ns, s, (size_t)(cp - s));	/* copy up to trailing number */
	ns += cp - s;
	/*
	 * if there's a trailing number, and not a preceding 'p' (pid) or
	 * 't' (tty) flag, then assume it's a pid and insert a 'p' flag.
	 */
	if (isdigit((unsigned char)*cp) &&
	    (cp == s || (cp[-1] != 'U' && cp[-1] != 't' && cp[-1] != 'p' &&
	    cp[-1] != '/' && (cp - 1 == s || cp[-2] != 't'))))
		*ns++ = 'p';
	/* and append the number */
	(void)strcpy(ns, cp);		/* XXX strcpy is safe here */

	return (newopts);
}

static int
parsenum(const char *str, const char *msg)
{
	char *ep;
	unsigned long ul;

	ul = strtoul(str, &ep, 0);

	if (*str == '\0' || *ep != '\0')
		errx(1, "Invalid %s: `%s'", msg, str);

	if (ul > INT_MAX)
		errx(1, "Out of range %s: `%s'", msg, str);

	return (int)ul;
}

static void
usage(void)
{

	(void)fprintf(stderr,
	    "usage:\t%s\n\t   %s\n\t%s\n",
	    "ps [-AaCcehjlmrSsTuvwx] [-k key] [-M core] [-N system] [-O fmt]",
	    "[-o fmt] [-p pid] [-t tty] [-U user] [-W swap]",
	    "ps -L");
	exit(1);
	/* NOTREACHED */
}
