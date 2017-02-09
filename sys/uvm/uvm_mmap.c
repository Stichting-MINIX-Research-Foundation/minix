/*	$NetBSD: uvm_mmap.c,v 1.153 2015/08/04 18:28:10 maxv Exp $	*/

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
 * from: Utah $Hdr: vm_mmap.c 1.6 91/10/21$
 *      @(#)vm_mmap.c   8.5 (Berkeley) 5/19/94
 * from: Id: uvm_mmap.c,v 1.1.2.14 1998/01/05 21:04:26 chuck Exp
 */

/*
 * uvm_mmap.c: system call interface into VM system, plus kernel vm_mmap
 * function.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_mmap.c,v 1.153 2015/08/04 18:28:10 maxv Exp $");

#include "opt_compat_netbsd.h"
#include "opt_pax.h"

#include <sys/types.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/resourcevar.h>
#include <sys/mman.h>

#if defined(PAX_ASLR) || defined(PAX_MPROTECT)
#include <sys/pax.h>
#endif /* PAX_ASLR || PAX_MPROTECT */

#include <sys/syscallargs.h>

#include <uvm/uvm.h>
#include <uvm/uvm_device.h>

static int uvm_mmap(struct vm_map *, vaddr_t *, vsize_t, vm_prot_t, vm_prot_t,
		    int, int, struct uvm_object *, voff_t, vsize_t);

static int
range_test(vaddr_t addr, vsize_t size, bool ismmap)
{
	vaddr_t vm_min_address = VM_MIN_ADDRESS;
	vaddr_t vm_max_address = VM_MAXUSER_ADDRESS;
	vaddr_t eaddr = addr + size;
	int res = 0;

	if (addr < vm_min_address)
		return EINVAL;
	if (eaddr > vm_max_address)
		return ismmap ? EFBIG : EINVAL;
	if (addr > eaddr) /* no wrapping! */
		return ismmap ? EOVERFLOW : EINVAL;

#ifdef MD_MMAP_RANGE_TEST
	res = MD_MMAP_RANGE_TEST(addr, eaddr);
#endif

	return res;
}

/*
 * unimplemented VM system calls:
 */

/*
 * sys_sbrk: sbrk system call.
 */

/* ARGSUSED */
int
sys_sbrk(struct lwp *l, const struct sys_sbrk_args *uap, register_t *retval)
{
	/* {
		syscallarg(intptr_t) incr;
	} */

	return (ENOSYS);
}

/*
 * sys_sstk: sstk system call.
 */

/* ARGSUSED */
int
sys_sstk(struct lwp *l, const struct sys_sstk_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) incr;
	} */

	return (ENOSYS);
}

/*
 * sys_mincore: determine if pages are in core or not.
 */

