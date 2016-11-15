/*	$NetBSD: genfs_io.c,v 1.61 2015/05/06 15:57:08 hannken Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: genfs_io.c,v 1.61 2015/05/06 15:57:08 hannken Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/kmem.h>
#include <sys/kauth.h>
#include <sys/fstrans.h>
#include <sys/buf.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/genfs/genfs_node.h>
#include <miscfs/specfs/specdev.h>

#include <uvm/uvm.h>
#include <uvm/uvm_pager.h>

static int genfs_do_directio(struct vmspace *, vaddr_t, size_t, struct vnode *,
    off_t, enum uio_rw);
static void genfs_dio_iodone(struct buf *);

static int genfs_getpages_read(struct vnode *, struct vm_page **, int, off_t,
    off_t, bool, bool, bool, bool);
static int genfs_do_io(struct vnode *, off_t, vaddr_t, size_t, int, enum uio_rw,
    void (*)(struct buf *));
static void genfs_rel_pages(struct vm_page **, unsigned int);
static void genfs_markdirty(struct vnode *);

int genfs_maxdio = MAXPHYS;

static void
genfs_rel_pages(struct vm_page **pgs, unsigned int npages)
{
	unsigned int i;

	for (i = 0; i < npages; i++) {
		struct vm_page *pg = pgs[i];

		if (pg == NULL || pg == PGO_DONTCARE)
			continue;
		KASSERT(uvm_page_locked_p(pg));
		if (pg->flags & PG_FAKE) {
			pg->flags |= PG_RELEASED;
		}
	}
	mutex_enter(&uvm_pageqlock);
	uvm_page_unbusy(pgs, npages);
	mutex_exit(&uvm_pageqlock);
}

static void
genfs_markdirty(struct vnode *vp)
{
	struct genfs_node * const gp = VTOG(vp);

	KASSERT(mutex_owned(vp->v_interlock));
	gp->g_dirtygen++;
	if ((vp->v_iflag & VI_ONWORKLST) == 0) {
		vn_syncer_add_to_worklist(vp, filedelay);
	}
	if ((vp->v_iflag & (VI_WRMAP|VI_WRMAPDIRTY)) == VI_WRMAP) {
		vp->v_iflag |= VI_WRMAPDIRTY;
	}
}

/*
 * generic VM getpages routine.
 * Return PG_BUSY pages for the given range,
 * reading from backing store if necessary.
 */

