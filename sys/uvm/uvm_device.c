/*	$NetBSD: uvm_device.c,v 1.64 2014/12/14 23:48:58 chs Exp $	*/

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
 * from: Id: uvm_device.c,v 1.1.2.9 1998/02/06 05:11:47 chs Exp
 */

/*
 * uvm_device.c: the device pager.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_device.c,v 1.64 2014/12/14 23:48:58 chs Exp $");

#include "opt_uvmhist.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/kmem.h>

#include <uvm/uvm.h>
#include <uvm/uvm_device.h>
#include <uvm/uvm_pmap.h>

/*
 * private global data structure
 *
 * we keep a list of active device objects in the system.
 */

LIST_HEAD(udv_list_struct, uvm_device);
static struct udv_list_struct udv_list;
static kmutex_t udv_lock;

/*
 * functions
 */

static void	udv_init(void);
static void	udv_reference(struct uvm_object *);
static void	udv_detach(struct uvm_object *);
static int	udv_fault(struct uvm_faultinfo *, vaddr_t,
			  struct vm_page **, int, int, vm_prot_t,
			  int);

/*
 * master pager structure
 */

const struct uvm_pagerops uvm_deviceops = {
	.pgo_init = udv_init,
	.pgo_reference = udv_reference,
	.pgo_detach = udv_detach,
	.pgo_fault = udv_fault,
};

/*
 * the ops!
 */

/*
 * udv_init
 *
 * init pager private data structures.
 */

static void
udv_init(void)
{
	LIST_INIT(&udv_list);
	mutex_init(&udv_lock, MUTEX_DEFAULT, IPL_NONE);
}

/*
 * udv_attach
 *
 * get a VM object that is associated with a device.   allocate a new
 * one if needed.
 *
 * => caller must _not_ already be holding the lock on the uvm_object.
 * => in fact, nothing should be locked so that we can sleep here.
 */

struct uvm_object *
udv_attach(dev_t device, vm_prot_t accessprot,
    voff_t off,		/* used only for access check */
    vsize_t size	/* used only for access check */)
{
	struct uvm_device *udv, *lcv;
	const struct cdevsw *cdev;
	dev_type_mmap((*mapfn));

	UVMHIST_FUNC("udv_attach"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist, "(device=0x%x)", device,0,0,0);

	/*
	 * before we do anything, ensure this device supports mmap
	 */

	cdev = cdevsw_lookup(device);
	if (cdev == NULL) {
		return (NULL);
	}
	mapfn = cdev->d_mmap;
	if (mapfn == NULL || mapfn == nommap || mapfn == nullmmap) {
		return(NULL);
	}

	/*
	 * Negative offsets on the object are not allowed.
	 */

	if ((cdev->d_flag & D_NEGOFFSAFE) == 0 &&
	    off != UVM_UNKNOWN_OFFSET && off < 0)
		return(NULL);

	/*
	 * Check that the specified range of the device allows the
	 * desired protection.
	 *
	 * XXX assumes VM_PROT_* == PROT_*
	 * XXX clobbers off and size, but nothing else here needs them.
	 */

	while (size != 0) {
		if (cdev_mmap(device, off, accessprot) == -1) {
			return (NULL);
		}
		off += PAGE_SIZE; size -= PAGE_SIZE;
	}

	/*
	 * keep looping until we get it
	 */

	for (;;) {

		/*
		 * first, attempt to find it on the main list
		 */

		mutex_enter(&udv_lock);
		LIST_FOREACH(lcv, &udv_list, u_list) {
			if (device == lcv->u_device)
				break;
		}

		/*
		 * got it on main list.  put a hold on it and unlock udv_lock.
		 */

		if (lcv) {

			/*
			 * if someone else has a hold on it, sleep and start
			 * over again.
			 */

			if (lcv->u_flags & UVM_DEVICE_HOLD) {
				lcv->u_flags |= UVM_DEVICE_WANTED;
				UVM_UNLOCK_AND_WAIT(lcv, &udv_lock, false,
				    "udv_attach",0);
				continue;
			}

			/* we are now holding it */
			lcv->u_flags |= UVM_DEVICE_HOLD;
			mutex_exit(&udv_lock);

			/*
			 * bump reference count, unhold, return.
			 */

			mutex_enter(lcv->u_obj.vmobjlock);
			lcv->u_obj.uo_refs++;
			mutex_exit(lcv->u_obj.vmobjlock);

			mutex_enter(&udv_lock);
			if (lcv->u_flags & UVM_DEVICE_WANTED)
				wakeup(lcv);
			lcv->u_flags &= ~(UVM_DEVICE_WANTED|UVM_DEVICE_HOLD);
			mutex_exit(&udv_lock);
			return(&lcv->u_obj);
		}

		/*
		 * Did not find it on main list.  Need to allocate a new one.
		 */

		mutex_exit(&udv_lock);

		/* Note: both calls may allocate memory and sleep. */
		udv = kmem_alloc(sizeof(*udv), KM_SLEEP);
		uvm_obj_init(&udv->u_obj, &uvm_deviceops, true, 1);

		mutex_enter(&udv_lock);

		/*
		 * now we have to double check to make sure no one added it
		 * to the list while we were sleeping...
		 */

		LIST_FOREACH(lcv, &udv_list, u_list) {
			if (device == lcv->u_device)
				break;
		}

		/*
		 * did we lose a race to someone else?
		 * free our memory and retry.
		 */

		if (lcv) {
			mutex_exit(&udv_lock);
			uvm_obj_destroy(&udv->u_obj, true);
			kmem_free(udv, sizeof(*udv));
			continue;
		}

		/*
		 * we have it!   init the data structures, add to list
		 * and return.
		 */

		udv->u_flags = 0;
		udv->u_device = device;
		LIST_INSERT_HEAD(&udv_list, udv, u_list);
		mutex_exit(&udv_lock);
		return(&udv->u_obj);
	}
	/*NOTREACHED*/
}

