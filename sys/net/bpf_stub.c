/*	$NetBSD: bpf_stub.c,v 1.6 2012/01/30 23:31:27 matt Exp $	*/

/*
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: bpf_stub.c,v 1.6 2012/01/30 23:31:27 matt Exp $");

#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>

#include <net/bpf.h>

struct laglist {
	struct ifnet *lag_ifp;
	u_int lag_dlt;
	u_int lag_hlen;
	struct bpf_if **lag_drvp;

	TAILQ_ENTRY(laglist) lag_entries;
};

static TAILQ_HEAD(, laglist) lagdrvs = TAILQ_HEAD_INITIALIZER(lagdrvs);

static void bpf_stub_attach(struct ifnet *, u_int, u_int, struct bpf_if **);
static void bpf_stub_detach(struct ifnet *);

static void bpf_stub_null(void);
static void bpf_stub_warn(void);

static kmutex_t handovermtx;
static kcondvar_t handovercv;
static bool handover;

struct bpf_ops bpf_ops_stub = {
	.bpf_attach =		bpf_stub_attach,
	.bpf_detach =		bpf_stub_detach,
	.bpf_change_type =	(void *)bpf_stub_null,

	.bpf_tap = 		(void *)bpf_stub_warn,
	.bpf_mtap = 		(void *)bpf_stub_warn,
	.bpf_mtap2 = 		(void *)bpf_stub_warn,
	.bpf_mtap_af = 		(void *)bpf_stub_warn,
	.bpf_mtap_sl_in = 	(void *)bpf_stub_warn,
	.bpf_mtap_sl_out =	(void *)bpf_stub_warn,
};
struct bpf_ops *bpf_ops;

static void
bpf_stub_attach(struct ifnet *ifp, u_int dlt, u_int hlen, struct bpf_if **drvp)
{
	struct laglist *lag;
	bool storeattach = true;

	lag = kmem_alloc(sizeof(*lag), KM_SLEEP);
	lag->lag_ifp = ifp;
	lag->lag_dlt = dlt;
	lag->lag_hlen = hlen;
	lag->lag_drvp = drvp;

	mutex_enter(&handovermtx);
	/*
	 * If handover is in progress, wait for it to finish and complete
	 * attach after that.  Otherwise record ourselves.
	 */
	while (handover) {
		storeattach = false;
		cv_wait(&handovercv, &handovermtx);
	}

	if (storeattach == false) {
		mutex_exit(&handovermtx);
		kmem_free(lag, sizeof(*lag));
		KASSERT(bpf_ops != &bpf_ops_stub); /* revisit when unloadable */
		bpf_ops->bpf_attach(ifp, dlt, hlen, drvp);
	} else {
		*drvp = NULL;
		TAILQ_INSERT_TAIL(&lagdrvs, lag, lag_entries);
		mutex_exit(&handovermtx);
	}
}

static void
bpf_stub_detach(struct ifnet *ifp)
{
	TAILQ_HEAD(, laglist) rmlist;
	struct laglist *lag, *lag_next;
	bool didhand;

	TAILQ_INIT(&rmlist);

	didhand = false;
	mutex_enter(&handovermtx);
	while (handover) {
		didhand = true;
		cv_wait(&handovercv, &handovermtx);
	}

	if (didhand == false) {
		/* atomically remove all */
		for (lag = TAILQ_FIRST(&lagdrvs); lag; lag = lag_next) {
			lag_next = TAILQ_NEXT(lag, lag_entries);
			if (lag->lag_ifp == ifp) {
				TAILQ_REMOVE(&lagdrvs, lag, lag_entries);
				TAILQ_INSERT_HEAD(&rmlist, lag, lag_entries);
			}
		}
		mutex_exit(&handovermtx);
		while ((lag = TAILQ_FIRST(&rmlist)) != NULL) {
			TAILQ_REMOVE(&rmlist, lag, lag_entries);
			kmem_free(lag, sizeof(*lag));
		}
	} else {
		mutex_exit(&handovermtx);
		KASSERT(bpf_ops != &bpf_ops_stub); /* revisit when unloadable */
		bpf_ops->bpf_detach(ifp);
	}
}

static void
bpf_stub_null(void)
{

}

static void
bpf_stub_warn(void)
{

#ifdef DEBUG
	panic("bpf method called without attached bpf_if");
#endif
#ifdef DIAGNOSTIC
	printf("bpf method called without attached bpf_if\n");
#endif
}

void
bpf_setops(void)
{

	mutex_init(&handovermtx, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&handovercv, "bpfops");
	bpf_ops = &bpf_ops_stub;
}

/*
 * Party's over, prepare for handover.
 * It needs to happen *before* bpf_ops is set to make it atomic
 * to callers (see also stub implementations, which wait if
 * called during handover).  The likelyhood of seeing a full
 * attach-detach *during* handover comes close to astronomical,
 * but handle it anyway since it's relatively easy.
 */
void
bpf_ops_handover_enter(struct bpf_ops *newops)
{
	struct laglist *lag;

	mutex_enter(&handovermtx);
	handover = true;

	while ((lag = TAILQ_FIRST(&lagdrvs)) != NULL) {
		TAILQ_REMOVE(&lagdrvs, lag, lag_entries);
		mutex_exit(&handovermtx);
		newops->bpf_attach(lag->lag_ifp, lag->lag_dlt,
		    lag->lag_hlen, lag->lag_drvp);
		kmem_free(lag, sizeof(*lag));
		mutex_enter(&handovermtx);
	}
	mutex_exit(&handovermtx);
}

/* hangover done */
void
bpf_ops_handover_exit(void)
{

	mutex_enter(&handovermtx);
	handover = false;
	cv_broadcast(&handovercv);
	mutex_exit(&handovermtx);
}
