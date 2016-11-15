/*	$NetBSD: uvm_init.c,v 1.46 2015/04/03 01:03:42 riastradh Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
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
 *
 * from: Id: uvm_init.c,v 1.1.2.3 1998/02/06 05:15:27 chs Exp
 */

/*
 * uvm_init.c: init the vm system.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_init.c,v 1.46 2015/04/03 01:03:42 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/resourcevar.h>
#include <sys/kmem.h>
#include <sys/mman.h>
#include <sys/vnode.h>

#include <uvm/uvm.h>
#include <uvm/uvm_pdpolicy.h>
#include <uvm/uvm_readahead.h>

/*
 * struct uvm: we store most global vars in this structure to make them
 * easier to spot...
 */

struct uvm uvm;		/* decl */
struct uvmexp uvmexp;	/* decl */
struct uvm_object *uvm_kernel_object;

#if defined(__uvmexp_pagesize)
const int * const uvmexp_pagesize = &uvmexp.pagesize;
const int * const uvmexp_pagemask = &uvmexp.pagemask;
const int * const uvmexp_pageshift = &uvmexp.pageshift;
#endif

kmutex_t uvm_pageqlock;
kmutex_t uvm_fpageqlock;
kmutex_t uvm_kentry_lock;
kmutex_t uvm_swap_data_lock;

/*
 * uvm_init: init the VM system.   called from kern/init_main.c.
 */

void
uvm_init(void)
{
	vaddr_t kvm_start, kvm_end;

	/*
	 * Ensure that the hardware set the page size, zero the UVM structure.
	 */

	if (uvmexp.pagesize == 0) {
		panic("uvm_init: page size not set");
	}

	memset(&uvm, 0, sizeof(uvm));
	averunnable.fscale = FSCALE;

	/*
	 * Init the page sub-system.  This includes allocating the vm_page
	 * structures, and setting up all the page queues (and locks).
	 * Available memory will be put in the "free" queue, kvm_start and
	 * kvm_end will be set to the area of kernel virtual memory which
	 * is available for general use.
	 */

	uvm_page_init(&kvm_start, &kvm_end);

	/*
	 * Init the map sub-system.
	 */

	uvm_map_init();

	/*
	 * Setup the kernel's virtual memory data structures.  This includes
	 * setting up the kernel_map/kernel_object.
	 * Bootstrap all kernel memory allocators.
	 */

	uao_init();
	uvm_km_bootstrap(kvm_start, kvm_end);

	/*
	 * Setup uvm_map caches and init the amap.
	 */

	uvm_map_init_caches();
	uvm_amap_init();

	/*
	 * Init the pmap module.  The pmap module is free to allocate
	 * memory for its private use (e.g. pvlists).
	 */

	pmap_init();

	/*
	 * Make kernel memory allocators ready for use.
	 * After this call the pool/kmem memory allocators can be used.
	 */

	uvm_km_init();
#ifdef __HAVE_PMAP_PV_TRACK
	pmap_pv_init();
#endif

#ifdef DEBUG
	debug_init();
#endif

	/*
	 * Init all pagers and the pager_map.
	 */

	uvm_pager_init();

	/*
	 * Initialize the uvm_loan() facility.
	 */

	uvm_loan_init();

	/*
	 * Init emap subsystem.
	 */

	uvm_emap_sysinit();

	/*
	 * The VM system is now up!  Now that kmem is up we can resize the
	 * <obj,off> => <page> hash table for general use and enable paging
	 * of kernel objects.
	 */

	uao_create(VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS,
	    UAO_FLAG_KERNSWAP);

	uvmpdpol_reinit();

	/*
	 * Init anonymous memory systems.
	 */

	uvm_anon_init();

	uvm_uarea_init();

	/*
	 * Init readahead mechanism.
	 */

	uvm_ra_init();
}
