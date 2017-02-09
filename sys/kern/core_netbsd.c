/*	$NetBSD: core_netbsd.c,v 1.22 2014/01/07 07:59:03 dsl Exp $	*/

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
 * from: NetBSD: uvm_unix.c,v 1.25 2001/11/10 07:37:01 lukem Exp
 */

/*
 * core_netbsd.c: Support for the historic NetBSD core file format.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: core_netbsd.c,v 1.22 2014/01/07 07:59:03 dsl Exp $");

#ifdef _KERNEL_OPT
#include "opt_coredump.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/core.h>

#include <uvm/uvm_extern.h>

#ifndef CORENAME
#define	CORENAME(x)	x
#endif
#ifdef COREINC
#include COREINC
#endif

#ifdef COREDUMP

struct coredump_state {
	struct coredump_iostate *iocookie;
	struct CORENAME(core) core;
};

static int	CORENAME(coredump_writesegs_netbsd)(struct uvm_coredump_state *);

int
CORENAME(coredump_netbsd)(struct lwp *l, struct coredump_iostate *iocookie)
{
	struct coredump_state cs;
	struct proc *p = l->l_proc;
	struct vmspace *vm = p->p_vmspace;
	int error;

	cs.iocookie = iocookie;
	cs.core.c_midmag = 0;
	strncpy(cs.core.c_name, p->p_comm, MAXCOMLEN);
	cs.core.c_nseg = 0;
	cs.core.c_signo = p->p_sigctx.ps_signo;
	cs.core.c_ucode = p->p_sigctx.ps_code;
	cs.core.c_cpusize = 0;
	cs.core.c_tsize = (u_long)ctob(vm->vm_tsize);
	cs.core.c_dsize = (u_long)ctob(vm->vm_dsize);
	cs.core.c_ssize = (u_long)round_page(ctob(vm->vm_ssize));

	error = CORENAME(cpu_coredump)(l, NULL, &cs.core);
	if (error)
		return (error);
	cs.core.c_nseg = uvm_coredump_count_segs(p);

	/* First write out the core header. */
	error = coredump_write(iocookie, UIO_SYSSPACE, &cs.core,
	    cs.core.c_hdrsize);
	if (error)
		return (error);

	/* Then the CPU specific stuff */
	error = CORENAME(cpu_coredump)(l, iocookie, &cs.core);
	if (error)
		return (error);

	/* Finally, the address space dump */
	return uvm_coredump_walkmap(p, CORENAME(coredump_writesegs_netbsd),
	    &cs);
}

static int
CORENAME(coredump_writesegs_netbsd)(struct uvm_coredump_state *us)
{
	struct coredump_state *cs = us->cookie;
	struct CORENAME(coreseg) cseg;
	int flag, error;

	if (us->flags & UVM_COREDUMP_STACK)
		flag = CORE_STACK;
	else
		flag = CORE_DATA;

	/*
	 * Set up a new core file segment.
	 */
	CORE_SETMAGIC(cseg, CORESEGMAGIC, CORE_GETMID(cs->core), flag);
	cseg.c_addr = us->start;

	if (us->start == us->realend)
		/* Not really wanted, but counted... */
		cseg.c_size = 0;
	else
		cseg.c_size = us->end - us->start;

	error = coredump_write(cs->iocookie, UIO_SYSSPACE,
	    &cseg, cs->core.c_seghdrsize);
	if (error)
		return (error);

	return coredump_write(cs->iocookie, UIO_USERSPACE,
	    (void *)(vaddr_t)us->start, cseg.c_size);
}

#else	/* COREDUMP */

int
CORENAME(coredump_netbsd)(struct lwp *l, struct coredump_iostate *cookie)
{

	return ENOSYS;
}

#endif	/* COREDUMP */
