/*	$NetBSD: keyword.c,v 1.54 2014/01/15 08:07:53 mlelstv Exp $	*/

/*-
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
#if 0
static char sccsid[] = "@(#)keyword.c	8.5 (Berkeley) 4/2/94";
#else
__RCSID("$NetBSD: keyword.c,v 1.54 2014/01/15 08:07:53 mlelstv Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/lwp.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/ucred.h>

#include <err.h>
#include <errno.h>
#include <kvm.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "ps.h"

static VAR *findvar(const char *);
static int  vcmp(const void *, const void *);

#if 0 	/* kernel doesn't calculate these */
	PUVAR("idrss", "IDRSS", 0, p_uru_idrss, UINT64, PRIu64),
	PUVAR("isrss", "ISRSS", 0, p_uru_isrss, UINT64, PRId64),
	PUVAR("ixrss", "IXRSS", 0, p_uru_ixrss, UINT64, PRId64),
	PUVAR("maxrss", "MAXRSS", 0, p_uru_maxrss, UINT64, PRIu64),
#endif

/* Compute offset in common structures. */
#define	POFF(x)	offsetof(struct kinfo_proc2, x)
#define	LOFF(x)	offsetof(struct kinfo_lwp, x)

#define	UIDFMT	"u"
#define	UID(n1, n2, of) \
	{ .name = n1, .header = n2, .flag = 0, .oproc = pvar, \
	  .off = POFF(of), .type = UINT32, .fmt = UIDFMT }
#define	GID(n1, n2, off)	UID(n1, n2, off)

#define	PIDFMT	"d"
#define	PID(n1, n2, of) \
	{ .name = n1, .header = n2, .flag = 0, .oproc = pvar, \
	  .off = POFF(of), .type = INT32, .fmt = PIDFMT }

#define	LVAR(n1, n2, fl, of, ty, fm) \
	{ .name = n1, .header = n2, .flag = (fl) | LWP, .oproc = pvar, \
	  .off = LOFF(of), .type = ty, .fmt = fm }
#define	PVAR(n1, n2, fl, of, ty, fm) \
	{ .name = n1, .header = n2, .flag = (fl) | 0, .oproc = pvar, \
	  .off = POFF(of), .type = ty, .fmt = fm }
#define	PUVAR(n1, n2, fl, of, ty, fm) \
	{ .name = n1, .header = n2, .flag = (fl) | UAREA, .oproc = pvar, \
	  .off = POFF(of), .type = ty, .fmt = fm }
#define VAR3(n1, n2, fl) \
	{ .name = n1, .header = n2, .flag = fl }
#define VAR4(n1, n2, fl, op) \
	{ .name = n1, .header = n2, .flag = fl, .oproc = op, }
#define VAR6(n1, n2, fl, op, of, ty) \
	{ .name = n1, .header = n2, .flag = fl, .oproc = op, \
	  .off = of, .type = ty }

/* NB: table must be sorted, in vi use:
 *	:/^VAR/,/end_sort/! sort -t\" +1
 * breaking long lines just makes the sort harder
 *
 * We support all the fields required by P1003.1-2004 (SUSv3), with
 * the correct default headers, except for the "tty" field, where the
 * standard says the header should be "TT", but we have "TTY".
 */
