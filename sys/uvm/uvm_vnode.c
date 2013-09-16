/*	$NetBSD: uvm_vnode.c,v 1.99 2012/07/30 23:56:48 matt Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * Copyright (c) 1991, 1993
 *      The Regents of the University of California.
 * Copyright (c) 1990 University of Utah.
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
 *      @(#)vnode_pager.c       8.8 (Berkeley) 2/13/94
 * from: Id: uvm_vnode.c,v 1.1.2.26 1998/02/02 20:38:07 chuck Exp
 */

/*
 * uvm_vnode.c: the vnode pager.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_vnode.c,v 1.99 2012/07/30 23:56:48 matt Exp $");

#include "opt_uvmhist.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/pool.h>
#include <sys/mount.h>

#include <miscfs/specfs/specdev.h>

#include <uvm/uvm.h>
#include <uvm/uvm_readahead.h>

#ifdef UVMHIST
UVMHIST_DEFINE(ubchist);
#endif

/*
 * functions
 */

static void	uvn_detach(struct uvm_object *);
static int	uvn_get(struct uvm_object *, voff_t, struct vm_page **, int *,
			int, vm_prot_t, int, int);
static int	uvn_put(struct uvm_object *, voff_t, voff_t, int);
static void	uvn_reference(struct uvm_object *);

static int	uvn_findpage(struct uvm_object *, voff_t, struct vm_page **,
			     int);

/*
 * master pager structure
 */

const struct uvm_pagerops uvm_vnodeops = {
	.pgo_reference = uvn_reference,
	.pgo_detach = uvn_detach,
	.pgo_get = uvn_get,
	.pgo_put = uvn_put,
};

/*
 * the ops!
 */

/*
 * uvn_reference
 *
 * duplicate a reference to a VM object.  Note that the reference
 * count must already be at least one (the passed in reference) so
 * there is no chance of the uvn being killed or locked out here.
 *
 * => caller must call with object unlocked.
 * => caller must be using the same accessprot as was used at attach time
 */

static void
uvn_reference(struct uvm_object *uobj)
{
	vref((struct vnode *)uobj);
}


/*
 * uvn_detach
 *
 * remove a reference to a VM object.
 *
 * => caller must call with object unlocked and map locked.
 */

static void
uvn_detach(struct uvm_object *uobj)
{
	vrele((struct vnode *)uobj);
}

/*
 * uvn_put: flush page data to backing store.
 *
 * => object must be locked on entry!   VOP_PUTPAGES must unlock it.
 * => flags: PGO_SYNCIO -- use sync. I/O
 * => note: caller must set PG_CLEAN and pmap_clear_modify (if needed)
 */

static int
uvn_put(struct uvm_object *uobj, voff_t offlo, voff_t offhi, int flags)
{
	struct vnode *vp = (struct vnode *)uobj;
	int error;

	KASSERT(mutex_owned(vp->v_interlock));
	error = VOP_PUTPAGES(vp, offlo, offhi, flags);

	return error;
}


/*
 * uvn_get: get pages (synchronously) from backing store
 *
 * => prefer map unlocked (not required)
 * => object must be locked!  we will _unlock_ it before starting any I/O.
 * => flags: PGO_ALLPAGES: get all of the pages
 *           PGO_LOCKED: fault data structures are locked
 * => NOTE: offset is the offset of pps[0], _NOT_ pps[centeridx]
 * => NOTE: caller must check for released pages!!
 */

