/*	$NetBSD: exec_subr.c,v 1.72 2015/09/26 16:12:24 maxv Exp $	*/

/*
 * Copyright (c) 1993, 1994, 1996 Christopher G. Demetriou
 * All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: exec_subr.c,v 1.72 2015/09/26 16:12:24 maxv Exp $");

#include "opt_pax.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kmem.h>
#include <sys/vnode.h>
#include <sys/filedesc.h>
#include <sys/exec.h>
#include <sys/mman.h>
#include <sys/resourcevar.h>
#include <sys/device.h>

#if defined(PAX_ASLR) || defined(PAX_MPROTECT)
#include <sys/pax.h>
#endif /* PAX_ASLR || PAX_MPROTECT */

#include <uvm/uvm_extern.h>

#define	VMCMD_EVCNT_DECL(name)					\
static struct evcnt vmcmd_ev_##name =				\
    EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "vmcmd", #name);	\
EVCNT_ATTACH_STATIC(vmcmd_ev_##name)

#define	VMCMD_EVCNT_INCR(name)					\
    vmcmd_ev_##name.ev_count++

VMCMD_EVCNT_DECL(calls);
VMCMD_EVCNT_DECL(extends);
VMCMD_EVCNT_DECL(kills);

#ifdef DEBUG_STACK
#define DPRINTF(a) uprintf a
#else
#define DPRINTF(a)
#endif

/*
 * new_vmcmd():
 *	create a new vmcmd structure and fill in its fields based
 *	on function call arguments.  make sure objects ref'd by
 *	the vmcmd are 'held'.
 */

void
new_vmcmd(struct exec_vmcmd_set *evsp,
    int (*proc)(struct lwp * l, struct exec_vmcmd *),
    vsize_t len, vaddr_t addr, struct vnode *vp, u_long offset,
    u_int prot, int flags)
{
	struct exec_vmcmd *vcp;

	VMCMD_EVCNT_INCR(calls);
	KASSERT(proc != vmcmd_map_pagedvn || (vp->v_iflag & VI_TEXT));
	KASSERT(vp == NULL || vp->v_usecount > 0);

	if (evsp->evs_used >= evsp->evs_cnt)
		vmcmdset_extend(evsp);
	vcp = &evsp->evs_cmds[evsp->evs_used++];
	vcp->ev_proc = proc;
	vcp->ev_len = len;
	vcp->ev_addr = addr;
	if ((vcp->ev_vp = vp) != NULL)
		vref(vp);
	vcp->ev_offset = offset;
	vcp->ev_prot = prot;
	vcp->ev_flags = flags;
}

void
vmcmdset_extend(struct exec_vmcmd_set *evsp)
{
	struct exec_vmcmd *nvcp;
	u_int ocnt;

#ifdef DIAGNOSTIC
	if (evsp->evs_used < evsp->evs_cnt)
		panic("vmcmdset_extend: not necessary");
#endif

	/* figure out number of entries in new set */
	if ((ocnt = evsp->evs_cnt) != 0) {
		evsp->evs_cnt += ocnt;
		VMCMD_EVCNT_INCR(extends);
	} else
		evsp->evs_cnt = EXEC_DEFAULT_VMCMD_SETSIZE;

	/* allocate it */
	nvcp = kmem_alloc(evsp->evs_cnt * sizeof(struct exec_vmcmd), KM_SLEEP);

	/* free the old struct, if there was one, and record the new one */
	if (ocnt) {
		memcpy(nvcp, evsp->evs_cmds,
		    (ocnt * sizeof(struct exec_vmcmd)));
		kmem_free(evsp->evs_cmds, ocnt * sizeof(struct exec_vmcmd));
	}
	evsp->evs_cmds = nvcp;
}

void
kill_vmcmds(struct exec_vmcmd_set *evsp)
{
	struct exec_vmcmd *vcp;
	u_int i;

	VMCMD_EVCNT_INCR(kills);

	if (evsp->evs_cnt == 0)
		return;

	for (i = 0; i < evsp->evs_used; i++) {
		vcp = &evsp->evs_cmds[i];
		if (vcp->ev_vp != NULL)
			vrele(vcp->ev_vp);
	}
	kmem_free(evsp->evs_cmds, evsp->evs_cnt * sizeof(struct exec_vmcmd));
	evsp->evs_used = evsp->evs_cnt = 0;
}

/*
 * vmcmd_map_pagedvn():
 *	handle vmcmd which specifies that a vnode should be mmap'd.
 *	appropriate for handling demand-paged text and data segments.
 */

int
vmcmd_map_pagedvn(struct lwp *l, struct exec_vmcmd *cmd)
{
	struct uvm_object *uobj;
	struct vnode *vp = cmd->ev_vp;
	struct proc *p = l->l_proc;
	int error;
	vm_prot_t prot, maxprot;

	KASSERT(vp->v_iflag & VI_TEXT);

	/*
	 * map the vnode in using uvm_map.
	 */

	if (cmd->ev_len == 0)
		return 0;
	if (cmd->ev_offset & PAGE_MASK)
		return EINVAL;
	if (cmd->ev_addr & PAGE_MASK)
		return EINVAL;
	if (cmd->ev_len & PAGE_MASK)
		return EINVAL;

	prot = cmd->ev_prot;
	maxprot = UVM_PROT_ALL;
#ifdef PAX_MPROTECT
	pax_mprotect(l, &prot, &maxprot);
#endif /* PAX_MPROTECT */

	/*
	 * check the file system's opinion about mmapping the file
	 */

	error = VOP_MMAP(vp, prot, l->l_cred);
	if (error)
		return error;

	if ((vp->v_vflag & VV_MAPPED) == 0) {
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		vp->v_vflag |= VV_MAPPED;
		VOP_UNLOCK(vp);
	}

	/*
	 * do the map, reference the object for this map entry
	 */
	uobj = &vp->v_uobj;
	vref(vp);

	error = uvm_map(&p->p_vmspace->vm_map, &cmd->ev_addr, cmd->ev_len,
		uobj, cmd->ev_offset, 0,
		UVM_MAPFLAG(prot, maxprot, UVM_INH_COPY,
			UVM_ADV_NORMAL, UVM_FLAG_COPYONW|UVM_FLAG_FIXED));
	if (error) {
		uobj->pgops->pgo_detach(uobj);
	}
	return error;
}

/*
 * vmcmd_map_readvn():
 *	handle vmcmd which specifies that a vnode should be read from.
 *	appropriate for non-demand-paged text/data segments, i.e. impure
 *	objects (a la OMAGIC and NMAGIC).
 */
int
vmcmd_map_readvn(struct lwp *l, struct exec_vmcmd *cmd)
{
	struct proc *p = l->l_proc;
	int error;
	long diff;

	if (cmd->ev_len == 0)
		return 0;

	diff = cmd->ev_addr - trunc_page(cmd->ev_addr);
	cmd->ev_addr -= diff;			/* required by uvm_map */
	cmd->ev_offset -= diff;
	cmd->ev_len += diff;

	error = uvm_map(&p->p_vmspace->vm_map, &cmd->ev_addr,
			round_page(cmd->ev_len), NULL, UVM_UNKNOWN_OFFSET, 0,
			UVM_MAPFLAG(UVM_PROT_ALL, UVM_PROT_ALL, UVM_INH_COPY,
			UVM_ADV_NORMAL,
			UVM_FLAG_FIXED|UVM_FLAG_OVERLAY|UVM_FLAG_COPYONW));

	if (error)
		return error;

	return vmcmd_readvn(l, cmd);
}

int
vmcmd_readvn(struct lwp *l, struct exec_vmcmd *cmd)
{
	struct proc *p = l->l_proc;
	int error;
	vm_prot_t prot, maxprot;

	error = vn_rdwr(UIO_READ, cmd->ev_vp, (void *)cmd->ev_addr,
	    cmd->ev_len, cmd->ev_offset, UIO_USERSPACE, IO_UNIT,
	    l->l_cred, NULL, l);
	if (error)
		return error;

	prot = cmd->ev_prot;
	maxprot = VM_PROT_ALL;
#ifdef PAX_MPROTECT
	pax_mprotect(l, &prot, &maxprot);
#endif /* PAX_MPROTECT */

#ifdef PMAP_NEED_PROCWR
	/*
	 * we had to write the process, make sure the pages are synched
	 * with the instruction cache.
	 */
	if (prot & VM_PROT_EXECUTE)
		pmap_procwr(p, cmd->ev_addr, cmd->ev_len);
#endif

	/*
	 * we had to map in the area at PROT_ALL so that vn_rdwr()
	 * could write to it.   however, the caller seems to want
	 * it mapped read-only, so now we are going to have to call
	 * uvm_map_protect() to fix up the protection.  ICK.
	 */
	if (maxprot != VM_PROT_ALL) {
		error = uvm_map_protect(&p->p_vmspace->vm_map,
				trunc_page(cmd->ev_addr),
				round_page(cmd->ev_addr + cmd->ev_len),
				maxprot, true);
		if (error)
			return error;
	}

	if (prot != maxprot) {
		error = uvm_map_protect(&p->p_vmspace->vm_map,
				trunc_page(cmd->ev_addr),
				round_page(cmd->ev_addr + cmd->ev_len),
				prot, false);
		if (error)
			return error;
	}

	return 0;
}

/*
 * vmcmd_map_zero():
 *	handle vmcmd which specifies a zero-filled address space region.  The
 *	address range must be first allocated, then protected appropriately.
 */

int
vmcmd_map_zero(struct lwp *l, struct exec_vmcmd *cmd)
{
	struct proc *p = l->l_proc;
	int error;
	long diff;
	vm_prot_t prot, maxprot;

	diff = cmd->ev_addr - trunc_page(cmd->ev_addr);
	cmd->ev_addr -= diff;			/* required by uvm_map */
	cmd->ev_len += diff;

	prot = cmd->ev_prot;
	maxprot = UVM_PROT_ALL;
#ifdef PAX_MPROTECT
	pax_mprotect(l, &prot, &maxprot);
#endif /* PAX_MPROTECT */

	error = uvm_map(&p->p_vmspace->vm_map, &cmd->ev_addr,
			round_page(cmd->ev_len), NULL, UVM_UNKNOWN_OFFSET, 0,
			UVM_MAPFLAG(prot, maxprot, UVM_INH_COPY,
			UVM_ADV_NORMAL,
			UVM_FLAG_FIXED|UVM_FLAG_COPYONW));
	if (cmd->ev_flags & VMCMD_STACK)
		curproc->p_vmspace->vm_issize += atop(round_page(cmd->ev_len));
	return error;
}

/*
 * exec_read_from():
 *
 *	Read from vnode into buffer at offset.
 */
int
exec_read_from(struct lwp *l, struct vnode *vp, u_long off, void *bf,
    size_t size)
{
	int error;
	size_t resid;

	if ((error = vn_rdwr(UIO_READ, vp, bf, size, off, UIO_SYSSPACE,
	    0, l->l_cred, &resid, NULL)) != 0)
		return error;
	/*
	 * See if we got all of it
	 */
	if (resid != 0)
		return ENOEXEC;
	return 0;
}

/*
 * exec_setup_stack(): Set up the stack segment for an elf
 * executable.
 *
 * Note that the ep_ssize parameter must be set to be the current stack
 * limit; this is adjusted in the body of execve() to yield the
 * appropriate stack segment usage once the argument length is
 * calculated.
 *
 * This function returns an int for uniformity with other (future) formats'
 * stack setup functions.  They might have errors to return.
 */

int
exec_setup_stack(struct lwp *l, struct exec_package *epp)
{
	vsize_t max_stack_size;
	vaddr_t access_linear_min;
	vsize_t access_size;
	vaddr_t noaccess_linear_min;
	vsize_t noaccess_size;

#ifndef	USRSTACK32
#define USRSTACK32	(0x00000000ffffffffL&~PGOFSET)
#endif
#ifndef MAXSSIZ32
#define MAXSSIZ32	(MAXSSIZ >> 2)
#endif

	if (epp->ep_flags & EXEC_32) {
		epp->ep_minsaddr = USRSTACK32;
		max_stack_size = MAXSSIZ32;
	} else {
		epp->ep_minsaddr = USRSTACK;
		max_stack_size = MAXSSIZ;
	}

	DPRINTF(("ep_minsaddr=%llx max_stack_size=%llx\n",
	    (unsigned long long)epp->ep_minsaddr,
	    (unsigned long long)max_stack_size));

	epp->ep_ssize = MIN(l->l_proc->p_rlimit[RLIMIT_STACK].rlim_cur,
	    max_stack_size);

#ifdef PAX_ASLR
	pax_aslr_stack(epp, &max_stack_size);
#endif /* PAX_ASLR */

	l->l_proc->p_stackbase = epp->ep_minsaddr;
	
	epp->ep_maxsaddr = (vaddr_t)STACK_GROW(epp->ep_minsaddr,
		max_stack_size);

	DPRINTF(("ep_ssize=%llx ep_maxsaddr=%llx\n",
	    (unsigned long long)epp->ep_ssize,
	    (unsigned long long)epp->ep_maxsaddr));

	/*
	 * set up commands for stack.  note that this takes *two*, one to
	 * map the part of the stack which we can access, and one to map
	 * the part which we can't.
	 *
	 * arguably, it could be made into one, but that would require the
	 * addition of another mapping proc, which is unnecessary
	 */
	access_size = epp->ep_ssize;
	access_linear_min = (vaddr_t)STACK_ALLOC(epp->ep_minsaddr, access_size);
	noaccess_size = max_stack_size - access_size;
	noaccess_linear_min = (vaddr_t)STACK_ALLOC(STACK_GROW(epp->ep_minsaddr,
	    access_size), noaccess_size);

	DPRINTF(("access_size=%llx, access_linear_min=%llx, "
	    "noaccess_size=%llx, noaccess_linear_min=%llx\n",
	    (unsigned long long)access_size,
	    (unsigned long long)access_linear_min,
	    (unsigned long long)noaccess_size,
	    (unsigned long long)noaccess_linear_min));

	if (noaccess_size > 0 && noaccess_size <= MAXSSIZ) {
		NEW_VMCMD2(&epp->ep_vmcmds, vmcmd_map_zero, noaccess_size,
		    noaccess_linear_min, NULL, 0, VM_PROT_NONE, VMCMD_STACK);
	}
	KASSERT(access_size > 0 && access_size <= MAXSSIZ);
	NEW_VMCMD2(&epp->ep_vmcmds, vmcmd_map_zero, access_size,
	    access_linear_min, NULL, 0, VM_PROT_READ | VM_PROT_WRITE,
	    VMCMD_STACK);

	return 0;
}
