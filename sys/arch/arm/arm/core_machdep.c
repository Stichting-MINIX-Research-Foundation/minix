/*	$NetBSD: core_machdep.c,v 1.8 2015/04/27 06:54:12 skrll Exp $	*/

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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>

__KERNEL_RCSID(0, "$NetBSD: core_machdep.c,v 1.8 2015/04/27 06:54:12 skrll Exp $");

#ifdef _KERNEL_OPT
#include "opt_execfmt.h"
#include "opt_compat_netbsd32.h"
#else
#define EXEC_ELF32 1
#endif

#include <sys/core.h>
#include <sys/exec.h>
#include <sys/ptrace.h>
#include <sys/signalvar.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/vnode.h>

#include <sys/exec_aout.h>	/* for MID_* */

#include <arm/locore.h>

#ifdef EXEC_ELF32
#include <sys/exec_elf.h>
#ifdef COMPAT_NETBSD32
#include <compat/netbsd32/netbsd32_exec.h>
#endif
#endif

#include <machine/reg.h>


/*
 * Dump the machine specific segment at the start of a core dump.
 */
int
cpu_coredump(struct lwp *l, struct coredump_iostate *iocookie,
    struct core *chdr)
{
	int error;
	struct {
		struct reg regs;
		struct fpreg fpregs;
	} cpustate;
	struct coreseg cseg;

	if (iocookie == NULL) {
		CORE_SETMAGIC(*chdr, COREMAGIC, MID_MACHINE, 0);
		chdr->c_hdrsize = ALIGN(sizeof(*chdr));
		chdr->c_seghdrsize = ALIGN(sizeof(cseg));
		chdr->c_cpusize = sizeof(cpustate);
		chdr->c_nseg++;
		return 0;
	}

	/* Save integer registers. */
	error = process_read_regs(l, &cpustate.regs);
	if (error)
		return error;
	/* Save floating point registers. */
	error = process_read_fpregs(l, &cpustate.fpregs, NULL);
	if (error)
		return error;

	CORE_SETMAGIC(cseg, CORESEGMAGIC, MID_MACHINE, CORE_CPU);
	cseg.c_addr = 0;
	cseg.c_size = chdr->c_cpusize;

	error = coredump_write(iocookie, UIO_SYSSPACE,
	    &cseg, chdr->c_seghdrsize);
	if (error)
		return error;

	return coredump_write(iocookie, UIO_SYSSPACE,
	    &cpustate, sizeof(cpustate));
}

#ifdef EXEC_ELF32
void
arm_netbsd_elf32_coredump_setup(struct lwp *l, void *arg)
{
#if defined(__ARMEB__) || defined(__ARM_EABI__) || defined(COMPAT_NETBSD32)
	Elf_Ehdr * const eh = arg;
#if defined(__ARM_EABI__) || defined(COMPAT_NETBSD32)
	struct proc * const p = l->l_proc;

#ifdef __ARM_EABI__
	if (p->p_emul == &emul_netbsd) {
		eh->e_flags |= EF_ARM_EABI_VER5;
	}
#elif defined(COMPAT_NETBSD32)
	if (p->p_emul == &emul_netbsd32) {
		eh->e_flags |= EF_ARM_EABI_VER5;
	}
#endif
#endif /* __ARM_EABI__ || COMPAT_NETBSD32 */
#ifdef __ARMEB__
        if (CPU_IS_ARMV7_P()
	    || (CPU_IS_ARMV6_P()
		&& (armreg_sctlr_read() & CPU_CONTROL_BEND_ENABLE) == 0)) {
		eh->e_flags |= EF_ARM_BE8;
	}
#endif
#endif
}
#endif
