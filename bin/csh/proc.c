/* $NetBSD: proc.c,v 1.36 2013/07/16 17:47:43 christos Exp $ */

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
static char sccsid[] = "@(#)proc.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: proc.c,v 1.36 2013/07/16 17:47:43 christos Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "csh.h"
#include "dir.h"
#include "extern.h"
#include "proc.h"

#define BIGINDEX 9 /* largest desirable job index */

extern int insource;

static void pflushall(void);
static void pflush(struct process *);
static void pclrcurr(struct process *);
static void padd(struct command *);
static int pprint(struct process *, int);
static void ptprint(struct process *);
static void pads(Char *);
static void pkill(Char **v, int);
static struct process *pgetcurr(struct process *);
static void okpcntl(void);

/*
 * pchild - called at interrupt level by the SIGCHLD signal
 *	indicating that at least one child has terminated or stopped
 *	thus at least one wait system call will definitely return a
 *	childs status.  Top level routines (like pwait) must be sure
 *	to mask interrupts when playing with the proclist data structures!
 */
/* ARGSUSED */
void
pchild(int notused)
{
    struct rusage ru;
    struct process *fp, *pp;
    int jobflags, pid, w;

loop:
    errno = 0;			/* reset, just in case */
    pid = wait3(&w,
       (setintr && (intty || insource) ? WNOHANG | WUNTRACED : WNOHANG), &ru);

    if (pid <= 0) {
	if (errno == EINTR) {
	    errno = 0;
	    goto loop;
	}
	pnoprocesses = pid == -1;
	return;
    }
    for (pp = proclist.p_next; pp != NULL; pp = pp->p_next)
	if (pid == pp->p_pid)
	    goto found;
    goto loop;
found:
    if (pid == atoi(short2str(value(STRchild))))
	unsetv(STRchild);
    pp->p_flags &= ~(PRUNNING | PSTOPPED | PREPORTED);
    if (WIFSTOPPED(w)) {
	pp->p_flags |= PSTOPPED;
	pp->p_reason = WSTOPSIG(w);
    }
    else {
	if (pp->p_flags & (PTIME | PPTIME) || adrof(STRtime))
	    (void)clock_gettime(CLOCK_MONOTONIC, &pp->p_etime);

	pp->p_rusage = ru;
	if (WIFSIGNALED(w)) {
	    if (WTERMSIG(w) == SIGINT)
		pp->p_flags |= PINTERRUPTED;
	    else
		pp->p_flags |= PSIGNALED;
	    if (WCOREDUMP(w))
		pp->p_flags |= PDUMPED;
	    pp->p_reason = WTERMSIG(w);
	}
	else {
	    pp->p_reason = WEXITSTATUS(w);
	    if (pp->p_reason != 0)
		pp->p_flags |= PAEXITED;
	    else
		pp->p_flags |= PNEXITED;
	}
    }
    jobflags = 0;
    fp = pp;
    do {
	if ((fp->p_flags & (PPTIME | PRUNNING | PSTOPPED)) == 0 &&
	    !child && adrof(STRtime) &&
	    fp->p_rusage.ru_utime.tv_sec + fp->p_rusage.ru_stime.tv_sec
	    >= atoi(short2str(value(STRtime))))
	    fp->p_flags |= PTIME;
	jobflags |= fp->p_flags;
    } while ((fp = fp->p_friends) != pp);
    pp->p_flags &= ~PFOREGND;
    if (pp == pp->p_friends && (pp->p_flags & PPTIME)) {
	pp->p_flags &= ~PPTIME;
	pp->p_flags |= PTIME;
    }
    if ((jobflags & (PRUNNING | PREPORTED)) == 0) {
	fp = pp;
	do {
	    if (fp->p_flags & PSTOPPED)
		fp->p_flags |= PREPORTED;
	} while ((fp = fp->p_friends) != pp);
	while (fp->p_pid != fp->p_jobid)
	    fp = fp->p_friends;
	if (jobflags & PSTOPPED) {
	    if (pcurrent && pcurrent != fp)
		pprevious = pcurrent;
	    pcurrent = fp;
	}
	else
	    pclrcurr(fp);
	if (jobflags & PFOREGND) {
	    if (jobflags & (PSIGNALED | PSTOPPED | PPTIME) ||
#ifdef IIASA
		jobflags & PAEXITED ||
#endif
		!eq(dcwd->di_name, fp->p_cwd->di_name)) {
		;		/* print in pjwait */
	    }
	    /* PWP: print a newline after ^C */
	    else if (jobflags & PINTERRUPTED) {
		(void)vis_fputc('\r' | QUOTE, cshout);
		(void)fputc('\n', cshout);
	    }
	}
	else {
	    if (jobflags & PNOTIFY || adrof(STRnotify)) {
		(void)vis_fputc('\r' | QUOTE, cshout);
		(void)fputc('\n', cshout);
		(void)pprint(pp, NUMBER | NAME | REASON);
		if ((jobflags & PSTOPPED) == 0)
		    pflush(pp);
	    }
	    else {
		fp->p_flags |= PNEEDNOTE;
		neednote++;
	    }
	}
    }
    goto loop;
}

