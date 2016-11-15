/*	$NetBSD: uvm_unix.c,v 1.45 2014/09/05 05:36:49 matt Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * Copyright (c) 1991, 1993 The Regents of the University of California.
 * Copyright (c) 1988 University of Utah.
 *
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: vm_unix.c 1.1 89/11/07$
 *      @(#)vm_unix.c   8.1 (Berkeley) 6/11/93
 * from: Id: uvm_unix.c,v 1.1.2.2 1997/08/25 18:52:30 chuck Exp
 */

/*
 * uvm_unix.c: traditional sbrk/grow interface to vm.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_unix.c,v 1.45 2014/09/05 05:36:49 matt Exp $");

#include "opt_pax.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#ifdef PAX_MPROTECT
#include <sys/pax.h>
#endif /* PAX_MPROTECT */

#include <uvm/uvm.h>

/*
 * sys_obreak: set break
 */

int
sys_obreak(struct lwp *l, const struct sys_obreak_args *uap, register_t *retval)
{
	/* {
		syscallarg(char *) nsize;
	} */
	struct proc *p = l->l_proc;
	struct vmspace *vm = p->p_vmspace;
	vaddr_t nbreak, obreak;
	int error;

	mutex_enter(&p->p_auxlock);
	obreak = (vaddr_t)vm->vm_daddr;
	nbreak = round_page((vaddr_t)SCARG(uap, nsize));
	if (nbreak == 0
	    || ((nbreak - obreak) > p->p_rlimit[RLIMIT_DATA].rlim_cur
		&& nbreak > obreak)) {
		mutex_exit(&p->p_auxlock);
		return (ENOMEM);
	}

	obreak = round_page(obreak + ptoa(vm->vm_dsize));

	if (nbreak == obreak) {
		mutex_exit(&p->p_auxlock);
		return (0);
	}

	/*
	 * grow or shrink?
	 */

	if (nbreak > obreak) {
		vm_prot_t prot = UVM_PROT_READ | UVM_PROT_WRITE;
		vm_prot_t maxprot = UVM_PROT_ALL;

#ifdef PAX_MPROTECT
		pax_mprotect(l, &prot, &maxprot);
#endif /* PAX_MPROTECT */

		error = uvm_map(&vm->vm_map, &obreak, nbreak - obreak, NULL,
		    UVM_UNKNOWN_OFFSET, 0,
		    UVM_MAPFLAG(prot, maxprot,
				UVM_INH_COPY,
				UVM_ADV_NORMAL, UVM_FLAG_AMAPPAD|UVM_FLAG_FIXED|
				UVM_FLAG_OVERLAY|UVM_FLAG_COPYONW));
		if (error) {
#ifdef DEBUG
			uprintf("sbrk: grow %#"PRIxVADDR" failed, error = %d\n",
			    nbreak - obreak, error);
#endif
			mutex_exit(&p->p_auxlock);
			return (error);
		}
		vm->vm_dsize += atop(nbreak - obreak);
	} else {
		uvm_deallocate(&vm->vm_map, nbreak, obreak - nbreak);
		vm->vm_dsize -= atop(obreak - nbreak);
	}
	mutex_exit(&p->p_auxlock);

	return (0);
}

/*
 * uvm_grow: enlarge the "stack segment" to include sp.
 */

int
uvm_grow(struct proc *p, vaddr_t sp)
{
	struct vmspace *vm = p->p_vmspace;
	vsize_t nss;

	/*
	 * For user defined stacks (from sendsig).
	 */
#ifdef __MACHINE_STACK_GROWS_UP
	if (sp < (vaddr_t)vm->vm_minsaddr)
#else
	if (sp < (vaddr_t)vm->vm_maxsaddr)
#endif
		return (0);

	/*
	 * For common case of already allocated (from trap).
	 */
#ifdef __MACHINE_STACK_GROWS_UP
	if (sp < USRSTACK + ctob(vm->vm_ssize))
#else
	if (sp >= USRSTACK - ctob(vm->vm_ssize))
#endif
		return (1);

	/*
	 * Really need to check vs limit and increment stack size if ok.
	 */
#ifdef __MACHINE_STACK_GROWS_UP
	nss = btoc(sp - USRSTACK);
#else
	nss = btoc(USRSTACK - sp);
#endif
	if (nss > btoc(p->p_rlimit[RLIMIT_STACK].rlim_cur))
		return (0);
	vm->vm_ssize = nss;
	return (1);
}

/*
 * sys_oadvise: old advice system call
 */

/* ARGSUSED */
int
sys_ovadvise(struct lwp *l, const struct sys_ovadvise_args *uap, register_t *retval)
{
#if 0
	/* {
		syscallarg(int) anom;
	} */
#endif

	return (EINVAL);
}
