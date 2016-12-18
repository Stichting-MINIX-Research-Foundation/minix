/*	$NetBSD: raw_cb.c,v 1.22 2014/05/21 20:43:56 rmind Exp $	*/

/*
 * Copyright (c) 1980, 1986, 1993
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
 *	@(#)raw_cb.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: raw_cb.c,v 1.22 2014/05/21 20:43:56 rmind Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/kmem.h>

#include <net/if.h>
#include <net/route.h>
#include <net/raw_cb.h>
#include <netinet/in.h>

/*
 * Routines to manage the raw protocol control blocks.
 *
 * TODO:
 *	hash lookups by protocol family/protocol + address family
 *	take care of unique address problems per AF?
 *	redo address binding to allow wildcards
 */

struct rawcbhead	rawcb = LIST_HEAD_INITIALIZER(rawcb);

static u_long		raw_sendspace = RAWSNDQ;
static u_long		raw_recvspace = RAWRCVQ;

/*
 * Allocate a nominal amount of buffer space for the socket.
 */
int
raw_attach(struct socket *so, int proto)
{
	struct rawcb *rp;
	int error;

	/*
	 * It is assumed that raw_attach() is called after space has been
	 * allocated for the rawcb; consumer protocols may simply allocate
	 * type struct rawcb, or a wrapper data structure that begins with a
	 * struct rawcb.
	 */
	rp = sotorawcb(so);
	KASSERT(rp != NULL);
	sosetlock(so);

	if ((error = soreserve(so, raw_sendspace, raw_recvspace)) != 0) {
		return error;
	}
	rp->rcb_socket = so;
	rp->rcb_proto.sp_family = so->so_proto->pr_domain->dom_family;
	rp->rcb_proto.sp_protocol = proto;
	LIST_INSERT_HEAD(&rawcb, rp, rcb_list);
	KASSERT(solocked(so));

	return 0;
}

/*
 * Detach the raw connection block and discard socket resources.
 */
void
raw_detach(struct socket *so)
{
	struct rawcb *rp = sotorawcb(so);
	const size_t rcb_len = rp->rcb_len;

	KASSERT(rp != NULL);
	KASSERT(solocked(so));
	KASSERT(so->so_lock == softnet_lock);	/* XXX */

	/* Remove the last reference. */
	LIST_REMOVE(rp, rcb_list);
	so->so_pcb = NULL;

	/* Note: sofree() drops the socket's lock. */
	sofree(so);
	kmem_free(rp, rcb_len);
	mutex_enter(softnet_lock);
}

/*
 * Disconnect and possibly release resources.
 */
void
raw_disconnect(struct rawcb *rp)
{
	struct socket *so = rp->rcb_socket;

	if (so->so_state & SS_NOFDREF) {
		raw_detach(so);
	}
}
