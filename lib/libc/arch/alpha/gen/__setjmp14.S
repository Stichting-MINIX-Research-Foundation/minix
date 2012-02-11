/* $NetBSD: __setjmp14.S,v 1.4 2004/03/21 18:04:27 matt Exp $ */

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <machine/asm.h>
#include "assym.h"

/*
 * C library -- setjmp, longjmp
 *
 *	longjmp(a,v)
 * will generate a "return(v)" from
 * the last call to
 *	setjmp(a)
 * by restoring registers from the stack,
 * and the previous signal state.
 */

	.set	noreorder

LEAF(__setjmp14, 1)
	LDGP(pv)
	stq	ra, SC_PC(a0)			/* sc_pc = return address */
	stq	s0, (SC_REGS+_REG_S0)(a0)	/* saved bits of sc_regs */
	stq	s1, (SC_REGS+_REG_S1)(a0)
	stq	s2, (SC_REGS+_REG_S2)(a0)
	stq	s3, (SC_REGS+_REG_S3)(a0)
	stq	s4, (SC_REGS+_REG_S4)(a0)
	stq	s5, (SC_REGS+_REG_S5)(a0)
	stq	s6, (SC_REGS+_REG_S6)(a0)
	stq	ra, (SC_REGS+_REG_RA)(a0)
	stq	sp, (SC_REGS+_REG_SP)(a0)

	/*
	 * get signal information
	 */
	mov	a0, s0				/* squirrel away ptr to sc */

	/* see what's blocked */
	mov	zero, a0			/* how (insignificant) */
	mov	zero, a1			/* set (NULL) */
	lda	a2, SC_MASK(s0)			/* point to mask in sc */
	CALL(__sigprocmask14)

	lda	sp, -24(sp)			/* sizeof struct sigaltstack */
	mov	zero, a0
	mov	sp, a1
	CALL(__sigaltstack14)
	ldl	t0, 16(sp)			/* offset of ss_flags */
	lda	sp, 24(sp)			/* sizeof struct sigaltstack */
	ldq	ra, (SC_REGS+_REG_RA)(s0)	/* restore return address */
	blt	v0, botch			/* check for error */
	and	t0, 0x1, t0			/* get SA_ONSTACK flag */
	stq	t0, SC_ONSTACK(s0)		/* and save it in sc_onstack */
	/*
	 * Restore old s0 and a0, and continue saving registers
	 */
	mov	s0, a0
	ldq	s0, (SC_REGS+_REG_S0)(a0)

	ldiq	t0, 0xacedbade			/* sigcontext magic number */
	stq	t0, (SC_REGS+_REG_UNIQUE)(a0)	/* magic in sc_regs[31] */
	/* Too bad we can't check if we actually used FP */
	ldiq	t0, 1
	stq	t0, SC_OWNEDFP(a0)		/* say we've used FP.  */
	stt	fs0, (2*8 + SC_FPREGS)(a0)	/* saved bits of sc_fpregs */
	stt	fs1, (3*8 + SC_FPREGS)(a0)
	stt	fs2, (4*8 + SC_FPREGS)(a0)
	stt	fs3, (5*8 + SC_FPREGS)(a0)
	stt	fs4, (6*8 + SC_FPREGS)(a0)
	stt	fs5, (7*8 + SC_FPREGS)(a0)
	stt	fs6, (8*8 + SC_FPREGS)(a0)
	stt	fs7, (9*8 + SC_FPREGS)(a0)
	mf_fpcr	ft0				/* get FP control reg */
	stt	ft0, SC_FPCR(a0)		/* and store it in sc_fpcr */
	stq	zero, SC_FP_CONTROL(a0)		/* FP software control XXX */
	stq	zero, (SC_RESERVED + 0*8)(a0)	/* sc_reserved[0] */
	stq	zero, (SC_RESERVED + 1*8)(a0)	/* sc_reserved[1] */
	stq	zero, (SC_XXX + 0*8)(a0)	/* sc_xxx[0] */
	stq	zero, (SC_XXX + 1*8)(a0)	/* sc_xxx[1] */
	stq	zero, (SC_XXX + 2*8)(a0)	/* sc_xxx[2] */
	stq	zero, (SC_XXX + 3*8)(a0)	/* sc_xxx[3] */
	stq	zero, (SC_XXX + 4*8)(a0)	/* sc_xxx[4] */
	stq	zero, (SC_XXX + 5*8)(a0)	/* sc_xxx[5] */
	stq	zero, (SC_XXX + 6*8)(a0)	/* sc_xxx[6] */
	stq	zero, (SC_XXX + 7*8)(a0)	/* sc_xxx[7] */

	mov	zero, v0			/* return zero */
	RET
botch:
	CALL(abort)
	RET					/* "can't" get here... */
END(__setjmp14)
