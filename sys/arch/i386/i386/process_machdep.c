/*	$NetBSD: process_machdep.c,v 1.85 2014/02/19 21:23:01 dsl Exp $	*/

/*-
 * Copyright (c) 1998, 2000, 2001, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum; by Jason R. Thorpe of Wasabi Systems, Inc.
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
 * This file may seem a bit stylized, but that so that it's easier to port.
 * Functions to be implemented here are:
 *
 * process_read_regs(proc, regs)
 *	Get the current user-visible register set from the process
 *	and copy it into the regs structure (<machine/reg.h>).
 *	The process is stopped at the time read_regs is called.
 *
 * process_write_regs(proc, regs)
 *	Update the current register set from the passed in regs
 *	structure.  Take care to avoid clobbering special CPU
 *	registers or privileged bits in the PSL.
 *	The process is stopped at the time write_regs is called.
 *
 * process_sstep(proc)
 *	Arrange for the process to trap after executing a single instruction.
 *
 * process_set_pc(proc)
 *	Set the process's program counter.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: process_machdep.c,v 1.85 2014/02/19 21:23:01 dsl Exp $");

#include "opt_vm86.h"
#include "opt_ptrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/ptrace.h>

#include <uvm/uvm_extern.h>

#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/segments.h>

#include <x86/fpu.h>

#ifdef VM86
#include <machine/vm86.h>
#endif

static inline struct trapframe *
process_frame(struct lwp *l)
{

	return (l->l_md.md_regs);
}

int
process_read_regs(struct lwp *l, struct reg *regs)
{
	struct trapframe *tf = process_frame(l);

#ifdef VM86
	if (tf->tf_eflags & PSL_VM) {
		regs->r_gs = tf->tf_vm86_gs;
		regs->r_fs = tf->tf_vm86_fs;
		regs->r_es = tf->tf_vm86_es;
		regs->r_ds = tf->tf_vm86_ds;
		regs->r_eflags = get_vflags(l);
	} else
#endif
	{
		regs->r_gs = tf->tf_gs & 0xffff;
		regs->r_fs = tf->tf_fs & 0xffff;
		regs->r_es = tf->tf_es & 0xffff;
		regs->r_ds = tf->tf_ds & 0xffff;
		regs->r_eflags = tf->tf_eflags;
	}
	regs->r_edi = tf->tf_edi;
	regs->r_esi = tf->tf_esi;
	regs->r_ebp = tf->tf_ebp;
	regs->r_ebx = tf->tf_ebx;
	regs->r_edx = tf->tf_edx;
	regs->r_ecx = tf->tf_ecx;
	regs->r_eax = tf->tf_eax;
	regs->r_eip = tf->tf_eip;
	regs->r_cs = tf->tf_cs & 0xffff;
	regs->r_esp = tf->tf_esp;
	regs->r_ss = tf->tf_ss & 0xffff;

	return (0);
}

int
process_read_fpregs(struct lwp *l, struct fpreg *regs, size_t *sz)
{

	__CTASSERT(sizeof *regs == sizeof (struct save87));
	process_read_fpregs_s87(l, (struct save87 *)regs);
	return 0;
}

#ifdef PTRACE
int
process_write_regs(struct lwp *l, const struct reg *regs)
{
	struct trapframe *tf = process_frame(l);

#ifdef VM86
	if (regs->r_eflags & PSL_VM) {
		void syscall_vm86(struct trapframe *);

		tf->tf_vm86_gs = regs->r_gs;
		tf->tf_vm86_fs = regs->r_fs;
		tf->tf_vm86_es = regs->r_es;
		tf->tf_vm86_ds = regs->r_ds;
		set_vflags(l, regs->r_eflags);
		/*
		 * Make sure that attempts at system calls from vm86
		 * mode die horribly.
		 */
		l->l_proc->p_md.md_syscall = syscall_vm86;
	} else