int
genfs_getpages(void *v)
{
	struct vop_getpages_args /* {
		struct vnode *a_vp;
		voff_t a_offset;
		struct vm_page **a_m;
		int *a_count;
		int a_centeridx;
		vm_prot_t a_access_type;
		int a_advice;
		int a_flags;
	} */ * const ap = v;

	off_t diskeof, memeof;
	int i, error, npages;
	const int flags = ap->a_flags;
	struct vnode * const vp = ap->a_vp;
	struct uvm_object * const uobj = &vp->v_uobj;
	const bool async = (flags & PGO_SYNCIO) == 0;
	const bool memwrite = (ap->a_access_type & VM_PROT_WRITE) != 0;
	const bool overwrite = (flags & PGO_OVERWRITE) != 0;
	const bool blockalloc = memwrite && (flags & PGO_NOBLOCKALLOC) == 0;
	const bool glocked = (flags & PGO_GLOCKHELD) != 0;
	const bool need_wapbl = blockalloc && vp->v_mount->mnt_wapbl;
	bool has_trans_wapbl = false;
	UVMHIST_FUNC("genfs_getpages"); UVMHIST_CALLED(ubchist);

	UVMHIST_LOG(ubchist, "vp %p off 0x%x/%x count %d",
	    vp, ap->a_offset >> 32, ap->a_offset, *ap->a_count);

	KASSERT(vp->v_type == VREG || vp->v_type == VDIR ||
	    vp->v_type == VLNK || vp->v_type == VBLK);

startover:
	error = 0;
	const voff_t origvsize = vp->v_size;
	const off_t origoffset = ap->a_offset;
	const int orignpages = *ap->a_count;

	GOP_SIZE(vp, origvsize, &diskeof, 0);
	if (flags & PGO_PASTEOF) {
		off_t newsize;
#if defined(DIAGNOSTIC)
		off_t writeeof;
#endif /* defined(DIAGNOSTIC) */

		newsize = MAX(origvsize,
		    origoffset + (orignpages << PAGE_SHIFT));
		GOP_SIZE(vp, newsize, &memeof, GOP_SIZE_MEM);
#if defined(DIAGNOSTIC)
		GOP_SIZE(vp, vp->v_writesize, &writeeof, GOP_SIZE_MEM);
		if (newsize > round_page(writeeof)) {
			panic("%s: past eof: %" PRId64 " vs. %" PRId64,
			    __func__, newsize, round_page(writeeof));
		}
#endif /* defined(DIAGNOSTIC) */
	} else {
		GOP_SIZE(vp, origvsize, &memeof, GOP_SIZE_MEM);
	}
	KASSERT(ap->a_centeridx >= 0 || ap->a_centeridx <= orignpages);
	KASSERT((origoffset & (PAGE_SIZE - 1)) == 0 && origoffset >= 0);
	KASSERT(orignpages > 0);

	/*
	 * Bounds-check the request.
	 */

	if (origoffset + (ap->a_centeridx << PAGE_SHIFT) >= memeof) {
		if ((flags & PGO_LOCKED) == 0) {
			mutex_exit(uobj->vmobjlock);
		}
		UVMHIST_LOG(ubchist, "off 0x%x count %d goes past EOF 0x%x",
		    origoffset, *ap->a_count, memeof,0);
		error = EINVAL;
		goto out_err;
	}

	/* uobj is locked */

	if ((flags & PGO_NOTIMESTAMP) == 0 &&
	    (vp->v_type != VBLK ||
	    (vp->v_mount->mnt_flag & MNT_NODEVMTIME) == 0)) {
		int updflags = 0;

		if ((vp->v_mount->mnt_flag & MNT_NOATIME) == 0) {
			updflags = GOP_UPDATE_ACCESSED;
		}
		if (memwrite) {
			updflags |= GOP_UPDATE_MODIFIED;
		}
		if (updflags != 0) {
			GOP_MARKUPDATE(vp, updflags);
		}
	}

	/*
	 * For PGO_LOCKED requests, just return whatever's in memory.
	 */

	if (flags & PGO_LOCKED) {
		int nfound;
		struct vm_page *pg;

		KASSERT(!glocked);
		npages = *ap->a_count;
#if defined(DEBUG)
		for (i = 0; i < npages; i++) {
			pg = ap->a_m[i];
			KASSERT(pg == NULL || pg == PGO_DONTCARE);
		}
#endif /* defined(DEBUG) */
		nfound = uvn_findpages(uobj, origoffset, &npages,
		    ap->a_m, UFP_NOWAIT|UFP_NOALLOC|(memwrite ? UFP_NORDONLY : 0));
		KASSERT(npages == *ap->a_count);
		if (nfound == 0) {
			error = EBUSY;
			goto out_err;
		}
		if (!genfs_node_rdtrylock(vp)) {
			genfs_rel_pages(ap->a_m, npages);

			/*
			 * restore the array.
			 */

			for (i = 0; i < npages; i++) {
				pg = ap->a_m[i];

				if (pg != NULL && pg != PGO_DONTCARE) {
					ap->a_m[i] = NULL;
				}
				KASSERT(ap->a_m[i] == NULL ||
				    ap->a_m[i] == PGO_DONTCARE);
			}
		} else {
			genfs_node_unlock(vp);
		}
		error = (ap->a_m[ap->a_centeridx] == NULL ? EBUSY : 0);
		if (error == 0 && memwrite) {
			genfs_markdirty(vp);
		}
		goto out_err;
	}
	mutex_exit(uobj->vmobjlock);

	/*
	 * find the requested pages and make some simple checks.
	 * leave space in the page array for a whole block.
	 */

	const int fs_bshift = (vp->v_type != VBLK) ?
	    vp->v_mount->mnt_fs_bshift : DEV_BSHIFT;
	const int fs_bsize = 1 << fs_bshift;
#define	blk_mask	(fs_bsize - 1)
#define	trunc_blk(x)	((x) & ~blk_mask)
#define	round_blk(x)	(((x) + blk_mask) & ~blk_mask)

	const int orignmempages = MIN(orignpages,
	    round_page(memeof - origoffset) >> PAGE_SHIFT);
	npages = orignmempages;
	const off_t startoffset = trunc_blk(origoffset);
	const off_t endoffset = MIN(
	    round_page(round_blk(origoffset + (npages << PAGE_SHIFT))),
	    round_page(memeof));
	const int ridx = (origoffset - startoffset) >> PAGE_SHIFT;

	const int pgs_size = sizeof(struct vm_page *) *
	    ((endoffset - startoffset) >> PAGE_SHIFT);
	struct vm_page **pgs, *pgs_onstack[UBC_MAX_PAGES];

	if (pgs_size > sizeof(pgs_onstack)) {
		pgs = kmem_zalloc(pgs_size, async ? KM_NOSLEEP : KM_SLEEP);
		if (pgs == NULL) {
			pgs = pgs_onstack;
			error = ENOMEM;
			goto out_err;
		}
	} else {
		pgs = pgs_onstack;
		(void)memset(pgs, 0, pgs_size);
	}

	UVMHIST_LOG(ubchist, "ridx %d npages %d startoff %ld endoff %ld",
	    ridx, npages, startoffset, endoffset);

	if (!has_trans_wapbl) {
		fstrans_start(vp->v_mount, FSTRANS_SHARED);
		/*
		 * XXX: This assumes that we come here only via
		 * the mmio path
		 */
		if (need_wapbl) {
			error = WAPBL_BEGIN(vp->v_mount);
			if (error) {
				fstrans_done(vp->v_mount);
				goto out_err_free;
			}
		}
		has_trans_wapbl = true;
	}

	/*
	 * hold g_glock to prevent a race with truncate.
	 *
	 * check if our idea of v_size is still valid.
	 */

	KASSERT(!glocked || genfs_node_wrlocked(vp));
	if (!glocked) {
		if (blockalloc) {
			genfs_node_wrlock(vp);
		} else {
			genfs_node_rdlock(vp);
		}
	}
	mutex_enter(uobj->vmobjlock);
	if (vp->v_size < origvsize) {
		if (!glocked) {
			genfs_node_unlock(vp);
		}
		if (pgs != pgs_onstack)
			kmem_free(pgs, pgs_size);
		goto startover;
	}

	if (uvn_findpages(uobj, origoffset, &npages, &pgs[ridx],
	    async ? UFP_NOWAIT : UFP_ALL) != orignmempages) {
		if (!glocked) {
			genfs_node_unlock(vp);
		}
		KASSERT(async != 0);
		genfs_rel_pages(&pgs[ridx], orignmempages);
		mutex_exit(uobj->vmobjlock);
		error = EBUSY;
		goto out_err_free;
	}

	/*
	 * if the pages are already resident, just return them.
	 */

	for (i = 0; i < npages; i++) {
		struct vm_page *pg = pgs[ridx + i];

		if ((pg->flags & PG_FAKE) ||
		    (blockalloc && (pg->flags & PG_RDONLY))) {
			break;
		}
	}
	if (i == npages) {
		if (!glocked) {
			genfs_node_unlock(vp);
		}
		UVMHIST_LOG(ubchist, "returning cached pages", 0,0,0,0);
		npages += ridx;
		goto out;
	}

	/*
	 * if PGO_OVERWRITE is set, don't bother reading the pages.
	 */

	if (overwrite) {
		if (!glocked) {
			genfs_node_unlock(vp);
		}
		UVMHIST_LOG(ubchist, "PGO_OVERWRITE",0,0,0,0);

		for (i = 0; i < npages; i++) {
			struct vm_page *pg = pgs[ridx + i];

			pg->flags &= ~(PG_RDONLY|PG_CLEAN);
		}
		npages += ridx;
		goto out;
	}

	/*
	 * the page wasn't resident and we're not overwriting,
	 * so we're going to have to do some i/o.
	 * find any additional pages needed to cover the expanded range.
	 */

	npages = (endoffset - startoffset) >> PAGE_SHIFT;
	if (startoffset != origoffset || npages != orignmempages) {
		int npgs;

		/*
		 * we need to avoid deadlocks caused by locking
		 * additional pages at lower offsets than pages we
		 * already have locked.  unlock them all and start over.
		 */

		genfs_rel_pages(&pgs[ridx], orignmempages);
		memset(pgs, 0, pgs_size);

		UVMHIST_LOG(ubchist, "reset npages start 0x%x end 0x%x",
		    startoffset, endoffset, 0,0);
		npgs = npages;
		if (uvn_findpages(uobj, startoffset, &npgs, pgs,
		    async ? UFP_NOWAIT : UFP_ALL) != npages) {
			if (!glocked) {
				genfs_node_unlock(vp);
			}
			KASSERT(async != 0);
			genfs_rel_pages(pgs, npages);
			mutex_exit(uobj->vmobjlock);
			error = EBUSY;
			goto out_err_free;
		}
	}

	mutex_exit(uobj->vmobjlock);
	error = genfs_getpages_read(vp, pgs, npages, startoffset, diskeof,
	    async, memwrite, blockalloc, glocked);
	if (error == 0 && async)
		goto out_err_free;
	if (!glocked) {
		genfs_node_unlock(vp);
	}
	mutex_enter(uobj->vmobjlock);

	/*
	 * we're almost done!  release the pages...
	 * for errors, we free the pages.
	 * otherwise we activate them and mark them as valid and clean.
	 * also, unbusy pages that were not actually requested.
	 */

	if (error) {
		genfs_rel_pages(pgs, npages);
		mutex_exit(uobj->vmobjlock);
		UVMHIST_LOG(ubchist, "returning error %d", error,0,0,0);
		goto out_err_free;
	}

out:
	UVMHIST_LOG(ubchist, "succeeding, npages %d", npages,0,0,0);
	error = 0;
	mutex_enter(&uvm_pageqlock);
	for (i = 0; i < npages; i++) {
		struct vm_page *pg = pgs[i];
		if (pg == NULL) {
			continue;
		}
		UVMHIST_LOG(ubchist, "examining pg %p flags 0x%x",
		    pg, pg->flags, 0,0);
		if (pg->flags & PG_FAKE && !overwrite) {
			pg->flags &= ~(PG_FAKE);
			pmap_clear_modify(pgs[i]);
		}
		KASSERT(!memwrite || !blockalloc || (pg->flags & PG_RDONLY) == 0);
		if (i < ridx || i >= ridx + orignmempages || async) {
			UVMHIST_LOG(ubchist, "unbusy pg %p offset 0x%x",
			    pg, pg->offset,0,0);
			if (pg->flags & PG_WANTED) {
				wakeup(pg);
			}
			if (pg->flags & PG_FAKE) {
				KASSERT(overwrite);
				uvm_pagezero(pg);
			}
			if (pg->flags & PG_RELEASED) {
				uvm_pagefree(pg);
				continue;
			}
			uvm_pageenqueue(pg);
			pg->flags &= ~(PG_WANTED|PG_BUSY|PG_FAKE);
			UVM_PAGE_OWN(pg, NULL);
		}
	}
	mutex_exit(&uvm_pageqlock);
	if (memwrite) {
		genfs_markdirty(vp);
	}
	mutex_exit(uobj->vmobjlock);
	if (ap->a_m != NULL) {
		memcpy(ap->a_m, &pgs[ridx],
		    orignmempages * sizeof(struct vm_page *));
	}

out_err_free:
	if (pgs != NULL && pgs != pgs_onstack)
		kmem_free(pgs, pgs_size);
out_err:
	if (has_trans_wapbl) {
		if (need_wapbl)
			WAPBL_END(vp->v_mount);
		fstrans_done(vp->v_mount);
	}
	return error;
}