void
pnote(void)
{
    struct process *pp;
    sigset_t osigset, nsigset;
    int flags;

    neednote = 0;
    sigemptyset(&nsigset);
    (void)sigaddset(&nsigset, SIGCHLD);
    for (pp = proclist.p_next; pp != NULL; pp = pp->p_next) {
	if (pp->p_flags & PNEEDNOTE) {
	    (void)sigprocmask(SIG_BLOCK, &nsigset, &osigset);
	    pp->p_flags &= ~PNEEDNOTE;
	    flags = pprint(pp, NUMBER | NAME | REASON);
	    if ((flags & (PRUNNING | PSTOPPED)) == 0)
		pflush(pp);
	    (void)sigprocmask(SIG_SETMASK, &osigset, NULL);
	}
    }
}

/*
 * pwait - wait for current job to terminate, maintaining integrity
 *	of current and previous job indicators.
 */
void
pwait(void)
{
    struct process *fp, *pp;
    sigset_t osigset, nsigset;

    /*
     * Here's where dead procs get flushed.
     */
    sigemptyset(&nsigset);
    (void)sigaddset(&nsigset, SIGCHLD);
    (void)sigprocmask(SIG_BLOCK, &nsigset, &osigset);
    for (pp = (fp = &proclist)->p_next; pp != NULL; pp = (fp = pp)->p_next)
	if (pp->p_pid == 0) {
	    fp->p_next = pp->p_next;
	    xfree((ptr_t) pp->p_command);
	    if (pp->p_cwd && --pp->p_cwd->di_count == 0)
		if (pp->p_cwd->di_next == 0)
		    dfree(pp->p_cwd);
	    xfree((ptr_t) pp);
	    pp = fp;
	}
    (void)sigprocmask(SIG_SETMASK, &osigset, NULL);
    pjwait(pcurrjob);
}


/*
 * pjwait - wait for a job to finish or become stopped
 *	It is assumed to be in the foreground state (PFOREGND)
 */
void
pjwait(struct process *pp)
{
    struct process *fp;
    sigset_t osigset, nsigset;
    int jobflags, reason;

    while (pp->p_pid != pp->p_jobid)
	pp = pp->p_friends;
    fp = pp;

    do {
	if ((fp->p_flags & (PFOREGND | PRUNNING)) == PRUNNING)
	    (void)fprintf(csherr, "BUG: waiting for background job!\n");
    } while ((fp = fp->p_friends) != pp);
    /*
     * Now keep pausing as long as we are not interrupted (SIGINT), and the
     * target process, or any of its friends, are running
     */
    fp = pp;
    sigemptyset(&nsigset);
    (void)sigaddset(&nsigset, SIGCHLD);
    (void)sigprocmask(SIG_BLOCK, &nsigset, &osigset);
    for (;;) {
	sigemptyset(&nsigset);
	(void)sigaddset(&nsigset, SIGCHLD);
	(void)sigprocmask(SIG_BLOCK, &nsigset, NULL);
	jobflags = 0;
	do
	    jobflags |= fp->p_flags;
	while ((fp = (fp->p_friends)) != pp);
	if ((jobflags & PRUNNING) == 0)
	    break;
#ifdef JOBDEBUG
	(void)fprintf(csherr, "starting to sigsuspend for  SIGCHLD on %d\n",
		       fp->p_pid);
#endif				/* JOBDEBUG */
	nsigset = osigset;
	(void)sigdelset(&nsigset, SIGCHLD);
	(void)sigsuspend(&nsigset);
    }
    (void)sigprocmask(SIG_SETMASK, &osigset, NULL);
    if (tpgrp > 0)		/* get tty back */
	(void)tcsetpgrp(FSHTTY, tpgrp);
    if ((jobflags & (PSIGNALED | PSTOPPED | PTIME)) ||
	!eq(dcwd->di_name, fp->p_cwd->di_name)) {
	if (jobflags & PSTOPPED) {
	    (void) fputc('\n', cshout);
	    if (adrof(STRlistjobs)) {
		Char *jobcommand[3];

		jobcommand[0] = STRjobs;
		if (eq(value(STRlistjobs), STRlong))
		    jobcommand[1] = STRml;
		else
		    jobcommand[1] = NULL;
		jobcommand[2] = NULL;

		dojobs(jobcommand, NULL);
		(void)pprint(pp, SHELLDIR);
	    }
	    else
		(void)pprint(pp, AREASON | SHELLDIR);
	}
	else
	    (void)pprint(pp, AREASON | SHELLDIR);
    }
    if ((jobflags & (PINTERRUPTED | PSTOPPED)) && setintr &&
	(!gointr || !eq(gointr, STRminus))) {
	if ((jobflags & PSTOPPED) == 0)
	    pflush(pp);
	pintr1(0);
    }
    reason = 0;
    fp = pp;
    do {
	if (fp->p_reason)
	    reason = fp->p_flags & (PSIGNALED | PINTERRUPTED) ?
		fp->p_reason | META : fp->p_reason;
    } while ((fp = fp->p_friends) != pp);
    if ((reason != 0) && (adrof(STRprintexitvalue))) {
	(void)fprintf(cshout, "Exit %d\n", reason);
    }
    set(STRstatus, putn(reason));
    if (reason && exiterr)
	exitstat();
    pflush(pp);
}

