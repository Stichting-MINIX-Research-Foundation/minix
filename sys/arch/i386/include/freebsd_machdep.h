/*	$NetBSD: freebsd_machdep.h,v 1.13 2014/02/12 23:24:09 dsl Exp $	*/

/*
 * Copyright (c) 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	from: @(#)signal.h	8.1 (Berkeley) 6/11/93
 *	from: Id: signal.h,v 1.4 1994/08/21 04:55:30 paul Exp 
 *
 *	from: @(#)frame.h	5.2 (Berkeley) 1/18/91
 *	from: Id: frame.h,v 1.10 1995/03/16 18:11:42 bde Exp 
 */
#ifndef _FREEBSD_MACHDEP_H
#define _FREEBSD_MACHDEP_H

#include <compat/sys/sigtypes.h>

/*
 * signal support
 */

struct freebsd_osigcontext {
	int	sc_onstack;	/* sigstack state to restore */
	sigset13_t sc_mask;	/* signal mask to restore */
	int	sc_esp;		/* machine state */
	int	sc_ebp;
	int	sc_isp;
	int	sc_eip;
	int	sc_eflags;
	int	sc_es;
	int	sc_ds;
	int	sc_cs;
	int	sc_ss;
	int	sc_edi;
	int	sc_esi;
	int	sc_ebx;
	int	sc_edx;
	int	sc_ecx;
	int	sc_eax;
};

/*
 * The sequence of the fields/registers in struct sigcontext should match
 * those in mcontext_t.
 */
struct freebsd_sigcontext {
	sigset_t sc_mask;		/* signal mask to restore */
	int	sc_onstack;		/* sigstack state to restore */
	int	sc_gs;			/* machine state (struct trapframe): */
	int	sc_fs;
	int	sc_es;
	int	sc_ds;
	int	sc_edi;
	int	sc_esi;
	int	sc_ebp;
	int	sc_isp;
	int	sc_ebx;
	int	sc_edx;
	int	sc_ecx;
	int	sc_eax;
	int	sc_trapno;
	int	sc_err;
	int	sc_eip;
	int	sc_cs;
	int	sc_efl;
	int	sc_esp;
	int	sc_ss;
	/*
	 * XXX FPU state is 27 * 4 bytes h/w, 1 * 4 bytes s/w (probably not
	 * needed here), or that + 16 * 4 bytes for emulators (probably all
	 * needed here).  The "spare" bytes are mostly not spare.
	 */
	int	sc_fpregs[28];		/* machine state (FPU): */
	int	sc_spare[17];
};

struct freebsd_sigframe {
	int	sf_signum;
	int	sf_code;
	struct	freebsd_sigcontext *sf_scp;
	char	*sf_addr;
	sig_t	sf_handler;
	struct	freebsd_sigcontext sf_sc;
};

/* sys/i386/include/exec.h */
#define FREEBSD___LDPGSZ	4096

void freebsd_syscall_intern(struct proc *);

#endif /* _FREEBSD_MACHDEP_H */
