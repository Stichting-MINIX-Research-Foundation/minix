/* $NetBSD: fenv.c,v 1.1 2010/07/31 21:47:53 joerg Exp $ */

/*-
 * Copyright (c) 2004-2005 David Schultz <das (at) FreeBSD.ORG>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: fenv.c,v 1.1 2010/07/31 21:47:53 joerg Exp $");

#include <assert.h>
#include <fenv.h>
#include <stddef.h>
#include <string.h>

/* Load x87 Control Word */
#define	__fldcw(__cw)		__asm__ __volatile__ \
	("fldcw %0" : : "m" (__cw))

/* No-Wait Store Control Word */
#define	__fnstcw(__cw)		__asm__ __volatile__ \
	("fnstcw %0" : "=m" (*(__cw)))

/* No-Wait Store Status Word */
#define	__fnstsw(__sw)		__asm__ __volatile__ \
	("fnstsw %0" : "=am" (*(__sw)))

/* No-Wait Clear Exception Flags */
#define	__fnclex()		__asm__ __volatile__ \
	("fnclex")

/* Load x87 Environment */
#define	__fldenv(__env)		__asm__ __volatile__ \
	("fldenv %0" : : "m" (__env))

/* No-Wait Store x87 environment */
#define	__fnstenv(__env)	__asm__ __volatile__ \
	("fnstenv %0" : "=m" (*(__env)))

/* Load the MXCSR register */
#define	__ldmxcsr(__mxcsr)	__asm__ __volatile__ \
	("ldmxcsr %0" : : "m" (__mxcsr))

/* Store the MXCSR register state */
#define	__stmxcsr(__mxcsr)	__asm__ __volatile__ \
	("stmxcsr %0" : "=m" (*(__mxcsr)))

/*
 * The following constant represents the default floating-point environment
 * (that is, the one installed at program startup) and has type pointer to
 * const-qualified fenv_t.
 *
 * It can be used as an argument to the functions within the <fenv.h> header
 * that manage the floating-point environment, namely fesetenv() and
 * feupdateenv().
 *
 * x87 fpu registers are 16bit wide. The upper bits, 31-16, are marked as
 * RESERVED. We provide a partial floating-point environment, where we
 * define only the lower bits. The reserved bits are extracted and set by
 * the consumers of FE_DFL_ENV, during runtime.
 */
fenv_t __fe_dfl_env = {
	{
		__NetBSD_NPXCW__,	/* Control word register */
		0x00000000,		/* Status word register */
		0x0000ffff,		/* Tag word register */
		{
			0x00000000,
			0x00000000,
			0x00000000,
			0x00000000,
		},
	},
	__INITIAL_MXCSR__       /* MXCSR register */
};
#define FE_DFL_ENV      ((const fenv_t *) &__fe_dfl_env)


/*
 * The feclearexcept() function clears the supported floating-point exceptions
 * represented by `excepts'.
 */
int
feclearexcept(int excepts)
{
	fenv_t fenv;
	int ex;

	_DIAGASSERT((excepts & ~FE_ALL_EXCEPT) == 0);

	ex = excepts & FE_ALL_EXCEPT;

	/* Store the current x87 floating-point environment */
	__fnstenv(&fenv);

	/* Clear the requested floating-point exceptions */
	fenv.x87.status &= ~ex;

	/* Load the x87 floating-point environent */
	__fldenv(fenv);

	/* Same for SSE environment */
	__stmxcsr(&fenv.mxcsr);
	fenv.mxcsr &= ~ex;
	__ldmxcsr(fenv.mxcsr);

	/* Success */
	return (0);
}

/*
 * The fegetexceptflag() function stores an implementation-defined
 * representation of the states of the floating-point status flags indicated by
 * the argument excepts in the object pointed to by the argument flagp.
 */
int
fegetexceptflag(fexcept_t *flagp, int excepts)
{
	uint32_t mxcsr;
	uint16_t x87_status;
	int ex;

	_DIAGASSERT(flagp != NULL);
	_DIAGASSERT((excepts & ~FE_ALL_EXCEPT) == 0);

	ex = excepts & FE_ALL_EXCEPT;

	/* Store the current x87 status register */
	__fnstsw(&x87_status);

	/* Store the MXCSR register */
	__stmxcsr(&mxcsr);

	/* Store the results in flagp */
	*flagp = (x87_status | mxcsr) & ex;

	/* Success */
	return (0);
}

/*
 * The feraiseexcept() function raises the supported floating-point exceptions
 * represented by the argument `excepts'.
 *
 * The standard explicitly allows us to execute an instruction that has the
 * exception as a side effect, but we choose to manipulate the status register
 * directly.
 *
 * The validation of input is being deferred to fesetexceptflag().
 */
int
feraiseexcept(int excepts)
{
	int ex;

	_DIAGASSERT((excepts & ~FE_ALL_EXCEPT) == 0);

	ex = excepts & FE_ALL_EXCEPT;
	fesetexceptflag((unsigned int *)&excepts, excepts);

	/* Success */
	return (0);
}