/*
 * dowait - wait for all processes to finish
 */
void
/*ARGSUSED*/
dowait(Char **v, struct command *t)
{
    struct process *pp;
    sigset_t osigset, nsigset;

    pjobs++;
    sigemptyset(&nsigset);
    (void)sigaddset(&nsigset, SIGCHLD);
    (void)sigprocmask(SIG_BLOCK, &nsigset, &osigset);
loop:
    for (pp = proclist.p_next; pp; pp = pp->p_next)
	if (pp->p_pid &&	/* pp->p_pid == pp->p_jobid && */
	    pp->p_flags & PRUNNING) {
	    sigemptyset(&nsigset);
	    (void)sigsuspend(&nsigset);
	    goto loop;
	}
    (void)sigprocmask(SIG_SETMASK, &osigset, NULL);
    pjobs = 0;
}

/*
 * pflushall - flush all jobs from list (e.g. at fork())
 */
static void
pflushall(void)
{
    struct process *pp;

    for (pp = proclist.p_next; pp != NULL; pp = pp->p_next)
	if (pp->p_pid)
	    pflush(pp);
}

/*
 * pflush - flag all process structures in the same job as the
 *	the argument process for deletion.  The actual free of the
 *	space is not done here since pflush is called at interrupt level.
 */
static void
pflush(struct process *pp)
{
    struct process *np;
    int idx;

    if (pp->p_pid == 0) {
	(void)fprintf(csherr, "BUG: process flushed twice");
	return;
    }
    while (pp->p_pid != pp->p_jobid)
	pp = pp->p_friends;
    pclrcurr(pp);
    if (pp == pcurrjob)
	pcurrjob = 0;
    idx = pp->p_index;
    np = pp;
    do {
	np->p_index = np->p_pid = 0;
	np->p_flags &= ~PNEEDNOTE;
    } while ((np = np->p_friends) != pp);
    if (idx == pmaxindex) {
	for (np = proclist.p_next, idx = 0; np; np = np->p_next)
	    if (np->p_index > idx)
		idx = np->p_index;
	pmaxindex = idx;
    }
}

/*
 * pclrcurr - make sure the given job is not the current or previous job;
 *	pp MUST be the job leader
 */
static void
pclrcurr(struct process *pp)
{
    if (pp == pcurrent) {
	if (pprevious != NULL) {
	    pcurrent = pprevious;
	    pprevious = pgetcurr(pp);
	}
	else {
	    pcurrent = pgetcurr(pp);
	    pprevious = pgetcurr(pp);
	}
    } else if (pp == pprevious)
	pprevious = pgetcurr(pp);
}

/* +4 here is 1 for '\0', 1 ea for << >& >> */
static Char command[PMAXLEN + 4];
static size_t cmdlen;
static Char *cmdp;

/*
 * palloc - allocate a process structure and fill it up.
 *	an important assumption is made that the process is running.
 */
void
palloc(int pid, struct command *t)
{
    struct process *pp;
    int i;

    pp = (struct process *)xcalloc(1, (size_t)sizeof(struct process));
    pp->p_pid = pid;
    pp->p_flags = t->t_dflg & F_AMPERSAND ? PRUNNING : PRUNNING | PFOREGND;
    if (t->t_dflg & F_TIME)
	pp->p_flags |= PPTIME;
    cmdp = command;
    cmdlen = 0;
    padd(t);
    *cmdp++ = 0;
    if (t->t_dflg & F_PIPEOUT) {
	pp->p_flags |= PPOU;
	if (t->t_dflg & F_STDERR)
	    pp->p_flags |= PERR;
    }
    pp->p_command = Strsave(command);
    if (pcurrjob) {
	struct process *fp;

	/* careful here with interrupt level */
	pp->p_cwd = 0;
	pp->p_index = pcurrjob->p_index;
	pp->p_friends = pcurrjob;
	pp->p_jobid = pcurrjob->p_pid;
	for (fp = pcurrjob; fp->p_friends != pcurrjob; fp = fp->p_friends)
	    continue;
	fp->p_friends = pp;
    }
    else {
	pcurrjob = pp;
	pp->p_jobid = pid;
	pp->p_friends = pp;
	pp->p_cwd = dcwd;
	dcwd->di_count++;
	if (pmaxindex < BIGINDEX)
	    pp->p_index = ++pmaxindex;
	else {
	    struct process *np;

	    for (i = 1;; i++) {
		for (np = proclist.p_next; np; np = np->p_next)
		    if (np->p_index == i)
			goto tryagain;
		pp->p_index = i;
		if (i > pmaxindex)
		    pmaxindex = i;
		break;
	tryagain:;
	    }
	}
	if (pcurrent == NULL)
	    pcurrent = pp;
	else if (pprevious == NULL)
	    pprevious = pp;
    }
    pp->p_next = proclist.p_next;
    proclist.p_next = pp;
    (void)clock_gettime(CLOCK_MONOTONIC, &pp->p_btime);
}

