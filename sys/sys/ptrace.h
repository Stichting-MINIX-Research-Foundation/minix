/*	$NetBSD: ptrace.h,v 1.46 2015/07/02 03:47:54 christos Exp $	*/

/*-
 * Copyright (c) 1984, 1993
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
 *	@(#)ptrace.h	8.2 (Berkeley) 1/4/94
 */

#ifndef	_SYS_PTRACE_H_
#define	_SYS_PTRACE_H_

#define	PT_TRACE_ME		0	/* child declares it's being traced */
#define	PT_READ_I		1	/* read word in child's I space */
#define	PT_READ_D		2	/* read word in child's D space */
#define	PT_WRITE_I		4	/* write word in child's I space */
#define	PT_WRITE_D		5	/* write word in child's D space */
#define	PT_CONTINUE		7	/* continue the child */
#define	PT_KILL			8	/* kill the child process */
#define	PT_ATTACH		9	/* attach to running process */
#define	PT_DETACH		10	/* detach from running process */
#define	PT_IO			11	/* do I/O to/from the stopped process */
#define	PT_DUMPCORE		12	/* make child generate a core dump */
#define	PT_LWPINFO		13	/* get info about the LWP */
#define	PT_SYSCALL		14	/* stop on syscall entry/exit */
#define	PT_SYSCALLEMU		15	/* cancel syscall, tracer emulates it */
#define	PT_SET_EVENT_MASK	16	/* set the event mask, defined below */
#define	PT_GET_EVENT_MASK	17	/* get the event mask, defined below */
#define	PT_GET_PROCESS_STATE	18	/* get process state, defined below */

#define	PT_FIRSTMACH		32	/* for machine-specific requests */
#include <machine/ptrace.h>		/* machine-specific requests, if any */

#define PT_STRINGS \
/*  0 */    "PT_TRACE_ME", \
/*  1 */    "PT_READ_I", \
/*  2 */    "PT_READ_D", \
/*  3 */    "*PT_INVALID_3*", \
/*  4 */    "PT_WRITE_I", \
/*  5 */    "PT_WRITE_D", \
/*  6 */    "*PT_INVALID_6*", \
/*  7 */    "PT_CONTINUE", \
/*  8 */    "PT_KILL", \
/*  9 */    "PT_ATTACH", \
/* 10 */    "PT_DETACH", \
/* 11 */    "PT_IO", \
/* 12 */    "PT_DUMPCORE", \
/* 13 */    "PT_LWPINFO", \
/* 14 */    "PT_SYSCALL", \
/* 15 */    "PT_SYSCALLEMU", \
/* 16 */    "PT_SET_EVENT_MASK", \
/* 17 */    "PT_GET_EVENT_MASK", \
/* 18 */    "PT_GET_PROCESS_STATE",

/* PT_{G,S}EVENT_MASK */
typedef struct ptrace_event {
	int	pe_set_event;
} ptrace_event_t;

/* PT_GET_PROCESS_STATE */
typedef struct ptrace_state {
	int	pe_report_event;
	pid_t	pe_other_pid;
} ptrace_state_t;

#define	PTRACE_FORK	0x0001	/* Report forks */

/*
 * Argument structure for PT_IO.
 */
struct ptrace_io_desc {
	int	piod_op;	/* I/O operation (see below) */
	void	*piod_offs;	/* child offset */
	void	*piod_addr;	/* parent offset */
	size_t	piod_len;	/* request length (in)/actual count (out) */
};

/* piod_op */
#define	PIOD_READ_D	1	/* read from D space */
#define	PIOD_WRITE_D	2	/* write to D spcae */
#define	PIOD_READ_I	3	/* read from I space */
#define	PIOD_WRITE_I	4	/* write to I space */
#define PIOD_READ_AUXV	5	/* Read from aux array */

/*
 * Argument structure for PT_LWPINFO.
 */
struct ptrace_lwpinfo {
	lwpid_t	pl_lwpid;	/* LWP described */
	int	pl_event;	/* Event that stopped the LWP */
	/* Add fields at the end */
};

#define PL_EVENT_NONE	0
#define PL_EVENT_SIGNAL	1

#ifdef _KERNEL

#if defined(PT_GETREGS) || defined(PT_SETREGS)
struct reg;
#ifndef process_reg32
#define process_reg32 struct reg
#endif
#ifndef process_reg64
#define process_reg64 struct reg
#endif
#endif
#if defined(PT_GETFPREGS) || defined(PT_SETFPREGS)
struct fpreg;
#ifndef process_fpreg32
#define process_fpreg32 struct fpreg
#endif
#ifndef process_fpreg64
#define process_fpreg64 struct fpreg
#endif
#endif