/* ARGSUSED */
int
sys_mincore(struct lwp *l, const struct sys_mincore_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(void *) addr;
		syscallarg(size_t) len;
		syscallarg(char *) vec;
	} */
	struct proc *p = l->l_proc;
	struct vm_page *pg;
	char *vec, pgi;
	struct uvm_object *uobj;
	struct vm_amap *amap;
	struct vm_anon *anon;
	struct vm_map_entry *entry;
	vaddr_t start, end, lim;
	struct vm_map *map;
	vsize_t len;
	int error = 0, npgs;

	map = &p->p_vmspace->vm_map;

	start = (vaddr_t)SCARG(uap, addr);
	len = SCARG(uap, len);
	vec = SCARG(uap, vec);

	if (start & PAGE_MASK)
		return (EINVAL);
	len = round_page(len);
	end = start + len;
	if (end <= start)
		return (EINVAL);

	/*
	 * Lock down vec, so our returned status isn't outdated by
	 * storing the status byte for a page.
	 */

	npgs = len >> PAGE_SHIFT;
	error = uvm_vslock(p->p_vmspace, vec, npgs, VM_PROT_WRITE);
	if (error) {
		return error;
	}
	vm_map_lock_read(map);

	if (uvm_map_lookup_entry(map, start, &entry) == false) {
		error = ENOMEM;
		goto out;
	}

	for (/* nothing */;
	     entry != &map->header && entry->start < end;
	     entry = entry->next) {
		KASSERT(!UVM_ET_ISSUBMAP(entry));
		KASSERT(start >= entry->start);

		/* Make sure there are no holes. */
		if (entry->end < end &&
		     (entry->next == &map->header ||
		      entry->next->start > entry->end)) {
			error = ENOMEM;
			goto out;
		}

		lim = end < entry->end ? end : entry->end;

		/*
		 * Special case for objects with no "real" pages.  Those
		 * are always considered resident (mapped devices).
		 */

		if (UVM_ET_ISOBJ(entry)) {
			KASSERT(!UVM_OBJ_IS_KERN_OBJECT(entry->object.uvm_obj));
			if (UVM_OBJ_IS_DEVICE(entry->object.uvm_obj)) {
				for (/* nothing */; start < lim;
				     start += PAGE_SIZE, vec++)
					subyte(vec, 1);
				continue;
			}
		}

		amap = entry->aref.ar_amap;	/* upper layer */
		uobj = entry->object.uvm_obj;	/* lower layer */

		if (amap != NULL)
			amap_lock(amap);
		if (uobj != NULL)
			mutex_enter(uobj->vmobjlock);

		for (/* nothing */; start < lim; start += PAGE_SIZE, vec++) {
			pgi = 0;
			if (amap != NULL) {
				/* Check the upper layer first. */
				anon = amap_lookup(&entry->aref,
				    start - entry->start);
				/* Don't need to lock anon here. */
				if (anon != NULL && anon->an_page != NULL) {

					/*
					 * Anon has the page for this entry
					 * offset.
					 */

					pgi = 1;
				}
			}
			if (uobj != NULL && pgi == 0) {
				/* Check the lower layer. */
				pg = uvm_pagelookup(uobj,
				    entry->offset + (start - entry->start));
				if (pg != NULL) {

					/*
					 * Object has the page for this entry
					 * offset.
					 */

					pgi = 1;
				}
			}
			(void) subyte(vec, pgi);
		}
		if (uobj != NULL)
			mutex_exit(uobj->vmobjlock);
		if (amap != NULL)
			amap_unlock(amap);
	}

 out:
	vm_map_unlock_read(map);
	uvm_vsunlock(p->p_vmspace, SCARG(uap, vec), npgs);
	return (error);
}

/*
 * sys_mmap: mmap system call.
 *
 * => file offset and address may not be page aligned
 *    - if MAP_FIXED, offset and address must have remainder mod PAGE_SIZE
 *    - if address isn't page aligned the mapping starts at trunc_page(addr)
 *      and the return value is adjusted up by the page offset.
 */