static void
padd(struct command *t)
{
    Char **argp;

    if (t == 0)
	return;
    switch (t->t_dtyp) {
    case NODE_PAREN:
	pads(STRLparensp);
	padd(t->t_dspr);
	pads(STRspRparen);
	break;
    case NODE_COMMAND:
	for (argp = t->t_dcom; *argp; argp++) {
	    pads(*argp);
	    if (argp[1])
		pads(STRspace);
	}
	break;
    case NODE_OR:
    case NODE_AND:
    case NODE_PIPE:
    case NODE_LIST:
	padd(t->t_dcar);
	switch (t->t_dtyp) {
	case NODE_OR:
	    pads(STRspor2sp);
	    break;
	case NODE_AND:
	    pads(STRspand2sp);
	    break;
	case NODE_PIPE:
	    pads(STRsporsp);
	    break;
	case NODE_LIST:
	    pads(STRsemisp);
	    break;
	}
	padd(t->t_dcdr);
	return;
    }
    if ((t->t_dflg & F_PIPEIN) == 0 && t->t_dlef) {
	pads((t->t_dflg & F_READ) ? STRspLarrow2sp : STRspLarrowsp);
	pads(t->t_dlef);
    }
    if ((t->t_dflg & F_PIPEOUT) == 0 && t->t_drit) {
	pads((t->t_dflg & F_APPEND) ? STRspRarrow2 : STRspRarrow);
	if (t->t_dflg & F_STDERR)
	    pads(STRand);
	pads(STRspace);
	pads(t->t_drit);
    }
}

static void
pads(Char *cp)
{
    size_t i;

    /*
     * Avoid the Quoted Space alias hack! Reported by:
     * sam@john-bigboote.ICS.UCI.EDU (Sam Horrocks)
     */
    if (cp[0] == STRQNULL[0])
	cp++;

    i = Strlen(cp);

    if (cmdlen >= PMAXLEN)
	return;
    if (cmdlen + i >= PMAXLEN) {
	(void)Strcpy(cmdp, STRsp3dots);
	cmdlen = PMAXLEN;
	cmdp += 4;
	return;
    }
    (void)Strcpy(cmdp, cp);
    cmdp += i;
    cmdlen += i;
}

/*
 * psavejob - temporarily save the current job on a one level stack
 *	so another job can be created.  Used for { } in exp6
 *	and `` in globbing.
 */
void
psavejob(void)
{
    pholdjob = pcurrjob;
    pcurrjob = NULL;
}

/*
 * prestjob - opposite of psavejob.  This may be missed if we are interrupted
 *	somewhere, but pendjob cleans up anyway.
 */
void
prestjob(void)
{
    pcurrjob = pholdjob;
    pholdjob = NULL;
}

/*
 * pendjob - indicate that a job (set of commands) has been completed
 *	or is about to begin.
 */
void
pendjob(void)
{
    struct process *pp, *tp;

    if (pcurrjob && (pcurrjob->p_flags & (PFOREGND | PSTOPPED)) == 0) {
	pp = pcurrjob;
	while (pp->p_pid != pp->p_jobid)
	    pp = pp->p_friends;
	(void)fprintf(cshout, "[%d]", pp->p_index);
	tp = pp;
	do {
	    (void)fprintf(cshout, " %ld", (long)pp->p_pid);
	    pp = pp->p_friends;
	} while (pp != tp);
	(void)fputc('\n', cshout);
    }
    pholdjob = pcurrjob = 0;
}

/*
 * pprint - print a job
 */