/*
 * udv_reference
 *
 * add a reference to a VM object.   Note that the reference count must
 * already be one (the passed in reference) so there is no chance of the
 * udv being released or locked out here.
 *
 * => caller must call with object unlocked.
 */

static void
udv_reference(struct uvm_object *uobj)
{
	UVMHIST_FUNC("udv_reference"); UVMHIST_CALLED(maphist);

	mutex_enter(uobj->vmobjlock);
	uobj->uo_refs++;
	UVMHIST_LOG(maphist, "<- done (uobj=0x%x, ref = %d)",
		    uobj, uobj->uo_refs,0,0);
	mutex_exit(uobj->vmobjlock);
}

/*
 * udv_detach
 *
 * remove a reference to a VM object.
 *
 * => caller must call with object unlocked and map locked.
 */

static void
udv_detach(struct uvm_object *uobj)
{
	struct uvm_device *udv = (struct uvm_device *)uobj;
	UVMHIST_FUNC("udv_detach"); UVMHIST_CALLED(maphist);

	/*
	 * loop until done
	 */
again:
	mutex_enter(uobj->vmobjlock);
	if (uobj->uo_refs > 1) {
		uobj->uo_refs--;
		mutex_exit(uobj->vmobjlock);
		UVMHIST_LOG(maphist," <- done, uobj=0x%x, ref=%d",
			  uobj,uobj->uo_refs,0,0);
		return;
	}

	/*
	 * is it being held?   if so, wait until others are done.
	 */

	mutex_enter(&udv_lock);
	if (udv->u_flags & UVM_DEVICE_HOLD) {
		udv->u_flags |= UVM_DEVICE_WANTED;
		mutex_exit(uobj->vmobjlock);
		UVM_UNLOCK_AND_WAIT(udv, &udv_lock, false, "udv_detach",0);
		goto again;
	}

	/*
	 * got it!   nuke it now.
	 */

	LIST_REMOVE(udv, u_list);
	if (udv->u_flags & UVM_DEVICE_WANTED)
		wakeup(udv);
	mutex_exit(&udv_lock);
	mutex_exit(uobj->vmobjlock);

	uvm_obj_destroy(uobj, true);
	kmem_free(udv, sizeof(*udv));
	UVMHIST_LOG(maphist," <- done, freed uobj=0x%x", uobj,0,0,0);
}