/*
 * genfs_getpages_read: Read the pages in with VOP_BMAP/VOP_STRATEGY.
 */
static int
genfs_getpages_read(struct vnode *vp, struct vm_page **pgs, int npages,
    off_t startoffset, off_t diskeof,
    bool async, bool memwrite, bool blockalloc, bool glocked)
{
	struct uvm_object * const uobj = &vp->v_uobj;
	const int fs_bshift = (vp->v_type != VBLK) ?
	    vp->v_mount->mnt_fs_bshift : DEV_BSHIFT;
	const int dev_bshift = (vp->v_type != VBLK) ?
	    vp->v_mount->mnt_dev_bshift : DEV_BSHIFT;
	kauth_cred_t const cred = curlwp->l_cred;		/* XXXUBC curlwp */
	size_t bytes, iobytes, tailstart, tailbytes, totalbytes, skipbytes;
	vaddr_t kva;
	struct buf *bp, *mbp;
	bool sawhole = false;
	int i;
	int error = 0;

	UVMHIST_FUNC(__func__); UVMHIST_CALLED(ubchist);

	/*
	 * read the desired page(s).
	 */

	totalbytes = npages << PAGE_SHIFT;
	bytes = MIN(totalbytes, MAX(diskeof - startoffset, 0));
	tailbytes = totalbytes - bytes;
	skipbytes = 0;

	kva = uvm_pagermapin(pgs, npages,
	    UVMPAGER_MAPIN_READ | (async ? 0 : UVMPAGER_MAPIN_WAITOK));
	if (kva == 0)
		return EBUSY;

	mbp = getiobuf(vp, true);
	mbp->b_bufsize = totalbytes;
	mbp->b_data = (void *)kva;
	mbp->b_resid = mbp->b_bcount = bytes;
	mbp->b_cflags = BC_BUSY;
	if (async) {
		mbp->b_flags = B_READ | B_ASYNC;
		mbp->b_iodone = uvm_aio_biodone;
	} else {
		mbp->b_flags = B_READ;
		mbp->b_iodone = NULL;
	}
	if (async)
		BIO_SETPRIO(mbp, BPRIO_TIMELIMITED);
	else
		BIO_SETPRIO(mbp, BPRIO_TIMECRITICAL);

	/*
	 * if EOF is in the middle of the range, zero the part past EOF.
	 * skip over pages which are not PG_FAKE since in that case they have
	 * valid data that we need to preserve.
	 */

	tailstart = bytes;
	while (tailbytes > 0) {
		const int len = PAGE_SIZE - (tailstart & PAGE_MASK);

		KASSERT(len <= tailbytes);
		if ((pgs[tailstart >> PAGE_SHIFT]->flags & PG_FAKE) != 0) {
			memset((void *)(kva + tailstart), 0, len);
			UVMHIST_LOG(ubchist, "tailbytes %p 0x%x 0x%x",
			    kva, tailstart, len, 0);
		}
		tailstart += len;
		tailbytes -= len;
	}

	/*
	 * now loop over the pages, reading as needed.
	 */

	bp = NULL;
	off_t offset;
	for (offset = startoffset;
	    bytes > 0;
	    offset += iobytes, bytes -= iobytes) {
		int run;
		daddr_t lbn, blkno;
		int pidx;
		struct vnode *devvp;

		/*
		 * skip pages which don't need to be read.
		 */

		pidx = (offset - startoffset) >> PAGE_SHIFT;
		while ((pgs[pidx]->flags & PG_FAKE) == 0) {
			size_t b;

			KASSERT((offset & (PAGE_SIZE - 1)) == 0);
			if ((pgs[pidx]->flags & PG_RDONLY)) {
				sawhole = true;
			}
			b = MIN(PAGE_SIZE, bytes);
			offset += b;
			bytes -= b;
			skipbytes += b;
			pidx++;
			UVMHIST_LOG(ubchist, "skipping, new offset 0x%x",
			    offset, 0,0,0);
			if (bytes == 0) {
				goto loopdone;
			}
		}

		/*
		 * bmap the file to find out the blkno to read from and
		 * how much we can read in one i/o.  if bmap returns an error,
		 * skip the rest of the top-level i/o.
		 */

		lbn = offset >> fs_bshift;
		error = VOP_BMAP(vp, lbn, &devvp, &blkno, &run);
		if (error) {
			UVMHIST_LOG(ubchist, "VOP_BMAP lbn 0x%x -> %d\n",
			    lbn,error,0,0);
			skipbytes += bytes;
			bytes = 0;
			goto loopdone;
		}

		/*
		 * see how many pages can be read with this i/o.
		 * reduce the i/o size if necessary to avoid
		 * overwriting pages with valid data.
		 */

		iobytes = MIN((((off_t)lbn + 1 + run) << fs_bshift) - offset,
		    bytes);
		if (offset + iobytes > round_page(offset)) {
			int pcount;

			pcount = 1;
			while (pidx + pcount < npages &&
			    pgs[pidx + pcount]->flags & PG_FAKE) {
				pcount++;
			}
			iobytes = MIN(iobytes, (pcount << PAGE_SHIFT) -
			    (offset - trunc_page(offset)));
		}

		/*
		 * if this block isn't allocated, zero it instead of
		 * reading it.  unless we are going to allocate blocks,
		 * mark the pages we zeroed PG_RDONLY.
		 */

		if (blkno == (daddr_t)-1) {
			int holepages = (round_page(offset + iobytes) -
			    trunc_page(offset)) >> PAGE_SHIFT;
			UVMHIST_LOG(ubchist, "lbn 0x%x -> HOLE", lbn,0,0,0);

			sawhole = true;
			memset((char *)kva + (offset - startoffset), 0,
			    iobytes);
			skipbytes += iobytes;

			mutex_enter(uobj->vmobjlock);
			for (i = 0; i < holepages; i++) {
				if (memwrite) {
					pgs[pidx + i]->flags &= ~PG_CLEAN;
				}
				if (!blockalloc) {
					pgs[pidx + i]->flags |= PG_RDONLY;
				}
			}
			mutex_exit(uobj->vmobjlock);
			continue;
		}

		/*
		 * allocate a sub-buf for this piece of the i/o
		 * (or just use mbp if there's only 1 piece),
		 * and start it going.
		 */

		if (offset == startoffset && iobytes == bytes) {
			bp = mbp;
		} else {
			UVMHIST_LOG(ubchist, "vp %p bp %p num now %d",
			    vp, bp, vp->v_numoutput, 0);
			bp = getiobuf(vp, true);
			nestiobuf_setup(mbp, bp, offset - startoffset, iobytes);
		}
		bp->b_lblkno = 0;

		/* adjust physical blkno for partial blocks */
		bp->b_blkno = blkno + ((offset - ((off_t)lbn << fs_bshift)) >>
		    dev_bshift);

		UVMHIST_LOG(ubchist,
		    "bp %p offset 0x%x bcount 0x%x blkno 0x%x",
		    bp, offset, bp->b_bcount, bp->b_blkno);

		VOP_STRATEGY(devvp, bp);
	}

loopdone:
	nestiobuf_done(mbp, skipbytes, error);
	if (async) {
		UVMHIST_LOG(ubchist, "returning 0 (async)",0,0,0,0);
		if (!glocked) {
			genfs_node_unlock(vp);
		}
		return 0;
	}
	if (bp != NULL) {
		error = biowait(mbp);
	}

	/* Remove the mapping (make KVA available as soon as possible) */
	uvm_pagermapout(kva, npages);

	/*
	 * if this we encountered a hole then we have to do a little more work.
	 * for read faults, we marked the page PG_RDONLY so that future
	 * write accesses to the page will fault again.
	 * for write faults, we must make sure that the backing store for
	 * the page is completely allocated while the pages are locked.
	 */

	if (!error && sawhole && blockalloc) {
		error = GOP_ALLOC(vp, startoffset,
		    npages << PAGE_SHIFT, 0, cred);
		UVMHIST_LOG(ubchist, "gop_alloc off 0x%x/0x%x -> %d",
		    startoffset, npages << PAGE_SHIFT, error,0);
		if (!error) {
			mutex_enter(uobj->vmobjlock);
			for (i = 0; i < npages; i++) {
				struct vm_page *pg = pgs[i];

				if (pg == NULL) {
					continue;
				}
				pg->flags &= ~(PG_CLEAN|PG_RDONLY);
				UVMHIST_LOG(ubchist, "mark dirty pg %p",
				    pg,0,0,0);
			}
			mutex_exit(uobj->vmobjlock);
		}
	}

	putiobuf(mbp);
	return error;
}

