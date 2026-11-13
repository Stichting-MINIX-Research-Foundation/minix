/*	$NetBSD: process_machdep.c,v 1.49 2020/10/19 17:47:37 christos Exp $	*/

/*
 * Copyright (c) 1998, 2000 The NetBSD Foundation, Inc.
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
 * process_read_fpregs(proc, regs, sz)
 *	Get the current user-visible register set from the process
 *	and copy it into the regs structure (<machine/reg.h>).
 *	The process is stopped at the time read_fpregs is called.
 *
 * process_write_fpregs(proc, regs, sz)
 *	Update the current register set from the passed in regs
 *	structure.  Take care to avoid clobbering special CPU
 *	registers or privileged bits in the PSL.
 *	The process is stopped at the time write_fpregs is called.
 *
 * process_read_dbregs(proc, regs, sz)
 *	Get the current user-visible register set from the process
 *	and copy it into the regs structure (<machine/reg.h>).
 *	The process is stopped at the time read_dbregs is called.
 *
 * process_write_dbregs(proc, regs, sz)
 *	Update the current register set from the passed in regs
 *	structure.  Take care to avoid clobbering special CPU
 *	registers or privileged bits in the PSL.
 *	The process is stopped at the time write_dbregs is called.
 *
 * process_sstep(proc)
 *	Arrange for the process to trap after executing a single instruction.
 *
 * process_set_pc(proc)
 *	Set the process's program counter.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: process_machdep.c,v 1.49 2020/10/19 17:47:37 christos Exp $");

#ifdef _KERNEL_OPT
#include "opt_xen.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/compat_stub.h>

#include <uvm/uvm_extern.h>

#include <compat/netbsd32/netbsd32.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/segments.h>
#include <x86/dbregs.h>
#include <x86/fpu.h>

struct netbsd32_process_doxmmregs_hook_t netbsd32_process_doxmmregs_hook;

static inline struct trapframe *process_frame(struct lwp *);

static inline struct trapframe *
process_frame(struct lwp *l)
{

	return l->l_md.md_regs;
}

int
process_read_regs(struct lwp *l, struct reg *regp)
{
	struct trapframe *tf = process_frame(l);
	long *regs = regp->regs;
	const bool pk32 = (l->l_proc->p_flag & PK_32) != 0;

	regs[_REG_RDI] = tf->tf_rdi;
	regs[_REG_RSI] = tf->tf_rsi;
	regs[_REG_RDX] = tf->tf_rdx;
	regs[_REG_R10] = tf->tf_r10;
	regs[_REG_R8]  = tf->tf_r8;
	regs[_REG_R9]  = tf->tf_r9;
	/* argX not touched */
	regs[_REG_RCX] = tf->tf_rcx;
	regs[_REG_R11] = tf->tf_r11;
	regs[_REG_R12] = tf->tf_r12;
	regs[_REG_R13] = tf->tf_r13;
	regs[_REG_R14] = tf->tf_r14;
	regs[_REG_R15] = tf->tf_r15;
	regs[_REG_RBP] = tf->tf_rbp;
	regs[_REG_RBX] = tf->tf_rbx;
	regs[_REG_RAX] = tf->tf_rax;
	if (pk32) {
		regs[_REG_GS] = tf->tf_gs & 0xffff;
		regs[_REG_FS] = tf->tf_fs & 0xffff;
		regs[_REG_ES] = tf->tf_es & 0xffff;
		regs[_REG_DS] = tf->tf_ds & 0xffff;
		regs[_REG_CS] = tf->tf_cs & 0xffff;
		regs[_REG_SS] = tf->tf_ss & 0xffff;
	} else {
		regs[_REG_GS] = 0;
		regs[_REG_FS] = 0;
		regs[_REG_ES] = GSEL(GUDATA_SEL, SEL_UPL);
		regs[_REG_DS] = GSEL(GUDATA_SEL, SEL_UPL);
		regs[_REG_CS] = LSEL(LUCODE_SEL, SEL_UPL);
		regs[_REG_SS] = LSEL(LUDATA_SEL, SEL_UPL);
	}
	regs[_REG_TRAPNO] = tf->tf_trapno;
	regs[_REG_ERR] = tf->tf_err;
	regs[_REG_RIP] = tf->tf_rip;
	regs[_REG_RFLAGS] = tf->tf_rflags;
	regs[_REG_RSP] = tf->tf_rsp;

	return 0;
}