static int
pprint(struct process *pp, int flag)
{
    static struct rusage zru;
    struct process *tp;
    const char *format;
    int jobflags, pstatus, reason, status;
    int hadnl;

    hadnl = 1; /* did we just have a newline */
    (void)fpurge(cshout);

    while (pp->p_pid != pp->p_jobid)
	pp = pp->p_friends;
    if (pp == pp->p_friends && (pp->p_flags & PPTIME)) {
	pp->p_flags &= ~PPTIME;
	pp->p_flags |= PTIME;
    }
    tp = pp;
    status = reason = -1;
    jobflags = 0;
    do {
	jobflags |= pp->p_flags;
	pstatus = pp->p_flags & PALLSTATES;
	if (tp != pp && !hadnl && !(flag & FANCY) &&
	    ((pstatus == status && pp->p_reason == reason) ||
	     !(flag & REASON))) {
	    (void)fputc(' ', cshout);
	    hadnl = 0;
	}
	else {
	    if (tp != pp && !hadnl) {
		(void)fputc('\n', cshout);
		hadnl = 1;
	    }
	    if (flag & NUMBER) {
		if (pp == tp)
		    (void)fprintf(cshout, "[%d]%s %c ", pp->p_index,
			    pp->p_index < 10 ? " " : "",
			    pp == pcurrent ? '+' :
			    (pp == pprevious ? '-' : ' '));
		else
		    (void)fprintf(cshout, "       ");
		hadnl = 0;
	    }
	    if (flag & FANCY) {
		(void)fprintf(cshout, "%5ld ", (long)pp->p_pid);
		hadnl = 0;
	    }
	    if (flag & (REASON | AREASON)) {
		if (flag & NAME)
		    format = "%-23s";
		else
		    format = "%s";
		if (pstatus == status) {
		    if (pp->p_reason == reason) {
			(void)fprintf(cshout, format, "");
			hadnl = 0;
			goto prcomd;
		    }
		    else
			reason = pp->p_reason;
		} else {
		    status = pstatus;
		    reason = pp->p_reason;
		}
		switch (status) {
		case PRUNNING:
		    (void)fprintf(cshout, format, "Running ");
		    hadnl = 0;
		    break;
		case PINTERRUPTED:
		case PSTOPPED:
		case PSIGNALED:
                    /*
                     * tell what happened to the background job
                     * From: Michael Schroeder
                     * <mlschroe@immd4.informatik.uni-erlangen.de>
                     */
                    if ((flag & REASON)
                        || ((flag & AREASON)
                            && reason != SIGINT
                            && (reason != SIGPIPE
                                || (pp->p_flags & PPOU) == 0))) {
			(void)fprintf(cshout, format,
				       sys_siglist[(unsigned char)
						   pp->p_reason]);
			hadnl = 0;
		    }
		    break;
		case PNEXITED:
		case PAEXITED:
		    if (flag & REASON) {
			if (pp->p_reason)
			    (void)fprintf(cshout, "Exit %-18d", pp->p_reason);
			else
			    (void)fprintf(cshout, format, "Done");
			hadnl = 0;
		    }
		    break;
		default:
		    (void)fprintf(csherr, "BUG: status=%-9o", status);
		}
	    }
	}
prcomd:
	if (flag & NAME) {
	    (void)fprintf(cshout, "%s", vis_str(pp->p_command));
	    if (pp->p_flags & PPOU)
		(void)fprintf(cshout, " |");
	    if (pp->p_flags & PERR)
		(void)fputc('&', cshout);
	    hadnl = 0;
	}
	if (flag & (REASON | AREASON) && pp->p_flags & PDUMPED) {
	    (void)fprintf(cshout, " (core dumped)");
	    hadnl = 0;
	}
	if (tp == pp->p_friends) {
	    if (flag & AMPERSAND) {
		(void)fprintf(cshout, " &");
		hadnl = 0;
	    }
	    if (flag & JOBDIR &&
		!eq(tp->p_cwd->di_name, dcwd->di_name)) {
		(void)fprintf(cshout, " (wd: ");
		dtildepr(value(STRhome), tp->p_cwd->di_name);
		(void)fputc(')', cshout);
		hadnl = 0;
	    }
	}
	if (pp->p_flags & PPTIME && !(status & (PSTOPPED | PRUNNING))) {
	    if (!hadnl)
		(void)fprintf(cshout, "\n\t");
	    prusage(cshout, &zru, &pp->p_rusage, &pp->p_etime,
		    &pp->p_btime);
	    hadnl = 1;
	}
	if (tp == pp->p_friends) {
	    if (!hadnl) {
		(void)fputc('\n', cshout);
		hadnl = 1;
	    }
	    if (flag & SHELLDIR && !eq(tp->p_cwd->di_name, dcwd->di_name)) {
		(void)fprintf(cshout, "(wd now: ");
		dtildepr(value(STRhome), dcwd->di_name);
		(void)fprintf(cshout, ")\n");
		hadnl = 1;
	    }
	}
    } while ((pp = pp->p_friends) != tp);
    if (jobflags & PTIME && (jobflags & (PSTOPPED | PRUNNING)) == 0) {
	if (jobflags & NUMBER)
	    (void)fprintf(cshout, "       ");
	ptprint(tp);
	hadnl = 1;
    }
    (void)fflush(cshout);
    return (jobflags);
}

static void
ptprint(struct process *tp)
{
    static struct rusage zru;
    static struct timespec ztime;
    struct rusage ru;
    struct timespec tetime, diff;
    struct process *pp;

    pp = tp;
    ru = zru;
    tetime = ztime;
    do {
	ruadd(&ru, &pp->p_rusage);
	timespecsub(&pp->p_etime, &pp->p_btime, &diff);
	if (timespeccmp(&diff, &tetime, >))
	    tetime = diff;
    } while ((pp = pp->p_friends) != tp);
    prusage(cshout, &zru, &ru, &tetime, &ztime);
}

/*
 * dojobs - print all jobs
 */
void
/*ARGSUSED*/
dojobs(Char **v, struct command *t)
{
    struct process *pp;
    int flag, i;

    flag = NUMBER | NAME | REASON;
    if (chkstop)
	chkstop = 2;
    if (*++v) {
	if (v[1] || !eq(*v, STRml))
	    stderror(ERR_JOBS);
	flag |= FANCY | JOBDIR;
    }
    for (i = 1; i <= pmaxindex; i++)
	for (pp = proclist.p_next; pp; pp = pp->p_next)
	    if (pp->p_index == i && pp->p_pid == pp->p_jobid) {
		pp->p_flags &= ~PNEEDNOTE;
		if (!(pprint(pp, flag) & (PRUNNING | PSTOPPED)))
		    pflush(pp);
		break;
	    }
}

/*
 * dofg - builtin - put the job into the foreground
 */
void
/*ARGSUSED*/
dofg(Char **v, struct command *t)
{
    struct process *pp;

    okpcntl();
    ++v;
    do {
	pp = pfind(*v);
	pstart(pp, 1);
	pjwait(pp);
    } while (*v && *++v);
}

