/*	$NetBSD: kern_physio.c,v 1.93 2015/04/21 10:54:52 pooka Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)kern_physio.c	8.1 (Berkeley) 6/10/93
 */

/*-
 * Copyright (c) 1994 Christopher G. Demetriou
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)kern_physio.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_physio.c,v 1.93 2015/04/21 10:54:52 pooka Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/once.h>
#include <sys/workqueue.h>
#include <sys/kmem.h>

#include <uvm/uvm_extern.h>

ONCE_DECL(physio_initialized);
struct workqueue *physio_workqueue;

int physio_concurrency = 16;

/* #define	PHYSIO_DEBUG */
#if defined(PHYSIO_DEBUG)
#define	DPRINTF(a)	printf a
#else /* defined(PHYSIO_DEBUG) */
#define	DPRINTF(a)	/* nothing */
#endif /* defined(PHYSIO_DEBUG) */

struct physio_stat {
	int ps_running;
	int ps_error;
	int ps_failed;
	off_t ps_endoffset;
	buf_t *ps_orig_bp;
	kmutex_t ps_lock;
	kcondvar_t ps_cv;
};

static void
physio_done(struct work *wk, void *dummy)
{
	struct buf *bp = (void *)wk;
	size_t todo = bp->b_bufsize;
	size_t done = bp->b_bcount - bp->b_resid;
	struct physio_stat *ps = bp->b_private;
	bool is_iobuf;

	KASSERT(&bp->b_work == wk);
	KASSERT(bp->b_bcount <= todo);
	KASSERT(bp->b_resid <= bp->b_bcount);
	KASSERT((bp->b_flags & B_PHYS) != 0);
	KASSERT(dummy == NULL);

	vunmapbuf(bp, todo);
	uvm_vsunlock(bp->b_proc->p_vmspace, bp->b_data, todo);

	mutex_enter(&ps->ps_lock);
	is_iobuf = (bp != ps->ps_orig_bp);
	if (__predict_false(done != todo)) {
		off_t endoffset = dbtob(bp->b_blkno) + done;

		/*
		 * we got an error or hit EOM.
		 *
		 * we only care about the first one.
		 * ie. the one at the lowest offset.
		 */

		KASSERT(ps->ps_endoffset != endoffset);
		DPRINTF(("%s: error=%d at %" PRIu64 " - %" PRIu64
		    ", blkno=%" PRIu64 ", bcount=%d, flags=0x%x\n",
		    __func__, bp->b_error, dbtob(bp->b_blkno), endoffset,
		    bp->b_blkno, bp->b_bcount, bp->b_flags));

		if (ps->ps_endoffset == -1 || endoffset < ps->ps_endoffset) {
			DPRINTF(("%s: ps=%p, error %d -> %d, endoff %" PRIu64
			    " -> %" PRIu64 "\n",
			    __func__, ps,
			    ps->ps_error, bp->b_error,
			    ps->ps_endoffset, endoffset));

			ps->ps_endoffset = endoffset;
			ps->ps_error = bp->b_error;
		}
		ps->ps_failed++;
	} else {
		KASSERT(bp->b_error == 0);
	}

	ps->ps_running--;
	cv_signal(&ps->ps_cv);
	mutex_exit(&ps->ps_lock);

	if (is_iobuf)
		putiobuf(bp);
}

static void
physio_biodone(struct buf *bp)
{
#if defined(DIAGNOSTIC)
	struct physio_stat *ps = bp->b_private;
	size_t todo = bp->b_bufsize;
	size_t done = bp->b_bcount - bp->b_resid;

	KASSERT(ps->ps_running > 0);
	KASSERT(bp->b_bcount <= todo);
	KASSERT(bp->b_resid <= bp->b_bcount);
	if (done == todo)
		KASSERT(bp->b_error == 0);
#endif /* defined(DIAGNOSTIC) */

	workqueue_enqueue(physio_workqueue, &bp->b_work, NULL);
}