void	ptrace_init(void);

int	process_doregs(struct lwp *, struct lwp *, struct uio *);
int	process_validregs(struct lwp *);

int	process_dofpregs(struct lwp *, struct lwp *, struct uio *);
int	process_validfpregs(struct lwp *);

int	process_domem(struct lwp *, struct lwp *, struct uio *);

void	process_stoptrace(void);

void	proc_reparent(struct proc *, struct proc *);

/*
 * 64bit architectures that support 32bit emulation (amd64 and sparc64)
 * will #define process_read_regs32 to netbsd32_process_read_regs (etc).
 * In all other cases these #defines drop the size suffix.
 */
#ifdef PT_GETFPREGS
int	process_read_fpregs(struct lwp *, struct fpreg *, size_t *);
#ifndef process_read_fpregs32
#define process_read_fpregs32	process_read_fpregs
#endif
#ifndef process_read_fpregs64
#define process_read_fpregs64	process_read_fpregs
#endif
#endif
#ifdef PT_GETREGS
int	process_read_regs(struct lwp *, struct reg *);
#ifndef process_read_regs32
#define process_read_regs32	process_read_regs
#endif
#ifndef process_read_regs64
#define process_read_regs64	process_read_regs
#endif
#endif
int	process_set_pc(struct lwp *, void *);
int	process_sstep(struct lwp *, int);
#ifdef PT_SETFPREGS
int	process_write_fpregs(struct lwp *, const struct fpreg *, size_t);
#endif
#ifdef PT_SETREGS
int	process_write_regs(struct lwp *, const struct reg *);
#endif

#ifdef __HAVE_PROCFS_MACHDEP
int	ptrace_machdep_dorequest(struct lwp *, struct lwp *, int,
	    void *, int);
#endif

#ifndef FIX_SSTEP
#define FIX_SSTEP(p)
#endif

#else /* !_KERNEL */

#include <sys/cdefs.h>

__BEGIN_DECLS
int	ptrace(int _request, pid_t _pid, void *_addr, int _data);
__END_DECLS

#endif /* !_KERNEL */

#if defined(__minix)
/* Trace options. */
#define TO_TRACEFORK   0x1     /* automatically attach to forked children */
#define TO_ALTEXEC     0x2     /* send SIGSTOP on successful exec() */
#define TO_NOEXEC      0x4     /* do not send signal on successful exec() */

/* Trace spaces. */
#define TS_INS         0       /* text space */
#define TS_DATA        1       /* data space */

/* Trance range structure. */
struct ptrace_range {
  int pr_space;                        /* space in traced process */
  long pr_addr;                        /* address in traced process */
  void *pr_ptr;                        /* buffer in caller process */
  size_t pr_size;              /* size of range, in bytes */
};

/* Trace requests aliases for minix. */
#define T_OK           PT_TRACE_ME /* enable tracing by parent for this process */
#define T_GETINS       PT_READ_I   /* return value from instruction space */
#define T_GETDATA      PT_READ_D   /* return value from data space */
#define T_SETINS       PT_WRITE_I  /* set value from instruction space */
#define T_SETDATA      PT_WRITE_D  /* set value from data space */
#define T_RESUME       PT_CONTINUE /* resume execution */
#define T_EXIT         PT_KILL     /* exit */
#define T_SYSCALL      PT_SYSCALL /* trace system call */
#define T_ATTACH       PT_ATTACH  /* attach to a running process */
#define T_DETACH       PT_DETACH  /* detach from a traced process */

/* Trace requests unique to minix */
#define T_STOP        -1       /* stop the process */
#define T_READB_INS    100     /* Read a byte from the text segment of an
                                * untraced process (only for root)
                                */
#define T_WRITEB_INS   101     /* Write a byte in the text segment of an
                                * untraced process (only for root)
                                */
#define T_GETUSER      102       /* return value from user process table */
#define T_SETUSER      103       /* set value in user process table */
#define T_STEP         104       /* set trace bit */
#define T_SETOPT       105       /* set trace options */
#define T_GETRANGE     106       /* get range of values */
#define T_SETRANGE     107       /* set range of values */

#endif /* defined(__minix) */

#endif	/* !_SYS_PTRACE_H_ */