VAR var[] = {
	VAR6("%cpu", "%CPU", 0, pcpu, 0, PCPU),
	VAR6("%mem", "%MEM", 0, pmem, POFF(p_vm_rssize), INT32),
	PVAR("acflag", "ACFLG", 0, p_acflag, USHORT, "x"),
	VAR3("acflg", "acflag", ALIAS),
	VAR3("args", "command", ALIAS),
	VAR3("blocked", "sigmask", ALIAS),
	VAR3("caught", "sigcatch", ALIAS),
	VAR4("comm", "COMMAND", COMM|ARGV0|LJUST, command),
	VAR4("command", "COMMAND", COMM|LJUST, command),
	PVAR("cpu", "CPU", 0, p_estcpu, UINT, "u"),
	VAR4("cpuid", "CPUID", LWP, cpuid),
	VAR3("cputime", "time", ALIAS),
	VAR6("ctime", "CTIME", 0, putimeval, POFF(p_uctime_sec), TIMEVAL),
	GID("egid", "EGID", p_gid),
	VAR4("egroup", "EGROUP", LJUST, gname),
	VAR4("emul", "EMUL", LJUST, emul),
	VAR6("etime", "ELAPSED", 0, elapsed, POFF(p_ustart_sec), TIMEVAL),
	UID("euid", "EUID", p_uid),
	VAR4("euser", "EUSER", LJUST, uname),
	PVAR("f", "F", 0, p_flag, INT, "x"),
	VAR3("flags", "f", ALIAS),
	GID("gid", "GID", p_gid),
	VAR4("group", "GROUP", LJUST, gname),
	VAR4("groupnames", "GROUPNAMES", LJUST, groupnames),
	VAR4("groups", "GROUPS", LJUST, groups),
	/* holdcnt: unused, left for compat. */
	LVAR("holdcnt", "HOLDCNT", 0, l_holdcnt, INT, "d"),
	VAR3("ignored", "sigignore", ALIAS),
	PUVAR("inblk", "INBLK", 0, p_uru_inblock, UINT64, PRIu64),
	VAR3("inblock", "inblk", ALIAS),
	PVAR("jobc", "JOBC", 0, p_jobc, SHORT, "d"),
	PVAR("ktrace", "KTRACE", 0, p_traceflag, INT, "x"),
/*XXX*/	PVAR("ktracep", "KTRACEP", 0, p_tracep, KPTR, PRIx64),
	LVAR("laddr", "LADDR", 0, l_laddr, KPTR, PRIx64),
	LVAR("lid", "LID", 0, l_lid, INT32, "d"),
	VAR4("lim", "LIM", 0, maxrss),
	VAR4("lname", "LNAME", LJUST|LWP, lname),
	VAR4("login", "LOGIN", LJUST, logname),
	VAR3("logname", "login", ALIAS),
	VAR6("lstart", "STARTED", LJUST, lstarted, POFF(p_ustart_sec), UINT32),
	VAR4("lstate", "STAT", LJUST|LWP, lstate),
	VAR6("ltime", "LTIME", LWP, lcputime, 0, CPUTIME),
	PUVAR("majflt", "MAJFLT", 0, p_uru_majflt, UINT64, PRIu64),
	PUVAR("minflt", "MINFLT", 0, p_uru_minflt, UINT64, PRIu64),
	PUVAR("msgrcv", "MSGRCV", 0, p_uru_msgrcv, UINT64, PRIu64),
	PUVAR("msgsnd", "MSGSND", 0, p_uru_msgsnd, UINT64, PRIu64),
	VAR3("ni", "nice", ALIAS),
	VAR6("nice", "NI", 0, pnice, POFF(p_nice), UCHAR),
	PUVAR("nivcsw", "NIVCSW", 0, p_uru_nivcsw, UINT64, PRIu64),
	PVAR("nlwp", "NLWP", 0, p_nlwps, UINT64, PRId64),
	VAR3("nsignals", "nsigs", ALIAS),
	PUVAR("nsigs", "NSIGS", 0, p_uru_nsignals, UINT64, PRIu64),
	/* nswap: unused, left for compat. */
	PUVAR("nswap", "NSWAP", 0, p_uru_nswap, UINT64, PRIu64),
	PUVAR("nvcsw", "NVCSW", 0, p_uru_nvcsw, UINT64, PRIu64),
/*XXX*/	LVAR("nwchan", "WCHAN", 0, l_wchan, KPTR, PRIx64),
	PUVAR("oublk", "OUBLK", 0, p_uru_oublock, UINT64, PRIu64),
	VAR3("oublock", "oublk", ALIAS),
/*XXX*/	PVAR("p_ru", "P_RU", 0, p_ru, KPTR, PRIx64),
/*XXX*/	PVAR("paddr", "PADDR", 0, p_paddr, KPTR, PRIx64),
	PUVAR("pagein", "PAGEIN", 0, p_uru_majflt, UINT64, PRIu64),
	VAR3("pcpu", "%cpu", ALIAS),
	VAR3("pending", "sig", ALIAS),
	PID("pgid", "PGID", p__pgid),
	PID("pid", "PID", p_pid),
	VAR3("pmem", "%mem", ALIAS),
	PID("ppid", "PPID", p_ppid),
	VAR4("pri", "PRI", LWP, pri),
	LVAR("re", "RE", INF127, l_swtime, UINT, "u"),
	GID("rgid", "RGID", p_rgid),
	VAR4("rgroup", "RGROUP", LJUST, rgname),
/*XXX*/	LVAR("rlink", "RLINK", 0, l_back, KPTR, PRIx64),
	PVAR("rlwp", "RLWP", 0, p_nrlwps, UINT64, PRId64),
	VAR6("rss", "RSS", 0, p_rssize, POFF(p_vm_rssize), INT32),
	VAR3("rssize", "rsz", ALIAS),
	VAR6("rsz", "RSZ", 0, rssize, POFF(p_vm_rssize), INT32),
	UID("ruid", "RUID", p_ruid),
	VAR4("ruser", "RUSER", LJUST, runame),
	PVAR("sess", "SESS", 0, p_sess, KPTR24, PRIx64),
	PID("sid", "SID", p_sid),
	PVAR("sig", "PENDING", 0, p_siglist, SIGLIST, "s"),
	PVAR("sigcatch", "CAUGHT", 0, p_sigcatch, SIGLIST, "s"),
	PVAR("sigignore", "IGNORED", 0, p_sigignore, SIGLIST, "s"),
	PVAR("sigmask", "BLOCKED", 0, p_sigmask, SIGLIST, "s"),
	LVAR("sl", "SL", INF127, l_slptime, UINT, "u"),
	VAR6("start", "STARTED", 0, started, POFF(p_ustart_sec), UINT32),
	VAR3("stat", "state", ALIAS),
	VAR4("state", "STAT", LJUST, state),
	VAR6("stime", "STIME", 0, putimeval, POFF(p_ustime_sec), TIMEVAL),
	GID("svgid", "SVGID", p_svgid),
	VAR4("svgroup", "SVGROUP", LJUST, svgname),
	UID("svuid", "SVUID", p_svuid),
	VAR4("svuser", "SVUSER", LJUST, svuname),
	/* "tdev" is UINT32, but we do this for sorting purposes */
	VAR6("tdev", "TDEV", 0, tdev, POFF(p_tdev), INT32),
	VAR6("time", "TIME", 0, cputime, 0, CPUTIME),
	PID("tpgid", "TPGID", p_tpgid),
	PVAR("tsess", "TSESS", 0, p_tsess, KPTR, PRIx64),
	VAR6("tsiz", "TSIZ", 0, tsize, POFF(p_vm_tsize), INT32),
#ifndef __minix
	VAR6("tt", "TTY", LJUST, tname, POFF(p_tdev), INT32),
#else /* __minix */
	VAR6("tt", "TT", LJUST, tname, POFF(p_tdev), INT32),
#endif /* __minix */
	VAR6("tty", "TTY", LJUST, longtname, POFF(p_tdev), INT32),
	LVAR("uaddr", "UADDR", 0, l_addr, KPTR, PRIx64),
	VAR4("ucomm", "UCOMM", LJUST, ucomm),
	UID("uid", "UID", p_uid),
	LVAR("upr", "UPR", 0, l_usrpri, UCHAR, "u"),
	VAR4("user", "USER", LJUST, uname),
	VAR3("usrpri", "upr", ALIAS),
	VAR6("utime", "UTIME", 0, putimeval, POFF(p_uutime_sec), TIMEVAL),
	VAR3("vsize", "vsz", ALIAS),
	VAR6("vsz", "VSZ", 0, vsize, 0, VSIZE),
	VAR4("wchan", "WCHAN", LJUST|LWP, wchan),
	PVAR("xstat", "XSTAT", 0, p_xstat, USHORT, "x"),
/* "zzzz" end_sort */
	{ .name = "" },
};

