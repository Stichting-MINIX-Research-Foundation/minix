/* $NetBSD: fenv.c,v 1.1 2014/09/19 17:36:25 matt Exp $ */

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: fenv.c,v 1.1 2014/09/19 17:36:25 matt Exp $");

#include <sys/param.h>
#include <sys/sysctl.h>
#include <assert.h>
#include <fenv.h>
#include <stddef.h>
#include <string.h>

#include <riscv/sysreg.h>

/*
 * The following constant represents the default floating-point environment
 * (that is, the one installed at program startup) and has type pointer to
 * const-qualified fenv_t.
 *
 * It can be used as an argument to the functions within the <fenv.h> header
 * that manage the floating-point environment, namely fesetenv() and
 * feupdateenv().
 */
fenv_t __fe_dfl_env = __SHIFTIN(FCSR_FRM_RNE, FCSR_FRM);

/*
 * The feclearexcept() function clears the supported floating-point exceptions
 * represented by `excepts'.
 */
int
feclearexcept(int excepts)
{
	_DIAGASSERT((excepts & ~FE_ALL_EXCEPT) == 0);

	int fflags = riscvreg_fcsr_read_fflags();

	fflags &= ~(excepts & FE_ALL_EXCEPT);

	riscvreg_fcsr_write_fflags(fflags);

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
	_DIAGASSERT(flagp != NULL);
	_DIAGASSERT((excepts & ~FE_ALL_EXCEPT) == 0);

	*flagp = riscvreg_fcsr_read_fflags() & excepts;

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
	fexcept_t ex = 0;

	_DIAGASSERT((excepts & ~FE_ALL_EXCEPT) == 0);

	excepts &= FE_ALL_EXCEPT;
	fesetexceptflag(&ex, excepts);
	/* XXX exception magic XXX */

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
	_DIAGASSERT(flagp != NULL);
	_DIAGASSERT((excepts & ~FE_ALL_EXCEPT) == 0);

	excepts &= FE_ALL_EXCEPT;

	int fflags = riscvreg_fcsr_read_fflags();

	fflags = (fflags & ~excepts) | (*flagp & excepts);

	riscvreg_fcsr_write_fflags(fflags);

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
	_DIAGASSERT((excepts & ~FE_ALL_EXCEPT) == 0);

	return riscvreg_fcsr_read_fflags() & excepts & FE_ALL_EXCEPT;
}

int
fegetround(void)
{
	return riscvreg_fcsr_read_frm();
}

/*
 * The fesetround() function shall establish the rounding direction represented
 * by its argument round. If the argument is not equal to the value of a
 * rounding direction macro, the rounding direction is not changed.
 */
int
fesetround(int round)
{
	if ((unsigned int)round > FCSR_FRM_RMM) {
		/* Failure */
		return (-1);
	}

	riscvreg_fcsr_write_frm(round);

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

	*envp = riscvreg_fcsr_read();

	/* Success */
	return (0);
}

/*
 * The feholdexcept() function saves the current floating-point environment in
 * the object pointed to by envp, clears the floating-point status flags, and
 * then installs a non-stop (continue on floating-point exceptions) mode, if
 * available, for all floating-point exceptions.
 */
int
feholdexcept(fenv_t *envp)
{
	_DIAGASSERT(envp != NULL);

	*envp = riscvreg_fcsr_read();

	riscvreg_fcsr_write_fflags(0);

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

	_DIAGASSERT(envp != NULL);

	fenv_t env = *envp;

	if ((env & ~(FCSR_FRM|FCSR_FFLAGS)
	    || __SHIFTOUT(env, FCSR_FRM) > FCSR_FRM_RMM)) {
		return -1;
	}

	riscvreg_fcsr_write(env);

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
	_DIAGASSERT(envp != NULL);

	int fflags = riscvreg_fcsr_read_fflags();

	fesetenv(envp);
	feraiseexcept(fflags);

	/* Success */
	return (0);
}

/*
 * The following functions are extentions to the standard
 */
int
feenableexcept(int nmask)
{
	return 0;
}

int
fedisableexcept(int nmask)
{
	return 0;
}

int
fegetexcept(void)
{
	return 0;
}