int
sys_mmap(struct lwp *l, const struct sys_mmap_args *uap, register_t *retval)
{
	/* {
		syscallarg(void *) addr;
		syscallarg(size_t) len;
		syscallarg(int) prot;
		syscallarg(int) flags;
		syscallarg(int) fd;
		syscallarg(long) pad;
		syscallarg(off_t) pos;
	} */
	struct proc *p = l->l_proc;
	vaddr_t addr;
	off_t pos;
	vsize_t size, pageoff, newsize;
	vm_prot_t prot, maxprot;
	int flags, fd, advice;
	vaddr_t defaddr;
	struct file *fp = NULL;
	struct uvm_object *uobj;
	int error;
#ifdef PAX_ASLR
	vaddr_t orig_addr;
#endif /* PAX_ASLR */

	/*
	 * first, extract syscall args from the uap.
	 */

	addr = (vaddr_t)SCARG(uap, addr);
	size = (vsize_t)SCARG(uap, len);
	prot = SCARG(uap, prot) & VM_PROT_ALL;
	flags = SCARG(uap, flags);
	fd = SCARG(uap, fd);
	pos = SCARG(uap, pos);

#ifdef PAX_ASLR
	orig_addr = addr;
#endif /* PAX_ASLR */

	/*
	 * Fixup the old deprecated MAP_COPY into MAP_PRIVATE, and
	 * validate the flags.
	 */
	if (flags & MAP_COPY) {
		flags = (flags & ~MAP_COPY) | MAP_PRIVATE;
#if defined(COMPAT_10) && defined(__i386__)
		/*
		 * Ancient kernel on x86 did not obey PROT_EXEC on i386 at least
		 * and ld.so did not turn it on. We take care of this on amd64
		 * in compat32.
		 */
		prot |= PROT_EXEC;
#endif
	}
	if ((flags & (MAP_SHARED|MAP_PRIVATE)) == (MAP_SHARED|MAP_PRIVATE))
		return (EINVAL);

	/*
	 * align file position and save offset.  adjust size.
	 */

	pageoff = (pos & PAGE_MASK);
	pos    -= pageoff;
	newsize = size + pageoff;		/* add offset */
	newsize = (vsize_t)round_page(newsize);	/* round up */

	if (newsize < size)
		return (ENOMEM);
	size = newsize;

	/*
	 * now check (MAP_FIXED) or get (!MAP_FIXED) the "addr"
	 */
	if (flags & MAP_FIXED) {

		/* ensure address and file offset are aligned properly */
		addr -= pageoff;
		if (addr & PAGE_MASK)
			return (EINVAL);

		error = range_test(addr, size, true);
		if (error) {
			return error;
		}

	} else if (addr == 0 || !(flags & MAP_TRYFIXED)) {

		/*
		 * not fixed: make sure we skip over the largest
		 * possible heap for non-topdown mapping arrangements.
		 * we will refine our guess later (e.g. to account for
		 * VAC, etc)
		 */

		defaddr = p->p_emul->e_vm_default_addr(p,
		    (vaddr_t)p->p_vmspace->vm_daddr, size);

		if (addr == 0 ||
		    !(p->p_vmspace->vm_map.flags & VM_MAP_TOPDOWN))
			addr = MAX(addr, defaddr);
		else
			addr = MIN(addr, defaddr);
	}

	/*
	 * check for file mappings (i.e. not anonymous) and verify file.
	 */

	advice = UVM_ADV_NORMAL;
	if ((flags & MAP_ANON) == 0) {
		if ((fp = fd_getfile(fd)) == NULL)
			return (EBADF);

		if (fp->f_ops->fo_mmap == NULL) {
			error = ENODEV;
			goto out;
		}
		error = (*fp->f_ops->fo_mmap)(fp, &pos, size, prot, &flags,
					      &advice, &uobj, &maxprot);
		if (error) {
			goto out;
		}
		if (uobj == NULL) {
			flags |= MAP_ANON;
			fd_putfile(fd);
			fp = NULL;
			goto is_anon;
		}
	} else {		/* MAP_ANON case */
		/*
		 * XXX What do we do about (MAP_SHARED|MAP_PRIVATE) == 0?
		 */
		if (fd != -1)
			return (EINVAL);

 is_anon:		/* label for SunOS style /dev/zero */
		uobj = NULL;
		maxprot = VM_PROT_ALL;
		pos = 0;
	}

#ifdef PAX_MPROTECT
	pax_mprotect(l, &prot, &maxprot);
#endif /* PAX_MPROTECT */

#ifdef PAX_ASLR
	pax_aslr_mmap(l, &addr, orig_addr, flags);
#endif /* PAX_ASLR */

	/*
	 * now let kernel internal function uvm_mmap do the work.
	 */

	error = uvm_mmap(&p->p_vmspace->vm_map, &addr, size, prot, maxprot,
	    flags, advice, uobj, pos, p->p_rlimit[RLIMIT_MEMLOCK].rlim_cur);

	/* remember to add offset */
	*retval = (register_t)(addr + pageoff);

 out:
     	if (fp != NULL)
		fd_putfile(fd);

	return (error);
}

/*
 * sys___msync13: the msync system call (a front-end for flush)
 */