static int
uvn_get(struct uvm_object *uobj, voff_t offset,
    struct vm_page **pps /* IN/OUT */,
    int *npagesp /* IN (OUT if PGO_LOCKED)*/,
    int centeridx, vm_prot_t access_type, int advice, int flags)
{
	struct vnode *vp = (struct vnode *)uobj;
	int error;

	UVMHIST_FUNC("uvn_get"); UVMHIST_CALLED(ubchist);

	UVMHIST_LOG(ubchist, "vp %p off 0x%x", vp, (int)offset, 0,0);

	if (vp->v_type == VREG && (access_type & VM_PROT_WRITE) == 0
	    && (flags & PGO_LOCKED) == 0) {
		vn_ra_allocctx(vp);
		uvm_ra_request(vp->v_ractx, advice, uobj, offset,
		    *npagesp << PAGE_SHIFT);
	}

	error = VOP_GETPAGES(vp, offset, pps, npagesp, centeridx,
			     access_type, advice, flags);

	KASSERT(((flags & PGO_LOCKED) != 0 && mutex_owned(vp->v_interlock)) ||
	    (flags & PGO_LOCKED) == 0);
	return error;
}


/*
 * uvn_findpages:
 * return the page for the uobj and offset requested, allocating if needed.
 * => uobj must be locked.
 * => returned pages will be BUSY.
 */

int
uvn_findpages(struct uvm_object *uobj, voff_t offset, int *npagesp,
    struct vm_page **pgs, int flags)
{
	int i, count, found, npages, rv;

	count = found = 0;
	npages = *npagesp;
	if (flags & UFP_BACKWARD) {
		for (i = npages - 1; i >= 0; i--, offset -= PAGE_SIZE) {
			rv = uvn_findpage(uobj, offset, &pgs[i], flags);
			if (rv == 0) {
				if (flags & UFP_DIRTYONLY)
					break;
			} else
				found++;
			count++;
		}
	} else {
		for (i = 0; i < npages; i++, offset += PAGE_SIZE) {
			rv = uvn_findpage(uobj, offset, &pgs[i], flags);
			if (rv == 0) {
				if (flags & UFP_DIRTYONLY)
					break;
			} else
				found++;
			count++;
		}
	}
	*npagesp = count;
	return (found);
}

static int
uvn_findpage(struct uvm_object *uobj, voff_t offset, struct vm_page **pgp,
    int flags)
{
	struct vm_page *pg;
	bool dirty;
	UVMHIST_FUNC("uvn_findpage"); UVMHIST_CALLED(ubchist);
	UVMHIST_LOG(ubchist, "vp %p off 0x%lx", uobj, offset,0,0);

	KASSERT(mutex_owned(uobj->vmobjlock));

	if (*pgp != NULL) {
		UVMHIST_LOG(ubchist, "dontcare", 0,0,0,0);
		return 0;
	}
	for (;;) {
		/* look for an existing page */
		pg = uvm_pagelookup(uobj, offset);

		/* nope?  allocate one now */
		if (pg == NULL) {
			if (flags & UFP_NOALLOC) {
				UVMHIST_LOG(ubchist, "noalloc", 0,0,0,0);
				return 0;
			}
			pg = uvm_pagealloc(uobj, offset, NULL,
			    UVM_FLAG_COLORMATCH);
			if (pg == NULL) {
				if (flags & UFP_NOWAIT) {
					UVMHIST_LOG(ubchist, "nowait",0,0,0,0);
					return 0;
				}
				mutex_exit(uobj->vmobjlock);
				uvm_wait("uvn_fp1");
				mutex_enter(uobj->vmobjlock);
				continue;
			}
			UVMHIST_LOG(ubchist, "alloced %p (color %u)", pg,
			    VM_PGCOLOR_BUCKET(pg), 0,0);
			break;
		} else if (flags & UFP_NOCACHE) {
			UVMHIST_LOG(ubchist, "nocache",0,0,0,0);
			return 0;
		}

		/* page is there, see if we need to wait on it */
		if ((pg->flags & PG_BUSY) != 0) {
			if (flags & UFP_NOWAIT) {
				UVMHIST_LOG(ubchist, "nowait",0,0,0,0);
				return 0;
			}
			pg->flags |= PG_WANTED;
			UVMHIST_LOG(ubchist, "wait %p (color %u)", pg,
			    VM_PGCOLOR_BUCKET(pg), 0,0);
			UVM_UNLOCK_AND_WAIT(pg, uobj->vmobjlock, 0,
					    "uvn_fp2", 0);
			mutex_enter(uobj->vmobjlock);
			continue;
		}

		/* skip PG_RDONLY pages if requested */
		if ((flags & UFP_NORDONLY) && (pg->flags & PG_RDONLY)) {
			UVMHIST_LOG(ubchist, "nordonly",0,0,0,0);
			return 0;
		}

		/* stop on clean pages if requested */
		if (flags & UFP_DIRTYONLY) {
			dirty = pmap_clear_modify(pg) ||
				(pg->flags & PG_CLEAN) == 0;
			pg->flags |= PG_CLEAN;
			if (!dirty) {
				UVMHIST_LOG(ubchist, "dirtonly", 0,0,0,0);
				return 0;
			}
		}

		/* mark the page BUSY and we're done. */
		pg->flags |= PG_BUSY;
		UVM_PAGE_OWN(pg, "uvn_findpage");
		UVMHIST_LOG(ubchist, "found %p (color %u)",
		    pg, VM_PGCOLOR_BUCKET(pg), 0,0);
		break;
	}
	*pgp = pg;
	return 1;
}