/*
 * This function sets the floating-point status flags indicated by the argument
 * `excepts' to the states stored in the object pointed to by `flagp'. It does
 * NOT raise any floating-point exceptions, but only sets the state of the flags.
 */
int
fesetexceptflag(const fexcept_t *flagp, int excepts)
{
	fenv_t fenv;
	int ex;

	_DIAGASSERT(flagp != NULL);
	_DIAGASSERT((excepts & ~FE_ALL_EXCEPT) == 0);

	ex = excepts & FE_ALL_EXCEPT;

	/* Store the current x87 floating-point environment */
	__fnstenv(&fenv);

	/* Set the requested status flags */
	fenv.x87.status |= *flagp & ex;

	/* Load the x87 floating-point environent */
	__fldenv(fenv);

	/* Same for SSE environment */
	__stmxcsr(&fenv.mxcsr);
	fenv.mxcsr |= *flagp & ex;
	__ldmxcsr(fenv.mxcsr);

	/* Success */
	return (0);
}

/*
 * The fetestexcept() function determines which of a specified subset of the
 * floating-point exception flags are currently set. The `excepts' argument
 * specifies the floating-point status flags to be queried.
 */
int
fetestexcept(int excepts)
{
	fenv_t fenv;
	uint32_t mxcsr;
	uint16_t status;
	int ex;

	_DIAGASSERT((excepts & ~FE_ALL_EXCEPT) == 0);

	ex = excepts & FE_ALL_EXCEPT;

	/* Store the current x87 floating-point environment */
	memset(&fenv, 0, sizeof(fenv));

	__fnstenv(&fenv);
	__fnstsw(&status);

	/* Store the MXCSR register state */
	__stmxcsr(&fenv.mxcsr);
	__stmxcsr(&mxcsr);

	return ((fenv.x87.status | fenv.mxcsr) & ex);
}

/*
 * The fegetround() function gets the current rounding direction.
 */
int
fegetround(void)
{
	uint32_t mxcsr;
	uint16_t control;

	/*
	 * We check both the x87 floating-point unit _and_ the SSE unit.
	 * Normally, those two must agree with respect to each other. If they
	 * don't, it's not our fault and the result is non-determinable, in
	 * which case POSIX says that a negative value should be returned.
	 */
	__fnstcw(&control);
	__stmxcsr(&mxcsr);

	if ((control & _X87_ROUNDING_MASK)
	    != ((mxcsr & _SSE_ROUNDING_MASK) >> 3)) {
		return (-1);
	}

	return (control & _X87_ROUNDING_MASK);
}

/*
 * The fesetround() function establishes the rounding direction represented by
 * its argument `round'. If the argument is not equal to the value of a rounding
 * direction macro, the rounding direction is not changed.
 */
int
fesetround(int round)
{
	uint32_t  mxcsr;
	uint16_t control;

	/* Check whether requested rounding direction is supported */
	if (round & (~_X87_ROUNDING_MASK))
		return (-1);

	/* Store the current x87 control word register  */
	__fnstcw(&control);

	/*
	 * Set the rounding direction
	 * Rounding Control is bits 10-11, so shift appropriately
	 */
	control &= ~_X87_ROUNDING_MASK;
	control |= round;

	/* Load the x87 control word register */
	__fldcw(control);

	/*
	 * Same for the SSE environment
	 * Rounding Control is bits 13-14, so shift appropriately
	 */
	__stmxcsr(&mxcsr);
	mxcsr &= ~_SSE_ROUNDING_MASK;
	mxcsr |= (round << _SSE_ROUND_SHIFT);
	__ldmxcsr(mxcsr);

	/* Success */
	return (0);
}

/*
 * The fegetenv() function attempts to store the current floating-point
 * environment in the object pointed to by envp.
 */
int
fegetenv(fenv_t *envp)
{
	_DIAGASSERT(envp != NULL);

	/* Store the current x87 floating-point environment */
	__fnstenv(envp);

	/* Store the MXCSR register state */
	__stmxcsr(&envp->mxcsr);

     /*
      * When an FNSTENV instruction is executed, all pending exceptions are
      * essentially lost (either the x87 FPU status register is cleared or all
      * exceptions are masked).
      *
      * 8.6 X87 FPU EXCEPTION SYNCHRONIZATION -
      * Intel(R) 64 and IA-32 Architectures Softare Developer's Manual - Vol 1
      *
      */
	__fldcw(envp->x87.control);

	/* Success */
	return (0);
}

/*
 * The feholdexcept() function saves the current floating-point environment
 * in the object pointed to by envp, clears the floating-point status flags, and
 * then installs a non-stop (continue on floating-point exceptions) mode, if
 * available, for all floating-point exceptions.
 */