/*
 * generic VM putpages routine.
 * Write the given range of pages to backing store.
 *
 * => "offhi == 0" means flush all pages at or after "offlo".
 * => object should be locked by caller.  we return with the
 *      object unlocked.
 * => if PGO_CLEANIT or PGO_SYNCIO is set, we may block (due to I/O).
 *	thus, a caller might want to unlock higher level resources
 *	(e.g. vm_map) before calling flush.
 * => if neither PGO_CLEANIT nor PGO_SYNCIO is set, we will not block
 * => if PGO_ALLPAGES is set, then all pages in the object will be processed.
 * => NOTE: we rely on the fact that the object's memq is a TAILQ and
 *	that new pages are inserted on the tail end of the list.   thus,
 *	we can make a complete pass through the object in one go by starting
 *	at the head and working towards the tail (new pages are put in
 *	front of us).
 * => NOTE: we are allowed to lock the page queues, so the caller
 *	must not be holding the page queue lock.
 *
 * note on "cleaning" object and PG_BUSY pages:
 *	this routine is holding the lock on the object.   the only time
 *	that it can run into a PG_BUSY page that it does not own is if
 *	some other process has started I/O on the page (e.g. either
 *	a pagein, or a pageout).    if the PG_BUSY page is being paged
 *	in, then it can not be dirty (!PG_CLEAN) because no one has
 *	had a chance to modify it yet.    if the PG_BUSY page is being
 *	paged out then it means that someone else has already started
 *	cleaning the page for us (how nice!).    in this case, if we
 *	have syncio specified, then after we make our pass through the
 *	object we need to wait for the other PG_BUSY pages to clear
 *	off (i.e. we need to do an iosync).   also note that once a
 *	page is PG_BUSY it must stay in its object until it is un-busyed.
 *
 * note on page traversal:
 *	we can traverse the pages in an object either by going down the
 *	linked list in "uobj->memq", or we can go over the address range
 *	by page doing hash table lookups for each address.    depending
 *	on how many pages are in the object it may be cheaper to do one
 *	or the other.   we set "by_list" to true if we are using memq.
 *	if the cost of a hash lookup was equal to the cost of the list
 *	traversal we could compare the number of pages in the start->stop
 *	range to the total number of pages in the object.   however, it
 *	seems that a hash table lookup is more expensive than the linked
 *	list traversal, so we multiply the number of pages in the
 *	range by an estimate of the relatively higher cost of the hash lookup.
 */

int
genfs_putpages(void *v)
{
	struct vop_putpages_args /* {
		struct vnode *a_vp;
		voff_t a_offlo;
		voff_t a_offhi;
		int a_flags;
	} */ * const ap = v;

	return genfs_do_putpages(ap->a_vp, ap->a_offlo, ap->a_offhi,
	    ap->a_flags, NULL);
}