/*
 * %... - builtin - put the job into the foreground
 */
void
/*ARGSUSED*/
dofg1(Char **v, struct command *t)
{
    struct process *pp;

    okpcntl();
    pp = pfind(v[0]);
    pstart(pp, 1);
    pjwait(pp);
}

/*
 * dobg - builtin - put the job into the background
 */
void
/*ARGSUSED*/
dobg(Char **v, struct command *t)
{
    struct process *pp;

    okpcntl();
    ++v;
    do {
	pp = pfind(*v);
	pstart(pp, 0);
    } while (*v && *++v);
}

/*
 * %... & - builtin - put the job into the background
 */
void
/*ARGSUSED*/
dobg1(Char **v, struct command *t)
{
    struct process *pp;

    pp = pfind(v[0]);
    pstart(pp, 0);
}

/*
 * dostop - builtin - stop the job
 */
void
/*ARGSUSED*/
dostop(Char **v, struct command *t)
{
    pkill(++v, SIGSTOP);
}

/*
 * dokill - builtin - superset of kill (1)
 */
void
/*ARGSUSED*/
dokill(Char **v, struct command *t)
{
    Char *signame;
    char *name;
    long signum;
    char *ep;

    signum = SIGTERM;
    v++;
    if (v[0] && v[0][0] == '-') {
	if (v[0][1] == 'l') {
	    if (v[1]) {
		if (!Isdigit(v[1][0]))
		    stderror(ERR_NAME | ERR_BADSIG);

		signum = strtol(short2str(v[1]), &ep, 10);
		if (signum < 0 || signum >= NSIG)
		    stderror(ERR_NAME | ERR_BADSIG);
		else if (signum == 0)
		    (void)fputc('0', cshout); /* 0's symbolic name is '0' */
		else
		    (void)fprintf(cshout, "%s ", sys_signame[signum]);
	    } else {
		for (signum = 1; signum < NSIG; signum++) {
		    (void)fprintf(cshout, "%s ", sys_signame[signum]);
		    if (signum == NSIG / 2)
			(void)fputc('\n', cshout);
	    	}
	    }
	    (void)fputc('\n', cshout);
	    return;
	}
	if (Isdigit(v[0][1])) {
	    signum = strtol(short2str(v[0] + 1), &ep, 10);
	    if (signum < 0 || signum >= NSIG || *ep)
		stderror(ERR_NAME | ERR_BADSIG);
	}
	else {
	    if (v[0][1] == 's' && v[0][2] == '\0')
		signame = *(++v);
	    else
		signame = &v[0][1];

	    if (signame == NULL || v[1] == NULL)
		stderror(ERR_NAME | ERR_TOOFEW);

	    name = short2str(signame);
	    for (signum = 1; signum < NSIG; signum++)
		if (!strcasecmp(sys_signame[signum], name) ||
		    (!strncasecmp("SIG", name, 3) &&	/* skip "SIG" prefix */
		     !strcasecmp(sys_signame[signum], name + 3)))
		    break;

	    if (signum == NSIG) {
		if (signame[0] == '0')
		    signum = 0;
		else {
		    setname(vis_str(signame));
		    stderror(ERR_NAME | ERR_UNKSIG);
		}
	    }
	}
	v++;
    }
    pkill(v, (int)signum);
}

static void
pkill(Char **v, int signum)
{
    struct process *pp, *np;
    Char *cp;
    sigset_t nsigset;
    int err1, jobflags, pid;
    char *ep;

    jobflags = 0;
    err1 = 0;    
    sigemptyset(&nsigset);
    (void)sigaddset(&nsigset, SIGCHLD);
    if (setintr)
	(void)sigaddset(&nsigset, SIGINT);
    (void)sigprocmask(SIG_BLOCK, &nsigset, NULL);
    gflag = 0, tglob(v);
    if (gflag) {
	v = globall(v);
	if (v == 0)
	    stderror(ERR_NAME | ERR_NOMATCH);
    }
    else {
	v = gargv = saveblk(v);
	trim(v);
    }

    while (v && (cp = *v)) {
	if (*cp == '%') {
	    np = pp = pfind(cp);
	    do
		jobflags |= np->p_flags;
	    while ((np = np->p_friends) != pp);
	    switch (signum) {
	    case SIGSTOP:
	    case SIGTSTP:
	    case SIGTTIN:
	    case SIGTTOU:
		if ((jobflags & PRUNNING) == 0) {
		    (void)fprintf(csherr, "%s: Already suspended\n",
				   vis_str(cp));
		    err1++;
		    goto cont;
		}
		break;
		/*
		 * suspend a process, kill -CONT %, then type jobs; the shell
		 * says it is suspended, but it is running; thanks jaap..
		 */
	    case SIGCONT:
		pstart(pp, 0);
		goto cont;
	    }
	    if (kill(-pp->p_jobid, signum) < 0) {
		(void)fprintf(csherr, "%s: %s\n", vis_str(cp),
			       strerror(errno));
		err1++;
	    }
	    if (signum == SIGTERM || signum == SIGHUP)
		(void)kill(-pp->p_jobid, SIGCONT);
	}
	else if (!(Isdigit(*cp) || *cp == '-'))
	    stderror(ERR_NAME | ERR_JOBARGS);
	else {
	    pid = (pid_t)strtoul(short2str(cp), &ep, 0);
	    if (*ep) {
		(void)fprintf(csherr, "%s: Badly formed number\n",
		    short2str(cp));
		err1++;
		goto cont;
	    } else if (kill(pid, signum) < 0) {
		(void)fprintf(csherr, "%d: %s\n", pid, strerror(errno));
		err1++;
		goto cont;
	    }
	    if (signum == SIGTERM || signum == SIGHUP)
		(void)kill((pid_t) pid, SIGCONT);
	}
cont:
	v++;
    }
    if (gargv)
	blkfree(gargv), gargv = 0;
    (void)sigprocmask(SIG_UNBLOCK, &nsigset, NULL);
    if (err1)
	stderror(ERR_SILENT);
}