int
sys___msync13(struct lwp *l, const struct sys___msync13_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(void *) addr;
		syscallarg(size_t) len;
		syscallarg(int) flags;
	} */
	struct proc *p = l->l_proc;
	vaddr_t addr;
	vsize_t size, pageoff;
	struct vm_map *map;
	int error, rv, flags, uvmflags;

	/*
	 * extract syscall args from the uap
	 */

	addr = (vaddr_t)SCARG(uap, addr);
	size = (vsize_t)SCARG(uap, len);
	flags = SCARG(uap, flags);

	/* sanity check flags */
	if ((flags & ~(MS_ASYNC | MS_SYNC | MS_INVALIDATE)) != 0 ||
	    (flags & (MS_ASYNC | MS_SYNC | MS_INVALIDATE)) == 0 ||
	    (flags & (MS_ASYNC | MS_SYNC)) == (MS_ASYNC | MS_SYNC))
		return (EINVAL);
	if ((flags & (MS_ASYNC | MS_SYNC)) == 0)
		flags |= MS_SYNC;

	/*
	 * align the address to a page boundary and adjust the size accordingly.
	 */

	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vsize_t)round_page(size);

	error = range_test(addr, size, false);
	if (error)
		return error;

	/*
	 * get map
	 */

	map = &p->p_vmspace->vm_map;

	/*
	 * XXXCDC: do we really need this semantic?
	 *
	 * XXX Gak!  If size is zero we are supposed to sync "all modified
	 * pages with the region containing addr".  Unfortunately, we
	 * don't really keep track of individual mmaps so we approximate
	 * by flushing the range of the map entry containing addr.
	 * This can be incorrect if the region splits or is coalesced
	 * with a neighbor.
	 */

	if (size == 0) {
		struct vm_map_entry *entry;

		vm_map_lock_read(map);
		rv = uvm_map_lookup_entry(map, addr, &entry);
		if (rv == true) {
			addr = entry->start;
			size = entry->end - entry->start;
		}
		vm_map_unlock_read(map);
		if (rv == false)
			return (EINVAL);
	}

	/*
	 * translate MS_ flags into PGO_ flags
	 */

	uvmflags = PGO_CLEANIT;
	if (flags & MS_INVALIDATE)
		uvmflags |= PGO_FREE;
	if (flags & MS_SYNC)
		uvmflags |= PGO_SYNCIO;

	error = uvm_map_clean(map, addr, addr+size, uvmflags);
	return error;
}

/*
 * sys_munmap: unmap a users memory
 */

int
sys_munmap(struct lwp *l, const struct sys_munmap_args *uap, register_t *retval)
{
	/* {
		syscallarg(void *) addr;
		syscallarg(size_t) len;
	} */
	struct proc *p = l->l_proc;
	vaddr_t addr;
	vsize_t size, pageoff;
	struct vm_map *map;
	struct vm_map_entry *dead_entries;
	int error;

	/*
	 * get syscall args.
	 */

	addr = (vaddr_t)SCARG(uap, addr);
	size = (vsize_t)SCARG(uap, len);

	/*
	 * align the address to a page boundary and adjust the size accordingly.
	 */

	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vsize_t)round_page(size);

	if (size == 0)
		return (0);

	error = range_test(addr, size, false);
	if (error)
		return error;

	map = &p->p_vmspace->vm_map;

	/*
	 * interesting system call semantic: make sure entire range is
	 * allocated before allowing an unmap.
	 */

	vm_map_lock(map);
#if 0
	if (!uvm_map_checkprot(map, addr, addr + size, VM_PROT_NONE)) {
		vm_map_unlock(map);
		return (EINVAL);
	}
#endif
	uvm_unmap_remove(map, addr, addr + size, &dead_entries, 0);
	vm_map_unlock(map);
	if (dead_entries != NULL)
		uvm_unmap_detach(dead_entries, 0);
	return (0);
}

/*
 * sys_mprotect: the mprotect system call
 */

int
sys_mprotect(struct lwp *l, const struct sys_mprotect_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(void *) addr;
		syscallarg(size_t) len;
		syscallarg(int) prot;
	} */
	struct proc *p = l->l_proc;
	vaddr_t addr;
	vsize_t size, pageoff;
	vm_prot_t prot;
	int error;

	/*
	 * extract syscall args from uap
	 */

	addr = (vaddr_t)SCARG(uap, addr);
	size = (vsize_t)SCARG(uap, len);
	prot = SCARG(uap, prot) & VM_PROT_ALL;

	/*
	 * align the address to a page boundary and adjust the size accordingly.
	 */

	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = round_page(size);

	error = range_test(addr, size, false);
	if (error)
		return error;

	error = uvm_map_protect(&p->p_vmspace->vm_map, addr, addr + size, prot,
				false);
	return error;
}

/*
 * sys_minherit: the minherit system call
 */