int
genfs_do_putpages(struct vnode *vp, off_t startoff, off_t endoff,
    int origflags, struct vm_page **busypg)
{
	struct uvm_object * const uobj = &vp->v_uobj;
	kmutex_t * const slock = uobj->vmobjlock;
	off_t off;
	/* Even for strange MAXPHYS, the shift rounds down to a page */
#define maxpages (MAXPHYS >> PAGE_SHIFT)
	int i, error, npages, nback;
	int freeflag;
	struct vm_page *pgs[maxpages], *pg, *nextpg, *tpg, curmp, endmp;
	bool wasclean, by_list, needs_clean, yld;
	bool async = (origflags & PGO_SYNCIO) == 0;
	bool pagedaemon = curlwp == uvm.pagedaemon_lwp;
	struct lwp * const l = curlwp ? curlwp : &lwp0;
	struct genfs_node * const gp = VTOG(vp);
	int flags;
	int dirtygen;
	bool modified;
	bool need_wapbl;
	bool has_trans;
	bool cleanall;
	bool onworklst;

	UVMHIST_FUNC("genfs_putpages"); UVMHIST_CALLED(ubchist);

	KASSERT(origflags & (PGO_CLEANIT|PGO_FREE|PGO_DEACTIVATE));
	KASSERT((startoff & PAGE_MASK) == 0 && (endoff & PAGE_MASK) == 0);
	KASSERT(startoff < endoff || endoff == 0);

	UVMHIST_LOG(ubchist, "vp %p pages %d off 0x%x len 0x%x",
	    vp, uobj->uo_npages, startoff, endoff - startoff);

	has_trans = false;
	need_wapbl = (!pagedaemon && vp->v_mount && vp->v_mount->mnt_wapbl &&
	    (origflags & PGO_JOURNALLOCKED) == 0);

retry:
	modified = false;
	flags = origflags;
	KASSERT((vp->v_iflag & VI_ONWORKLST) != 0 ||
	    (vp->v_iflag & VI_WRMAPDIRTY) == 0);
	if (uobj->uo_npages == 0) {
		if (vp->v_iflag & VI_ONWORKLST) {
			vp->v_iflag &= ~VI_WRMAPDIRTY;
			if (LIST_FIRST(&vp->v_dirtyblkhd) == NULL)
				vn_syncer_remove_from_worklist(vp);
		}
		if (has_trans) {
			if (need_wapbl)
				WAPBL_END(vp->v_mount);
			fstrans_done(vp->v_mount);
		}
		mutex_exit(slock);
		return (0);
	}

	/*
	 * the vnode has pages, set up to process the request.
	 */

	if (!has_trans && (flags & PGO_CLEANIT) != 0) {
		mutex_exit(slock);
		if (pagedaemon) {
			error = fstrans_start_nowait(vp->v_mount, FSTRANS_LAZY);
			if (error)
				return error;
		} else
			fstrans_start(vp->v_mount, FSTRANS_LAZY);
		if (need_wapbl) {
			error = WAPBL_BEGIN(vp->v_mount);
			if (error) {
				fstrans_done(vp->v_mount);
				return error;
			}
		}
		has_trans = true;
		mutex_enter(slock);
		goto retry;
	}

	error = 0;
	wasclean = (vp->v_numoutput == 0);
	off = startoff;
	if (endoff == 0 || flags & PGO_ALLPAGES) {
		endoff = trunc_page(LLONG_MAX);
	}
	by_list = (uobj->uo_npages <=
	    ((endoff - startoff) >> PAGE_SHIFT) * UVM_PAGE_TREE_PENALTY);

	/*
	 * if this vnode is known not to have dirty pages,
	 * don't bother to clean it out.
	 */

	if ((vp->v_iflag & VI_ONWORKLST) == 0) {
#if !defined(DEBUG)
		if ((flags & (PGO_FREE|PGO_DEACTIVATE)) == 0) {
			goto skip_scan;
		}
#endif /* !defined(DEBUG) */
		flags &= ~PGO_CLEANIT;
	}

	/*
	 * start the loop.  when scanning by list, hold the last page
	 * in the list before we start.  pages allocated after we start
	 * will be added to the end of the list, so we can stop at the
	 * current last page.
	 */

	cleanall = (flags & PGO_CLEANIT) != 0 && wasclean &&
	    startoff == 0 && endoff == trunc_page(LLONG_MAX) &&
	    (vp->v_iflag & VI_ONWORKLST) != 0;
	dirtygen = gp->g_dirtygen;
	freeflag = pagedaemon ? PG_PAGEOUT : PG_RELEASED;
	if (by_list) {
		curmp.flags = PG_MARKER;
		endmp.flags = PG_MARKER;
		pg = TAILQ_FIRST(&uobj->memq);
		TAILQ_INSERT_TAIL(&uobj->memq, &endmp, listq.queue);
	} else {
		pg = uvm_pagelookup(uobj, off);
	}
	nextpg = NULL;
	while (by_list || off < endoff) {

		/*
		 * if the current page is not interesting, move on to the next.
		 */

		KASSERT(pg == NULL || pg->uobject == uobj ||
		    (pg->flags & PG_MARKER) != 0);
		KASSERT(pg == NULL ||
		    (pg->flags & (PG_RELEASED|PG_PAGEOUT)) == 0 ||
		    (pg->flags & (PG_BUSY|PG_MARKER)) != 0);
		if (by_list) {
			if (pg == &endmp) {
				break;
			}
			if (pg->flags & PG_MARKER) {
				pg = TAILQ_NEXT(pg, listq.queue);
				continue;
			}
			if (pg->offset < startoff || pg->offset >= endoff ||
			    pg->flags & (PG_RELEASED|PG_PAGEOUT)) {
				if (pg->flags & (PG_RELEASED|PG_PAGEOUT)) {
					wasclean = false;
				}
				pg = TAILQ_NEXT(pg, listq.queue);
				continue;
			}
			off = pg->offset;
		} else if (pg == NULL || pg->flags & (PG_RELEASED|PG_PAGEOUT)) {
			if (pg != NULL) {
				wasclean = false;
			}
			off += PAGE_SIZE;
			if (off < endoff) {
				pg = uvm_pagelookup(uobj, off);
			}
			continue;
		}

		/*
		 * if the current page needs to be cleaned and it's busy,
		 * wait for it to become unbusy.
		 */

		yld = (l->l_cpu->ci_schedstate.spc_flags &
		    SPCF_SHOULDYIELD) && !pagedaemon;
		if (pg->flags & PG_BUSY || yld) {
			UVMHIST_LOG(ubchist, "busy %p", pg,0,0,0);
			if (flags & PGO_BUSYFAIL && pg->flags & PG_BUSY) {
				UVMHIST_LOG(ubchist, "busyfail %p", pg, 0,0,0);
				error = EDEADLK;
				if (busypg != NULL)
					*busypg = pg;
				break;
			}
			if (pagedaemon) {
				/*
				 * someone has taken the page while we
				 * dropped the lock for fstrans_start.
				 */
				break;
			}
			if (by_list) {
				TAILQ_INSERT_BEFORE(pg, &curmp, listq.queue);
				UVMHIST_LOG(ubchist, "curmp next %p",
				    TAILQ_NEXT(&curmp, listq.queue), 0,0,0);
			}
			if (yld) {
				mutex_exit(slock);
				preempt();
				mutex_enter(slock);
			} else {
				pg->flags |= PG_WANTED;
				UVM_UNLOCK_AND_WAIT(pg, slock, 0, "genput", 0);
				mutex_enter(slock);
			}
			if (by_list) {
				UVMHIST_LOG(ubchist, "after next %p",
				    TAILQ_NEXT(&curmp, listq.queue), 0,0,0);
				pg = TAILQ_NEXT(&curmp, listq.queue);
				TAILQ_REMOVE(&uobj->memq, &curmp, listq.queue);
			} else {
				pg = uvm_pagelookup(uobj, off);
			}
			continue;
		}

		/*
		 * if we're freeing, remove all mappings of the page now.
		 * if we're cleaning, check if the page is needs to be cleaned.
		 */

		if (flags & PGO_FREE) {
			pmap_page_protect(pg, VM_PROT_NONE);
		} else if (flags & PGO_CLEANIT) {

			/*
			 * if we still have some hope to pull this vnode off
			 * from the syncer queue, write-protect the page.
			 */

			if (cleanall && wasclean &&
			    gp->g_dirtygen == dirtygen) {

				/*
				 * uobj pages get wired only by uvm_fault
				 * where uobj is locked.
				 */

				if (pg->wire_count == 0) {
					pmap_page_protect(pg,
					    VM_PROT_READ|VM_PROT_EXECUTE);
				} else {
					cleanall = false;
				}
			}
		}

		if (flags & PGO_CLEANIT) {
			needs_clean = pmap_clear_modify(pg) ||
			    (pg->flags & PG_CLEAN) == 0;
			pg->flags |= PG_CLEAN;
		} else {
			needs_clean = false;
		}

		/*
		 * if we're cleaning, build a cluster.
		 * the cluster will consist of pages which are currently dirty,
		 * but they will be returned to us marked clean.
		 * if not cleaning, just operate on the one page.
		 */

		if (needs_clean) {
			KDASSERT((vp->v_iflag & VI_ONWORKLST));
			wasclean = false;
			memset(pgs, 0, sizeof(pgs));
			pg->flags |= PG_BUSY;
			UVM_PAGE_OWN(pg, "genfs_putpages");

			/*
			 * first look backward.
			 */

			npages = MIN(maxpages >> 1, off >> PAGE_SHIFT);
			nback = npages;
			uvn_findpages(uobj, off - PAGE_SIZE, &nback, &pgs[0],
			    UFP_NOWAIT|UFP_NOALLOC|UFP_DIRTYONLY|UFP_BACKWARD);
			if (nback) {
				memmove(&pgs[0], &pgs[npages - nback],
				    nback * sizeof(pgs[0]));
				if (npages - nback < nback)
					memset(&pgs[nback], 0,
					    (npages - nback) * sizeof(pgs[0]));
				else
					memset(&pgs[npages - nback], 0,
					    nback * sizeof(pgs[0]));
			}

			/*
			 * then plug in our page of interest.
			 */

			pgs[nback] = pg;

			/*
			 * then look forward to fill in the remaining space in
			 * the array of pages.
			 */

			npages = maxpages - nback - 1;
			uvn_findpages(uobj, off + PAGE_SIZE, &npages,
			    &pgs[nback + 1],
			    UFP_NOWAIT|UFP_NOALLOC|UFP_DIRTYONLY);
			npages += nback + 1;
		} else {
			pgs[0] = pg;
			npages = 1;
			nback = 0;
		}

		/*
		 * apply FREE or DEACTIVATE options if requested.
		 */

		if (flags & (PGO_DEACTIVATE|PGO_FREE)) {
			mutex_enter(&uvm_pageqlock);
		}
		for (i = 0; i < npages; i++) {
			tpg = pgs[i];
			KASSERT(tpg->uobject == uobj);
			if (by_list && tpg == TAILQ_NEXT(pg, listq.queue))
				pg = tpg;
			if (tpg->offset < startoff || tpg->offset >= endoff)
				continue;
			if (flags & PGO_DEACTIVATE && tpg->wire_count == 0) {
				uvm_pagedeactivate(tpg);
			} else if (flags & PGO_FREE) {
				pmap_page_protect(tpg, VM_PROT_NONE);
				if (tpg->flags & PG_BUSY) {
					tpg->flags |= freeflag;
					if (pagedaemon) {
						uvm_pageout_start(1);
						uvm_pagedequeue(tpg);
					}
				} else {

					/*
					 * ``page is not busy''
					 * implies that npages is 1
					 * and needs_clean is false.
					 */

					nextpg = TAILQ_NEXT(tpg, listq.queue);
					uvm_pagefree(tpg);
					if (pagedaemon)
						uvmexp.pdfreed++;
				}
			}
		}
		if (flags & (PGO_DEACTIVATE|PGO_FREE)) {
			mutex_exit(&uvm_pageqlock);
		}
		if (needs_clean) {
			modified = true;

			/*
			 * start the i/o.  if we're traversing by list,
			 * keep our place in the list with a marker page.
			 */

			if (by_list) {
				TAILQ_INSERT_AFTER(&uobj->memq, pg, &curmp,
				    listq.queue);
			}
			mutex_exit(slock);
			error = GOP_WRITE(vp, pgs, npages, flags);
			mutex_enter(slock);
			if (by_list) {
				pg = TAILQ_NEXT(&curmp, listq.queue);
				TAILQ_REMOVE(&uobj->memq, &curmp, listq.queue);
			}
			if (error) {
				break;
			}
			if (by_list) {
				continue;
			}
		}

		/*
		 * find the next page and continue if there was no error.
		 */

		if (by_list) {
			if (nextpg) {
				pg = nextpg;
				nextpg = NULL;
			} else {
				pg = TAILQ_NEXT(pg, listq.queue);
			}
		} else {
			off += (npages - nback) << PAGE_SHIFT;
			if (off < endoff) {
				pg = uvm_pagelookup(uobj, off);
			}
		}
	}
	if (by_list) {
		TAILQ_REMOVE(&uobj->memq, &endmp, listq.queue);
	}

	if (modified && (vp->v_iflag & VI_WRMAPDIRTY) != 0 &&
	    (vp->v_type != VBLK ||
	    (vp->v_mount->mnt_flag & MNT_NODEVMTIME) == 0)) {
		GOP_MARKUPDATE(vp, GOP_UPDATE_MODIFIED);
	}

	/*
	 * if we're cleaning and there was nothing to clean,
	 * take us off the syncer list.  if we started any i/o
	 * and we're doing sync i/o, wait for all writes to finish.
	 */

	if (cleanall && wasclean && gp->g_dirtygen == dirtygen &&
	    (vp->v_iflag & VI_ONWORKLST) != 0) {
#if defined(DEBUG)
		TAILQ_FOREACH(pg, &uobj->memq, listq.queue) {
			if ((pg->flags & (PG_FAKE | PG_MARKER)) != 0) {
				continue;
			}
			if ((pg->flags & PG_CLEAN) == 0) {
				printf("%s: %p: !CLEAN\n", __func__, pg);
			}
			if (pmap_is_modified(pg)) {
				printf("%s: %p: modified\n", __func__, pg);
			}
		}
#endif /* defined(DEBUG) */
		vp->v_iflag &= ~VI_WRMAPDIRTY;
		if (LIST_FIRST(&vp->v_dirtyblkhd) == NULL)
			vn_syncer_remove_from_worklist(vp);
	}

#if !defined(DEBUG)
skip_scan:
#endif /* !defined(DEBUG) */

	/* Wait for output to complete. */
	if (!wasclean && !async && vp->v_numoutput != 0) {
		while (vp->v_numoutput != 0)
			cv_wait(&vp->v_cv, slock);
	}
	onworklst = (vp->v_iflag & VI_ONWORKLST) != 0;
	mutex_exit(slock);

	if ((flags & PGO_RECLAIM) != 0 && onworklst) {
		/*
		 * in the case of PGO_RECLAIM, ensure to make the vnode clean.
		 * retrying is not a big deal because, in many cases,
		 * uobj->uo_npages is already 0 here.
		 */
		mutex_enter(slock);
		goto retry;
	}

	if (has_trans) {
		if (need_wapbl)
			WAPBL_END(vp->v_mount);
		fstrans_done(vp->v_mount);
	}

	return (error);
}