/*
 * pstart - start the job in foreground/background
 */
void
pstart(struct process *pp, int foregnd)
{
    struct process *np;
    sigset_t osigset, nsigset;
    long jobflags;

    jobflags = 0;
    sigemptyset(&nsigset);
    (void)sigaddset(&nsigset, SIGCHLD);
    (void)sigprocmask(SIG_BLOCK, &nsigset, &osigset);
    np = pp;
    do {
	jobflags |= np->p_flags;
	if (np->p_flags & (PRUNNING | PSTOPPED)) {
	    np->p_flags |= PRUNNING;
	    np->p_flags &= ~PSTOPPED;
	    if (foregnd)
		np->p_flags |= PFOREGND;
	    else
		np->p_flags &= ~PFOREGND;
	}
    } while ((np = np->p_friends) != pp);
    if (!foregnd)
	pclrcurr(pp);
    (void)pprint(pp, foregnd ? NAME | JOBDIR : NUMBER | NAME | AMPERSAND);
    if (foregnd)
	(void)tcsetpgrp(FSHTTY, pp->p_jobid);
    if (jobflags & PSTOPPED)
	(void)kill(-pp->p_jobid, SIGCONT);
    (void)sigprocmask(SIG_SETMASK, &osigset, NULL);
}

void
panystop(int neednl)
{
    struct process *pp;

    chkstop = 2;
    for (pp = proclist.p_next; pp; pp = pp->p_next)
	if (pp->p_flags & PSTOPPED)
	    stderror(ERR_STOPPED, neednl ? "\n" : "");
}

struct process *
pfind(Char *cp)
{
    struct process *pp, *np;

    if (cp == 0 || cp[1] == 0 || eq(cp, STRcent2) || eq(cp, STRcentplus)) {
	if (pcurrent == NULL)
	    stderror(ERR_NAME | ERR_JOBCUR);
	return (pcurrent);
    }
    if (eq(cp, STRcentminus) || eq(cp, STRcenthash)) {
	if (pprevious == NULL)
	    stderror(ERR_NAME | ERR_JOBPREV);
	return (pprevious);
    }
    if (Isdigit(cp[1])) {
	int     idx = atoi(short2str(cp + 1));

	for (pp = proclist.p_next; pp; pp = pp->p_next)
	    if (pp->p_index == idx && pp->p_pid == pp->p_jobid)
		return (pp);
	stderror(ERR_NAME | ERR_NOSUCHJOB);
    }
    np = NULL;
    for (pp = proclist.p_next; pp; pp = pp->p_next)
	if (pp->p_pid == pp->p_jobid) {
	    if (cp[1] == '?') {
		Char *dp;

		for (dp = pp->p_command; *dp; dp++) {
		    if (*dp != cp[2])
			continue;
		    if (prefix(cp + 2, dp))
			goto match;
		}
	    }
	    else if (prefix(cp + 1, pp->p_command)) {
	match:
		if (np)
		    stderror(ERR_NAME | ERR_AMBIG);
		np = pp;
	    }
	}
    if (np)
	return (np);
    stderror(ERR_NAME | (cp[1] == '?' ? ERR_JOBPAT : ERR_NOSUCHJOB));
    /* NOTREACHED */
}

/*
 * pgetcurr - find most recent job that is not pp, preferably stopped
 */
static struct process *
pgetcurr(struct process *pp)
{
    struct process *np, *xp;

    xp = NULL;
    for (np = proclist.p_next; np; np = np->p_next)
	if (np != pcurrent && np != pp && np->p_pid &&
	    np->p_pid == np->p_jobid) {
	    if (np->p_flags & PSTOPPED)
		return (np);
	    if (xp == NULL)
		xp = np;
	}
    return (xp);
}

/*
 * donotify - flag the job so as to report termination asynchronously
 */
void
/*ARGSUSED*/
donotify(Char **v, struct command *t)
{
    struct process *pp;

    pp = pfind(*++v);
    pp->p_flags |= PNOTIFY;
}

/*
 * Do the fork and whatever should be done in the child side that
 * should not be done if we are not forking at all (like for simple builtin's)
 * Also do everything that needs any signals fiddled with in the parent side
 *
 * Wanttty tells whether process and/or tty pgrps are to be manipulated:
 *	-1:	leave tty alone; inherit pgrp from parent
 *	 0:	already have tty; manipulate process pgrps only
 *	 1:	want to claim tty; manipulate process and tty pgrps
 * It is usually just the value of tpgrp.
 */

