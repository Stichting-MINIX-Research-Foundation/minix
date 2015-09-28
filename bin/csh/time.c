/* $NetBSD: time.c,v 1.20 2013/07/16 17:47:43 christos Exp $ */

/*-
 * Copyright (c) 1980, 1991, 1993
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
static char sccsid[] = "@(#)time.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: time.c,v 1.20 2013/07/16 17:47:43 christos Exp $");
#endif
#endif /* not lint */

#ifndef NOT_CSH
#include <sys/types.h>
#include <stdarg.h>
#include "csh.h"
#include "extern.h"
#endif
#include <util.h>

/*
 * C Shell - routines handling process timing and niceing
 */
static void pdeltat(FILE *, struct timeval *, struct timeval *);
static void pcsecs(FILE *, long);

#ifndef NOT_CSH
void
settimes(void)
{
    struct rusage ruch;

    (void)clock_gettime(CLOCK_MONOTONIC, &time0);
    (void)getrusage(RUSAGE_SELF, &ru0);
    (void)getrusage(RUSAGE_CHILDREN, &ruch);
    ruadd(&ru0, &ruch);
}

/*
 * dotime is only called if it is truly a builtin function and not a
 * prefix to another command
 */
void
/*ARGSUSED*/
dotime(Char **v, struct command *t)
{
    struct rusage ru1, ruch;
    struct timespec timedol;

    (void)getrusage(RUSAGE_SELF, &ru1);
    (void)getrusage(RUSAGE_CHILDREN, &ruch);
    ruadd(&ru1, &ruch);
    (void)clock_gettime(CLOCK_MONOTONIC, &timedol);
    prusage(cshout, &ru0, &ru1, &timedol, &time0);
}

/*
 * donice is only called when it on the line by itself or with a +- value
 */
void
/*ARGSUSED*/
donice(Char **v, struct command *t)
{
    Char *cp;
    int nval;

    nval = 0;
    v++;
    cp = *v++;
    if (cp == 0)
	nval = 4;
    else if (*v == 0 && any("+-", cp[0]))
	nval = getn(cp);
    (void)setpriority(PRIO_PROCESS, 0, nval);
}

void
ruadd(struct rusage *ru, struct rusage *ru2)
{
    timeradd(&ru->ru_utime, &ru2->ru_utime, &ru->ru_utime);
    timeradd(&ru->ru_stime, &ru2->ru_stime, &ru->ru_stime);
    if (ru2->ru_maxrss > ru->ru_maxrss)
	ru->ru_maxrss = ru2->ru_maxrss;

    ru->ru_ixrss += ru2->ru_ixrss;
    ru->ru_idrss += ru2->ru_idrss;
    ru->ru_isrss += ru2->ru_isrss;
    ru->ru_minflt += ru2->ru_minflt;
    ru->ru_majflt += ru2->ru_majflt;
    ru->ru_nswap += ru2->ru_nswap;
    ru->ru_inblock += ru2->ru_inblock;
    ru->ru_oublock += ru2->ru_oublock;
    ru->ru_msgsnd += ru2->ru_msgsnd;
    ru->ru_msgrcv += ru2->ru_msgrcv;
    ru->ru_nsignals += ru2->ru_nsignals;
    ru->ru_nvcsw += ru2->ru_nvcsw;
    ru->ru_nivcsw += ru2->ru_nivcsw;
}
#endif /* NOT_CSH */

void
prusage(FILE *fp, struct rusage *r0, struct rusage *r1, struct timespec *e,
        struct timespec *b)
{
#ifndef NOT_CSH
    struct varent *vp;
#endif
    const char *cp;
    long i;
    time_t t;
    time_t ms;

    cp = "%Uu %Ss %E %P %X+%Dk %I+%Oio %Fpf+%Ww";
    ms = (e->tv_sec - b->tv_sec) * 100 + (e->tv_nsec - b->tv_nsec) / 10000000;
    t = (r1->ru_utime.tv_sec - r0->ru_utime.tv_sec) * 100 +
        (r1->ru_utime.tv_usec - r0->ru_utime.tv_usec) / 10000 +
        (r1->ru_stime.tv_sec - r0->ru_stime.tv_sec) * 100 +
        (r1->ru_stime.tv_usec - r0->ru_stime.tv_usec) / 10000;
#ifndef NOT_CSH
    vp = adrof(STRtime);

    if (vp && vp->vec[0] && vp->vec[1])
	cp = short2str(vp->vec[1]);
#endif

