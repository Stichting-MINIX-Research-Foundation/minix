/*	$NetBSD: mm.c,v 1.19 2014/07/25 08:10:35 dholland Exp $	*/

/*-
 * Copyright (c) 2002, 2008, 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas, Joerg Sonnenberger and Mindaugas Rasiukevicius.
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
 * Special /dev/{mem,kmem,zero,null} memory devices.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mm.c,v 1.19 2014/07/25 08:10:35 dholland Exp $");

#include "opt_compat_netbsd.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <sys/termios.h>

#include <dev/mm.h>

#include <uvm/uvm_extern.h>

static void *		dev_zero_page	__read_mostly;
static kmutex_t		dev_mem_lock	__cacheline_aligned;
static vaddr_t		dev_mem_addr	__read_mostly;

static dev_type_read(mm_readwrite);
static dev_type_ioctl(mm_ioctl);
static dev_type_mmap(mm_mmap);
static dev_type_ioctl(mm_ioctl);

const struct cdevsw mem_cdevsw = {
#ifdef __HAVE_MM_MD_OPEN
	.d_open = mm_md_open,
#else
	.d_open = nullopen,
#endif
	.d_close = nullclose,
	.d_read = mm_readwrite,
	.d_write = mm_readwrite,
	.d_ioctl = mm_ioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = mm_mmap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_MPSAFE
};

#ifdef pmax	/* XXX */
const struct cdevsw mem_ultrix_cdevsw = {
	.d_open = nullopen,
	.d_close = nullclose,
	.d_read = mm_readwrite,
	.d_write = mm_readwrite,
	.d_ioctl = mm_ioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = mm_mmap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_MPSAFE
};
#endif

/*
 * mm_init: initialize memory device driver.
 */
void
mm_init(void)
{
	vaddr_t pg;

	mutex_init(&dev_mem_lock, MUTEX_DEFAULT, IPL_NONE);

	/* Read-only zero-page. */
	pg = uvm_km_alloc(kernel_map, PAGE_SIZE, 0, UVM_KMF_WIRED|UVM_KMF_ZERO);
	KASSERT(pg != 0);
	pmap_protect(pmap_kernel(), pg, pg + PAGE_SIZE, VM_PROT_READ);
	pmap_update(pmap_kernel());
	dev_zero_page = (void *)pg;

#ifndef __HAVE_MM_MD_CACHE_ALIASING
	/* KVA for mappings during I/O. */
	dev_mem_addr = uvm_km_alloc(kernel_map, PAGE_SIZE, 0,
	    UVM_KMF_VAONLY|UVM_KMF_WAITVA);
	KASSERT(dev_mem_addr != 0);
#else
	dev_mem_addr = 0;
#endif
}


/*
 * dev_mem_getva: get a special virtual address.  If architecture requires,
 * allocate VA according to PA, which avoids cache-aliasing issues.  Use a
 * constant, general mapping address otherwise.
 */
static inline vaddr_t
dev_mem_getva(paddr_t pa)
{
#ifdef __HAVE_MM_MD_CACHE_ALIASING
	return uvm_km_alloc(kernel_map, PAGE_SIZE,
	    atop(pa) & uvmexp.colormask,
	    UVM_KMF_VAONLY | UVM_KMF_WAITVA | UVM_KMF_COLORMATCH);
#else
	return dev_mem_addr;
#endif
}

static inline void
dev_mem_relva(paddr_t pa, vaddr_t va)
{
#ifdef __HAVE_MM_MD_CACHE_ALIASING
	uvm_km_free(kernel_map, va, PAGE_SIZE, UVM_KMF_VAONLY);
#else
	KASSERT(dev_mem_addr == va);
#endif
}

/*
 * dev_kmem_readwrite: helper for DEV_MEM (/dev/mem) case of R/W.
 */
static int
dev_mem_readwrite(struct uio *uio, struct iovec *iov)
{
	paddr_t paddr;
	vaddr_t vaddr;
	vm_prot_t prot;
	size_t len, offset;
	bool have_direct;
	int error;

	/* Check for wrap around. */
	if ((intptr_t)uio->uio_offset != uio->uio_offset) {
		return EFAULT;
	}
	paddr = uio->uio_offset & ~PAGE_MASK;
	prot = (uio->uio_rw == UIO_WRITE) ? VM_PROT_WRITE : VM_PROT_READ;
	error = mm_md_physacc(paddr, prot);
	if (error) {
		return error;
	}
	offset = uio->uio_offset & PAGE_MASK;
	len = MIN(uio->uio_resid, PAGE_SIZE - offset);

#ifdef __HAVE_MM_MD_DIRECT_MAPPED_PHYS
	/* Is physical address directly mapped?  Return VA. */
	have_direct = mm_md_direct_mapped_phys(paddr, &vaddr);
#else
	vaddr = 0;
	have_direct = false;
#endif
	if (!have_direct) {
		/* Get a special virtual address. */
		const vaddr_t va = dev_mem_getva(paddr);

		/* Map selected KVA to physical address. */
		mutex_enter(&dev_mem_lock);
		pmap_kenter_pa(va, paddr, prot, 0);
		pmap_update(pmap_kernel());

		/* Perform I/O. */
		vaddr = va + offset;
		error = uiomove((void *)vaddr, len, uio);

		/* Unmap, flush before unlock. */
		pmap_kremove(va, PAGE_SIZE);
		pmap_update(pmap_kernel());
		mutex_exit(&dev_mem_lock);

		/* "Release" the virtual address. */
		dev_mem_relva(paddr, va);
	} else {
		/* Direct map, just perform I/O. */
		vaddr += offset;
		error = uiomove((void *)vaddr, len, uio);
	}
	return error;
}

/*
 * dev_kmem_readwrite: helper for DEV_KMEM (/dev/kmem) case of R/W.
 */
