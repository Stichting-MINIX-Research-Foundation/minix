/*	$NetBSD: syscall.c,v 1.60 2014/08/13 21:41:32 matt Exp $	*/

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
 * syscall entry handling
 *
 * Created      : 09/11/94
 */

#include <sys/param.h>

__KERNEL_RCSID(0, "$NetBSD: syscall.c,v 1.60 2014/08/13 21:41:32 matt Exp $");

#include <sys/cpu.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/reboot.h>
#include <sys/signalvar.h>
#include <sys/syscall.h>
#include <sys/syscallvar.h>
#include <sys/systm.h>
#include <sys/ktrace.h>

#include <uvm/uvm_extern.h>

#include <machine/frame.h>
#include <arm/swi.h>
#include <arm/locore.h>

#ifdef acorn26
#include <machine/machdep.h>
#endif

void
swi_handler(trapframe_t *tf)
{
	lwp_t * const l = curlwp;
	uint32_t insn;

	/*
	 * Enable interrupts if they were enabled before the exception.
	 * Since all syscalls *should* come from user mode it will always
	 * be safe to enable them, but check anyway. 
	 */
#ifdef acorn26
	if ((tf->tf_r15 & R15_IRQ_DISABLE) == 0)
		int_on();
#else
	KASSERT(VALID_R15_PSR(tf->tf_pc, tf->tf_spsr));
	restore_interrupts(tf->tf_spsr & IF32_bits);
#endif

#ifdef acorn26
	tf->tf_pc += INSN_SIZE;
#endif

#ifndef THUMB_CODE
	/*
	 * Make sure the program counter is correctly aligned so we
	 * don't take an alignment fault trying to read the opcode.
	 */
	if (__predict_false(((tf->tf_pc - INSN_SIZE) & 3) != 0)) {
		ksiginfo_t ksi;
		/* Give the user an illegal instruction signal. */
		KSI_INIT_TRAP(&ksi);
		ksi.ksi_signo = SIGILL;
		ksi.ksi_code = ILL_ILLOPC;
		ksi.ksi_addr = (uint32_t *)(intptr_t) (tf->tf_pc-INSN_SIZE);
#if 0
		/* maybe one day we'll do emulations */
		(*l->l_proc->p_emul->e_trapsignal)(l, &ksi);
#else
		trapsignal(l, &ksi);
#endif
		userret(l);
		return;
	}
#endif

#ifdef THUMB_CODE
	if (tf->tf_spsr & PSR_T_bit) {
		insn = 0xef000000 | SWI_OS_NETBSD | tf->tf_r0;
		tf->tf_r0 = tf->tf_ip;
	}
	else
#endif
	{
#ifdef __PROG32
		insn = read_insn(tf->tf_pc - INSN_SIZE, true);
#else
		insn = read_insn((tf->tf_r15 & R15_PC) - INSN_SIZE, true);
#endif
	}

	lwp_settrapframe(l, tf);

#ifdef CPU_ARM7
	/*
	 * This code is only needed if we are including support for the ARM7
	 * core. Other CPUs do not need it but it does not hurt.
	 */

	/*
	 * ARM700/ARM710 match sticks and sellotape job ...
	 *
	 * I know this affects GPS/VLSI ARM700/ARM710 + various ARM7500.
	 *
	 * On occasion data aborts are mishandled and end up calling
	 * the swi vector.
	 *
	 * If the instruction that caused the exception is not a SWI
	 * then we hit the bug.
	 */
	if ((insn & 0x0f000000) != 0x0f000000) {
		tf->tf_pc -= INSN_SIZE;
		curcpu()->ci_arm700bugcount.ev_count++;
		userret(l);
		return;
	}
#endif	/* CPU_ARM7 */

	curcpu()->ci_data.cpu_nsyscall++;

	LWP_CACHE_CREDS(l, l->l_proc);
	(*l->l_proc->p_md.md_syscall)(tf, l, insn);
}