int
feholdexcept(fenv_t *envp)
{
	uint32_t mxcsr;

	_DIAGASSERT(envp != NULL);

	/* Store the current x87 floating-point environment */
	__fnstenv(envp);

	/* Clear all exception flags in FPU */
	__fnclex();

	/* Store the MXCSR register state */
	__stmxcsr(&envp->mxcsr);

	/* Clear exception flags in MXCSR XXX */
	mxcsr = envp->mxcsr;
	mxcsr &= ~FE_ALL_EXCEPT;

	/* Mask all exceptions */
	mxcsr |= FE_ALL_EXCEPT << _SSE_EMASK_SHIFT;

	__ldmxcsr(mxcsr);

	/* Success */
	return (0);
}

/*
 * The fesetenv() function attempts to establish the floating-point environment
 * represented by the object pointed to by envp. The argument `envp' points
 * to an object set by a call to fegetenv() or feholdexcept(), or equal a
 * floating-point environment macro. The fesetenv() function does not raise
 * floating-point exceptions, but only installs the state of the floating-point
 * status flags represented through its argument.
 */
int
fesetenv(const fenv_t *envp)
{
	fenv_t fenv;

	_DIAGASSERT(envp != NULL);

	/* Store the x87 floating-point environment */
	memset(&fenv, 0, sizeof fenv);
	__fnstenv(&fenv);

	__fe_dfl_env.x87.control = (fenv.x87.control & 0xffff0000)
	    | (__fe_dfl_env.x87.control & 0x0000ffff);
	__fe_dfl_env.x87.status = (fenv.x87.status & 0xffff0000)
	    | (__fe_dfl_env.x87.status & 0x0000ffff);
	__fe_dfl_env.x87.tag = (fenv.x87.tag & 0xffff0000)
	    | (__fe_dfl_env.x87.tag & 0x0000ffff);
	__fe_dfl_env.x87.others[3] = (fenv.x87.others[3] & 0xffff0000)
	    | (__fe_dfl_env.x87.others[3] & 0x0000ffff);
	__fldenv(*envp);

	/* Store the MXCSR register */
	__ldmxcsr(envp->mxcsr);

	/* Success */
	return (0);
}

/*
 * The feupdateenv() function saves the currently raised floating-point
 * exceptions in its automatic storage, installs the floating-point environment
 * represented by the object pointed to by `envp', and then raises the saved
 * floating-point exceptions. The argument `envp' shall point to an object set
 * by a call to feholdexcept() or fegetenv(), or equal a floating-point
 * environment macro.
 */
int
feupdateenv(const fenv_t *envp)
{
	fenv_t fenv;
	uint32_t mxcsr;
	uint16_t sw;

	_DIAGASSERT(envp != NULL);

	/* Store the x87 floating-point environment */
	memset(&fenv, 0, sizeof(fenv));
	__fnstenv(&fenv);

	__fe_dfl_env.x87.control = (fenv.x87.control & 0xffff0000)
	    | (__fe_dfl_env.x87.control & 0x0000ffff);
	__fe_dfl_env.x87.status = (fenv.x87.status & 0xffff0000)
	    | (__fe_dfl_env.x87.status & 0x0000ffff);
	__fe_dfl_env.x87.tag = (fenv.x87.tag & 0xffff0000)
	    | (__fe_dfl_env.x87.tag & 0x0000ffff);
	__fe_dfl_env.x87.others[3] = (fenv.x87.others[3] & 0xffff0000)
	    | (__fe_dfl_env.x87.others[3] & 0x0000ffff);

	/* Store the x87 status register */
	__fnstsw(&sw);

	/* Store the MXCSR register */
	__stmxcsr(&mxcsr);

	/* Install new floating-point environment */
	fesetenv(envp);

	/* Raise any previously accumulated exceptions */
	feraiseexcept((sw | mxcsr) & FE_ALL_EXCEPT);

	/* Success */
	return (0);
}

/*
 * The following functions are extentions to the standard
 */
int
feenableexcept(int mask)
{
	uint32_t mxcsr, omask;
	uint16_t control;

	_DIAGASSERT((mask & ~FE_ALL_EXCEPT) == 0);
	mask &= FE_ALL_EXCEPT;

	__fnstcw(&control);
	__stmxcsr(&mxcsr);

	omask = (control | mxcsr >> _SSE_EMASK_SHIFT) & FE_ALL_EXCEPT;
	control &= ~mask;
	__fldcw(control);

	mxcsr &= ~(mask << _SSE_EMASK_SHIFT);
	__ldmxcsr(mxcsr);

	return (~omask);

}

int
fedisableexcept(int mask)
{
	uint32_t mxcsr, omask;
	uint16_t control;

	_DIAGASSERT((mask & ~FE_ALL_EXCEPT) == 0);

	__fnstcw(&control);
	__stmxcsr(&mxcsr);

	omask = (control | mxcsr >> _SSE_EMASK_SHIFT) & FE_ALL_EXCEPT;
	control |= mask;
	__fldcw(control);

	mxcsr |= mask << _SSE_EMASK_SHIFT;
	__ldmxcsr(mxcsr);

	return (~omask);
}

int
fegetexcept(void)
{
	uint16_t control;

	/*
	 * We assume that the masks for the x87 and the SSE unit are
	 * the same.
	 */
	__fnstcw(&control);

	return (control & FE_ALL_EXCEPT);
}

