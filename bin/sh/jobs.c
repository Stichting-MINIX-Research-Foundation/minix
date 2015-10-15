/*	$NetBSD: jobs.c,v 1.75 2015/08/22 12:12:47 christos Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
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
static char sccsid[] = "@(#)jobs.c	8.5 (Berkeley) 5/4/95";
#else
__RCSID("$NetBSD: jobs.c,v 1.75 2015/08/22 12:12:47 christos Exp $");
#endif
#endif /* not lint */

#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <paths.h>
#include <sys/types.h>
#include <sys/param.h>
#ifdef BSD
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif
#include <sys/ioctl.h>

#include "shell.h"
#if JOBS
#if OLD_TTY_DRIVER
#include "sgtty.h"
#else
#include <termios.h>
#endif
#undef CEOF			/* syntax.h redefines this */
#endif
#include "redir.h"
#include "show.h"
#include "main.h"
#include "parser.h"
#include "nodes.h"
#include "jobs.h"
#include "options.h"
#include "builtins.h"
#include "trap.h"
#include "syntax.h"
#include "input.h"
#include "output.h"
#include "memalloc.h"
#include "error.h"
#include "mystring.h"


static struct job *jobtab;		/* array of jobs */
static int njobs;			/* size of array */
static int jobs_invalid;		/* set in child */
MKINIT pid_t backgndpid = -1;	/* pid of last background process */
#if JOBS
int initialpgrp;		/* pgrp of shell on invocation */
static int curjob = -1;		/* current job */
#endif
#if JOBS && defined(__minix)
static int ttyfd = -1;
#endif /* JOBS && defined(__minix) */

STATIC void restartjob(struct job *);
STATIC void freejob(struct job *);
STATIC struct job *getjob(const char *, int);
STATIC int dowait(int, struct job *);
#define WBLOCK	1
#define WNOFREE 2
STATIC int waitproc(int, struct job *, int *);
STATIC void cmdtxt(union node *);
STATIC void cmdlist(union node *, int);
STATIC void cmdputs(const char *);

#ifdef SYSV
STATIC int onsigchild(void);
#endif

#ifdef OLD_TTY_DRIVER
static pid_t tcgetpgrp(int fd);
static int tcsetpgrp(int fd, pid_t pgrp);

static pid_t
tcgetpgrp(int fd)
{
	pid_t pgrp;
	if (ioctl(fd, TIOCGPGRP, (char *)&pgrp) == -1)
		return -1;
	else
		return pgrp;
}

static int
tcsetpgrp(int fd, pid_tpgrp)
{
	return ioctl(fd, TIOCSPGRP, (char *)&pgrp);
}
#endif

/*
 * Turn job control on and off.
 *
 * Note:  This code assumes that the third arg to ioctl is a character
 * pointer, which is true on Berkeley systems but not System V.  Since
 * System V doesn't have job control yet, this isn't a problem now.
 */

MKINIT int jobctl;

#if JOBS /* Minix setjobctl is defined to empty, compilation error */
void
setjobctl(int on)
{
#ifdef OLD_TTY_DRIVER
	int ldisc;
#endif

	if (on == jobctl || rootshell == 0)
		return;
	if (on) {
#if defined(FIOCLEX) || defined(FD_CLOEXEC)
		int err;
		int i;
		if (ttyfd != -1)
			close(ttyfd);
		if ((ttyfd = open("/dev/tty", O_RDWR)) == -1) {
			for (i = 0; i < 3; i++) {
				if (isatty(i) && (ttyfd = dup(i)) != -1)
					break;
			}
			if (i == 3)
				goto out;
		}
		/* Move to a high fd */
		for (i = 10; i > 2; i--) {
			if ((err = fcntl(ttyfd, F_DUPFD, (1 << i) - 1)) != -1)
				break;
		}
		if (err != -1) {
			close(ttyfd);
			ttyfd = err;
		}
#ifdef FIOCLEX
		err = ioctl(ttyfd, FIOCLEX, 0);
#elif FD_CLOEXEC
		err = fcntl(ttyfd, F_SETFD,
		    fcntl(ttyfd, F_GETFD, 0) | FD_CLOEXEC);
#endif
		if (err == -1) {
			close(ttyfd);
			ttyfd = -1;
			goto out;
		}
#else
		out2str("sh: Need FIOCLEX or FD_CLOEXEC to support job control");
		goto out;
#endif
		do { /* while we are in the background */
			if ((initialpgrp = tcgetpgrp(ttyfd)) < 0) {
out:
				out2str("sh: can't access tty; job control turned off\n");
				mflag = 0;
				return;
			}
			if (initialpgrp == -1)
				initialpgrp = getpgrp();
			else if (initialpgrp != getpgrp()) {
				killpg(0, SIGTTIN);
				continue;
			}
		} while (0);

#ifdef OLD_TTY_DRIVER
		if (ioctl(ttyfd, TIOCGETD, (char *)&ldisc) < 0
		    || ldisc != NTTYDISC) {
			out2str("sh: need new tty driver to run job control; job control turned off\n");
			mflag = 0;
			return;
		}
#endif
		setsignal(SIGTSTP, 0);
		setsignal(SIGTTOU, 0);
		setsignal(SIGTTIN, 0);
		if (getpgrp() != rootpid && setpgid(0, rootpid) == -1)
			error("Cannot set process group (%s) at %d",
			    strerror(errno), __LINE__);
		if (tcsetpgrp(ttyfd, rootpid) == -1)
			error("Cannot set tty process group (%s) at %d",
			    strerror(errno), __LINE__);
	} else { /* turning job control off */
		if (getpgrp() != initialpgrp && setpgid(0, initialpgrp) == -1)
			error("Cannot set process group (%s) at %d",
			    strerror(errno), __LINE__);
		if (tcsetpgrp(ttyfd, initialpgrp) == -1)
			error("Cannot set tty process group (%s) at %d",
			    strerror(errno), __LINE__);
		close(ttyfd);
		ttyfd = -1;
		setsignal(SIGTSTP, 0);
		setsignal(SIGTTOU, 0);
		setsignal(SIGTTIN, 0);
	}
	jobctl = on;
}
#endif /* JOBS */