int
genfs_gop_write(struct vnode *vp, struct vm_page **pgs, int npages, int flags)
{
	off_t off;
	vaddr_t kva;
	size_t len;
	int error;
	UVMHIST_FUNC(__func__); UVMHIST_CALLED(ubchist);

	UVMHIST_LOG(ubchist, "vp %p pgs %p npages %d flags 0x%x",
	    vp, pgs, npages, flags);

	off = pgs[0]->offset;
	kva = uvm_pagermapin(pgs, npages,
	    UVMPAGER_MAPIN_WRITE | UVMPAGER_MAPIN_WAITOK);
	len = npages << PAGE_SHIFT;

	error = genfs_do_io(vp, off, kva, len, flags, UIO_WRITE,
			    uvm_aio_biodone);

	return error;
}

int
genfs_gop_write_rwmap(struct vnode *vp, struct vm_page **pgs, int npages, int flags)
{
	off_t off;
	vaddr_t kva;
	size_t len;
	int error;
	UVMHIST_FUNC(__func__); UVMHIST_CALLED(ubchist);

	UVMHIST_LOG(ubchist, "vp %p pgs %p npages %d flags 0x%x",
	    vp, pgs, npages, flags);

	off = pgs[0]->offset;
	kva = uvm_pagermapin(pgs, npages,
	    UVMPAGER_MAPIN_READ | UVMPAGER_MAPIN_WAITOK);
	len = npages << PAGE_SHIFT;

	error = genfs_do_io(vp, off, kva, len, flags, UIO_WRITE,
			    uvm_aio_biodone);

	return error;
}

/*
 * Backend routine for doing I/O to vnode pages.  Pages are already locked
 * and mapped into kernel memory.  Here we just look up the underlying
 * device block addresses and call the strategy routine.
 */

static int
genfs_do_io(struct vnode *vp, off_t off, vaddr_t kva, size_t len, int flags,
    enum uio_rw rw, void (*iodone)(struct buf *))
{
	int s, error;
	int fs_bshift, dev_bshift;
	off_t eof, offset, startoffset;
	size_t bytes, iobytes, skipbytes;
	struct buf *mbp, *bp;
	const bool async = (flags & PGO_SYNCIO) == 0;
	const bool lazy = (flags & PGO_LAZY) == 0;
	const bool iowrite = rw == UIO_WRITE;
	const int brw = iowrite ? B_WRITE : B_READ;
	UVMHIST_FUNC(__func__); UVMHIST_CALLED(ubchist);

	UVMHIST_LOG(ubchist, "vp %p kva %p len 0x%x flags 0x%x",
	    vp, kva, len, flags);

	KASSERT(vp->v_size <= vp->v_writesize);
	GOP_SIZE(vp, vp->v_writesize, &eof, 0);
	if (vp->v_type != VBLK) {
		fs_bshift = vp->v_mount->mnt_fs_bshift;
		dev_bshift = vp->v_mount->mnt_dev_bshift;
	} else {
		fs_bshift = DEV_BSHIFT;
		dev_bshift = DEV_BSHIFT;
	}
	error = 0;
	startoffset = off;
	bytes = MIN(len, eof - startoffset);
	skipbytes = 0;
	KASSERT(bytes != 0);

