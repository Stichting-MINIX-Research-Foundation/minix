/* $NetBSD: fenv.c,v 1.2 2014/12/27 17:52:45 martin Exp $ */

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
__RCSID("$NetBSD: fenv.c,v 1.2 2014/12/27 17:52:45 martin Exp $");

#include <sys/param.h>
#include <sys/types.h>
#include <assert.h>
#include <fenv.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>

#include <aarch64/armreg.h>

const fenv_t __fe_dfl_env = {
	.__fpsr = 0,
	.__fpcr = FPCR_FZ|FPCR_DN|FPCR_RN,
};

/*
 * The feclearexcept() function shall attempt to clear the supported
 * floating-point exceptions represented by excepts.
 */
int
feclearexcept(int excepts)
{
#ifndef lint
	_DIAGASSERT((except & ~FE_ALL_EXCEPT) == 0);
#endif
	unsigned int tmp = reg_fpsr_read() & ~__SHIFTIN(excepts, FPSR_CSUM);
	reg_fpsr_write(tmp);
	return 0;
}

/*
 * The fegetexceptflag() function shall attempt to store an
 * implementation-defined representation of the states of the floating-point
 * status flags indicated by the argument excepts in the object pointed to by
 * the argument flagp.
 */
int
fegetexceptflag(fexcept_t *flagp, int excepts)
{
	_DIAGASSERT((except & ~FE_ALL_EXCEPT) == 0);
	*flagp = __SHIFTOUT(reg_fpsr_read(), FPSR_CSUM) & excepts;
	return 0;
}

/*
 * The feraiseexcept() function shall attempt to raise the supported
 * floating-point exceptions represented by the argument excepts. The order
 * in which these floating-point exceptions are raised is unspecified. 
 */
int
feraiseexcept(int excepts)
{
#ifndef lint
	_DIAGASSERT((except & ~FE_ALL_EXCEPT) == 0);
#endif
#ifdef __SOFTFP__
	excepts &= fpgetsticky();

	if (excepts) {
		siginfo_t info;
		memset(&info, 0, sizeof info);
		info.si_signo = SIGFPE;
		info.si_pid = getpid();
		info.si_uid = geteuid();
		if (excepts & FE_UNDERFLOW)
			info.si_code = FPE_FLTUND;
		else if (excepts & FE_OVERFLOW)
			info.si_code = FPE_FLTOVF;
		else if (excepts & FE_DIVBYZERO)
			info.si_code = FPE_FLTDIV;
		else if (excepts & FE_INVALID)
			info.si_code = FPE_FLTINV;
		else if (excepts & FE_INEXACT)
			info.si_code = FPE_FLTRES;
		sigqueueinfo(getpid(), &info);
	}
#else
	unsigned int fpsr = reg_fpsr_read();
	fpsr = (fpsr & ~FPSR_CSUM) | __SHIFTIN(excepts, FPSR_CSUM);
	reg_fpsr_write(fpsr);
	unsigned int fpcr = reg_fpcr_read();
	fpcr = (fpcr & ~FPCR_ESUM) | __SHIFTIN(excepts, FPCR_ESUM);
	reg_fpcr_write(fpcr);
#endif
	return 0;
}

/*
 * The fesetexceptflag() function shall attempt to set the floating-point
 * status flags indicated by the argument excepts to the states stored in the
 * object pointed to by flagp. The value pointed to by flagp shall have been
 * set by a previous call to fegetexceptflag() whose second argument
 * represented at least those floating-point exceptions represented by the
 * argument excepts. This function does not raise floating-point exceptions,
 * but only sets the state of the flags.
 */
int
fesetexceptflag(const fexcept_t *flagp, int excepts)
{
#ifndef lint
	_DIAGASSERT((except & ~FE_ALL_EXCEPT) == 0);
#endif
	unsigned int fpsr = reg_fpsr_read();
	fpsr &= ~__SHIFTIN(excepts, FPSR_CSUM);
	fpsr |= __SHIFTIN((*flagp & excepts), FPSR_CSUM);
	reg_fpsr_write(fpsr);
	return 0;
}

