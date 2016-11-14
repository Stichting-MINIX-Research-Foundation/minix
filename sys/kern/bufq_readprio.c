/*	$NetBSD: bufq_readprio.c,v 1.13 2009/01/19 14:54:28 yamt Exp $	*/
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
__KERNEL_RCSID(0, "$NetBSD: bufq_readprio.c,v 1.13 2009/01/19 14:54:28 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/bufq_impl.h>
#include <sys/kmem.h>

/*
 * Seek sort for disks.
 *
 * There are two queues.  The first queue holds read requests; the second
 * holds write requests.  The read queue is first-come first-served; the
 * write queue is sorted in ascendening block order.
 * The read queue is processed first.  After PRIO_READ_BURST consecutive
 * read requests with non-empty write queue PRIO_WRITE_REQ requests from
 * the write queue will be processed.
 */

#define PRIO_READ_BURST		48
#define PRIO_WRITE_REQ		16

struct bufq_prio {
	TAILQ_HEAD(, buf) bq_read, bq_write; /* actual list of buffers */
	struct buf *bq_write_next;	/* next request in bq_write */
	struct buf *bq_next;		/* current request */
	int bq_read_burst;		/* # of consecutive reads */
};

static void bufq_readprio_init(struct bufq_state *);
static void bufq_prio_put(struct bufq_state *, struct buf *);
static struct buf *bufq_prio_get(struct bufq_state *, int);

BUFQ_DEFINE(readprio, 30, bufq_readprio_init);

static void
bufq_prio_put(struct bufq_state *bufq, struct buf *bp)
{
	struct bufq_prio *prio = bufq->bq_private;
	struct buf *bq;
	int sortby;

	sortby = bufq->bq_flags & BUFQ_SORT_MASK;

	/*
	 * If it's a read request append it to the list.
	 */
	if ((bp->b_flags & B_READ) == B_READ) {
		TAILQ_INSERT_TAIL(&prio->bq_read, bp, b_actq);
		return;
	}

	bq = TAILQ_FIRST(&prio->bq_write);

	/*
	 * If the write list is empty, simply append it to the list.
	 */
	if (bq == NULL) {
		TAILQ_INSERT_TAIL(&prio->bq_write, bp, b_actq);
		prio->bq_write_next = bp;
		return;
	}

	/*
	 * If we lie after the next request, insert after this request.
	 */
	if (buf_inorder(prio->bq_write_next, bp, sortby))
		bq = prio->bq_write_next;

	/*
	 * Search for the first request at a larger block number.
	 * We go before this request if it exists.
	 */
	while (bq != NULL && buf_inorder(bq, bp, sortby))
		bq = TAILQ_NEXT(bq, b_actq);

	if (bq != NULL)
		TAILQ_INSERT_BEFORE(bq, bp, b_actq);
	else
		TAILQ_INSERT_TAIL(&prio->bq_write, bp, b_actq);
}

static struct buf *
bufq_prio_get(struct bufq_state *bufq, int remove)
{
	struct bufq_prio *prio = bufq->bq_private;
	struct buf *bp;

	/*
	 * If no current request, get next from the lists.
	 */
	if (prio->bq_next == NULL) {
		/*
		 * If at least one list is empty, select the other.
		 */
		if (TAILQ_FIRST(&prio->bq_read) == NULL) {
			prio->bq_next = prio->bq_write_next;
			prio->bq_read_burst = 0;
		} else if (prio->bq_write_next == NULL) {
			bp = prio->bq_next = TAILQ_FIRST(&prio->bq_read);
			prio->bq_read_burst = 0;
			KASSERT((bp == NULL) ||
			    ((bp->b_flags & B_READ) == B_READ));
		} else {
			/*
			 * Both list have requests.  Select the read list up
			 * to PRIO_READ_BURST times, then select the write
			 * list PRIO_WRITE_REQ times.
			 */
			if (prio->bq_read_burst++ < PRIO_READ_BURST)
				prio->bq_next = TAILQ_FIRST(&prio->bq_read);
			else if (prio->bq_read_burst <
			    PRIO_READ_BURST + PRIO_WRITE_REQ)
				prio->bq_next = prio->bq_write_next;
			else {
				prio->bq_next = TAILQ_FIRST(&prio->bq_read);
				prio->bq_read_burst = 0;
			}
		}
	}

	bp = prio->bq_next;

	if (bp != NULL && remove) {
		if ((bp->b_flags & B_READ) == B_READ)
			TAILQ_REMOVE(&prio->bq_read, bp, b_actq);
		else {
			/*
			 * Advance the write pointer before removing
			 * bp since it is actually prio->bq_write_next.
			 */
			prio->bq_write_next =
			    TAILQ_NEXT(prio->bq_write_next, b_actq);
			TAILQ_REMOVE(&prio->bq_write, bp, b_actq);
			if (prio->bq_write_next == NULL)
				prio->bq_write_next =
				    TAILQ_FIRST(&prio->bq_write);
		}

		prio->bq_next = NULL;
	}

	return (bp);
}

static struct buf *
bufq_prio_cancel(struct bufq_state *bufq, struct buf *buf)
{
	struct bufq_prio *prio = bufq->bq_private;
	struct buf *bq;

	/* search read queue */
	TAILQ_FOREACH(bq, &prio->bq_read, b_actq) {
		if (bq == buf) {
			TAILQ_REMOVE(&prio->bq_read, bq, b_actq);
			/* force new section */
			prio->bq_next = NULL;
			return buf;
		}
	}

	/* not found in read queue, search write queue */
	TAILQ_FOREACH(bq, &prio->bq_write, b_actq) {
		if (bq == buf) {
			if (bq == prio->bq_write_next) {
				/*
				 * Advance the write pointer before removing
				 * bp since it is actually prio->bq_write_next.
				 */
				prio->bq_write_next =
				    TAILQ_NEXT(prio->bq_write_next, b_actq);
				TAILQ_REMOVE(&prio->bq_write, bq, b_actq);
				if (prio->bq_write_next == NULL)
					prio->bq_write_next =
					    TAILQ_FIRST(&prio->bq_write);
			} else {
				TAILQ_REMOVE(&prio->bq_write, bq, b_actq);
			}
			/* force new section */
			prio->bq_next = NULL;
			return buf;
		}
	}

	/* still not found */
	return NULL;
}

static void
bufq_prio_fini(struct bufq_state *bufq)
{

	KASSERT(bufq->bq_private != NULL);
	kmem_free(bufq->bq_private, sizeof(struct bufq_prio));
}

static void
bufq_readprio_init(struct bufq_state *bufq)
{
	struct bufq_prio *prio;

	bufq->bq_get = bufq_prio_get;
	bufq->bq_put = bufq_prio_put;
	bufq->bq_cancel = bufq_prio_cancel;
	bufq->bq_fini = bufq_prio_fini;
	bufq->bq_private = kmem_zalloc(sizeof(struct bufq_prio), KM_SLEEP);
	prio = (struct bufq_prio *)bufq->bq_private;
	TAILQ_INIT(&prio->bq_read);
	TAILQ_INIT(&prio->bq_write);
}

