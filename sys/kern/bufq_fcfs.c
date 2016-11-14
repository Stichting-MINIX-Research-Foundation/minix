/*	$NetBSD: bufq_fcfs.c,v 1.10 2009/01/19 14:54:28 yamt Exp $	*/
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
__KERNEL_RCSID(0, "$NetBSD: bufq_fcfs.c,v 1.10 2009/01/19 14:54:28 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/bufq_impl.h>
#include <sys/kmem.h>

/*
 * First-come first-served sort for disks.
 *
 * Requests are appended to the queue without any reordering.
 */

struct bufq_fcfs {
	TAILQ_HEAD(, buf) bq_head;	/* actual list of buffers */
};

static void bufq_fcfs_init(struct bufq_state *);
static void bufq_fcfs_put(struct bufq_state *, struct buf *);
static struct buf *bufq_fcfs_get(struct bufq_state *, int);

BUFQ_DEFINE(fcfs, 10, bufq_fcfs_init);

static void
bufq_fcfs_put(struct bufq_state *bufq, struct buf *bp)
{
	struct bufq_fcfs *fcfs = bufq->bq_private;

	TAILQ_INSERT_TAIL(&fcfs->bq_head, bp, b_actq);
}

static struct buf *
bufq_fcfs_get(struct bufq_state *bufq, int remove)
{
	struct bufq_fcfs *fcfs = bufq->bq_private;
	struct buf *bp;

	bp = TAILQ_FIRST(&fcfs->bq_head);

	if (bp != NULL && remove)
		TAILQ_REMOVE(&fcfs->bq_head, bp, b_actq);

	return (bp);
}

static struct buf *
bufq_fcfs_cancel(struct bufq_state *bufq, struct buf *buf)
{
	struct bufq_fcfs *fcfs = bufq->bq_private;
	struct buf *bp;

	TAILQ_FOREACH(bp, &fcfs->bq_head, b_actq) {
		if (bp == buf) {
			TAILQ_REMOVE(&fcfs->bq_head, bp, b_actq);
			return buf;
		}
	}
	return NULL;
}

static void
bufq_fcfs_fini(struct bufq_state *bufq)
{

	KASSERT(bufq->bq_private != NULL);
	kmem_free(bufq->bq_private, sizeof(struct bufq_fcfs));
}

static void
bufq_fcfs_init(struct bufq_state *bufq)
{
	struct bufq_fcfs *fcfs;

	bufq->bq_get = bufq_fcfs_get;
	bufq->bq_put = bufq_fcfs_put;
	bufq->bq_cancel = bufq_fcfs_cancel;
	bufq->bq_fini = bufq_fcfs_fini;
	bufq->bq_private = kmem_zalloc(sizeof(struct bufq_fcfs), KM_SLEEP);
	fcfs = (struct bufq_fcfs *)bufq->bq_private;
	TAILQ_INIT(&fcfs->bq_head);
}
