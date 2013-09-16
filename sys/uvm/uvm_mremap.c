/*	$NetBSD: uvm_mremap.c,v 1.17 2011/06/12 03:36:03 rmind Exp $	*/

/*-
 * Copyright (c)2006,2007,2009 YAMAMOTO Takashi,
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_mremap.c,v 1.17 2011/06/12 03:36:03 rmind Exp $");

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/sched.h>
#include <sys/syscallargs.h>
#include <sys/proc.h>
#include <sys/atomic.h>

#include <uvm/uvm.h>

static int
uvm_mapent_extend(struct vm_map *map, vaddr_t endva, vsize_t size)
{
	struct vm_map_entry *entry;
	struct vm_map_entry *reserved_entry;
	struct uvm_object *uobj;
	int error = 0;

	vm_map_lock(map);
	if (!uvm_map_lookup_entry(map, endva, &reserved_entry)) {
		error = ENOENT;
		goto done;
	}
	if (reserved_entry->start != endva ||
	    reserved_entry->end != endva + size ||
	    reserved_entry->object.uvm_obj != NULL ||
	    reserved_entry->aref.ar_amap != NULL ||
	    reserved_entry->protection != VM_PROT_NONE) {
		error = EINVAL;
		goto done;
	}
	entry = reserved_entry->prev;
	if (&map->header == entry || entry->end != endva) {
		error = EINVAL;
		goto done;
	}

	/*
	 * now, make reserved_entry compatible with entry, and then
	 * try to merge.
	 */

	uobj = entry->object.uvm_obj;
	if (uobj) {
		voff_t offset = entry->offset;
		voff_t newoffset;
		
		newoffset = offset + entry->end - entry->start;
		if (newoffset <= offset) {
			error = E2BIG; /* XXX */
			goto done;
		}
		mutex_enter(uobj->vmobjlock);
		KASSERT(uobj->uo_refs > 0);
		atomic_inc_uint(&uobj->uo_refs);
		mutex_exit(uobj->vmobjlock);
		reserved_entry->object.uvm_obj = uobj;
		reserved_entry->offset = newoffset;
	}
	reserved_entry->etype = entry->etype;
	if (UVM_ET_ISCOPYONWRITE(entry)) {
		reserved_entry->etype |= UVM_ET_NEEDSCOPY;
	}
	reserved_entry->flags &= ~UVM_MAP_NOMERGE;
	reserved_entry->protection = entry->protection;
	reserved_entry->max_protection = entry->max_protection;
	reserved_entry->inheritance = entry->inheritance;
	reserved_entry->advice = entry->advice;
	reserved_entry->wired_count = 0; /* XXX should inherit? */
	uvm_mapent_trymerge(map, reserved_entry, 0);
done:
	vm_map_unlock(map);

	return error;
}

/*
 * uvm_mremap: move and/or resize existing mappings.
 */

int
uvm_mremap(struct vm_map *oldmap, vaddr_t oldva, vsize_t oldsize,
    struct vm_map *newmap, vaddr_t *newvap, vsize_t newsize,
    struct proc *newproc, int flags)
{
	vaddr_t dstva;
	vsize_t movesize;
	vaddr_t newva;
	int alignshift;
	vaddr_t align = 0;
	int error = 0;
	const bool fixed = (flags & MAP_FIXED) != 0;

	if (fixed) {
		newva = *newvap;
	} else {
		newva = 0;
	}
	if ((oldva & PAGE_MASK) != 0 ||
	    (newva & PAGE_MASK) != 0 ||
	    (oldsize & PAGE_MASK) != 0 ||
	    (newsize & PAGE_MASK) != 0) {
		return EINVAL;
	}
	/* XXX zero-size should be allowed? */
	if (oldva + oldsize <= oldva || newva + newsize <= newva) {
		return EINVAL;
	}

	/*
	 * Try to see if any requested alignment can even be attempted.
	 * Make sure we can express the alignment (asking for a >= 4GB
	 * alignment on an ILP32 architecure make no sense) and the
	 * alignment is at least for a page sized quanitiy.  If the
	 * request was for a fixed mapping, make sure supplied address
	 * adheres to the request alignment.
	 */
	alignshift = (flags & MAP_ALIGNMENT_MASK) >> MAP_ALIGNMENT_SHIFT;
	if (alignshift != 0) {
		if (alignshift >= sizeof(vaddr_t) * NBBY)
			return EINVAL;
		align = 1L << alignshift;
		if (align < PAGE_SIZE)
			return EINVAL;
		if (align >= vm_map_max(oldmap))
			return ENOMEM;
		if ((flags & MAP_FIXED) != 0) {
			if ((*newvap & (align - 1)) != 0)
				return EINVAL;
			align = 0;
		}
	}