int
process_read_fpregs(struct lwp *l, struct fpreg *regs, size_t *sz)
{

	process_read_fpregs_xmm(l, &regs->fxstate);

	return 0;
}

int
process_read_dbregs(struct lwp *l, struct dbreg *regs, size_t *sz)
{

	x86_dbregs_read(l, regs);

	return 0;
}

int
process_write_regs(struct lwp *l, const struct reg *regp)
{
	struct trapframe *tf = process_frame(l);
	int error;
	const long *regs = regp->regs;
	const bool pk32 = (l->l_proc->p_flag & PK_32) != 0;

	/*
	 * Check for security violations. Note that struct regs is compatible
	 * with the __gregs array in mcontext_t.
	 */
	if (pk32) {
		MODULE_HOOK_CALL(netbsd32_reg_validate_hook, (l, regp), EINVAL,
		    error);
	} else {
		error = cpu_mcontext_validate(l, (const mcontext_t *)regs);
	}
	if (error != 0)
		return error;

	tf->tf_rdi  = regs[_REG_RDI];
	tf->tf_rsi  = regs[_REG_RSI];
	tf->tf_rdx  = regs[_REG_RDX];
	tf->tf_r10  = regs[_REG_R10];
	tf->tf_r8   = regs[_REG_R8];
	tf->tf_r9   = regs[_REG_R9];
	/* argX not touched */
	tf->tf_rcx  = regs[_REG_RCX];
	tf->tf_r11  = regs[_REG_R11];
	tf->tf_r12  = regs[_REG_R12];
	tf->tf_r13  = regs[_REG_R13];
	tf->tf_r14  = regs[_REG_R14];
	tf->tf_r15  = regs[_REG_R15];
	tf->tf_rbp  = regs[_REG_RBP];
	tf->tf_rbx  = regs[_REG_RBX];
	tf->tf_rax  = regs[_REG_RAX];
	if (pk32) {
		tf->tf_gs = regs[_REG_GS] & 0xffff;
		tf->tf_fs = regs[_REG_FS] & 0xffff;
		tf->tf_es = regs[_REG_ES] & 0xffff;
		tf->tf_ds = regs[_REG_DS] & 0xffff;
		tf->tf_cs = regs[_REG_CS] & 0xffff;
		tf->tf_ss = regs[_REG_SS] & 0xffff;
	} else {
		tf->tf_gs = 0;
		tf->tf_fs = 0;
		tf->tf_es = GSEL(GUDATA_SEL, SEL_UPL);
		tf->tf_ds = GSEL(GUDATA_SEL, SEL_UPL);
		tf->tf_cs = LSEL(LUCODE_SEL, SEL_UPL);
		tf->tf_ss = LSEL(LUDATA_SEL, SEL_UPL);
	}
	/* trapno, err not touched */
	tf->tf_rip  = regs[_REG_RIP];
	tf->tf_rflags = regs[_REG_RFLAGS];
	tf->tf_rsp  = regs[_REG_RSP];

	return 0;
}

int
process_write_fpregs(struct lwp *l, const struct fpreg *regs, size_t sz)
{

	process_write_fpregs_xmm(l, &regs->fxstate);
	return 0;
}

int
process_write_dbregs(struct lwp *l, const struct dbreg *regs, size_t sz)
{
	int error;

	/*
	 * Check for security violations.
	 */
	error = x86_dbregs_validate(regs);
	if (error != 0)
		return error;

	x86_dbregs_write(l, regs);

	return 0;
}

int
process_sstep(struct lwp *l, int sstep)
{
	struct trapframe *tf = process_frame(l);

	if (sstep)
		tf->tf_rflags |= PSL_T;
	else
		tf->tf_rflags &= ~PSL_T;
	
	return 0;
}

int
process_set_pc(struct lwp *l, void *addr)
{
	struct trapframe *tf = process_frame(l);
	const bool pk32 = (l->l_proc->p_flag & PK_32) != 0;
	const uint64_t rip = (uint64_t)addr;

	if (rip >= (pk32 ? VM_MAXUSER_ADDRESS32 : VM_MAXUSER_ADDRESS))
		return EINVAL;
	tf->tf_rip = rip;

	return 0;
}