static int
dev_kmem_readwrite(struct uio *uio, struct iovec *iov)
{
	void *addr;
	size_t len, offset;
	vm_prot_t prot;
	int error;
	bool md_kva;

	/* Check for wrap around. */
	addr = (void *)(intptr_t)uio->uio_offset;
	if ((uintptr_t)addr != uio->uio_offset) {
		return EFAULT;
	}
	/*
	 * Handle non-page aligned offset.
	 * Otherwise, we operate in page-by-page basis.
	 */
	offset = uio->uio_offset & PAGE_MASK;
	len = MIN(uio->uio_resid, PAGE_SIZE - offset);
	prot = (uio->uio_rw == UIO_WRITE) ? VM_PROT_WRITE : VM_PROT_READ;

	md_kva = false;

#ifdef __HAVE_MM_MD_DIRECT_MAPPED_IO
	paddr_t paddr;
	/* MD case: is this is a directly mapped address? */
	if (mm_md_direct_mapped_io(addr, &paddr)) {
		/* If so, validate physical address. */
		error = mm_md_physacc(paddr, prot);
		if (error) {
			return error;
		}
		md_kva = true;
	}
#endif
	if (!md_kva) {
		bool checked = false;

#ifdef __HAVE_MM_MD_KERNACC
		/* MD check for the address. */
		error = mm_md_kernacc(addr, prot, &checked);
		if (error) {
			return error;
		}
#endif
		/* UVM check for the address (unless MD indicated to not). */
		if (!checked && !uvm_kernacc(addr, len, prot)) {
			return EFAULT;
		}
	}
	error = uiomove(addr, len, uio);
	return error;
}

/*
 * dev_zero_readwrite: helper for DEV_ZERO (/dev/null) case of R/W.
 */
static inline int
dev_zero_readwrite(struct uio *uio, struct iovec *iov)
{
	size_t len;

	/* Nothing to do for the write case. */
	if (uio->uio_rw == UIO_WRITE) {
		uio->uio_resid = 0;
		return 0;
	}
	/*
	 * Read in page-by-page basis, caller will continue.
	 * Cut appropriately for a single/last-iteration cases.
	 */
	len = MIN(iov->iov_len, PAGE_SIZE);
	return uiomove(dev_zero_page, len, uio);
}

/*
 * mm_readwrite: general memory R/W function.
 */
static int
mm_readwrite(dev_t dev, struct uio *uio, int flags)
{
	struct iovec *iov;
	int error;

#ifdef __HAVE_MM_MD_READWRITE
	/* If defined - there are extra MD cases. */
	switch (minor(dev)) {
	case DEV_MEM:
	case DEV_KMEM:
	case DEV_NULL:
	case DEV_ZERO:
#if defined(COMPAT_16) && defined(__arm)
	case _DEV_ZERO_oARM:
#endif
		break;
	default:
		return mm_md_readwrite(dev, uio);
	}
#endif
	error = 0;
	while (uio->uio_resid > 0 && error == 0) {
		iov = uio->uio_iov;
		if (iov->iov_len == 0) {
			/* Processed; next I/O vector. */
			uio->uio_iov++;
			uio->uio_iovcnt--;
			KASSERT(uio->uio_iovcnt >= 0);
			continue;
		}
		/* Helper functions will process in page-by-page basis. */
		switch (minor(dev)) {
		case DEV_MEM:
			error = dev_mem_readwrite(uio, iov);
			break;
		case DEV_KMEM:
			error = dev_kmem_readwrite(uio, iov);
			break;
		case DEV_NULL:
			if (uio->uio_rw == UIO_WRITE) {
				uio->uio_resid = 0;
			}
			/* Break directly out of the loop. */
			return 0;
#if defined(COMPAT_16) && defined(__arm)
		case _DEV_ZERO_oARM:
#endif
		case DEV_ZERO:
			error = dev_zero_readwrite(uio, iov);
			break;
		default:
			error = ENXIO;
			break;
		}
	}
	return error;
}

/*
 * mm_mmap: general mmap() handler.
 */
static paddr_t
mm_mmap(dev_t dev, off_t off, int acc)
{
	vm_prot_t prot;

#ifdef __HAVE_MM_MD_MMAP
	/* If defined - there are extra mmap() MD cases. */
	switch (minor(dev)) {
	case DEV_MEM:
	case DEV_KMEM:
	case DEV_NULL:
#if defined(COMPAT_16) && defined(__arm)
	case _DEV_ZERO_oARM:
#endif
	case DEV_ZERO:
		break;
	default:
		return mm_md_mmap(dev, off, acc);
	}
#endif
	/*
	 * /dev/null does not make sense, /dev/kmem is volatile and
	 * /dev/zero is handled in mmap already.
	 */
	if (minor(dev) != DEV_MEM) {
		return -1;
	}

	prot = 0;
	if (acc & PROT_EXEC)
		prot |= VM_PROT_EXECUTE;
	if (acc & PROT_READ)
		prot |= VM_PROT_READ;
	if (acc & PROT_WRITE)
		prot |= VM_PROT_WRITE;

	/* Validate the physical address. */
	if (mm_md_physacc(off, prot) != 0) {
		return -1;
	}
	return off >> PGSHIFT;
}

static int
mm_ioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{

	switch (cmd) {
	case FIONBIO:
		/* We never block anyway. */
		return 0;

	case FIOSETOWN:
	case FIOGETOWN:
	case TIOCGPGRP:
	case TIOCSPGRP:
	case TIOCGETA:
		return ENOTTY;

	case FIOASYNC:
		if ((*(int *)data) == 0) {
			return 0;
		}
		/* FALLTHROUGH */
	default:
		return EOPNOTSUPP;
	}
}