void
showkey(void)
{
	VAR *v;
	int i;
	const char *p;
	const char *sep;

	i = 0;
	sep = "";
	for (v = var; *(p = v->name); ++v) {
		int len = strlen(p);
		if (termwidth && (i += len + 1) > termwidth) {
			i = len;
			sep = "\n";
		}
		(void)printf("%s%s", sep, p);
		sep = " ";
	}
	(void)printf("\n");
}

/*
 * Parse the string pp, and insert or append entries to the list
 * referenced by listptr.  If pos in non-null and *pos is non-null, then
 * *pos specifies where to insert (instead of appending).  If pos is
 * non-null, then a new value is returned through *pos referring to the
 * last item inserted.
 */
static void
parsevarlist(const char *pp, struct varlist *listptr, struct varent **pos)
{
	char *p, *sp, *equalsp;

	/* dup to avoid zapping arguments.  We will free sp later. */
	p = sp = strdup(pp);

	/*
	 * Everything after the first '=' is part of a custom header.
	 * Temporarily replace it with '\0' to simplify other code.
	 */
	equalsp = strchr(p, '=');
	if (equalsp)
	    *equalsp = '\0';

#define	FMTSEP	" \t,\n"
	while (p && *p) {
		char *cp;
		VAR *v;
		struct varent *vent;

		/*
		 * skip separators before the first keyword, and
		 * look for the separator after the keyword.
		 */
		for (cp = p; *cp != '\0'; cp++) {
		    p = strpbrk(cp, FMTSEP);
		    if (p != cp)
			break;
		}
		if (*cp == '\0')
		    break;
		/*
		 * Now cp points to the start of a keyword,
		 * and p is NULL or points past the end of the keyword.
		 *
		 * Terminate the keyword with '\0', or reinstate the
		 * '=' that was removed earlier, if appropriate.
		 */
		if (p) {
			*p = '\0';
			p++;
		} else if (equalsp) {
			*equalsp = '=';
		}

		/*
		 * If findvar() likes the keyword or keyword=header,
		 * add it to our list.  If findvar() doesn't like it,
		 * it will print a warning, so we ignore it.
		 */
		if ((v = findvar(cp)) == NULL)
			continue;
		if ((vent = malloc(sizeof(struct varent))) == NULL)
			err(1, NULL);
		vent->var = v;
		if (pos && *pos)
		    SIMPLEQ_INSERT_AFTER(listptr, *pos, vent, next);
		else {
		    SIMPLEQ_INSERT_TAIL(listptr, vent, next);
		}
		if (pos)
		    *pos = vent;
	}
 	free(sp);
	if (SIMPLEQ_EMPTY(listptr))
		errx(1, "no valid keywords");
}