	if (iowrite) {
		mutex_enter(vp->v_interlock);
		vp->v_numoutput += 2;
		mutex_exit(vp->v_interlock);
	}
	mbp = getiobuf(vp, true);
	UVMHIST_LOG(ubchist, "vp %p mbp %p num now %d bytes 0x%x",
	    vp, mbp, vp->v_numoutput, bytes);
	mbp->b_bufsize = len;
	mbp->b_data = (void *)kva;
	mbp->b_resid = mbp->b_bcount = bytes;
	mbp->b_cflags = BC_BUSY | BC_AGE;
	if (async) {
		mbp->b_flags = brw | B_ASYNC;
		mbp->b_iodone = iodone;
	} else {
		mbp->b_flags = brw;
		mbp->b_iodone = NULL;
	}
	if (curlwp == uvm.pagedaemon_lwp)
		BIO_SETPRIO(mbp, BPRIO_TIMELIMITED);
	else if (async || lazy)
		BIO_SETPRIO(mbp, BPRIO_TIMENONCRITICAL);
	else
		BIO_SETPRIO(mbp, BPRIO_TIMECRITICAL);

	bp = NULL;
	for (offset = startoffset;
	    bytes > 0;
	    offset += iobytes, bytes -= iobytes) {
		int run;
		daddr_t lbn, blkno;
		struct vnode *devvp;

		/*
		 * bmap the file to find out the blkno to read from and
		 * how much we can read in one i/o.  if bmap returns an error,
		 * skip the rest of the top-level i/o.
		 */

		lbn = offset >> fs_bshift;
		error = VOP_BMAP(vp, lbn, &devvp, &blkno, &run);
		if (error) {
			UVMHIST_LOG(ubchist, "VOP_BMAP lbn 0x%x -> %d\n",
			    lbn,error,0,0);
			skipbytes += bytes;
			bytes = 0;
			goto loopdone;
		}

		/*
		 * see how many pages can be read with this i/o.
		 * reduce the i/o size if necessary to avoid
		 * overwriting pages with valid data.
		 */

		iobytes = MIN((((off_t)lbn + 1 + run) << fs_bshift) - offset,
		    bytes);

		/*
		 * if this block isn't allocated, zero it instead of
		 * reading it.  unless we are going to allocate blocks,
		 * mark the pages we zeroed PG_RDONLY.
		 */

		if (blkno == (daddr_t)-1) {
			if (!iowrite) {
				memset((char *)kva + (offset - startoffset), 0,
				    iobytes);
			}
			skipbytes += iobytes;
			continue;
		}

		/*
		 * allocate a sub-buf for this piece of the i/o
		 * (or just use mbp if there's only 1 piece),
		 * and start it going.
		 */

		if (offset == startoffset && iobytes == bytes) {
			bp = mbp;
		} else {
			UVMHIST_LOG(ubchist, "vp %p bp %p num now %d",
			    vp, bp, vp->v_numoutput, 0);
			bp = getiobuf(vp, true);
			nestiobuf_setup(mbp, bp, offset - startoffset, iobytes);
		}
		bp->b_lblkno = 0;

		/* adjust physical blkno for partial blocks */
		bp->b_blkno = blkno + ((offset - ((off_t)lbn << fs_bshift)) >>
		    dev_bshift);

		UVMHIST_LOG(ubchist,
		    "bp %p offset 0x%x bcount 0x%x blkno 0x%x",
		    bp, offset, bp->b_bcount, bp->b_blkno);

		VOP_STRATEGY(devvp, bp);
	}

loopdone:
	if (skipbytes) {
		UVMHIST_LOG(ubchist, "skipbytes %d", skipbytes, 0,0,0);
	}
	nestiobuf_done(mbp, skipbytes, error);
	if (async) {
		UVMHIST_LOG(ubchist, "returning 0 (async)", 0,0,0,0);
		return (0);
	}
	UVMHIST_LOG(ubchist, "waiting for mbp %p", mbp,0,0,0);
	error = biowait(mbp);
	s = splbio();
	(*iodone)(mbp);
	splx(s);
	UVMHIST_LOG(ubchist, "returning, error %d", error,0,0,0);
	return (error);
}

int
genfs_compat_getpages(void *v)
{
	struct vop_getpages_args /* {
		struct vnode *a_vp;
		voff_t a_offset;
		struct vm_page **a_m;
		int *a_count;
		int a_centeridx;
		vm_prot_t a_access_type;
		int a_advice;
		int a_flags;
	} */ *ap = v;

	off_t origoffset;
	struct vnode *vp = ap->a_vp;
	struct uvm_object *uobj = &vp->v_uobj;
	struct vm_page *pg, **pgs;
	vaddr_t kva;
	int i, error, orignpages, npages;
	struct iovec iov;
	struct uio uio;
	kauth_cred_t cred = curlwp->l_cred;
	const bool memwrite = (ap->a_access_type & VM_PROT_WRITE) != 0;

	error = 0;
	origoffset = ap->a_offset;
	orignpages = *ap->a_count;
	pgs = ap->a_m;

	if (ap->a_flags & PGO_LOCKED) {
		uvn_findpages(uobj, origoffset, ap->a_count, ap->a_m,
		    UFP_NOWAIT|UFP_NOALLOC| (memwrite ? UFP_NORDONLY : 0));

		error = ap->a_m[ap->a_centeridx] == NULL ? EBUSY : 0;
		if (error == 0 && memwrite) {
			genfs_markdirty(vp);
		}
		return error;
	}
	if (origoffset + (ap->a_centeridx << PAGE_SHIFT) >= vp->v_size) {
		mutex_exit(uobj->vmobjlock);
		return EINVAL;
	}
	if ((ap->a_flags & PGO_SYNCIO) == 0) {
		mutex_exit(uobj->vmobjlock);
		return 0;
	}
	npages = orignpages;
	uvn_findpages(uobj, origoffset, &npages, pgs, UFP_ALL);
	mutex_exit(uobj->vmobjlock);
	kva = uvm_pagermapin(pgs, npages,
	    UVMPAGER_MAPIN_READ | UVMPAGER_MAPIN_WAITOK);
	for (i = 0; i < npages; i++) {
		pg = pgs[i];
		if ((pg->flags & PG_FAKE) == 0) {
			continue;
		}
		iov.iov_base = (char *)kva + (i << PAGE_SHIFT);
		iov.iov_len = PAGE_SIZE;
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_offset = origoffset + (i << PAGE_SHIFT);
		uio.uio_rw = UIO_READ;
		uio.uio_resid = PAGE_SIZE;
		UIO_SETUP_SYSSPACE(&uio);
		/* XXX vn_lock */
		error = VOP_READ(vp, &uio, 0, cred);
		if (error) {
			break;
		}
		if (uio.uio_resid) {
			memset(iov.iov_base, 0, uio.uio_resid);
		}
	}
	uvm_pagermapout(kva, npages);
	mutex_enter(uobj->vmobjlock);
	mutex_enter(&uvm_pageqlock);
	for (i = 0; i < npages; i++) {
		pg = pgs[i];
		if (error && (pg->flags & PG_FAKE) != 0) {
			pg->flags |= PG_RELEASED;
		} else {
			pmap_clear_modify(pg);
			uvm_pageactivate(pg);
		}
	}
	if (error) {
		uvm_page_unbusy(pgs, npages);
	}
	mutex_exit(&uvm_pageqlock);
	if (error == 0 && memwrite) {
		genfs_markdirty(vp);
	}
	mutex_exit(uobj->vmobjlock);
	return error;
}