/*
 * uvm_vnp_setsize: grow or shrink a vnode uobj
 *
 * grow   => just update size value
 * shrink => toss un-needed pages
 *
 * => we assume that the caller has a reference of some sort to the
 *	vnode in question so that it will not be yanked out from under
 *	us.
 */

void
uvm_vnp_setsize(struct vnode *vp, voff_t newsize)
{
	struct uvm_object *uobj = &vp->v_uobj;
	voff_t pgend = round_page(newsize);
	voff_t oldsize;
	UVMHIST_FUNC("uvm_vnp_setsize"); UVMHIST_CALLED(ubchist);

	mutex_enter(uobj->vmobjlock);
	UVMHIST_LOG(ubchist, "vp %p old 0x%x new 0x%x",
	    vp, vp->v_size, newsize, 0);

	/*
	 * now check if the size has changed: if we shrink we had better
	 * toss some pages...
	 */

	KASSERT(newsize != VSIZENOTSET);
	KASSERT(vp->v_size <= vp->v_writesize);
	KASSERT(vp->v_size == vp->v_writesize ||
	    newsize == vp->v_writesize || newsize <= vp->v_size);

	oldsize = vp->v_writesize;
	KASSERT(oldsize != VSIZENOTSET || pgend > oldsize);

	if (oldsize > pgend) {
		(void) uvn_put(uobj, pgend, 0, PGO_FREE | PGO_SYNCIO);
		mutex_enter(uobj->vmobjlock);
	}
	vp->v_size = vp->v_writesize = newsize;
	mutex_exit(uobj->vmobjlock);
}

void
uvm_vnp_setwritesize(struct vnode *vp, voff_t newsize)
{

	mutex_enter(vp->v_interlock);
	KASSERT(newsize != VSIZENOTSET);
	KASSERT(vp->v_size != VSIZENOTSET);
	KASSERT(vp->v_writesize != VSIZENOTSET);
	KASSERT(vp->v_size <= vp->v_writesize);
	KASSERT(vp->v_size <= newsize);
	vp->v_writesize = newsize;
	mutex_exit(vp->v_interlock);
}

bool
uvn_text_p(struct uvm_object *uobj)
{
	struct vnode *vp = (struct vnode *)uobj;

	return (vp->v_iflag & VI_EXECMAP) != 0;
}

bool
uvn_clean_p(struct uvm_object *uobj)
{
	struct vnode *vp = (struct vnode *)uobj;

	return (vp->v_iflag & VI_ONWORKLST) == 0;
}

bool
uvn_needs_writefault_p(struct uvm_object *uobj)
{
	struct vnode *vp = (struct vnode *)uobj;

	return uvn_clean_p(uobj) ||
	    (vp->v_iflag & (VI_WRMAP|VI_WRMAPDIRTY)) == VI_WRMAP;
}