int
sys_minherit(struct lwp *l, const struct sys_minherit_args *uap,
   register_t *retval)
{
	/* {
		syscallarg(void *) addr;
		syscallarg(int) len;
		syscallarg(int) inherit;
	} */
	struct proc *p = l->l_proc;
	vaddr_t addr;
	vsize_t size, pageoff;
	vm_inherit_t inherit;
	int error;

	addr = (vaddr_t)SCARG(uap, addr);
	size = (vsize_t)SCARG(uap, len);
	inherit = SCARG(uap, inherit);

	/*
	 * align the address to a page boundary and adjust the size accordingly.
	 */

	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vsize_t)round_page(size);

	error = range_test(addr, size, false);
	if (error)
		return error;

	error = uvm_map_inherit(&p->p_vmspace->vm_map, addr, addr + size,
				inherit);
	return error;
}

/*
 * sys_madvise: give advice about memory usage.
 */

/* ARGSUSED */
int
sys_madvise(struct lwp *l, const struct sys_madvise_args *uap,
   register_t *retval)
{
	/* {
		syscallarg(void *) addr;
		syscallarg(size_t) len;
		syscallarg(int) behav;
	} */
	struct proc *p = l->l_proc;
	vaddr_t addr;
	vsize_t size, pageoff;
	int advice, error;

	addr = (vaddr_t)SCARG(uap, addr);
	size = (vsize_t)SCARG(uap, len);
	advice = SCARG(uap, behav);

	/*
	 * align the address to a page boundary, and adjust the size accordingly
	 */

	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vsize_t)round_page(size);

	error = range_test(addr, size, false);
	if (error)
		return error;

	switch (advice) {
	case MADV_NORMAL:
	case MADV_RANDOM:
	case MADV_SEQUENTIAL:
		error = uvm_map_advice(&p->p_vmspace->vm_map, addr, addr + size,
		    advice);
		break;

	case MADV_WILLNEED:

		/*
		 * Activate all these pages, pre-faulting them in if
		 * necessary.
		 */
		error = uvm_map_willneed(&p->p_vmspace->vm_map,
		    addr, addr + size);
		break;

	case MADV_DONTNEED:

		/*
		 * Deactivate all these pages.  We don't need them
		 * any more.  We don't, however, toss the data in
		 * the pages.
		 */

		error = uvm_map_clean(&p->p_vmspace->vm_map, addr, addr + size,
		    PGO_DEACTIVATE);
		break;

	case MADV_FREE:

		/*
		 * These pages contain no valid data, and may be
		 * garbage-collected.  Toss all resources, including
		 * any swap space in use.
		 */

		error = uvm_map_clean(&p->p_vmspace->vm_map, addr, addr + size,
		    PGO_FREE);
		break;

	case MADV_SPACEAVAIL:

		/*
		 * XXXMRG What is this?  I think it's:
		 *
		 *	Ensure that we have allocated backing-store
		 *	for these pages.
		 *
		 * This is going to require changes to the page daemon,
		 * as it will free swap space allocated to pages in core.
		 * There's also what to do for device/file/anonymous memory.
		 */

		return (EINVAL);

	default:
		return (EINVAL);
	}

	return error;
}

/*
 * sys_mlock: memory lock
 */

int
sys_mlock(struct lwp *l, const struct sys_mlock_args *uap, register_t *retval)
{
	/* {
		syscallarg(const void *) addr;
		syscallarg(size_t) len;
	} */
	struct proc *p = l->l_proc;
	vaddr_t addr;
	vsize_t size, pageoff;
	int error;

	/*
	 * extract syscall args from uap
	 */

	addr = (vaddr_t)SCARG(uap, addr);
	size = (vsize_t)SCARG(uap, len);

	/*
	 * align the address to a page boundary and adjust the size accordingly
	 */

	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vsize_t)round_page(size);

	error = range_test(addr, size, false);
	if (error)
		return error;

	if (atop(size) + uvmexp.wired > uvmexp.wiredmax)
		return (EAGAIN);

	if (size + ptoa(pmap_wired_count(vm_map_pmap(&p->p_vmspace->vm_map))) >
			p->p_rlimit[RLIMIT_MEMLOCK].rlim_cur)
		return (EAGAIN);

	error = uvm_map_pageable(&p->p_vmspace->vm_map, addr, addr+size, false,
	    0);
	if (error == EFAULT)
		error = ENOMEM;
	return error;
}

/*
 * sys_munlock: unlock wired pages
 */