static void
physio_wait(struct physio_stat *ps, int n)
{

	KASSERT(mutex_owned(&ps->ps_lock));

	while (ps->ps_running > n)
		cv_wait(&ps->ps_cv, &ps->ps_lock);
}

static int
physio_init(void)
{
	int error;

	KASSERT(physio_workqueue == NULL);

	error = workqueue_create(&physio_workqueue, "physiod",
	    physio_done, NULL, PRI_BIO, IPL_BIO, WQ_MPSAFE);

	return error;
}

/*
 * Do "physical I/O" on behalf of a user.  "Physical I/O" is I/O directly
 * from the raw device to user buffers, and bypasses the buffer cache.
 */
int
physio(void (*strategy)(struct buf *), struct buf *obp, dev_t dev, int flags,
    void (*min_phys)(struct buf *), struct uio *uio)
{
	struct iovec *iovp;
	struct lwp *l = curlwp;
	struct proc *p = l->l_proc;
	int i, error;
	struct buf *bp = NULL;
	struct physio_stat *ps;
	int concurrency = physio_concurrency - 1;

	error = RUN_ONCE(&physio_initialized, physio_init);
	if (__predict_false(error != 0)) {
		return error;
	}

	DPRINTF(("%s: called: off=%" PRIu64 ", resid=%zu\n",
	    __func__, uio->uio_offset, uio->uio_resid));

	flags &= B_READ | B_WRITE;

	ps = kmem_zalloc(sizeof(*ps), KM_SLEEP);
	/* ps->ps_running = 0; */
	/* ps->ps_error = 0; */
	/* ps->ps_failed = 0; */
	ps->ps_orig_bp = obp;
	ps->ps_endoffset = -1;
	mutex_init(&ps->ps_lock, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&ps->ps_cv, "physio");

	/* Make sure we have a buffer, creating one if necessary. */
	if (obp != NULL) {
		mutex_enter(&bufcache_lock);
		/* Mark it busy, so nobody else will use it. */
		while (bbusy(obp, false, 0, NULL) == EPASSTHROUGH)
			;
		mutex_exit(&bufcache_lock);
		concurrency = 0; /* see "XXXkludge" comment below */
	}

	for (i = 0; i < uio->uio_iovcnt; i++) {
		bool sync = true;

		iovp = &uio->uio_iov[i];
		while (iovp->iov_len > 0) {
			size_t todo;
			vaddr_t endp;

			mutex_enter(&ps->ps_lock);
			if (ps->ps_failed != 0) {
				goto done_locked;
			}
			physio_wait(ps, sync ? 0 : concurrency);
			mutex_exit(&ps->ps_lock);
			if (obp != NULL) {
				/*
				 * XXXkludge
				 * some drivers use "obp" as an identifier.
				 */
				bp = obp;
			} else {
				bp = getiobuf(NULL, true);
				bp->b_cflags = BC_BUSY;
			}
			bp->b_dev = dev;
			bp->b_proc = p;
			bp->b_private = ps;

			/*
			 * Mrk the buffer busy for physical I/O.  Also set
			 * B_PHYS because it's an I/O to user memory, and
			 * B_RAW because B_RAW is to be "set by physio for
			 * raw transfers".
			 */
			bp->b_oflags = 0;
			bp->b_cflags = BC_BUSY;
			bp->b_flags = flags | B_PHYS | B_RAW;
			bp->b_iodone = physio_biodone;

			/* Set up the buffer for a maximum-sized transfer. */
			bp->b_blkno = btodb(uio->uio_offset);
			if (dbtob(bp->b_blkno) != uio->uio_offset) {
				error = EINVAL;
				goto done;
			}
			bp->b_bcount = MIN(MAXPHYS, iovp->iov_len);
			bp->b_data = iovp->iov_base;

			/*
			 * Call minphys to bound the transfer size,
			 * and remember the amount of data to transfer,
			 * for later comparison.
			 */
			(*min_phys)(bp);
			todo = bp->b_bufsize = bp->b_bcount;
#if defined(DIAGNOSTIC)
			if (todo > MAXPHYS)
				panic("todo(%zu) > MAXPHYS; minphys broken",
				    todo);
#endif /* defined(DIAGNOSTIC) */

			sync = false;
			endp = (vaddr_t)bp->b_data + todo;
			if (trunc_page(endp) != endp) {
				/*
				 * Following requests can overlap.
				 * note that uvm_vslock does round_page.
				 */
				sync = true;
			}

			/*
			 * Lock the part of the user address space involved
			 * in the transfer.
			 */
			error = uvm_vslock(p->p_vmspace, bp->b_data, todo,
			    (flags & B_READ) ?  VM_PROT_WRITE : VM_PROT_READ);
			if (error) {
				goto done;
			}

			/*
			 * Beware vmapbuf(); if succesful it clobbers
			 * b_data and saves it in b_saveaddr.
			 * However, vunmapbuf() restores b_data.
			 */
			if ((error = vmapbuf(bp, todo)) != 0) {
				uvm_vsunlock(p->p_vmspace, bp->b_data, todo);
				goto done;
			}

			BIO_SETPRIO(bp, BPRIO_TIMECRITICAL);

			mutex_enter(&ps->ps_lock);
			ps->ps_running++;
			mutex_exit(&ps->ps_lock);

			/* Call strategy to start the transfer. */
			(*strategy)(bp);
			bp = NULL;

			iovp->iov_len -= todo;
			iovp->iov_base = (char *)iovp->iov_base + todo;
			uio->uio_offset += todo;
			uio->uio_resid -= todo;
		}
	}

done:
	mutex_enter(&ps->ps_lock);
done_locked:
	physio_wait(ps, 0);
	mutex_exit(&ps->ps_lock);

	if (ps->ps_failed != 0) {
		off_t delta;

		delta = uio->uio_offset - ps->ps_endoffset;
		KASSERT(delta > 0);
		uio->uio_resid += delta;
		/* uio->uio_offset = ps->ps_endoffset; */
	} else {
		KASSERT(ps->ps_endoffset == -1);
	}
	if (bp != NULL && bp != obp) {
		putiobuf(bp);
	}
	if (error == 0) {
		error = ps->ps_error;
	}
	mutex_destroy(&ps->ps_lock);
	cv_destroy(&ps->ps_cv);
	kmem_free(ps, sizeof(*ps));

	/*
	 * Clean up the state of the buffer.  Remember if somebody wants
	 * it, so we can wake them up below.  Also, if we had to steal it,
	 * give it back.
	 */
	if (obp != NULL) {
		KASSERT((obp->b_cflags & BC_BUSY) != 0);

		/*
		 * If another process is waiting for the raw I/O buffer,
		 * wake up processes waiting to do physical I/O;
		 */
		mutex_enter(&bufcache_lock);
		obp->b_cflags &= ~(BC_BUSY | BC_WANTED);
		obp->b_flags &= ~(B_PHYS | B_RAW);
		obp->b_iodone = NULL;
		cv_broadcast(&obp->b_busy);
		mutex_exit(&bufcache_lock);
	}

	DPRINTF(("%s: done: off=%" PRIu64 ", resid=%zu\n",
	    __func__, uio->uio_offset, uio->uio_resid));

	return error;
}

/*
 * A minphys() routine is called by physio() to adjust the size of each
 * I/O transfer before the latter is passed to the strategy routine.
 *
 * This minphys() is a default that must be called to enforce limits
 * that are applicable to all devices, because of limitations in the
 * kernel or the hardware platform.
 */
void
minphys(struct buf *bp)
{

	if (bp->b_bcount > MAXPHYS)
		bp->b_bcount = MAXPHYS;
}