#ifdef mkinit
INCLUDE <stdlib.h>

SHELLPROC {
	backgndpid = -1;
#if JOBS
	jobctl = 0;
#endif
}

#endif



#if JOBS
static int
do_fgcmd(const char *arg_ptr)
{
	struct job *jp;
	int i;
	int status;

	jp = getjob(arg_ptr, 0);
	if (jp->jobctl == 0)
		error("job not created under job control");
	out1fmt("%s", jp->ps[0].cmd);
	for (i = 1; i < jp->nprocs; i++)
		out1fmt(" | %s", jp->ps[i].cmd );
	out1c('\n');
	flushall();

	for (i = 0; i < jp->nprocs; i++)
	    if (tcsetpgrp(ttyfd, jp->ps[i].pid) != -1)
		    break;

	if (i >= jp->nprocs) {
		error("Cannot set tty process group (%s) at %d",
		    strerror(errno), __LINE__);
	}
	restartjob(jp);
	INTOFF;
	status = waitforjob(jp);
	INTON;
	return status;
}

int
fgcmd(int argc, char **argv)
{
	nextopt("");
	return do_fgcmd(*argptr);
}

int
fgcmd_percent(int argc, char **argv)
{
	nextopt("");
	return do_fgcmd(*argv);
}

static void
set_curjob(struct job *jp, int mode)
{
	struct job *jp1, *jp2;
	int i, ji;

	ji = jp - jobtab;

	/* first remove from list */
	if (ji == curjob)
		curjob = jp->prev_job;
	else {
		for (i = 0; i < njobs; i++) {
			if (jobtab[i].prev_job != ji)
				continue;
			jobtab[i].prev_job = jp->prev_job;
			break;
		}
	}

	/* Then re-insert in correct position */
	switch (mode) {
	case 0:	/* job being deleted */
		jp->prev_job = -1;
		break;
	case 1:	/* newly created job or backgrounded job,
		   put after all stopped jobs. */
		if (curjob != -1 && jobtab[curjob].state == JOBSTOPPED) {
			for (jp1 = jobtab + curjob; ; jp1 = jp2) {
				if (jp1->prev_job == -1)
					break;
				jp2 = jobtab + jp1->prev_job;
				if (jp2->state != JOBSTOPPED)
					break;
			}
			jp->prev_job = jp1->prev_job;
			jp1->prev_job = ji;
			break;
		}
		/* FALLTHROUGH */
	case 2:	/* newly stopped job - becomes curjob */
		jp->prev_job = curjob;
		curjob = ji;
		break;
	}
}

int
bgcmd(int argc, char **argv)
{
	struct job *jp;
	int i;

	nextopt("");
	do {
		jp = getjob(*argptr, 0);
		if (jp->jobctl == 0)
			error("job not created under job control");
		set_curjob(jp, 1);
		out1fmt("[%ld] %s", (long)(jp - jobtab + 1), jp->ps[0].cmd);
		for (i = 1; i < jp->nprocs; i++)
			out1fmt(" | %s", jp->ps[i].cmd );
		out1c('\n');
		flushall();
		restartjob(jp);
	} while (*argptr && *++argptr);
	return 0;
}


STATIC void
restartjob(struct job *jp)
{
	struct procstat *ps;
	int i;

	if (jp->state == JOBDONE)
		return;
	INTOFF;
	for (i = 0; i < jp->nprocs; i++)
		if (killpg(jp->ps[i].pid, SIGCONT) != -1)
			break;
	if (i >= jp->nprocs)
		error("Cannot continue job (%s)", strerror(errno));
	for (ps = jp->ps, i = jp->nprocs ; --i >= 0 ; ps++) {
		if (WIFSTOPPED(ps->status)) {
			ps->status = -1;
			jp->state = JOBRUNNING;
		}
	}
	INTON;
}
#else
/* LSC: For Minix add dummy functions. */
int
fgcmd(int argc, char **argv)
{
	error("no job control in this shell.");
	return 1;
}

int
fgcmd_percent(int argc, char **argv)
{
	error("no job control in this shell.");
	return 1;
}


int
bgcmd(int argc, char **argv)
{
	error("no job control in this shell.");
	return 1;
}
#endif

