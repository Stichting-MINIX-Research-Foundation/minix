/*	$NetBSD: kern_50.c,v 1.1 2014/04/04 18:17:36 njoly Exp $	*/

/*-
 * Copyright (c) 2008, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
__KERNEL_RCSID(0, "$NetBSD: kern_50.c,v 1.1 2014/04/04 18:17:36 njoly Exp $");

#include <sys/param.h>
#include <sys/lwp.h>
#include <sys/proc.h>
#include <sys/syscallargs.h>

#include <compat/sys/resource.h>
#include <compat/sys/time.h>

int
compat_50_sys__lwp_park(struct lwp *l,
    const struct compat_50_sys__lwp_park_args *uap, register_t *retval)
{
	/* {
		syscallarg(const struct timespec50 *)	ts;
		syscallarg(lwpid_t)			unpark;
		syscallarg(const void *)		hint;
		syscallarg(const void *)		unparkhint;
	} */
	struct timespec ts, *tsp;
	struct timespec50 ts50;
	int error;

	if (SCARG(uap, ts) == NULL)
		tsp = NULL;
	else {
		error = copyin(SCARG(uap, ts), &ts50, sizeof(ts50));
		if (error != 0)
			return error;
		timespec50_to_timespec(&ts50, &ts);
		tsp = &ts;
	}

	if (SCARG(uap, unpark) != 0) {
		error = lwp_unpark(SCARG(uap, unpark), SCARG(uap, unparkhint));
		if (error != 0)
			return error;
	}

	return lwp_park(CLOCK_REALTIME, TIMER_ABSTIME, tsp, SCARG(uap, hint));
}

static int
tscopyin(const void *u, void *s, size_t len)
{
	struct timespec50 ts50;
	int error;

	KASSERT(len == sizeof(struct timespec));
	error = copyin(u, &ts50, sizeof(ts50));
	if (error)
		return error;
	timespec50_to_timespec(&ts50, s);
	return 0;
}

static int
tscopyout(const void *s, void *u, size_t len)
{
	struct timespec50 ts50;

	KASSERT(len == sizeof(struct timespec));
	timespec_to_timespec50(s, &ts50);
	return copyout(&ts50, u, sizeof(ts50));
}

int
compat_50_sys___sigtimedwait(struct lwp *l,
    const struct compat_50_sys___sigtimedwait_args *uap, register_t *retval)
{
	int res;

	res = sigtimedwait1(l,
	    (const struct sys_____sigtimedwait50_args *)uap, retval, copyin,
	    copyout, tscopyin, tscopyout);
	if (!res)
		*retval = 0; /* XXX NetBSD<=5 was not POSIX compliant */
	return res;
}

int
compat_50_sys_wait4(struct lwp *l, const struct compat_50_sys_wait4_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int)			pid;
		syscallarg(int *)		status;
		syscallarg(int)			options;
		syscallarg(struct rusage50 *)	rusage;
	} */
	int status, error, pid = SCARG(uap, pid);
	struct rusage50 ru50;
	struct rusage ru;

	error = do_sys_wait(&pid, &status, SCARG(uap, options),
	    SCARG(uap, rusage) != NULL ? &ru : NULL);

	retval[0] = pid;
	if (pid == 0)
		return error;

	if (SCARG(uap, rusage)) {
		rusage_to_rusage50(&ru, &ru50);
		error = copyout(&ru50, SCARG(uap, rusage), sizeof(ru50));
	}

	if (error == 0 && SCARG(uap, status))
		error = copyout(&status, SCARG(uap, status), sizeof(status));

	return error;
}
