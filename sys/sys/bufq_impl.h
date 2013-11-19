/*	$NetBSD: bufq_impl.h,v 1.9 2011/11/02 13:52:34 yamt Exp $	*/
/*	NetBSD: bufq.h,v 1.3 2005/03/31 11:28:53 yamt Exp	*/
/*	NetBSD: buf.h,v 1.75 2004/09/18 16:40:11 yamt Exp 	*/

/*-
 * Copyright (c) 1999, 2000 The NetBSD Foundation, Inc.
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
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)buf.h	8.9 (Berkeley) 3/30/95
 */

#if !defined(_KERNEL)
#error not supposed to be exposed to userland.
#endif

struct bufq_strat;

/*
 * Device driver buffer queue.
 */
struct bufq_state {
	void (*bq_put)(struct bufq_state *, struct buf *);
	struct buf *(*bq_get)(struct bufq_state *, int);
	struct buf *(*bq_cancel)(struct bufq_state *, struct buf *);
	void (*bq_fini)(struct bufq_state *);
	void *bq_private;
	int bq_flags;			/* Flags from bufq_alloc() */
	const struct bufq_strat *bq_strat;
};

static __inline void *bufq_private(const struct bufq_state *) __unused;
static __inline bool buf_inorder(const struct buf *, const struct buf *, int)
    __unused;

#include <sys/null.h> /* for NULL */

static __inline void *
bufq_private(const struct bufq_state *bufq)
{

	return bufq->bq_private;
}

/*
 * Check if two buf's are in ascending order.
 *
 * this function consider a NULL buf is after any non-NULL buf.
 *
 * this function returns false if two are "same".
 */
static __inline bool
buf_inorder(const struct buf *bp, const struct buf *bq, int sortby)
{

	KASSERT(bp != NULL || bq != NULL);
	if (bp == NULL || bq == NULL)
		return (bq == NULL);

	if (sortby == BUFQ_SORT_CYLINDER) {
		if (bp->b_cylinder != bq->b_cylinder)
			return bp->b_cylinder < bq->b_cylinder;
		else
			return bp->b_rawblkno < bq->b_rawblkno;
	} else
		return bp->b_rawblkno < bq->b_rawblkno;
}

struct bufq_strat {
	const char *bs_name;
	void (*bs_initfn)(struct bufq_state *);
	int bs_prio;
};

#define	BUFQ_DEFINE(name, prio, initfn)			\
static const struct bufq_strat bufq_strat_##name = {	\
	.bs_name = #name,				\
	.bs_prio = prio,					\
	.bs_initfn = initfn				\
};							\
__link_set_add_rodata(bufq_strats, bufq_strat_##name)