int
pfork(struct command *t /* command we are forking for */, int wanttty)
{
    int pgrp, pid;
    sigset_t osigset, nsigset;
    int ignint;

    ignint = 0;
    /*
     * A child will be uninterruptible only under very special conditions.
     * Remember that the semantics of '&' is implemented by disconnecting the
     * process from the tty so signals do not need to ignored just for '&'.
     * Thus signals are set to default action for children unless: we have had
     * an "onintr -" (then specifically ignored) we are not playing with
     * signals (inherit action)
     */
    if (setintr)
	ignint = (tpgrp == -1 && (t->t_dflg & F_NOINTERRUPT))
	    || (gointr && eq(gointr, STRminus));
    /*
     * Check for maximum nesting of 16 processes to avoid Forking loops
     */
    if (child == 16)
	stderror(ERR_NESTING, 16);
    /*
     * Hold SIGCHLD until we have the process installed in our table.
     */
    sigemptyset(&nsigset);
    (void)sigaddset(&nsigset, SIGCHLD);
    (void)sigprocmask(SIG_BLOCK, &nsigset, &osigset);
    while ((pid = fork()) < 0)
	if (setintr == 0)
	    (void)sleep(FORKSLEEP);
	else {
	    (void)sigprocmask(SIG_SETMASK, &osigset, NULL);
	    stderror(ERR_NOPROC);
	}
    if (pid == 0) {
	settimes();
	pgrp = pcurrjob ? pcurrjob->p_jobid : getpid();
	pflushall();
	pcurrjob = NULL;
	child++;
	if (setintr) {
	    setintr = 0;	/* until I think otherwise */
	    /*
	     * Children just get blown away on SIGINT, SIGQUIT unless "onintr
	     * -" seen.
	     */
	    (void)signal(SIGINT, ignint ? SIG_IGN : SIG_DFL);
	    (void)signal(SIGQUIT, ignint ? SIG_IGN : SIG_DFL);
	    if (wanttty >= 0) {
		/* make stoppable */
		(void)signal(SIGTSTP, SIG_DFL);
		(void)signal(SIGTTIN, SIG_DFL);
		(void)signal(SIGTTOU, SIG_DFL);
	    }
	    (void)signal(SIGTERM, parterm);
	}
	else if (tpgrp == -1 && (t->t_dflg & F_NOINTERRUPT)) {
	    (void)signal(SIGINT, SIG_IGN);
	    (void)signal(SIGQUIT, SIG_IGN);
	}
	pgetty(wanttty, pgrp);
	/*
	 * Nohup and nice apply only to NODE_COMMAND's but it would be nice
	 * (?!?) if you could say "nohup (foo;bar)" Then the parser would have
	 * to know about nice/nohup/time
	 */
	if (t->t_dflg & F_NOHUP)
	    (void)signal(SIGHUP, SIG_IGN);
	if (t->t_dflg & F_NICE)
	    (void)setpriority(PRIO_PROCESS, 0, t->t_nice);
    }
    else {
	if (wanttty >= 0)
	    (void)setpgid(pid, pcurrjob ? pcurrjob->p_jobid : pid);
	palloc(pid, t);
	(void)sigprocmask(SIG_SETMASK, &osigset, NULL);
    }

    return (pid);
}

static void
okpcntl(void)
{
    if (tpgrp == -1)
	stderror(ERR_JOBCONTROL);
    if (tpgrp == 0)
	stderror(ERR_JOBCTRLSUB);
    /* NOTREACHED */
}

/*
 * if we don't have vfork(), things can still go in the wrong order
 * resulting in the famous 'Stopped (tty output)'. But some systems
 * don't permit the setpgid() call, (these are more recent secure
 * systems such as ibm's aix). Then we'd rather print an error message
 * than hang the shell!
 * I am open to suggestions how to fix that.
 */
void
pgetty(int wanttty, int pgrp)
{
    sigset_t osigset, nsigset;

    /*
     * christos: I am blocking the tty signals till I've set things
     * correctly....
     */
    if (wanttty > 0) {
	sigemptyset(&nsigset);
	(void)sigaddset(&nsigset, SIGTSTP);
	(void)sigaddset(&nsigset, SIGTTIN);
	(void)sigaddset(&nsigset, SIGTTOU);
	(void)sigprocmask(SIG_BLOCK, &nsigset, &osigset);
    }
    /*
     * From: Michael Schroeder <mlschroe@immd4.informatik.uni-erlangen.de>
     * Don't check for tpgrp >= 0 so even non-interactive shells give
     * background jobs process groups Same for the comparison in the other part
     * of the #ifdef
     */
    if (wanttty >= 0)
	if (setpgid(0, pgrp) == -1) {
	    (void)fprintf(csherr, "csh: setpgid error.\n");
	    xexit(0);
	}

    if (wanttty > 0) {
	(void)tcsetpgrp(FSHTTY, pgrp);
	(void)sigprocmask(SIG_SETMASK, &osigset, NULL);
    }

    if (tpgrp > 0)
	tpgrp = 0;		/* gave tty away */
}
