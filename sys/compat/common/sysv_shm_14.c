/*	$NetBSD: sysv_shm_14.c,v 1.17 2011/01/19 10:21:16 tsutsui Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
__KERNEL_RCSID(0, "$NetBSD: sysv_shm_14.c,v 1.17 2011/01/19 10:21:16 tsutsui Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/proc.h>
#include <sys/shm.h>

#ifndef SYSVSHM
#define	SYSVSHM
#endif

#include <sys/syscallargs.h>

#include <compat/sys/shm.h>


int
compat_14_sys_shmctl(struct lwp *l, const struct compat_14_sys_shmctl_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) shmid;
		syscallarg(int) cmd;
		syscallarg(struct shmid_ds14 *) buf;
	} */
	struct shmid_ds shmbuf;
	struct shmid_ds14 oshmbuf;
	int cmd, error;

	cmd = SCARG(uap, cmd);

	if (cmd == IPC_SET) {
		error = copyin(SCARG(uap, buf), &oshmbuf, sizeof(oshmbuf));
		if (error)
			return (error);
		__shmid_ds14_to_native(&oshmbuf, &shmbuf);
	}

	error = shmctl1(l, SCARG(uap, shmid), cmd,
	    (cmd == IPC_SET || cmd == IPC_STAT) ? &shmbuf : NULL);

	if (error == 0 && cmd == IPC_STAT) {
		__native_to_shmid_ds14(&shmbuf, &oshmbuf);
		error = copyout(&oshmbuf, SCARG(uap, buf), sizeof(oshmbuf));
	}

	return (error);
}
