/*	$NetBSD: bufq_disksort.c,v 1.11 2009/01/19 14:54:28 yamt Exp $	*/
/*	NetBSD: subr_disk.c,v 1.61 2004/09/25 03:30:44 thorpej Exp 	*/

/*-
 * Copyright (c) 1996, 1997, 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * Copyright (c) 1982, 1986, 1988, 1993
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
 *	@(#)ufs_disksubr.c	8.5 (Berkeley) 1/21/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: bufq_disksort.c,v 1.11 2009/01/19 14:54:28 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/bufq_impl.h>
#include <sys/kmem.h>

/*
 * Seek sort for disks.
 *
 * There are actually two queues, sorted in ascendening order.  The first
 * queue holds those requests which are positioned after the current block;
 * the second holds requests which came in after their position was passed.
 * Thus we implement a one-way scan, retracting after reaching the end of
 * the drive to the first request on the second queue, at which time it
 * becomes the first queue.
 *
 * A one-way scan is natural because of the way UNIX read-ahead blocks are
 * allocated.
 */

struct bufq_disksort {
	TAILQ_HEAD(, buf) bq_head;	/* actual list of buffers */
};

static void bufq_disksort_init(struct bufq_state *);
static void bufq_disksort_put(struct bufq_state *, struct buf *);
static struct buf *bufq_disksort_get(struct bufq_state *, int);

BUFQ_DEFINE(disksort, 20, bufq_disksort_init);

static void
bufq_disksort_put(struct bufq_state *bufq, struct buf *bp)
{
	struct bufq_disksort *disksort = bufq_private(bufq);
	struct buf *bq, *nbq;
	int sortby;

	sortby = bufq->bq_flags & BUFQ_SORT_MASK;

	bq = TAILQ_FIRST(&disksort->bq_head);

	/*
	 * If the queue is empty it's easy; we just go on the end.
	 */
	if (bq == NULL) {
		TAILQ_INSERT_TAIL(&disksort->bq_head, bp, b_actq);
		return;
	}

	/*
	 * If we lie before the currently active request, then we
	 * must locate the second request list and add ourselves to it.
	 */
	if (buf_inorder(bp, bq, sortby)) {
		while ((nbq = TAILQ_NEXT(bq, b_actq)) != NULL) {
			/*
			 * Check for an ``inversion'' in the normally ascending
			 * block numbers, indicating the start of the second
			 * request list.
			 */
			if (buf_inorder(nbq, bq, sortby)) {
				/*
				 * Search the second request list for the first
				 * request at a larger block number.  We go
				 * after that; if there is no such request, we
				 * go at the end.
				 */
				do {
					if (buf_inorder(bp, nbq, sortby))
						goto insert;
					bq = nbq;
				} while ((nbq =
				    TAILQ_NEXT(bq, b_actq)) != NULL);
				goto insert;		/* after last */
			}
			bq = nbq;
		}
		/*
		 * No inversions... we will go after the last, and
		 * be the first request in the second request list.
		 */
		goto insert;
	}
	/*
	 * Request is at/after the current request...
	 * sort in the first request list.
	 */
	while ((nbq = TAILQ_NEXT(bq, b_actq)) != NULL) {
		/*
		 * We want to go after the current request if there is an
		 * inversion after it (i.e. it is the end of the first
		 * request list), or if the next request is a larger cylinder
		 * than our request.
		 */
		if (buf_inorder(nbq, bq, sortby) ||
		    buf_inorder(bp, nbq, sortby))
			goto insert;
		bq = nbq;
	}
	/*
	 * Neither a second list nor a larger request... we go at the end of
	 * the first list, which is the same as the end of the whole schebang.
	 */
insert:	TAILQ_INSERT_AFTER(&disksort->bq_head, bq, bp, b_actq);
}

static struct buf *
bufq_disksort_get(struct bufq_state *bufq, int remove)
{
	struct bufq_disksort *disksort = bufq->bq_private;
	struct buf *bp;

	bp = TAILQ_FIRST(&disksort->bq_head);

	if (bp != NULL && remove)
		TAILQ_REMOVE(&disksort->bq_head, bp, b_actq);

	return (bp);
}

static struct buf *
bufq_disksort_cancel(struct bufq_state *bufq, struct buf *buf)
{
	struct bufq_disksort *disksort = bufq->bq_private;
	struct buf *bq;

	TAILQ_FOREACH(bq, &disksort->bq_head, b_actq) {
		if (bq == buf) {
			TAILQ_REMOVE(&disksort->bq_head, bq, b_actq);
			return buf;
		}
	}
	return NULL;
}

static void
bufq_disksort_fini(struct bufq_state *bufq)
{

	KASSERT(bufq->bq_private != NULL);
	kmem_free(bufq->bq_private, sizeof(struct bufq_disksort));
}

static void
bufq_disksort_init(struct bufq_state *bufq)
{
	struct bufq_disksort *disksort;

	disksort = kmem_zalloc(sizeof(*disksort), KM_SLEEP);
	bufq->bq_private = disksort;
	bufq->bq_get = bufq_disksort_get;
	bufq->bq_put = bufq_disksort_put;
	bufq->bq_cancel = bufq_disksort_cancel;
	bufq->bq_fini = bufq_disksort_fini;
	TAILQ_INIT(&disksort->bq_head);
}