static void
showjob(struct output *out, struct job *jp, int mode)
{
	int procno;
	int st;
	struct procstat *ps;
	int col;
	char s[64];

#if JOBS
	if (mode & SHOW_PGID) {
		/* just output process (group) id of pipeline */
		outfmt(out, "%ld\n", (long)jp->ps->pid);
		return;
	}
#endif

	procno = jp->nprocs;
	if (!procno)
		return;

	if (mode & SHOW_PID)
		mode |= SHOW_MULTILINE;

	if ((procno > 1 && !(mode & SHOW_MULTILINE))
	    || (mode & SHOW_SIGNALLED)) {
		/* See if we have more than one status to report */
		ps = jp->ps;
		st = ps->status;
		do {
			int st1 = ps->status;
			if (st1 != st)
				/* yes - need multi-line output */
				mode |= SHOW_MULTILINE;
			if (st1 == -1 || !(mode & SHOW_SIGNALLED) || WIFEXITED(st1))
				continue;
			if (WIFSTOPPED(st1) || ((st1 = WTERMSIG(st1) & 0x7f)
			    && st1 != SIGINT && st1 != SIGPIPE))
				mode |= SHOW_ISSIG;

		} while (ps++, --procno);
		procno = jp->nprocs;
	}

	if (mode & SHOW_SIGNALLED && !(mode & SHOW_ISSIG)) {
		if (jp->state == JOBDONE && !(mode & SHOW_NO_FREE)) {
			TRACE(("showjob: freeing job %d\n", jp - jobtab + 1));
			freejob(jp);
		}
		return;
	}

	for (ps = jp->ps; --procno >= 0; ps++) {	/* for each process */
		if (ps == jp->ps)
			fmtstr(s, 16, "[%ld] %c ",
				(long)(jp - jobtab + 1),
#if JOBS
				jp == jobtab + curjob ? '+' :
				curjob != -1 && jp == jobtab +
					    jobtab[curjob].prev_job ? '-' :
#endif
				' ');
		else
			fmtstr(s, 16, "      " );
		col = strlen(s);
		if (mode & SHOW_PID) {
			fmtstr(s + col, 16, "%ld ", (long)ps->pid);
			     col += strlen(s + col);
		}
		if (ps->status == -1) {
			scopy("Running", s + col);
		} else if (WIFEXITED(ps->status)) {
			st = WEXITSTATUS(ps->status);
			if (st)
				fmtstr(s + col, 16, "Done(%d)", st);
			else
				fmtstr(s + col, 16, "Done");
		} else {
#if JOBS
			if (WIFSTOPPED(ps->status)) 
				st = WSTOPSIG(ps->status);
			else /* WIFSIGNALED(ps->status) */
#endif
				st = WTERMSIG(ps->status);
			st &= 0x7f;
			if (st < NSIG && sys_siglist[st])
				scopyn(sys_siglist[st], s + col, 32);
			else
				fmtstr(s + col, 16, "Signal %d", st);
			if (WCOREDUMP(ps->status)) {
				col += strlen(s + col);
				scopyn(" (core dumped)", s + col,  64 - col);
			}
		}
		col += strlen(s + col);
		outstr(s, out);
		do {
			outc(' ', out);
			col++;
		} while (col < 30);
		outstr(ps->cmd, out);
		if (mode & SHOW_MULTILINE) {
			if (procno > 0) {
				outc(' ', out);
				outc('|', out);
			}
		} else {
			while (--procno >= 0)
				outfmt(out, " | %s", (++ps)->cmd );
		}
		outc('\n', out);
	}
	flushout(out);
	jp->changed = 0;
	if (jp->state == JOBDONE && !(mode & SHOW_NO_FREE))
		freejob(jp);
}


int
jobscmd(int argc, char **argv)
{
	int mode, m;
	int sv = jobs_invalid;

	jobs_invalid = 0;
	mode = 0;
	while ((m = nextopt("lp")))
		if (m == 'l')
			mode = SHOW_PID;
		else
			mode = SHOW_PGID;
	if (*argptr)
		do
			showjob(out1, getjob(*argptr,0), mode);
		while (*++argptr);
	else
		showjobs(out1, mode);
	jobs_invalid = sv;
	return 0;
}


/*
 * Print a list of jobs.  If "change" is nonzero, only print jobs whose
 * statuses have changed since the last call to showjobs.
 *
 * If the shell is interrupted in the process of creating a job, the
 * result may be a job structure containing zero processes.  Such structures
 * will be freed here.
 */