int
genfs_compat_gop_write(struct vnode *vp, struct vm_page **pgs, int npages,
    int flags)
{
	off_t offset;
	struct iovec iov;
	struct uio uio;
	kauth_cred_t cred = curlwp->l_cred;
	struct buf *bp;
	vaddr_t kva;
	int error;

	offset = pgs[0]->offset;
	kva = uvm_pagermapin(pgs, npages,
	    UVMPAGER_MAPIN_WRITE | UVMPAGER_MAPIN_WAITOK);

	iov.iov_base = (void *)kva;
	iov.iov_len = npages << PAGE_SHIFT;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = offset;
	uio.uio_rw = UIO_WRITE;
	uio.uio_resid = npages << PAGE_SHIFT;
	UIO_SETUP_SYSSPACE(&uio);
	/* XXX vn_lock */
	error = VOP_WRITE(vp, &uio, 0, cred);

	mutex_enter(vp->v_interlock);
	vp->v_numoutput++;
	mutex_exit(vp->v_interlock);

	bp = getiobuf(vp, true);
	bp->b_cflags = BC_BUSY | BC_AGE;
	bp->b_lblkno = offset >> vp->v_mount->mnt_fs_bshift;
	bp->b_data = (char *)kva;
	bp->b_bcount = npages << PAGE_SHIFT;
	bp->b_bufsize = npages << PAGE_SHIFT;
	bp->b_resid = 0;
	bp->b_error = error;
	uvm_aio_aiodone(bp);
	return (error);
}

/*
 * Process a uio using direct I/O.  If we reach a part of the request
 * which cannot be processed in this fashion for some reason, just return.
 * The caller must handle some additional part of the request using
 * buffered I/O before trying direct I/O again.
 */

void
genfs_directio(struct vnode *vp, struct uio *uio, int ioflag)
{
	struct vmspace *vs;
	struct iovec *iov;
	vaddr_t va;
	size_t len;
	const int mask = DEV_BSIZE - 1;
	int error;
	bool need_wapbl = (vp->v_mount && vp->v_mount->mnt_wapbl &&
	    (ioflag & IO_JOURNALLOCKED) == 0);

	/*
	 * We only support direct I/O to user space for now.
	 */

	if (VMSPACE_IS_KERNEL_P(uio->uio_vmspace)) {
		return;
	}

	/*
	 * If the vnode is mapped, we would need to get the getpages lock
	 * to stabilize the bmap, but then we would get into trouble while
	 * locking the pages if the pages belong to this same vnode (or a
	 * multi-vnode cascade to the same effect).  Just fall back to
	 * buffered I/O if the vnode is mapped to avoid this mess.
	 */

	if (vp->v_vflag & VV_MAPPED) {
		return;
	}

	if (need_wapbl) {
		error = WAPBL_BEGIN(vp->v_mount);
		if (error)
			return;
	}

	/*
	 * Do as much of the uio as possible with direct I/O.
	 */

	vs = uio->uio_vmspace;
	while (uio->uio_resid) {
		iov = uio->uio_iov;
		if (iov->iov_len == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			continue;
		}
		va = (vaddr_t)iov->iov_base;
		len = MIN(iov->iov_len, genfs_maxdio);
		len &= ~mask;

		/*
		 * If the next chunk is smaller than DEV_BSIZE or extends past
		 * the current EOF, then fall back to buffered I/O.
		 */

		if (len == 0 || uio->uio_offset + len > vp->v_size) {
			break;
		}

		/*
		 * Check alignment.  The file offset must be at least
		 * sector-aligned.  The exact constraint on memory alignment
		 * is very hardware-dependent, but requiring sector-aligned
		 * addresses there too is safe.
		 */

		if (uio->uio_offset & mask || va & mask) {
			break;
		}
		error = genfs_do_directio(vs, va, len, vp, uio->uio_offset,
					  uio->uio_rw);
		if (error) {
			break;
		}
		iov->iov_base = (char *)iov->iov_base + len;
		iov->iov_len -= len;
		uio->uio_offset += len;
		uio->uio_resid -= len;
	}

	if (need_wapbl)
		WAPBL_END(vp->v_mount);
}

/*
 * Iodone routine for direct I/O.  We don't do much here since the request is
 * always synchronous, so the caller will do most of the work after biowait().
 */

static void
genfs_dio_iodone(struct buf *bp)
{

	KASSERT((bp->b_flags & B_ASYNC) == 0);
	if ((bp->b_flags & B_READ) == 0 && (bp->b_cflags & BC_AGE) != 0) {
		mutex_enter(bp->b_objlock);
		vwakeup(bp);
		mutex_exit(bp->b_objlock);
	}
	putiobuf(bp);
}

/*
 * Process one chunk of a direct I/O request.
 */

static int
genfs_do_directio(struct vmspace *vs, vaddr_t uva, size_t len, struct vnode *vp,
    off_t off, enum uio_rw rw)
{
	struct vm_map *map;
	struct pmap *upm, *kpm __unused;
	size_t klen = round_page(uva + len) - trunc_page(uva);
	off_t spoff, epoff;
	vaddr_t kva, puva;
	paddr_t pa;
	vm_prot_t prot;
	int error, rv __diagused, poff, koff;
	const int pgoflags = PGO_CLEANIT | PGO_SYNCIO | PGO_JOURNALLOCKED |
		(rw == UIO_WRITE ? PGO_FREE : 0);

	/*
	 * For writes, verify that this range of the file already has fully
	 * allocated backing store.  If there are any holes, just punt and
	 * make the caller take the buffered write path.
	 */

	if (rw == UIO_WRITE) {
		daddr_t lbn, elbn, blkno;
		int bsize, bshift, run;

		bshift = vp->v_mount->mnt_fs_bshift;
		bsize = 1 << bshift;
		lbn = off >> bshift;
		elbn = (off + len + bsize - 1) >> bshift;
		while (lbn < elbn) {
			error = VOP_BMAP(vp, lbn, NULL, &blkno, &run);
			if (error) {
				return error;
			}
			if (blkno == (daddr_t)-1) {
				return ENOSPC;
			}
			lbn += 1 + run;
		}
	}

	/*
	 * Flush any cached pages for parts of the file that we're about to
	 * access.  If we're writing, invalidate pages as well.
	 */

	spoff = trunc_page(off);
	epoff = round_page(off + len);
	mutex_enter(vp->v_interlock);
	error = VOP_PUTPAGES(vp, spoff, epoff, pgoflags);
	if (error) {
		return error;
	}

	/*
	 * Wire the user pages and remap them into kernel memory.
	 */

	prot = rw == UIO_READ ? VM_PROT_READ | VM_PROT_WRITE : VM_PROT_READ;
	error = uvm_vslock(vs, (void *)uva, len, prot);
	if (error) {
		return error;
	}

	map = &vs->vm_map;
	upm = vm_map_pmap(map);
	kpm = vm_map_pmap(kernel_map);
	puva = trunc_page(uva);
	kva = uvm_km_alloc(kernel_map, klen, atop(puva) & uvmexp.colormask,
	    UVM_KMF_VAONLY | UVM_KMF_WAITVA | UVM_KMF_COLORMATCH);
	for (poff = 0; poff < klen; poff += PAGE_SIZE) {
		rv = pmap_extract(upm, puva + poff, &pa);
		KASSERT(rv);
		pmap_kenter_pa(kva + poff, pa, prot, PMAP_WIRED);
	}
	pmap_update(kpm);

	/*
	 * Do the I/O.
	 */

	koff = uva - trunc_page(uva);
	error = genfs_do_io(vp, off, kva + koff, len, PGO_SYNCIO, rw,
			    genfs_dio_iodone);

	/*
	 * Tear down the kernel mapping.
	 */

	pmap_kremove(kva, klen);
	pmap_update(kpm);
	uvm_km_free(kernel_map, kva, klen, UVM_KMF_VAONLY);

	/*
	 * Unwire the user pages.
	 */

	uvm_vsunlock(vs, (void *)uva, len);
	return error;
}