void syscall(struct trapframe *, lwp_t *, uint32_t);

void
syscall_intern(struct proc *p)
{
	p->p_md.md_syscall = syscall;
}

void
syscall(struct trapframe *tf, lwp_t *l, uint32_t insn)
{
	struct proc * const p = l->l_proc;
	const struct sysent *callp;
	int error;
	u_int nargs;
	register_t *args;
	uint64_t copyargs64[sizeof(register_t)*(2+SYS_MAXSYSARGS+1)/sizeof(uint64_t)];
	register_t *copyargs = (register_t *)copyargs64;
	register_t rval[2];
	ksiginfo_t ksi;
	const uint32_t os_mask = insn & SWI_OS_MASK;
	uint32_t code = insn & 0x000fffff;

	/* test new official and old unofficial NetBSD ranges */
	if (__predict_false(os_mask != SWI_OS_NETBSD)
	    && __predict_false(os_mask != 0)) {
		if (os_mask == SWI_OS_ARM
		    && (code == SWI_IMB || code == SWI_IMBrange)) {
			userret(l);
			return;
		}

		/* Undefined so illegal instruction */
		KSI_INIT_TRAP(&ksi);
		ksi.ksi_signo = SIGILL;
		ksi.ksi_code = ILL_ILLTRP;
#ifdef THUMB_CODE
		if (tf->tf_spsr & PSR_T_bit) 
			ksi.ksi_addr = (void *)(tf->tf_pc - THUMB_INSN_SIZE);
		else
#endif
			ksi.ksi_addr = (void *)(tf->tf_pc - INSN_SIZE);
		ksi.ksi_trap = insn;
		trapsignal(l, &ksi);
		userret(l);
		return;
	}

	code &= (SYS_NSYSENT - 1);
	callp = p->p_emul->e_sysent + code;
	nargs = callp->sy_narg;
	if (nargs > 4) {
		args = copyargs;
		memcpy(args, &tf->tf_r0, 4 * sizeof(register_t));
		error = copyin((void *)tf->tf_usr_sp, args + 4,
		    (nargs - 4) * sizeof(register_t));
		if (error)
			goto bad;
	} else {
		args = &tf->tf_r0;
	}

	error = sy_invoke(callp, l, args, rval, code);

	switch (error) {
	case 0:
		tf->tf_r0 = rval[0];
		tf->tf_r1 = rval[1];

#ifdef __PROG32
		tf->tf_spsr &= ~PSR_C_bit;	/* carry bit */
#else
		tf->tf_r15 &= ~R15_FLAG_C;	/* carry bit */
#endif
		break;

	case ERESTART:
		/*
		 * Reconstruct the pc to point at the swi.
		 */
#ifdef THUMB_CODE
		if (tf->tf_spsr & PSR_T_bit)
			tf->tf_pc -= THUMB_INSN_SIZE;
		else
#endif
			tf->tf_pc -= INSN_SIZE;
		break;

	case EJUSTRETURN:
		/* nothing to do */
		break;

	default:
	bad:
		tf->tf_r0 = error;
#ifdef __PROG32
		tf->tf_spsr |= PSR_C_bit;	/* carry bit */
#else
		tf->tf_r15 |= R15_FLAG_C;	/* carry bit */
#endif
		break;
	}

	userret(l);
}

void
child_return(void *arg)
{
	lwp_t * const l = arg;
	struct trapframe * const tf = lwp_trapframe(l);

	tf->tf_r0 = 0;
#ifdef __PROG32
	tf->tf_spsr &= ~PSR_C_bit;	/* carry bit */
#else
	tf->tf_r15 &= ~R15_FLAG_C;	/* carry bit */
#endif

	userret(l);
	ktrsysret(SYS_fork, 0, 0);
}

/*
 * Process the tail end of a posix_spawn() for the child.
 */
void
cpu_spawn_return(struct lwp *l)
{

	userret(l);
}