int
sys_munlock(struct lwp *l, const struct sys_munlock_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(const void *) addr;
		syscallarg(size_t) len;
	} */
	struct proc *p = l->l_proc;
	vaddr_t addr;
	vsize_t size, pageoff;
	int error;

	/*
	 * extract syscall args from uap
	 */

	addr = (vaddr_t)SCARG(uap, addr);
	size = (vsize_t)SCARG(uap, len);

	/*
	 * align the address to a page boundary, and adjust the size accordingly
	 */

	pageoff = (addr & PAGE_MASK);
	addr -= pageoff;
	size += pageoff;
	size = (vsize_t)round_page(size);

	error = range_test(addr, size, false);
	if (error)
		return error;

	error = uvm_map_pageable(&p->p_vmspace->vm_map, addr, addr+size, true,
	    0);
	if (error == EFAULT)
		error = ENOMEM;
	return error;
}

/*
 * sys_mlockall: lock all pages mapped into an address space.
 */

int
sys_mlockall(struct lwp *l, const struct sys_mlockall_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int) flags;
	} */
	struct proc *p = l->l_proc;
	int error, flags;

	flags = SCARG(uap, flags);

	if (flags == 0 ||
	    (flags & ~(MCL_CURRENT|MCL_FUTURE)) != 0)
		return (EINVAL);

	error = uvm_map_pageable_all(&p->p_vmspace->vm_map, flags,
	    p->p_rlimit[RLIMIT_MEMLOCK].rlim_cur);
	return (error);
}

/*
 * sys_munlockall: unlock all pages mapped into an address space.
 */

int
sys_munlockall(struct lwp *l, const void *v, register_t *retval)
{
	struct proc *p = l->l_proc;

	(void) uvm_map_pageable_all(&p->p_vmspace->vm_map, 0, 0);
	return (0);
}

/*
 * uvm_mmap: internal version of mmap
 *
 * - used by sys_mmap and various framebuffers
 * - uobj is a struct uvm_object pointer or NULL for MAP_ANON
 * - caller must page-align the file offset
 */

int
uvm_mmap(struct vm_map *map, vaddr_t *addr, vsize_t size, vm_prot_t prot,
    vm_prot_t maxprot, int flags, int advice, struct uvm_object *uobj,
    voff_t foff, vsize_t locklimit)
{
	vaddr_t align = 0;
	int error;
	uvm_flag_t uvmflag = 0;

	/*
	 * check params
	 */

	if (size == 0)
		return(0);
	if (foff & PAGE_MASK)
		return(EINVAL);
	if ((prot & maxprot) != prot)
		return(EINVAL);

	/*
	 * for non-fixed mappings, round off the suggested address.
	 * for fixed mappings, check alignment and zap old mappings.
	 */

	if ((flags & MAP_FIXED) == 0) {
		*addr = round_page(*addr);
	} else {
		if (*addr & PAGE_MASK)
			return(EINVAL);
		uvmflag |= UVM_FLAG_FIXED;
		(void) uvm_unmap(map, *addr, *addr + size);
	}

	/*
	 * Try to see if any requested alignment can even be attemped.
	 * Make sure we can express the alignment (asking for a >= 4GB
	 * alignment on an ILP32 architecure make no sense) and the
	 * alignment is at least for a page sized quanitiy.  If the
	 * request was for a fixed mapping, make sure supplied address
	 * adheres to the request alignment.
	 */
	align = (flags & MAP_ALIGNMENT_MASK) >> MAP_ALIGNMENT_SHIFT;
	if (align) {
		if (align >= sizeof(vaddr_t) * NBBY)
			return(EINVAL);
		align = 1L << align;
		if (align < PAGE_SIZE)
			return(EINVAL);
		if (align >= vm_map_max(map))
			return(ENOMEM);
		if (flags & MAP_FIXED) {
			if ((*addr & (align-1)) != 0)
				return(EINVAL);
			align = 0;
		}
	}

	/*
	 * check resource limits
	 */

	if (!VM_MAP_IS_KERNEL(map) &&
	    (((rlim_t)curproc->p_vmspace->vm_map.size + (rlim_t)size) >
	    curproc->p_rlimit[RLIMIT_AS].rlim_cur))
		return ENOMEM;

	/*
	 * handle anon vs. non-anon mappings.   for non-anon mappings attach
	 * to underlying vm object.
	 */

	if (flags & MAP_ANON) {
		KASSERT(uobj == NULL);
		foff = UVM_UNKNOWN_OFFSET;
		if ((flags & MAP_SHARED) == 0)
			/* XXX: defer amap create */
			uvmflag |= UVM_FLAG_COPYONW;
		else
			/* shared: create amap now */
			uvmflag |= UVM_FLAG_OVERLAY;

	} else {
		KASSERT(uobj != NULL);
		if ((flags & MAP_SHARED) == 0) {
			uvmflag |= UVM_FLAG_COPYONW;
		}
	}

	uvmflag = UVM_MAPFLAG(prot, maxprot,
			(flags & MAP_SHARED) ? UVM_INH_SHARE : UVM_INH_COPY,
			advice, uvmflag);
	error = uvm_map(map, addr, size, uobj, foff, align, uvmflag);
	if (error) {
		if (uobj)
			uobj->pgops->pgo_detach(uobj);
		return error;
	}

	/*
	 * POSIX 1003.1b -- if our address space was configured
	 * to lock all future mappings, wire the one we just made.
	 *
	 * Also handle the MAP_WIRED flag here.
	 */

	if (prot == VM_PROT_NONE) {

		/*
		 * No more work to do in this case.
		 */

		return (0);
	}
	if ((flags & MAP_WIRED) != 0 || (map->flags & VM_MAP_WIREFUTURE) != 0) {
		vm_map_lock(map);
		if (atop(size) + uvmexp.wired > uvmexp.wiredmax ||
		    (locklimit != 0 &&
		     size + ptoa(pmap_wired_count(vm_map_pmap(map))) >
		     locklimit)) {
			vm_map_unlock(map);
			uvm_unmap(map, *addr, *addr + size);
			return ENOMEM;
		}

		/*
		 * uvm_map_pageable() always returns the map unlocked.
		 */

		error = uvm_map_pageable(map, *addr, *addr + size,
					 false, UVM_LK_ENTER);
		if (error) {
			uvm_unmap(map, *addr, *addr + size);
			return error;
		}
		return (0);
	}
	return 0;
}