void
showjobs(struct output *out, int mode)
{
	int jobno;
	struct job *jp;
#if JOBS /* MINIX: gotpid. */
	int silent = 0, gotpid;
#else
	int silent = 0;
#endif

	TRACE(("showjobs(%x) called\n", mode));

	/* If not even one one job changed, there is nothing to do */
#if JOBS /* MINIX: gotpid. */
	gotpid = dowait(0, NULL);
#endif
	while (dowait(0, NULL) > 0)
		continue;
#if JOBS /* MINIX: #ifdef fails when JOBS = 0 */
	/*
	 * Check if we are not in our foreground group, and if not
	 * put us in it.
	 */
	if (mflag && gotpid != -1 && tcgetpgrp(ttyfd) != getpid()) {
		if (tcsetpgrp(ttyfd, getpid()) == -1)
			error("Cannot set tty process group (%s) at %d",
			    strerror(errno), __LINE__);
		TRACE(("repaired tty process group\n"));
		silent = 1;
	}
#endif
	if (jobs_invalid)
		return;

	for (jobno = 1, jp = jobtab ; jobno <= njobs ; jobno++, jp++) {
		if (!jp->used)
			continue;
		if (jp->nprocs == 0) {
			freejob(jp);
			continue;
		}
		if ((mode & SHOW_CHANGED) && !jp->changed)
			continue;
		if (silent && jp->changed) {
			jp->changed = 0;
			continue;
		}
		showjob(out, jp, mode);
	}
}

/*
 * Mark a job structure as unused.
 */

STATIC void
freejob(struct job *jp)
{
	INTOFF;
	if (jp->ps != &jp->ps0) {
		ckfree(jp->ps);
		jp->ps = &jp->ps0;
	}
	jp->nprocs = 0;
	jp->used = 0;
#if JOBS
	set_curjob(jp, 0);
#endif
	INTON;
}



int
waitcmd(int argc, char **argv)
{
	struct job *job;
	int status, retval;
	struct job *jp;

	nextopt("");

	if (!*argptr) {
		/* wait for all jobs */
		jp = jobtab;
		if (jobs_invalid)
			return 0;
		for (;;) {
			if (jp >= jobtab + njobs) {
				/* no running procs */
				return 0;
			}
			if (!jp->used || jp->state != JOBRUNNING) {
				jp++;
				continue;
			}
			if (dowait(WBLOCK, NULL) == -1)
			       return 128 + lastsig();
			jp = jobtab;
		}
	}

	retval = 127;		/* XXXGCC: -Wuninitialized */
	for (; *argptr; argptr++) {
		job = getjob(*argptr, 1);
		if (!job) {
			retval = 127;
			continue;
		}
		/* loop until process terminated or stopped */
		while (job->state == JOBRUNNING) {
			if (dowait(WBLOCK|WNOFREE, job) == -1)
			       return 128 + lastsig();
		}
		status = job->ps[job->nprocs ? job->nprocs - 1 : 0].status;
		if (WIFEXITED(status))
			retval = WEXITSTATUS(status);
#if JOBS
		else if (WIFSTOPPED(status))
			retval = WSTOPSIG(status) + 128;
#endif
		else {
			/* XXX: limits number of signals */
			retval = WTERMSIG(status) + 128;
		}
		if (!iflag)
			freejob(job);
	}
	return retval;
}



int
jobidcmd(int argc, char **argv)
{
	struct job *jp;
	int i;

	nextopt("");
	jp = getjob(*argptr, 0);
	for (i = 0 ; i < jp->nprocs ; ) {
		out1fmt("%ld", (long)jp->ps[i].pid);
		out1c(++i < jp->nprocs ? ' ' : '\n');
	}
	return 0;
}

int
getjobpgrp(const char *name)
{
	struct job *jp;

	jp = getjob(name, 1);
	if (jp == 0)
		return 0;
	return -jp->ps[0].pid;
}

/*
 * Convert a job name to a job structure.
 */

STATIC struct job *
getjob(const char *name, int noerror)
{
	int jobno = -1;
	struct job *jp;
	int pid;
	int i;
	const char *err_msg = "No such job: %s";
		
	if (name == NULL) {
#if JOBS
		jobno = curjob;
#endif
		err_msg = "No current job";
	} else if (name[0] == '%') {
		if (is_number(name + 1)) {
			jobno = number(name + 1) - 1;
		} else if (!name[2]) {
			switch (name[1]) {
#if JOBS
			case 0:
			case '+':
			case '%':
				jobno = curjob;
				err_msg = "No current job";
				break;
			case '-':
				jobno = curjob;
				if (jobno != -1)
					jobno = jobtab[jobno].prev_job;
				err_msg = "No previous job";
				break;
#endif
			default:
				goto check_pattern;
			}
		} else {
			struct job *found;
    check_pattern:
			found = NULL;
			for (jp = jobtab, i = njobs ; --i >= 0 ; jp++) {
				if (!jp->used || jp->nprocs <= 0)
					continue;
				if ((name[1] == '?'
					&& strstr(jp->ps[0].cmd, name + 2))
				    || prefix(name + 1, jp->ps[0].cmd)) {
					if (found) {
						err_msg = "%s: ambiguous";
						found = 0;
						break;
					}
					found = jp;
				}
			}
			if (found)
				return found;
		}

	} else if (is_number(name)) {
		pid = number(name);
		for (jp = jobtab, i = njobs ; --i >= 0 ; jp++) {
			if (jp->used && jp->nprocs > 0
			 && jp->ps[jp->nprocs - 1].pid == pid)
				return jp;
		}
	}

	if (!jobs_invalid && jobno >= 0 && jobno < njobs) {
		jp = jobtab + jobno;
		if (jp->used)
			return jp;
	}
	if (!noerror)
		error(err_msg, name);
	return 0;
}



/*
 * Return a new job structure,
 */

