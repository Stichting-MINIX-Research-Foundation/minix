/*	$NetBSD: strmacros.h,v 1.1 2013/03/17 00:42:32 christos Exp $	*/

/*
 * Copyright (c) 1996-2002 Eduardo Horvath
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *notice, this list of conditions and the following disclaimer.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR  ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR  BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <machine/asm.h>
#if defined(_KERNEL) && !defined(_RUMPKERNEL)
#define USE_BLOCK_STORE_LOAD	/* enable block load/store ops */
#include "assym.h"
#include <machine/param.h>
#include <machine/ctlreg.h>
#include <machine/psl.h>
#include <machine/frame.h>
#include <machine/intr.h>
#include <machine/locore.h>

#ifdef USE_BLOCK_STORE_LOAD

#define BLOCK_SIZE SPARC64_BLOCK_SIZE
#define BLOCK_ALIGN SPARC64_BLOCK_ALIGN

/*
 * The following routines allow fpu use in the kernel.
 *
 * They allocate a stack frame and use all local regs.	Extra
 * local storage can be requested by setting the siz parameter,
 * and can be accessed at %sp+CC64FSZ.
 */

#define ENABLE_FPU(siz)							\
	save	%sp, -(CC64FSZ), %sp;	/* Allocate a stack frame */	\
	sethi	%hi(FPLWP), %l1;					\
	add	%fp, STKB-FS_SIZE, %l0;		/* Allocate a fpstate */\
	LDPTR	[%l1 + %lo(FPLWP)], %l2;	/* Load fplwp */	\
	andn	%l0, BLOCK_ALIGN, %l0;		/* Align it */		\
	clr	%l3;				/* NULL fpstate */	\
	brz,pt	%l2, 1f;			/* fplwp == NULL? */	\
	 add	%l0, -STKB-CC64FSZ-(siz), %sp;	/* Set proper %sp */	\
	LDPTR	[%l2 + L_FPSTATE], %l3;					\
	brz,pn	%l3, 1f;	/* Make sure we have an fpstate */	\
	 mov	%l3, %o0;						\
	call	_C_LABEL(savefpstate);	/* Save the old fpstate */	\
1:									\
	 set	EINTSTACK-STKB, %l4;	/* Are we on intr stack? */	\
	cmp	%sp, %l4;						\
	bgu,pt	%xcc, 1f;						\
	 set	INTSTACK-STKB, %l4;					\
	cmp	%sp, %l4;						\
	blu	%xcc, 1f;						\
0:									\
	 sethi	%hi(_C_LABEL(lwp0)), %l4;	/* Yes, use lpw0 */	\
	ba,pt	%xcc, 2f; /* XXXX needs to change to CPUs idle proc */	\
	 or	%l4, %lo(_C_LABEL(lwp0)), %l5;				\
1:									\
	sethi	%hi(CURLWP), %l4;		/* Use curlwp */	\
	LDPTR	[%l4 + %lo(CURLWP)], %l5;				\
	brz,pn	%l5, 0b; nop;	/* If curlwp is NULL need to use lwp0 */\
2:									\
	LDPTR	[%l5 + L_FPSTATE], %l6;		/* Save old fpstate */	\
	STPTR	%l0, [%l5 + L_FPSTATE];		/* Insert new fpstate */\
	STPTR	%l5, [%l1 + %lo(FPLWP)];	/* Set new fplwp */	\
	wr	%g0, FPRS_FEF, %fprs		/* Enable FPU */

/*
 * Weve saved our possible fpstate, now disable the fpu
 * and continue with life.
 */
#ifdef DEBUG
#define __CHECK_FPU				\
	LDPTR	[%l5 + L_FPSTATE], %l7;		\
	cmp	%l7, %l0;			\
	tnz	1;
#else
#define __CHECK_FPU
#endif
	
#define RESTORE_FPU							 \
	__CHECK_FPU							 \
	STPTR	%l2, [%l1 + %lo(FPLWP)];	/* Restore old fproc */	 \
	wr	%g0, 0, %fprs;			/* Disable fpu */	 \
	brz,pt	%l3, 1f;			/* Skip if no fpstate */ \
	 STPTR	%l6, [%l5 + L_FPSTATE];		/* Restore old fpstate */\
									 \
	mov	%l3, %o0;						 \
	call	_C_LABEL(loadfpstate);		/* Reload orig fpstate */\
1: 									 \
	 membar #Sync;				/* Finish all FP ops */

#endif	/* USE_BLOCK_STORE_LOAD */

#ifdef USE_BLOCK_STORE_LOAD
#if 0
#define ASI_STORE	ASI_BLK_COMMIT_P
#else
#define ASI_STORE	ASI_BLK_P
#endif
#endif	/* USE_BLOCK_STORE_LOAD */
#endif /* _KERNEL && !_RUMPKERNEL */