vaddr_t
uvm_default_mapaddr(struct proc *p, vaddr_t base, vsize_t sz)
{

	if (p->p_vmspace->vm_map.flags & VM_MAP_TOPDOWN)
		return VM_DEFAULT_ADDRESS_TOPDOWN(base, sz);
	else
		return VM_DEFAULT_ADDRESS_BOTTOMUP(base, sz);
}

int
uvm_mmap_dev(struct proc *p, void **addrp, size_t len, dev_t dev,
    off_t off)
{
	struct uvm_object *uobj;
	int error, flags, prot;

	flags = MAP_SHARED;
	prot = VM_PROT_READ | VM_PROT_WRITE;
	if (*addrp)
		flags |= MAP_FIXED;
	else
		*addrp = (void *)p->p_emul->e_vm_default_addr(p,
		    (vaddr_t)p->p_vmspace->vm_daddr, len);

	uobj = udv_attach(dev, prot, off, len);
	if (uobj == NULL)
		return EINVAL;

	error = uvm_mmap(&p->p_vmspace->vm_map, (vaddr_t *)addrp,
			 (vsize_t)len, prot, prot, flags, UVM_ADV_RANDOM,
			 uobj, off, p->p_rlimit[RLIMIT_MEMLOCK].rlim_cur);
	return error;
}

int
uvm_mmap_anon(struct proc *p, void **addrp, size_t len)
{
	int error, flags, prot;

	flags = MAP_PRIVATE | MAP_ANON;
	prot = VM_PROT_READ | VM_PROT_WRITE;
	if (*addrp)
		flags |= MAP_FIXED;
	else
		*addrp = (void *)p->p_emul->e_vm_default_addr(p,
		    (vaddr_t)p->p_vmspace->vm_daddr, len);

	error = uvm_mmap(&p->p_vmspace->vm_map, (vaddr_t *)addrp,
			 (vsize_t)len, prot, prot, flags, UVM_ADV_NORMAL,
			 NULL, 0, p->p_rlimit[RLIMIT_MEMLOCK].rlim_cur);
	return error;
}