struct job *
makejob(union node *node, int nprocs)
{
	int i;
	struct job *jp;

	if (jobs_invalid) {
		for (i = njobs, jp = jobtab ; --i >= 0 ; jp++) {
			if (jp->used)
				freejob(jp);
		}
		jobs_invalid = 0;
	}

	for (i = njobs, jp = jobtab ; ; jp++) {
		if (--i < 0) {
			INTOFF;
			if (njobs == 0) {
				jobtab = ckmalloc(4 * sizeof jobtab[0]);
			} else {
				jp = ckmalloc((njobs + 4) * sizeof jobtab[0]);
				memcpy(jp, jobtab, njobs * sizeof jp[0]);
				/* Relocate `ps' pointers */
				for (i = 0; i < njobs; i++)
					if (jp[i].ps == &jobtab[i].ps0)
						jp[i].ps = &jp[i].ps0;
				ckfree(jobtab);
				jobtab = jp;
			}
			jp = jobtab + njobs;
			for (i = 4 ; --i >= 0 ; )
				jobtab[njobs++].used = 0;
			INTON;
			break;
		}
		if (jp->used == 0)
			break;
	}
	INTOFF;
	jp->state = JOBRUNNING;
	jp->used = 1;
	jp->changed = 0;
	jp->nprocs = 0;
#if JOBS
	jp->jobctl = jobctl;
	set_curjob(jp, 1);
#endif
	if (nprocs > 1) {
		jp->ps = ckmalloc(nprocs * sizeof (struct procstat));
	} else {
		jp->ps = &jp->ps0;
	}
	INTON;
	TRACE(("makejob(0x%lx, %d) returns %%%d\n", (long)node, nprocs,
	    jp - jobtab + 1));
	return jp;
}


/*
 * Fork off a subshell.  If we are doing job control, give the subshell its
 * own process group.  Jp is a job structure that the job is to be added to.
 * N is the command that will be evaluated by the child.  Both jp and n may
 * be NULL.  The mode parameter can be one of the following:
 *	FORK_FG - Fork off a foreground process.
 *	FORK_BG - Fork off a background process.
 *	FORK_NOJOB - Like FORK_FG, but don't give the process its own
 *		     process group even if job control is on.
 *
 * When job control is turned off, background processes have their standard
 * input redirected to /dev/null (except for the second and later processes
 * in a pipeline).
 */

int
forkshell(struct job *jp, union node *n, int mode)
{
	pid_t pid;
	int serrno;

	TRACE(("forkshell(%%%d, %p, %d) called\n", jp - jobtab, n, mode));
	switch ((pid = fork())) {
	case -1:
		serrno = errno;
		TRACE(("Fork failed, errno=%d\n", serrno));
		INTON;
		error("Cannot fork (%s)", strerror(serrno));
		break;
	case 0:
		forkchild(jp, n, mode, 0);
		return 0;
	default:
		return forkparent(jp, n, mode, pid);
	}
}

int
forkparent(struct job *jp, union node *n, int mode, pid_t pid)
{
	int pgrp;

	if (rootshell && mode != FORK_NOJOB && mflag) {
		if (jp == NULL || jp->nprocs == 0)
			pgrp = pid;
		else
			pgrp = jp->ps[0].pid;
		/* This can fail because we are doing it in the child also */
		(void)setpgid(pid, pgrp);
	}
	if (mode == FORK_BG)
		backgndpid = pid;		/* set $! */
	if (jp) {
		struct procstat *ps = &jp->ps[jp->nprocs++];
		ps->pid = pid;
		ps->status = -1;
		ps->cmd[0] = 0;
		if (/* iflag && rootshell && */ n)
			commandtext(ps, n);
	}
	TRACE(("In parent shell:  child = %d\n", pid));
	return pid;
}

void
forkchild(struct job *jp, union node *n, int mode, int vforked)
{
	int wasroot;
#if JOBS /* LSC: for proper compilation with JOBS == 0 */
	int pgrp;
#endif
	const char *devnull = _PATH_DEVNULL;
	const char *nullerr = "Can't open %s";

	wasroot = rootshell;
	TRACE(("Child shell %d\n", getpid()));
	if (!vforked)
		rootshell = 0;

	closescript(vforked);
	clear_traps(vforked);
#if JOBS
	if (!vforked)
		jobctl = 0;		/* do job control only in root shell */
	if (wasroot && mode != FORK_NOJOB && mflag) {
		if (jp == NULL || jp->nprocs == 0)
			pgrp = getpid();
		else
			pgrp = jp->ps[0].pid;
		/* This can fail because we are doing it in the parent also */
		(void)setpgid(0, pgrp);
		if (mode == FORK_FG) {
			if (tcsetpgrp(ttyfd, pgrp) == -1)
				error("Cannot set tty process group (%s) at %d",
				    strerror(errno), __LINE__);
		}
		setsignal(SIGTSTP, vforked);
		setsignal(SIGTTOU, vforked);
	} else if (mode == FORK_BG) {
		ignoresig(SIGINT, vforked);
		ignoresig(SIGQUIT, vforked);
		if ((jp == NULL || jp->nprocs == 0) &&
		    ! fd0_redirected_p ()) {
			close(0);
			if (open(devnull, O_RDONLY) != 0)
				error(nullerr, devnull);
		}
	}
#else
	if (mode == FORK_BG) {
		ignoresig(SIGINT, vforked);
		ignoresig(SIGQUIT, vforked);
		if ((jp == NULL || jp->nprocs == 0) &&
		    ! fd0_redirected_p ()) {
			close(0);
			if (open(devnull, O_RDONLY) != 0)
				error(nullerr, devnull);
		}
	}
#endif
	if (wasroot && iflag) {
		setsignal(SIGINT, vforked);
		setsignal(SIGQUIT, vforked);
		setsignal(SIGTERM, vforked);
	}

	if (!vforked)
		jobs_invalid = 1;
}

