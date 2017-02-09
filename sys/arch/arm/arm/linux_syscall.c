/*	$NetBSD: linux_syscall.c,v 1.27 2015/03/07 18:52:46 christos Exp $	*/

/*-
 * Copyright (c) 2000, 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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

/*
 * Copyright (c) 1994-1998 Mark Brinicombe.
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
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * ARMLinux emulation: syscall entry handling
 */

#include <sys/param.h>

__KERNEL_RCSID(0, "$NetBSD: linux_syscall.c,v 1.27 2015/03/07 18:52:46 christos Exp $");

#include <sys/device.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/reboot.h>
#include <sys/signalvar.h>
#include <sys/systm.h>
#include <sys/syscallvar.h>
#include <sys/cpu.h>

#include <uvm/uvm_extern.h>

#include <arm/swi.h>
#include <arm/locore.h>

#include <compat/linux/common/linux_errno.h>
#include <compat/linux/linux_syscall.h>

/* ARMLinux has some system calls of its very own. */
#define LINUX_ARM_NR_BASE	0x9f0000
#define LINUX_EARM_NR_BASE	0x0f0000
#define LINUX_SYS_ARMBASE	0x000180 /* Must agree with syscalls.master */

void linux_syscall_intern(struct proc *);
void linux_syscall_plain(struct trapframe *, struct lwp *, uint32_t);
void linux_syscall_fancy(struct trapframe *, struct lwp *, uint32_t);

void
linux_syscall_intern(struct proc *p)
{

	if (trace_is_enabled(p))
		p->p_md.md_syscall = linux_syscall_fancy;
	else
		p->p_md.md_syscall = linux_syscall_plain;
}

void
linux_syscall_plain(trapframe_t *frame, struct lwp *l, uint32_t insn)
{
	const struct sysent *callp;
	struct proc *p = l->l_proc;
	int code, error;
	register_t *args, rval[2];

	code = insn & 0x00ffffff;
	if (code == 0) {	/* EABI */
		code = frame->tf_r7;
		if (code > LINUX_EARM_NR_BASE)
			code = code - LINUX_EARM_NR_BASE + LINUX_SYS_ARMBASE;
	} else {
		/* Remap ARM-specific syscalls onto the end of the standard range. */
		if (code > LINUX_ARM_NR_BASE)
			code = code - LINUX_ARM_NR_BASE + LINUX_SYS_ARMBASE;
	}
	code &= LINUX_SYS_NSYSENT - 1;

	/* Linux passes all arguments in order in registers, which is nice. */
	args = &frame->tf_r0;
	callp = p->p_emul->e_sysent + code;

	rval[0] = 0;
	rval[1] = 0;
	error = sy_call(callp, l, args, rval);

	switch (error) {
	case 0:
		frame->tf_r0 = rval[0];
		break;

	case ERESTART:
		/* Reconstruct the pc to point at the swi.  */
 		frame->tf_pc -= INSN_SIZE;
		break;

	case EJUSTRETURN:
		/* nothing to do */
		break;

	default:
		error = native_to_linux_errno[error];
		frame->tf_r0 = error;
		break;
	}

	userret(l);
}

void
linux_syscall_fancy(trapframe_t *frame, struct lwp *l, uint32_t insn)
{
	const struct sysent *callp;
	struct proc *p = l->l_proc;
	int code, error;
	register_t *args, rval[2];

	code = insn & 0x00ffffff;
	if (code == 0) {	/* EABI */
		code = frame->tf_r7;
		if (code > LINUX_EARM_NR_BASE)
			code = code - LINUX_EARM_NR_BASE + LINUX_SYS_ARMBASE;
	} else {
		/* Remap ARM-specific syscalls onto the end of the standard range. */
		if (code > LINUX_ARM_NR_BASE)
			code = code - LINUX_ARM_NR_BASE + LINUX_SYS_ARMBASE;
	}
	code &= LINUX_SYS_NSYSENT - 1;

	/* Linux passes all arguments in order in registers, which is nice. */
	args = &frame->tf_r0;
	callp = p->p_emul->e_sysent + code;

	if ((error = trace_enter(code, callp, args)) != 0)
		goto out;

	rval[0] = 0;
	rval[1] = 0;
	error = sy_call(callp, l, args, rval);
out:
	switch (error) {
	case 0:
		frame->tf_r0 = rval[0];
		break;

	case ERESTART:
		/* Reconstruct the pc to point at the swi.  */
 		frame->tf_pc -= INSN_SIZE;
		break;

	case EJUSTRETURN:
		/* nothing to do */
		break;

	default:
		error = native_to_linux_errno[error];
		frame->tf_r0 = error;
		break;
	}

	trace_exit(code, callp, args, rval, error);

	userret(l);
}
