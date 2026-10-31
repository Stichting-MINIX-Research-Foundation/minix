/*	$NetBSD: signal.h,v 1.13 2021/10/27 04:45:42 thorpej Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1991 Regents of the University of California.
 * All rights reserved.
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
 *	@(#)signal.h	7.16 (Berkeley) 3/17/91
 */

#ifndef _AMD64_SIGNAL_H_
#define _AMD64_SIGNAL_H_

#ifdef __x86_64__

#include <sys/featuretest.h>

#ifdef _KERNEL
/* This is needed to support COMPAT_NETBSD32. */
#define	__HAVE_STRUCT_SIGCONTEXT
#endif /* _KERNEL */

#if defined(__minix) && (defined(_LIBMINC) || !defined(_STANDALONE))
#include <machine/fpu.h>
#endif

typedef int sig_atomic_t;

#if defined(_NETBSD_SOURCE)
/*
 * Get the "code" values
 */
#include <machine/trap.h>
#include <machine/mcontext.h>

#if defined(_KERNEL) || defined(__minix)
/*
 * Signal context structure for x86_64 MINIX.  Mirrors the i386 layout but
 * uses 64-bit register fields and omits the 16-bit segment registers that
 * have no meaningful user-space content on AMD64.
 */
struct sigcontext {
	long	sc_rdi;
	long	sc_rsi;
	long	sc_rbp;
	long	sc_rbx;
	long	sc_rdx;
	long	sc_rcx;
	long	sc_rax;
	long	sc_r8;
	long	sc_r9;
	long	sc_r10;
	long	sc_r11;
	long	sc_r12;
	long	sc_r13;
	long	sc_r14;
	long	sc_r15;
	long	sc_rip;
	long	sc_rflags;
	long	sc_rsp;
	long	sc_ss;
	long	sc_cs;

	int	sc_onstack;		/* sigstack state to restore */
	int	__sc_mask13;		/* signal mask to restore (old style) */

	int	sc_trapno;
	int	sc_err;

	sigset_t sc_mask;		/* signal mask to restore (new style) */
#if defined(__minix) && (defined(_LIBMINC) || !defined(_STANDALONE))
	union fpu_state_u sc_fpu_state;
	int	trap_style;		/* KTS_* method of entering kernel */
	int	sc_flags;		/* MF_FPU_INITIALIZED if fpu state valid */
#define SC_MAGIC 0xc0ffee1
	int	sc_magic;
#endif
};
#endif /* _KERNEL || __minix */

#if defined(__minix) && (defined(_LIBMINC) || !defined(_STANDALONE))
__BEGIN_DECLS
int sigreturn(struct sigcontext *_scp);
__END_DECLS
#endif

#ifdef _KERNEL_OPT
#include "opt_compat_netbsd.h"
#include "opt_compat_netbsd32.h"
#endif

#endif	/* _NETBSD_SOURCE */

#else	/*	__x86_64__	*/

#include <i386/signal.h>

#endif	/*	__x86_64__	*/

#endif	/* !_AMD64_SIGNAL_H_ */