/*
 * Wait for job to finish.
 *
 * Under job control we have the problem that while a child process is
 * running interrupts generated by the user are sent to the child but not
 * to the shell.  This means that an infinite loop started by an inter-
 * active user may be hard to kill.  With job control turned off, an
 * interactive user may place an interactive program inside a loop.  If
 * the interactive program catches interrupts, the user doesn't want
 * these interrupts to also abort the loop.  The approach we take here
 * is to have the shell ignore interrupt signals while waiting for a
 * forground process to terminate, and then send itself an interrupt
 * signal if the child process was terminated by an interrupt signal.
 * Unfortunately, some programs want to do a bit of cleanup and then
 * exit on interrupt; unless these processes terminate themselves by
 * sending a signal to themselves (instead of calling exit) they will
 * confuse this approach.
 */

int
waitforjob(struct job *jp)
{
#if JOBS
	int mypgrp = getpgrp();
#endif
	int status;
	int st;

	INTOFF;
	TRACE(("waitforjob(%%%d) called\n", jp - jobtab + 1));
	while (jp->state == JOBRUNNING) {
		dowait(WBLOCK, jp);
	}
#if JOBS
	if (jp->jobctl) {
		if (tcsetpgrp(ttyfd, mypgrp) == -1)
			error("Cannot set tty process group (%s) at %d",
			    strerror(errno), __LINE__);
	}
	if (jp->state == JOBSTOPPED && curjob != jp - jobtab)
		set_curjob(jp, 2);
#endif
	status = jp->ps[jp->nprocs - 1].status;
	/* convert to 8 bits */
	if (WIFEXITED(status))
		st = WEXITSTATUS(status);
#if JOBS
	else if (WIFSTOPPED(status))
		st = WSTOPSIG(status) + 128;
#endif
	else
		st = WTERMSIG(status) + 128;
	TRACE(("waitforjob: job %d, nproc %d, status %x, st %x\n",
		jp - jobtab + 1, jp->nprocs, status, st ));
#if JOBS
	if (jp->jobctl) {
		/*
		 * This is truly gross.
		 * If we're doing job control, then we did a TIOCSPGRP which
		 * caused us (the shell) to no longer be in the controlling
		 * session -- so we wouldn't have seen any ^C/SIGINT.  So, we
		 * intuit from the subprocess exit status whether a SIGINT
		 * occurred, and if so interrupt ourselves.  Yuck.  - mycroft
		 */
		if (WIFSIGNALED(status) && WTERMSIG(status) == SIGINT)
			raise(SIGINT);
	}
#endif
	if (! JOBS || jp->state == JOBDONE)
		freejob(jp);
	INTON;
	return st;
}



/*
 * Wait for a process to terminate.
 */

STATIC int
dowait(int flags, struct job *job)
{
	int pid;
	int status;
	struct procstat *sp;
	struct job *jp;
	struct job *thisjob;
	int done;
	int stopped;

	TRACE(("dowait(%x) called\n", flags));
	do {
		pid = waitproc(flags & WBLOCK, job, &status);
		TRACE(("wait returns pid %d, status %d\n", pid, status));
	} while (pid == -1 && errno == EINTR && pendingsigs == 0);
	if (pid <= 0)
		return pid;
	INTOFF;
	thisjob = NULL;
	for (jp = jobtab ; jp < jobtab + njobs ; jp++) {
		if (jp->used) {
			done = 1;
			stopped = 1;
			for (sp = jp->ps ; sp < jp->ps + jp->nprocs ; sp++) {
				if (sp->pid == -1)
					continue;
				if (sp->pid == pid) {
					TRACE(("Job %d: changing status of proc %d from 0x%x to 0x%x\n", jp - jobtab + 1, pid, sp->status, status));
					sp->status = status;
					thisjob = jp;
				}
				if (sp->status == -1)
					stopped = 0;
				else if (WIFSTOPPED(sp->status))
					done = 0;
			}
			if (stopped) {		/* stopped or done */
				int state = done ? JOBDONE : JOBSTOPPED;
				if (jp->state != state) {
					TRACE(("Job %d: changing state from %d to %d\n", jp - jobtab + 1, jp->state, state));
					jp->state = state;
#if JOBS
					if (done)
						set_curjob(jp, 0);
#endif
				}
			}
		}
	}

	if (thisjob && thisjob->state != JOBRUNNING) {
		int mode = 0;
		if (!rootshell || !iflag)
			mode = SHOW_SIGNALLED;
		if ((job == thisjob && (flags & WNOFREE) == 0) ||
		    (job != thisjob && (flags & WNOFREE) != 0))
			mode = SHOW_SIGNALLED | SHOW_NO_FREE;
		if (mode)
			showjob(out2, thisjob, mode);
		else {
			TRACE(("Not printing status, rootshell=%d, job=%p\n",
				rootshell, job));
			thisjob->changed = 1;
		}
	}

	INTON;
	return pid;
}



