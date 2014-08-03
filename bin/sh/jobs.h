/*	$NetBSD: jobs.h,v 1.20 2011/06/18 21:18:46 christos Exp $	*/

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
 *
 *	@(#)jobs.h	8.2 (Berkeley) 5/4/95
 */

#include "output.h"

/* Mode argument to forkshell.  Don't change FORK_FG or FORK_BG. */
#define FORK_FG 0
#define FORK_BG 1
#define FORK_NOJOB 2

/* mode flags for showjob(s) */
#define	SHOW_PGID	0x01	/* only show pgid - for jobs -p */
#define	SHOW_MULTILINE	0x02	/* one line per process */
#define	SHOW_PID	0x04	/* include process pid */
#define	SHOW_CHANGED	0x08	/* only jobs whose state has changed */
#define	SHOW_SIGNALLED	0x10	/* only if stopped/exited on signal */
#define	SHOW_ISSIG	0x20	/* job was signalled */
#define	SHOW_NO_FREE	0x40	/* do not free job */


/*
 * A job structure contains information about a job.  A job is either a
 * single process or a set of processes contained in a pipeline.  In the
 * latter case, pidlist will be non-NULL, and will point to a -1 terminated
 * array of pids.
 */
#define	MAXCMDTEXT	200

struct procstat {
	pid_t	pid;		/* process id */
 	int	status;		/* last process status from wait() */
 	char	cmd[MAXCMDTEXT];/* text of command being run */
};

struct job {
	struct procstat ps0;	/* status of process */
	struct procstat *ps;	/* status or processes when more than one */
	int	nprocs;		/* number of processes */
	pid_t	pgrp;		/* process group of this job */
	char	state;
#define	JOBRUNNING	0	/* at least one proc running */
#define	JOBSTOPPED	1	/* all procs are stopped */
#define	JOBDONE		2	/* all procs are completed */
	char	used;		/* true if this entry is in used */
	char	changed;	/* true if status has changed */
#if JOBS
	char 	jobctl;		/* job running under job control */
	int	prev_job;	/* previous job index */
#endif
};

extern pid_t backgndpid;	/* pid of last background process */
extern int job_warning;		/* user was warned about stopped jobs */

void setjobctl(int);
void showjobs(struct output *, int);
struct job *makejob(union node *, int);
int forkshell(struct job *, union node *, int);
void forkchild(struct job *, union node *, int, int);
int forkparent(struct job *, union node *, int, pid_t);
int waitforjob(struct job *);
int stoppedjobs(void);
void commandtext(struct procstat *, union node *);
int getjobpgrp(const char *);

#if ! JOBS
#define setjobctl(on)	/* do nothing */
#endif