/*
 * udv_fault: non-standard fault routine for device "pages"
 *
 * => rather than having a "get" function, we have a fault routine
 *	since we don't return vm_pages we need full control over the
 *	pmap_enter map in
 * => all the usual fault data structured are locked by the caller
 *	(i.e. maps(read), amap (if any), uobj)
 * => on return, we unlock all fault data structures
 * => flags: PGO_ALLPAGES: get all of the pages
 *	     PGO_LOCKED: fault data structures are locked
 *    XXX: currently PGO_LOCKED is always required ... consider removing
 *	it as a flag
 * => NOTE: vaddr is the VA of pps[0] in ufi->entry, _NOT_ pps[centeridx]
 */

static int
udv_fault(struct uvm_faultinfo *ufi, vaddr_t vaddr, struct vm_page **pps,
    int npages, int centeridx, vm_prot_t access_type,
    int flags)
{
	struct vm_map_entry *entry = ufi->entry;
	struct uvm_object *uobj = entry->object.uvm_obj;
	struct uvm_device *udv = (struct uvm_device *)uobj;
	vaddr_t curr_va;
	off_t curr_offset;
	paddr_t paddr, mdpgno;
	u_int mmapflags;
	int lcv, retval;
	dev_t device;
	vm_prot_t mapprot;
	UVMHIST_FUNC("udv_fault"); UVMHIST_CALLED(maphist);
	UVMHIST_LOG(maphist,"  flags=%d", flags,0,0,0);

	/*
	 * we do not allow device mappings to be mapped copy-on-write
	 * so we kill any attempt to do so here.
	 */

	if (UVM_ET_ISCOPYONWRITE(entry)) {
		UVMHIST_LOG(maphist, "<- failed -- COW entry (etype=0x%x)",
		entry->etype, 0,0,0);
		uvmfault_unlockall(ufi, ufi->entry->aref.ar_amap, uobj);
		return(EIO);
	}

	/*
	 * get device map function.
	 */

	device = udv->u_device;
	if (cdevsw_lookup(device) == NULL) {
		/* XXX This should not happen */
		uvmfault_unlockall(ufi, ufi->entry->aref.ar_amap, uobj);
		return (EIO);
	}

	/*
	 * now we must determine the offset in udv to use and the VA to
	 * use for pmap_enter.  note that we always use orig_map's pmap
	 * for pmap_enter (even if we have a submap).   since virtual
	 * addresses in a submap must match the main map, this is ok.
	 */

	/* udv offset = (offset from start of entry) + entry's offset */
	curr_offset = entry->offset + (vaddr - entry->start);
	/* pmap va = vaddr (virtual address of pps[0]) */
	curr_va = vaddr;

	/*
	 * loop over the page range entering in as needed
	 */

	retval = 0;
	for (lcv = 0 ; lcv < npages ; lcv++, curr_offset += PAGE_SIZE,
	    curr_va += PAGE_SIZE) {
		if ((flags & PGO_ALLPAGES) == 0 && lcv != centeridx)
			continue;

		if (pps[lcv] == PGO_DONTCARE)
			continue;

		mdpgno = cdev_mmap(device, curr_offset, access_type);
		if (mdpgno == -1) {
			retval = EIO;
			break;
		}
		paddr = pmap_phys_address(mdpgno);
		mmapflags = pmap_mmap_flags(mdpgno);
		mapprot = ufi->entry->protection;
		UVMHIST_LOG(maphist,
		    "  MAPPING: device: pm=0x%x, va=0x%x, pa=0x%lx, at=%d",
		    ufi->orig_map->pmap, curr_va, paddr, mapprot);
		if (pmap_enter(ufi->orig_map->pmap, curr_va, paddr, mapprot,
		    PMAP_CANFAIL | mapprot | mmapflags) != 0) {
			/*
			 * pmap_enter() didn't have the resource to
			 * enter this mapping.  Unlock everything,
			 * wait for the pagedaemon to free up some
			 * pages, and then tell uvm_fault() to start
			 * the fault again.
			 *
			 * XXX Needs some rethinking for the PGO_ALLPAGES
			 * XXX case.
			 */
			pmap_update(ufi->orig_map->pmap);	/* sync what we have so far */
			uvmfault_unlockall(ufi, ufi->entry->aref.ar_amap,
			    uobj);
			uvm_wait("udv_fault");
			return (ERESTART);
		}
	}

	pmap_update(ufi->orig_map->pmap);
	uvmfault_unlockall(ufi, ufi->entry->aref.ar_amap, uobj);
	return (retval);
}