/*
 * Do a wait system call.  If job control is compiled in, we accept
 * stopped processes.  If block is zero, we return a value of zero
 * rather than blocking.
 *
 * System V doesn't have a non-blocking wait system call.  It does
 * have a SIGCLD signal that is sent to a process when one of its
 * children dies.  The obvious way to use SIGCLD would be to install
 * a handler for SIGCLD which simply bumped a counter when a SIGCLD
 * was received, and have waitproc bump another counter when it got
 * the status of a process.  Waitproc would then know that a wait
 * system call would not block if the two counters were different.
 * This approach doesn't work because if a process has children that
 * have not been waited for, System V will send it a SIGCLD when it
 * installs a signal handler for SIGCLD.  What this means is that when
 * a child exits, the shell will be sent SIGCLD signals continuously
 * until is runs out of stack space, unless it does a wait call before
 * restoring the signal handler.  The code below takes advantage of
 * this (mis)feature by installing a signal handler for SIGCLD and
 * then checking to see whether it was called.  If there are any
 * children to be waited for, it will be.
 *
 * If neither SYSV nor BSD is defined, we don't implement nonblocking
 * waits at all.  In this case, the user will not be informed when
 * a background process until the next time she runs a real program
 * (as opposed to running a builtin command or just typing return),
 * and the jobs command may give out of date information.
 */

#ifdef SYSV
STATIC int gotsigchild;

STATIC int onsigchild() {
	gotsigchild = 1;
}
#endif


STATIC int
waitproc(int block, struct job *jp, int *status)
{
#ifdef BSD
	int flags = 0;

#if JOBS
	if (jp != NULL && jp->jobctl)
		flags |= WUNTRACED;
#endif
	if (block == 0)
		flags |= WNOHANG;
	return waitpid(-1, status, flags);
#else
#ifdef SYSV
	int (*save)();

	if (block == 0) {
		gotsigchild = 0;
		save = signal(SIGCLD, onsigchild);
		signal(SIGCLD, save);
		if (gotsigchild == 0)
			return 0;
	}
	return wait(status);
#else
	if (block == 0)
		return 0;
	return wait(status);
#endif
#endif
}

/*
 * return 1 if there are stopped jobs, otherwise 0
 */
int job_warning = 0;
int
stoppedjobs(void)
{
	int jobno;
	struct job *jp;

	if (job_warning || jobs_invalid)
		return (0);
	for (jobno = 1, jp = jobtab; jobno <= njobs; jobno++, jp++) {
		if (jp->used == 0)
			continue;
		if (jp->state == JOBSTOPPED) {
			out2str("You have stopped jobs.\n");
			job_warning = 2;
			return (1);
		}
	}

	return (0);
}

/*
 * Return a string identifying a command (to be printed by the
 * jobs command).
 */

STATIC char *cmdnextc;
STATIC int cmdnleft;

void
commandtext(struct procstat *ps, union node *n)
{
	int len;

	cmdnextc = ps->cmd;
	if (iflag || mflag || sizeof ps->cmd < 100)
		len = sizeof(ps->cmd);
	else
		len = sizeof(ps->cmd) / 10;
	cmdnleft = len;
	cmdtxt(n);
	if (cmdnleft <= 0) {
		char *p = ps->cmd + len - 4;
		p[0] = '.';
		p[1] = '.';
		p[2] = '.';
		p[3] = 0;
	} else
		*cmdnextc = '\0';
	TRACE(("commandtext: ps->cmd %x, end %x, left %d\n\t\"%s\"\n",
		ps->cmd, cmdnextc, cmdnleft, ps->cmd));
}