#endif
	{
		/*
		 * Check for security violations.
		 */
		if (((regs->r_eflags ^ tf->tf_eflags) & PSL_USERSTATIC) != 0 ||
		    !USERMODE(regs->r_cs, regs->r_eflags))
			return (EINVAL);

		tf->tf_gs = regs->r_gs;
		tf->tf_fs = regs->r_fs;
		tf->tf_es = regs->r_es;
		tf->tf_ds = regs->r_ds;
#ifdef VM86
		/* Restore normal syscall handler */
		if (tf->tf_eflags & PSL_VM)
			(*l->l_proc->p_emul->e_syscall_intern)(l->l_proc);
#endif
		tf->tf_eflags = regs->r_eflags;
	}
	tf->tf_edi = regs->r_edi;
	tf->tf_esi = regs->r_esi;
	tf->tf_ebp = regs->r_ebp;
	tf->tf_ebx = regs->r_ebx;
	tf->tf_edx = regs->r_edx;
	tf->tf_ecx = regs->r_ecx;
	tf->tf_eax = regs->r_eax;
	tf->tf_eip = regs->r_eip;
	tf->tf_cs = regs->r_cs;
	tf->tf_esp = regs->r_esp;
	tf->tf_ss = regs->r_ss;

	return (0);
}

int
process_write_fpregs(struct lwp *l, const struct fpreg *regs, size_t sz)
{

	__CTASSERT(sizeof *regs == sizeof (struct save87));
	process_write_fpregs_s87(l, (const struct save87 *)regs);
	return 0;
}


int
process_sstep(struct lwp *l, int sstep)
{
	struct trapframe *tf = process_frame(l);

	if (sstep)
		tf->tf_eflags |= PSL_T;
	else
		tf->tf_eflags &= ~PSL_T;
	
	return (0);
}

int
process_set_pc(struct lwp *l, void *addr)
{
	struct trapframe *tf = process_frame(l);

	tf->tf_eip = (int)addr;

	return (0);
}

#ifdef __HAVE_PTRACE_MACHDEP
static int
process_machdep_read_xmmregs(struct lwp *l, struct xmmregs *regs)
{

	__CTASSERT(sizeof *regs == sizeof (struct fxsave));
	process_read_fpregs_xmm(l, (struct fxsave *)regs);
	return 0;
}

static int
process_machdep_write_xmmregs(struct lwp *l, struct xmmregs *regs)
{

	__CTASSERT(sizeof *regs == sizeof (struct fxsave));
	process_write_fpregs_xmm(l, (const struct fxsave *)regs);
	return 0;
}

int
ptrace_machdep_dorequest(
    struct lwp *l,
    struct lwp *lt,
    int req,
    void *addr,
    int data
)
{
	struct uio uio;
	struct iovec iov;
	int write = 0;

	switch (req) {
	case PT_SETXMMREGS:
		write = 1;

	case PT_GETXMMREGS:
		/* write = 0 done above. */
		if (!process_machdep_validxmmregs(lt->l_proc))
			return (EINVAL);
		else {
			struct vmspace *vm;
			int error;

			error = proc_vmspace_getref(l->l_proc, &vm);
			if (error) {
				return error;
			}
			iov.iov_base = addr;
			iov.iov_len = sizeof(struct xmmregs);
			uio.uio_iov = &iov;
			uio.uio_iovcnt = 1;
			uio.uio_offset = 0;
			uio.uio_resid = sizeof(struct xmmregs);
			uio.uio_rw = write ? UIO_WRITE : UIO_READ;
			uio.uio_vmspace = vm;
			error = process_machdep_doxmmregs(l, lt, &uio);
			uvmspace_free(vm);
			return error;
		}
	}

#ifdef DIAGNOSTIC
	panic("ptrace_machdep: impossible");
#endif

	return (0);
}

/*
 * The following functions are used by both ptrace(2) and procfs.
 */

int
process_machdep_doxmmregs(struct lwp *curl, struct lwp *l, struct uio *uio)
	/* curl:		 tracer */
	/* l:			 traced */
{
	int error;
	struct xmmregs r;
	char *kv;
	int kl;

	kl = sizeof(r);
	kv = (char *) &r;

	kv += uio->uio_offset;
	kl -= uio->uio_offset;
	if (kl > uio->uio_resid)
		kl = uio->uio_resid;

	if (kl < 0)
		error = EINVAL;
	else
		error = process_machdep_read_xmmregs(l, &r);
	if (error == 0)
		error = uiomove(kv, kl, uio);
	if (error == 0 && uio->uio_rw == UIO_WRITE) {
		if (l->l_proc->p_stat != SSTOP)
			error = EBUSY;
		else
			error = process_machdep_write_xmmregs(l, &r);
	}

	uio->uio_offset = 0;
	return (error);
}

int
process_machdep_validxmmregs(struct proc *p)
{

	if (p->p_flag & PK_SYSTEM)
		return (0);

	return (i386_use_fxsave);
}
#endif /* __HAVE_PTRACE_MACHDEP */
#endif /* PTRACE */