    for (; *cp; cp++)
	if (*cp != '%')
	    (void) fputc(*cp, fp);
	else if (cp[1])
	    switch (*++cp) {
	    case 'D':		/* (average) unshared data size */
		(void)fprintf(fp, "%ld", t == 0 ? 0L :
			(long)((r1->ru_idrss + r1->ru_isrss -
			 (r0->ru_idrss + r0->ru_isrss)) / t));
		break;
	    case 'E':		/* elapsed (wall-clock) time */
		pcsecs(fp, (long) ms);
		break;
	    case 'F':		/* page faults */
		(void)fprintf(fp, "%ld", r1->ru_majflt - r0->ru_majflt);
		break;
	    case 'I':		/* FS blocks in */
		(void)fprintf(fp, "%ld", r1->ru_inblock - r0->ru_inblock);
		break;
	    case 'K':		/* (average) total data memory used  */
		(void)fprintf(fp, "%ld", t == 0 ? 0L :
			(long)(((r1->ru_ixrss + r1->ru_isrss + r1->ru_idrss) -
			 (r0->ru_ixrss + r0->ru_idrss + r0->ru_isrss)) / t));
		break;
	    case 'M':		/* max. Resident Set Size */
		(void)fprintf(fp, "%ld", r1->ru_maxrss / 2L);
		break;
	    case 'O':		/* FS blocks out */
		(void)fprintf(fp, "%ld", r1->ru_oublock - r0->ru_oublock);
		break;
	    case 'P':		/* percent time spent running */
		/* check if it did not run at all */
		if (ms == 0) {
			(void)fputs("0.0%", fp);
		} else {
			char pb[32];
			(void)fputs(strpct(pb, sizeof(pb),
			    (uintmax_t)t, (uintmax_t)ms, 1), fp);
			(void)fputc('%', fp);
		}
		break;
	    case 'R':		/* page reclaims */
		(void)fprintf(fp, "%ld", r1->ru_minflt - r0->ru_minflt);
		break;
	    case 'S':		/* system CPU time used */
		pdeltat(fp, &r1->ru_stime, &r0->ru_stime);
		break;
	    case 'U':		/* user CPU time used */
		pdeltat(fp, &r1->ru_utime, &r0->ru_utime);
		break;
	    case 'W':		/* number of swaps */
		i = r1->ru_nswap - r0->ru_nswap;
		(void)fprintf(fp, "%ld", i);
		break;
	    case 'X':		/* (average) shared text size */
		(void)fprintf(fp, "%ld", t == 0 ? 0L : 
			       (long)((r1->ru_ixrss - r0->ru_ixrss) / t));
		break;
	    case 'c':		/* num. involuntary context switches */
		(void)fprintf(fp, "%ld", r1->ru_nivcsw - r0->ru_nivcsw);
		break;
	    case 'k':		/* number of signals received */
		(void)fprintf(fp, "%ld", r1->ru_nsignals-r0->ru_nsignals);
		break;
	    case 'r':		/* socket messages received */
		(void)fprintf(fp, "%ld", r1->ru_msgrcv - r0->ru_msgrcv);
		break;
	    case 's':		/* socket messages sent */
		(void)fprintf(fp, "%ld", r1->ru_msgsnd - r0->ru_msgsnd);
		break;
	    case 'w':		/* num. voluntary context switches (waits) */
		(void)fprintf(fp, "%ld", r1->ru_nvcsw - r0->ru_nvcsw);
		break;
	    }
    (void)fputc('\n', fp);
}

static void
pdeltat(FILE *fp, struct timeval *t1, struct timeval *t0)
{
    struct timeval td;

    timersub(t1, t0, &td);
    (void)fprintf(fp, "%ld.%01ld", (long)td.tv_sec,
	(long)(td.tv_usec / 100000));
}

#define  P2DIG(fp, i) (void)fprintf(fp, "%ld%ld", (i) / 10, (i) % 10)

#ifndef NOT_CSH
void
psecs(long l)
{
    long i;

    i = l / 3600;
    if (i) {
	(void)fprintf(cshout, "%ld:", i);
	i = l % 3600;
	P2DIG(cshout, i / 60);
	goto minsec;
    }
    i = l;
    (void)fprintf(cshout, "%ld", i / 60);
minsec:
    i %= 60;
    (void)fputc(':', cshout);
    P2DIG(cshout, i);
}
#endif

static void
pcsecs(FILE *fp, long l)	/* PWP: print mm:ss.dd, l is in sec*100 */
{
    long i;

    i = l / 360000;
    if (i) {
	(void)fprintf(fp, "%ld:", i);
	i = (l % 360000) / 100;
	P2DIG(fp, i / 60);
	goto minsec;
    }
    i = l / 100;
    (void)fprintf(fp, "%ld", i / 60);
minsec:
    i %= 60;
    (void)fputc(':', fp);
    P2DIG(fp, i);
    (void)fputc('.', fp);
    P2DIG(fp, (l % 100));
}