STATIC void
cmdtxt(union node *n)
{
	union node *np;
	struct nodelist *lp;
	const char *p;
	int i;
	char s[2];

	if (n == NULL || cmdnleft <= 0)
		return;
	switch (n->type) {
	case NSEMI:
		cmdtxt(n->nbinary.ch1);
		cmdputs("; ");
		cmdtxt(n->nbinary.ch2);
		break;
	case NAND:
		cmdtxt(n->nbinary.ch1);
		cmdputs(" && ");
		cmdtxt(n->nbinary.ch2);
		break;
	case NOR:
		cmdtxt(n->nbinary.ch1);
		cmdputs(" || ");
		cmdtxt(n->nbinary.ch2);
		break;
	case NPIPE:
		for (lp = n->npipe.cmdlist ; lp ; lp = lp->next) {
			cmdtxt(lp->n);
			if (lp->next)
				cmdputs(" | ");
		}
		break;
	case NSUBSHELL:
		cmdputs("(");
		cmdtxt(n->nredir.n);
		cmdputs(")");
		break;
	case NREDIR:
	case NBACKGND:
		cmdtxt(n->nredir.n);
		break;
	case NIF:
		cmdputs("if ");
		cmdtxt(n->nif.test);
		cmdputs("; then ");
		cmdtxt(n->nif.ifpart);
		if (n->nif.elsepart) {
			cmdputs("; else ");
			cmdtxt(n->nif.elsepart);
		}
		cmdputs("; fi");
		break;
	case NWHILE:
		cmdputs("while ");
		goto until;
	case NUNTIL:
		cmdputs("until ");
until:
		cmdtxt(n->nbinary.ch1);
		cmdputs("; do ");
		cmdtxt(n->nbinary.ch2);
		cmdputs("; done");
		break;
	case NFOR:
		cmdputs("for ");
		cmdputs(n->nfor.var);
		cmdputs(" in ");
		cmdlist(n->nfor.args, 1);
		cmdputs("; do ");
		cmdtxt(n->nfor.body);
		cmdputs("; done");
		break;
	case NCASE:
		cmdputs("case ");
		cmdputs(n->ncase.expr->narg.text);
		cmdputs(" in ");
		for (np = n->ncase.cases; np; np = np->nclist.next) {
			cmdtxt(np->nclist.pattern);
			cmdputs(") ");
			cmdtxt(np->nclist.body);
			cmdputs(";; ");
		}
		cmdputs("esac");
		break;
	case NDEFUN:
		cmdputs(n->narg.text);
		cmdputs("() { ... }");
		break;
	case NCMD:
		cmdlist(n->ncmd.args, 1);
		cmdlist(n->ncmd.redirect, 0);
		break;
	case NARG:
		cmdputs(n->narg.text);
		break;
	case NTO:
		p = ">";  i = 1;  goto redir;
	case NCLOBBER:
		p = ">|";  i = 1;  goto redir;
	case NAPPEND:
		p = ">>";  i = 1;  goto redir;
	case NTOFD:
		p = ">&";  i = 1;  goto redir;
	case NFROM:
		p = "<";  i = 0;  goto redir;
	case NFROMFD:
		p = "<&";  i = 0;  goto redir;
	case NFROMTO:
		p = "<>";  i = 0;  goto redir;
redir:
		if (n->nfile.fd != i) {
			s[0] = n->nfile.fd + '0';
			s[1] = '\0';
			cmdputs(s);
		}
		cmdputs(p);
		if (n->type == NTOFD || n->type == NFROMFD) {
			s[0] = n->ndup.dupfd + '0';
			s[1] = '\0';
			cmdputs(s);
		} else {
			cmdtxt(n->nfile.fname);
		}
		break;
	case NHERE:
	case NXHERE:
		cmdputs("<<...");
		break;
	default:
		cmdputs("???");
		break;
	}
}

STATIC void
cmdlist(union node *np, int sep)
{
	for (; np; np = np->narg.next) {
		if (!sep)
			cmdputs(" ");
		cmdtxt(np);
		if (sep && np->narg.next)
			cmdputs(" ");
	}
}


STATIC void
cmdputs(const char *s)
{
	const char *p, *str = 0;
	char c, cc[2] = " ";
	char *nextc;
	int nleft;
	int subtype = 0;
	int quoted = 0;
	static char vstype[16][4] = { "", "}", "-", "+", "?", "=",
					"#", "##", "%", "%%" };

	p = s;
	nextc = cmdnextc;
	nleft = cmdnleft;
	while (nleft > 0 && (c = *p++) != 0) {
		switch (c) {
		case CTLESC:
			c = *p++;
			break;
		case CTLVAR:
			subtype = *p++;
			if ((subtype & VSTYPE) == VSLENGTH)
				str = "${#";
			else
				str = "${";
			if (!(subtype & VSQUOTE) != !(quoted & 1)) {
				quoted ^= 1;
				c = '"';
			} else
				c = *str++;
			break;
		case CTLENDVAR:
			if (quoted & 1) {
				c = '"';
				str = "}";
			} else
				c = '}';
			quoted >>= 1;
			subtype = 0;
			break;
		case CTLBACKQ:
			c = '$';
			str = "(...)";
			break;
		case CTLBACKQ+CTLQUOTE:
			c = '"';
			str = "$(...)\"";
			break;
		case CTLARI:
			c = '$';
			str = "((";
			break;
		case CTLENDARI:
			c = ')';
			str = ")";
			break;
		case CTLQUOTEMARK:
			quoted ^= 1;
			c = '"';
			break;
		case '=':
			if (subtype == 0)
				break;
			str = vstype[subtype & VSTYPE];
			if (subtype & VSNUL)
				c = ':';
			else
				c = *str++;
			if (c != '}')
				quoted <<= 1;
			break;
		case '\'':
		case '\\':
		case '"':
		case '$':
			/* These can only happen inside quotes */
			cc[0] = c;
			str = cc;
			c = '\\';
			break;
		default:
			break;
		}
		do {
			*nextc++ = c;
		} while (--nleft > 0 && str && (c = *str++));
		str = 0;
	}
	if ((quoted & 1) && nleft) {
		*nextc++ = '"';
		nleft--;
	}
	cmdnleft = nleft;
	cmdnextc = nextc;
}