/*
 * The fetestexcept() function shall determine which of a specified subset of
 * the floating-point exception flags are currently set. The excepts argument
 * specifies the floating-point status flags to be queried.
 */
int
fetestexcept(int excepts)
{
	_DIAGASSERT((except & ~FE_ALL_EXCEPT) == 0);
	return __SHIFTOUT(reg_fpsr_read(), FPSR_CSUM) & excepts;
}

/*
 * The fegetround() function shall get the current rounding direction.
 */
int
fegetround(void)
{
	return __SHIFTOUT(reg_fpcr_read(), FPCR_RMODE);
}

/*
 * The fesetround() function shall establish the rounding direction represented
 * by its argument round. If the argument is not equal to the value of a
 * rounding direction macro, the rounding direction is not changed.
 */
int
fesetround(int round)
{
#ifndef lint
	_DIAGASSERT(!(round & ~__SHIFTOUT(FPCR_RMODE, FPCR_RMODE)));
#endif
	unsigned int fpcr = reg_fpcr_read() & ~FPCR_RMODE;
	fpcr |= __SHIFTIN(round, FPCR_RMODE);
	reg_fpcr_write(fpcr);
	return 0;
}

/*
 * The fegetenv() function shall attempt to store the current floating-point
 * environment in the object pointed to by envp.
 */
int
fegetenv(fenv_t *envp)
{
	envp->__fpcr = reg_fpcr_read();
	envp->__fpsr = reg_fpsr_read();
	return 0;
}

/*
 * The feholdexcept() function shall save the current floating-point
 * environment in the object pointed to by envp, clear the floating-point
 * status flags, and then install a non-stop (continue on floating-point
 * exceptions) mode, if available, for all floating-point exceptions.
 */
int
feholdexcept(fenv_t *envp)
{
	envp->__fpsr = reg_fpsr_read();
	envp->__fpcr = reg_fpcr_read();
	reg_fpsr_write(envp->__fpsr & ~FPSR_CSUM);
	reg_fpcr_write(envp->__fpcr & ~FPCR_ESUM);
	return 0;
}

/*
 * The fesetenv() function shall attempt to establish the floating-point
 * environment represented by the object pointed to by envp. The fesetenv()
 * function does not raise floating-point exceptions, but only installs the
 * state of the floating-point status flags represented through its argument.
 */
int
fesetenv(const fenv_t *envp)
{
	reg_fpsr_write(envp->__fpsr);
	return 0;
}

/*
 * The feupdateenv() function shall attempt to save the currently raised
 * floating-point exceptions in its automatic storage, attempt to install the
 * floating-point environment represented by the object pointed to by envp,
 * and then attempt to raise the saved floating-point exceptions. 
 */
int
feupdateenv(const fenv_t *envp)
{
#ifndef lint
	_DIAGASSERT(envp != NULL);
#endif
	reg_fpsr_write(envp->__fpsr);
	reg_fpcr_write(envp->__fpcr);

	/* Success */
	return 0;
}

int
feenableexcept(int excepts)
{
	const uint32_t __fpcr = reg_fpcr_read();
	reg_fpcr_write((__fpcr & ~FPCR_ESUM) | __SHIFTIN(excepts, FPCR_ESUM));
	return __SHIFTOUT(__fpcr, FPCR_ESUM);
}

int
fedisableexcept(int excepts)
{
	const uint32_t __fpcr = reg_fpcr_read();
	reg_fpcr_write(__fpcr & ~__SHIFTIN(excepts, FPCR_ESUM));
	return __SHIFTOUT(__fpcr, FPCR_ESUM);
}

int
fegetexcept(void)
{
	const uint32_t __fpcr = reg_fpcr_read();
	return __SHIFTOUT(__fpcr, FPCR_ESUM);
}