void
parsefmt(const char *p)
{

	parsevarlist(p, &displaylist, NULL);
}

void
parsefmt_insert(const char *p, struct varent **pos)
{

	parsevarlist(p, &displaylist, pos);
}

void
parsesort(const char *p)
{

	parsevarlist(p, &sortlist, NULL);
}

/* Search through a list for an entry with a specified name. */
struct varent *
varlist_find(struct varlist *list, const char *name)
{
	struct varent *vent;

	SIMPLEQ_FOREACH(vent, list, next) {
		if (strcmp(vent->var->name, name) == 0)
			break;
	}
	return vent;
}

static VAR *
findvar(const char *p)
{
	VAR *v;
	char *hp;

	hp = strchr(p, '=');
	if (hp)
		*hp++ = '\0';

	v = bsearch(p, var, sizeof(var)/sizeof(VAR) - 1, sizeof(VAR), vcmp);
	if (v && v->flag & ALIAS)
		v = findvar(v->header);
	if (!v) {
		warnx("%s: keyword not found", p);
		eval = 1;
		return NULL;
	}

	if (v && hp) {
		/*
		 * Override the header.
		 *
		 * We need to copy the entry first, and override the
		 * header in the copy, because the same field might be
		 * used multiple times with different headers.  We also
		 * need to strdup the header.
		 */
		struct var *newvar;
		char *newheader;

		if ((newvar = malloc(sizeof(struct var))) == NULL)
			err(1, NULL);
		if ((newheader = strdup(hp)) == NULL)
			err(1, NULL);
		memcpy(newvar, v, sizeof(struct var));
		newvar->header = newheader;

		/*
		 * According to P1003.1-2004, if the header text is null,
		 * such as -o user=, the field width will be at least as
		 * wide as the default header text.
		 */
		if (*hp == '\0')
			newvar->width = strlen(v->header);

		v = newvar;
	}
	return v;
}

static int
vcmp(const void *a, const void *b)
{
        return strcmp(a, ((const VAR *)b)->name);
}
