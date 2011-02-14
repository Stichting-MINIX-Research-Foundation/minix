/* $NetBSD: _mcount.S,v 1.2 2005/12/21 18:11:11 christos Exp $ */

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
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

#if defined(GPROF) && !defined(_STANDALONE)

#include <machine/asm.h>
#include <machine/profile.h>

#define	OFFSET_AT	(0 * 8)
#define OFFSET_V0	(1 * 8)
#define OFFSET_T0	(2 * 8)
#define OFFSET_T1	(3 * 8)
#define OFFSET_T2	(4 * 8)
#define OFFSET_T3	(5 * 8)
#define OFFSET_T4	(6 * 8)
#define OFFSET_T5	(7 * 8)
#define OFFSET_T6	(8 * 8)
#define OFFSET_T7	(9 * 8)
#define OFFSET_S6	(10 * 8)
#define OFFSET_A0	(11 * 8)
#define OFFSET_A1	(12 * 8)
#define OFFSET_A2	(13 * 8)
#define OFFSET_A3	(14 * 8)
#define OFFSET_A4	(15 * 8)
#define OFFSET_A5	(16 * 8)
#define OFFSET_T8	(17 * 8)
#define OFFSET_T9	(18 * 8)
#define OFFSET_T10	(19 * 8)
#define OFFSET_T11	(20 * 8)
#define OFFSET_RA	(21 * 8)
#define OFFSET_T12	(22 * 8)
#define OFFSET_GP	(23 * 8)
#define	FRAME_SIZE	(24 * 8)

LEAF_NOPROFILE(_mcount,0)		/* XXX */
	.set noat
	.set noreorder

	lda	sp, -FRAME_SIZE(sp)

	stq	at_reg, OFFSET_AT(sp)
	stq	v0, OFFSET_V0(sp)
	stq	t0, OFFSET_T0(sp)
	stq	t1, OFFSET_T1(sp)
	stq	t2, OFFSET_T2(sp)
	stq	t3, OFFSET_T3(sp)
	stq	t4, OFFSET_T4(sp)
	stq	t5, OFFSET_T5(sp)
	stq	t6, OFFSET_T6(sp)
	stq	t7, OFFSET_T7(sp)
	stq	s6, OFFSET_S6(sp)	/* XXX because run _after_ prologue. */
	stq	a0, OFFSET_A0(sp)
	stq	a1, OFFSET_A1(sp)
	stq	a2, OFFSET_A2(sp)
	stq	a3, OFFSET_A3(sp)
	stq	a4, OFFSET_A4(sp)
	stq	a5, OFFSET_A5(sp)
	stq	t8, OFFSET_T8(sp)
	stq	t9, OFFSET_T9(sp)
	stq	t10, OFFSET_T10(sp)
	stq	t11, OFFSET_T11(sp)
	stq	ra, OFFSET_RA(sp)
	stq	t12, OFFSET_T12(sp)
	stq	gp, OFFSET_GP(sp)

	br	pv, 1f
1:	LDGP(pv)
	mov	ra, a0
	mov	at_reg, a1
	CALL(_MCOUNT_FUNC)

	ldq	v0, OFFSET_V0(sp)
	ldq	t0, OFFSET_T0(sp)
	ldq	t1, OFFSET_T1(sp)
	ldq	t2, OFFSET_T2(sp)
	ldq	t3, OFFSET_T3(sp)
	ldq	t4, OFFSET_T4(sp)
	ldq	t5, OFFSET_T5(sp)
	ldq	t6, OFFSET_T6(sp)
	ldq	t7, OFFSET_T7(sp)
	ldq	s6, OFFSET_S6(sp)	/* XXX because run _after_ prologue. */
	ldq	a0, OFFSET_A0(sp)
	ldq	a1, OFFSET_A1(sp)
	ldq	a2, OFFSET_A2(sp)
	ldq	a3, OFFSET_A3(sp)
	ldq	a4, OFFSET_A4(sp)
	ldq	a5, OFFSET_A5(sp)
	ldq	t8, OFFSET_T8(sp)
	ldq	t9, OFFSET_T9(sp)
	ldq	t10, OFFSET_T10(sp)
	ldq	t11, OFFSET_T11(sp)
	ldq	ra, OFFSET_RA(sp)
	ldq	t12, OFFSET_T12(sp)
	ldq	gp, OFFSET_GP(sp)

	ldq	at_reg, OFFSET_AT(sp)

	lda	sp, FRAME_SIZE(sp)
	ret	zero, (at_reg), 1

	END(_mcount)
#endif