#ifdef __HAVE_PTRACE_MACHDEP
static int
process_machdep_read_xstate(struct lwp *l, struct xstate *regs)
{
	return process_read_xstate(l, regs);
}

static int
process_machdep_write_xstate(struct lwp *l, const struct xstate *regs)
{
	int error;

	/*
	 * Check for security violations.
	 */
	error = process_verify_xstate(regs);
	if (error != 0)
		return error;

	return process_write_xstate(l, regs);
}

int
ptrace_machdep_dorequest(
    struct lwp *l,
    struct lwp **lt,
    int req,
    void *addr,
    int data
)
{
	struct uio uio;
	struct iovec iov;
	struct vmspace *vm;
	int error;
	bool write = false;

	switch (req) {
	case PT_SETXSTATE:
		write = true;

		/* FALLTHROUGH */
	case PT_GETXSTATE:
		/* write = false done above. */
		if ((error = ptrace_update_lwp((*lt)->l_proc, lt, data)) != 0)
			return error;
		if (!process_machdep_validfpu((*lt)->l_proc))
			return EINVAL;
		if (__predict_false(l->l_proc->p_flag & PK_32)) {
			struct netbsd32_iovec user_iov;
			if ((error = copyin(addr, &user_iov, sizeof(user_iov)))
			    != 0)
				return error;

			iov.iov_base = NETBSD32PTR64(user_iov.iov_base);
			iov.iov_len = user_iov.iov_len;
		} else {
			struct iovec user_iov;
			if ((error = copyin(addr, &user_iov, sizeof(user_iov)))
			    != 0)
				return error;

			iov.iov_base = user_iov.iov_base;
			iov.iov_len = user_iov.iov_len;
		}

		error = proc_vmspace_getref(l->l_proc, &vm);
		if (error)
			return error;
		if (iov.iov_len > sizeof(struct xstate))
			iov.iov_len = sizeof(struct xstate);
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_offset = 0;
		uio.uio_resid = iov.iov_len;
		uio.uio_rw = write ? UIO_WRITE : UIO_READ;
		uio.uio_vmspace = vm;
		error = process_machdep_doxstate(l, *lt, &uio);
		uvmspace_free(vm);
		return error;

	case PT_SETXMMREGS:		/* only for COMPAT_NETBSD32 */
		write = true;

		/* FALLTHROUGH */
	case PT_GETXMMREGS:		/* only for COMPAT_NETBSD32 */
		/* write = false done above. */
		if ((error = ptrace_update_lwp((*lt)->l_proc, lt, data)) != 0)
			return error;
		MODULE_HOOK_CALL(netbsd32_process_doxmmregs_hook,
		    (l, *lt, addr, write), EINVAL, error);
		return error;
	}

#ifdef DIAGNOSTIC
	panic("ptrace_machdep: impossible");
#endif

	return 0;
}

/*
 * The following functions are used by both ptrace(2) and procfs.
 */

int
process_machdep_doxstate(struct lwp *curl, struct lwp *l, struct uio *uio)
	/* curl:		 tracer */
	/* l:			 traced */
{
	int error;
	struct xstate r;
	char *kv;
	ssize_t kl;

	memset(&r, 0, sizeof(r));
	kl = MIN(uio->uio_iov->iov_len, sizeof(r));
	kv = (char *) &r;

	kv += uio->uio_offset;
	kl -= uio->uio_offset;
	if (kl > uio->uio_resid)
		kl = uio->uio_resid;

	if (kl < 0)
		error = EINVAL;
	else
		error = process_machdep_read_xstate(l, &r);
	if (error == 0)
		error = uiomove(kv, kl, uio);
	if (error == 0 && uio->uio_rw == UIO_WRITE)
		error = process_machdep_write_xstate(l, &r);

	uio->uio_offset = 0;
	return error;
}

int
process_machdep_validfpu(struct proc *p)
{

	if (p->p_flag & PK_SYSTEM)
		return 0;

	return 1;
}
#endif /* __HAVE_PTRACE_MACHDEP */