	/*
	 * check the easy cases first.
	 */

	if ((!fixed || newva == oldva) && newmap == oldmap &&
	    (align == 0 || (oldva & (align - 1)) == 0)) {
		vaddr_t va;

		if (newsize == oldsize) {
			newva = oldva;
			goto done;
		}
		if (newsize < oldsize) {
			uvm_unmap(oldmap, oldva + newsize, oldva + oldsize);
			newva = oldva;
			goto done;
		}
		va = oldva + oldsize;
		if (uvm_map_reserve(oldmap, newsize - oldsize, 0, 0, &va,
		    UVM_FLAG_FIXED)) {
			newva = oldva;
			goto extend;
		}
		if (fixed) {
			return ENOMEM;
		}
	}

	/*
	 * we need to move mappings.
	 */

	if (!fixed) {
		KASSERT(&newproc->p_vmspace->vm_map == newmap);
		newva = newproc->p_emul->e_vm_default_addr(newproc,
		    (vaddr_t)newproc->p_vmspace->vm_daddr, newsize);
	}
	dstva = newva;
	if (!uvm_map_reserve(newmap, newsize, oldva, align, &dstva, 
	    fixed ? UVM_FLAG_FIXED : 0)) {
		return ENOMEM;
	}
	KASSERT(!fixed || dstva == newva);
	newva = dstva;
	movesize = MIN(oldsize, newsize);
	error = uvm_map_extract(oldmap, oldva, movesize, newmap, &dstva,
	    UVM_EXTRACT_RESERVED);
	KASSERT(dstva == newva);
	if (error != 0) {
		/*
		 * undo uvm_map_reserve.
		 */
		uvm_unmap(newmap, newva, newva + newsize);
		return error;
	}
	if (newsize > oldsize) {
extend:
		error = uvm_mapent_extend(newmap, newva + oldsize,
		    newsize - oldsize);
		if (error != 0) {
			/*
			 * undo uvm_map_reserve and uvm_map_extract.
			 */
			if (newva == oldva && newmap == oldmap) {
				uvm_unmap(newmap, newva + oldsize,
				    newva + newsize);
			} else {
				uvm_unmap(newmap, newva, newva + newsize);
			}
			return error;
		}
	}

	/*
	 * now we won't fail.
	 * remove original entries unless we did in-place extend.
	 */

	if (oldva != newva || oldmap != newmap) {
		uvm_unmap(oldmap, oldva, oldva + oldsize);
	}
done:
	*newvap = newva;
	return 0;
}

/*
 * sys_mremap: mremap system call.
 */

int
sys_mremap(struct lwp *l, const struct sys_mremap_args *uap, register_t *retval)
{
	/* {
		syscallarg(void *) old_address;
		syscallarg(size_t) old_size;
		syscallarg(void *) new_address;
		syscallarg(size_t) new_size;
		syscallarg(int) flags;
	} */

	struct proc *p;
	struct vm_map *map;
	vaddr_t oldva;
	vaddr_t newva;
	size_t oldsize;
	size_t newsize;
	int flags;
	int error;

	flags = SCARG(uap, flags);
	oldva = (vaddr_t)SCARG(uap, old_address);
	oldsize = (vsize_t)(SCARG(uap, old_size));
	newva = (vaddr_t)SCARG(uap, new_address);
	newsize = (vsize_t)(SCARG(uap, new_size));

	if ((flags & ~(MAP_FIXED | MAP_ALIGNMENT_MASK)) != 0) {
		error = EINVAL;
		goto done;
	}

	oldsize = round_page(oldsize);
	newsize = round_page(newsize);

	p = l->l_proc;
	map = &p->p_vmspace->vm_map;
	error = uvm_mremap(map, oldva, oldsize, map, &newva, newsize, p, flags);

done:
	*retval = (error != 0) ? 0 : (register_t)newva;
	return error;
}
