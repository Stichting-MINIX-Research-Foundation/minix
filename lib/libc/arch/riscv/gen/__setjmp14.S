/*	$NetBSD: __setjmp14.S,v 1.2 2015/03/27 06:44:28 matt Exp $	*/

#include "SYS.h"
#include "assym.h"

#if defined(LIBC_SCCS)
__RCSID("$NetBSD: __setjmp14.S,v 1.2 2015/03/27 06:44:28 matt Exp $")
#endif

/*
 * C library -- _setjmp, _longjmp
 *
 *	longjmp(a,v)
 * will generate a "return(v?v:1)" from the last call to
 *	setjmp(a)
 * by restoring registers from the stack.
 * The previous signal state is restored.
 */

ENTRY(__setjmp14)
	li	a1, 1
	REG_S	a1, JB_MAGIC(a0)	/* indicate sigmask */
	REG_S	ra, JB_RA(a0)		/* save return address */
	REG_S	sp, JB_SP(a0)		/* save stack pointer */
	REG_S	gp, JB_GP(a0)		/* save global pointer */
	REG_S	tp, JB_TP(a0)		/* save thread pointer */
	REG_S	s0, JB_S0(a0)		/* save callee saved register */
	REG_S	s1, JB_S1(a0)		/* save callee saved register */
	REG_S	s2, JB_S2(a0)		/* save callee saved register */
	REG_S	s3, JB_S3(a0)		/* save callee saved register */
	REG_S	s4, JB_S4(a0)		/* save callee saved register */
	REG_S	s5, JB_S5(a0)		/* save callee saved register */
	REG_S	s6, JB_S6(a0)		/* save callee saved register */
	REG_S	s7, JB_S7(a0)		/* save callee saved register */
	REG_S	s8, JB_S8(a0)		/* save callee saved register */
	REG_S	s9, JB_S9(a0)		/* save callee saved register */
	REG_S	s10, JB_S10(a0)		/* save callee saved register */
	REG_S	s11, JB_S11(a0)		/* save callee saved register */
#ifndef _SOFT_FLOAT
	frcsr	t2			/* get FSCR */
	REG_S	t2, JB_FCSR(a0)		/* save it */
	fsd	fs0, JB_FS0(a0)		/* save callee saved register */
	fsd	fs1, JB_FS1(a0)		/* save callee saved register */
	fsd	fs2, JB_FS2(a0)		/* save callee saved register */
	fsd	fs3, JB_FS3(a0)		/* save callee saved register */
	fsd	fs4, JB_FS4(a0)		/* save callee saved register */
	fsd	fs5, JB_FS5(a0)		/* save callee saved register */
	fsd	fs6, JB_FS6(a0)		/* save callee saved register */
	fsd	fs7, JB_FS7(a0)		/* save callee saved register */
	fsd	fs8, JB_FS8(a0)		/* save callee saved register */
	fsd	fs9, JB_FS9(a0)		/* save callee saved register */
	fsd	fs10, JB_FS10(a0)	/* save callee saved register */
	fsd	fs11, JB_FS11(a0)	/* save callee saved register */
#endif

	addi	a2, a0, JB_SIGMASK	/* &sigmask */
	li	a1, 0
	li	a0, SIG_BLOCK
	SYSTRAP(__sigprocmask14)	/* assume no error */
	li	a0, 0	/* success */	/* indicate success */
	ret		/* error */	/* return */
END(__setjmp14)

ENTRY(__longjmp14)
	REG_L	ra, JB_RA(a0)		/* restore return address */
	REG_L	sp, JB_SP(a0)		/* restore stack pointer */
	REG_L	gp, JB_GP(a0)		/* restore global pointer */
	#REG_L	tp, JB_TP(a0)		/* restore thread pointer */
	REG_L	s0, JB_S0(a0)		/* restore callee saved register */
	REG_L	s1, JB_S1(a0)		/* restore callee saved register */
	REG_L	s2, JB_S2(a0)		/* restore callee saved register */
	REG_L	s3, JB_S3(a0)		/* restore callee saved register */
	REG_L	s4, JB_S4(a0)		/* restore callee saved register */
	REG_L	s5, JB_S5(a0)		/* restore callee saved register */
	REG_L	s6, JB_S6(a0)		/* restore callee saved register */
	REG_L	s7, JB_S7(a0)		/* restore callee saved register */
	REG_L	s8, JB_S8(a0)		/* restore callee saved register */
	REG_L	s9, JB_S9(a0)		/* restore callee saved register */
	REG_L	s10, JB_S10(a0)		/* restore callee saved register */
	REG_L	s11, JB_S11(a0)		/* restore callee saved register */
#ifndef _SOFT_FLOAT
	REG_L	t2, JB_FCSR(a0)		/* get it */
	fscsr	t2			/* restore FSCR */
	fld	fs0, JB_FS0(a0)		/* restore callee saved register */
	fld	fs1, JB_FS1(a0)		/* restore callee saved register */
	fld	fs2, JB_FS2(a0)		/* restore callee saved register */
	fld	fs3, JB_FS3(a0)		/* restore callee saved register */
	fld	fs4, JB_FS4(a0)		/* restore callee saved register */
	fld	fs5, JB_FS5(a0)		/* restore callee saved register */
	fld	fs6, JB_FS6(a0)		/* restore callee saved register */
	fld	fs7, JB_FS7(a0)		/* restore callee saved register */
	fld	fs8, JB_FS8(a0)		/* restore callee saved register */
	fld	fs9, JB_FS9(a0)		/* restore callee saved register */
	fld	fs10, JB_FS10(a0)	/* restore callee saved register */
	fld	fs11, JB_FS11(a0)	/* restore callee saved register */
#endif
	move	t1, a1			/* save return value */
	li	a2, 0
	addi	a1, a0, JB_SIGMASK	/* &sigmask */
	li	a0, SIG_SETMASK
	SYSTRAP(__sigprocmask14)	/* assume no error */
	nop		/* error */
	li	a0, 1	/* success */	/* default return value */
	beqz	t1, 1f			/* test return value */
	move	a0, t1			/* use it if not 0 */
1:	ret
END(__longjmp14)
