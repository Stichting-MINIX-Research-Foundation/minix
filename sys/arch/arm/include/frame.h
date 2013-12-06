/*	$NetBSD: frame.h,v 1.18 2013/08/18 05:07:19 matt Exp $	*/

/*
 * Copyright (c) 1994-1997 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * arm/frame.h - Stack frames structures common to arm26 and arm32
 */

#ifndef _ARM_FRAME_H_
#define _ARM_FRAME_H_

#ifndef _LOCORE

#include <sys/signal.h>
#include <sys/ucontext.h>

/*
 * Trap frame.  Pushed onto the kernel stack on a trap (synchronous exception).
 */

typedef struct trapframe {
	register_t tf_spsr; /* Zero on arm26 */
	register_t tf_fill; /* fill here so r0 will be dword aligned */
	register_t tf_r0;
	register_t tf_r1;
	register_t tf_r2;
	register_t tf_r3;
	register_t tf_r4;
	register_t tf_r5;
	register_t tf_r6;
	register_t tf_r7;
	register_t tf_r8;
	register_t tf_r9;
	register_t tf_r10;
	register_t tf_r11;
	register_t tf_r12;
	register_t tf_usr_sp;
	register_t tf_usr_lr;
	register_t tf_svc_sp; /* Not used on arm26 */
	register_t tf_svc_lr; /* Not used on arm26 */
	register_t tf_pc;
} trapframe_t;

/* Register numbers */
#define tf_ip tf_r12
#define tf_r13 tf_usr_sp
#define tf_r14 tf_usr_lr
#define tf_r15 tf_pc

#ifdef __PROG32
#define TRAP_USERMODE(tf)	(((tf)->tf_spsr & PSR_MODE) == PSR_USR32_MODE)
#elif defined(__PROG26)
#define TRAP_USERMODE(tf)	(((tf)->tf_r15 & R15_MODE) == R15_MODE_USR)
#endif

/*
 * Signal frame.  Pushed onto user stack before calling sigcode.
 */
#if defined(COMPAT_16) || defined(__minix)
struct sigframe_sigcontext {
#if defined(__minix)
	struct	sigcontext *sf_scp;	/* Let sigreturn find sigcontext */
#endif /* defined(__minix) */
	struct	sigcontext sf_sc;
};
#endif

/* the pointers are use in the trampoline code to locate the ucontext */
struct sigframe_siginfo {
	siginfo_t	sf_si;		/* actual saved siginfo */
	ucontext_t	sf_uc;		/* actual saved ucontext */
};

#ifdef _KERNEL
__BEGIN_DECLS
void sendsig_sigcontext(const ksiginfo_t *, const sigset_t *);
void *getframe(struct lwp *, int, int *);
__END_DECLS
#define lwp_trapframe(l)		((l)->l_md.md_tf)
#define lwp_settrapframe(l, tf)		((l)->l_md.md_tf = (tf))
#endif

#endif /* _LOCORE */

#endif /* _ARM_FRAME_H_ */
  
/* End of frame.h */
