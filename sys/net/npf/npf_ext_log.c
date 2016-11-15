/*	$NetBSD: npf_ext_log.c,v 1.8 2014/07/20 00:37:41 rmind Exp $	*/

/*-
 * Copyright (c) 2010-2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This material is based upon work partially supported by The
 * NetBSD Foundation under a contract with Mindaugas Rasiukevicius.
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
 * NPF logging extension.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: npf_ext_log.c,v 1.8 2014/07/20 00:37:41 rmind Exp $");

#include <sys/types.h>
#include <sys/module.h>

#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/bpf.h>

#include "npf_impl.h"

NPF_EXT_MODULE(npf_ext_log, "");

#define	NPFEXT_LOG_VER		1

static void *		npf_ext_log_id;

typedef struct {
	unsigned int	if_idx;
} npf_ext_log_t;

static int
npf_log_ctor(npf_rproc_t *rp, prop_dictionary_t params)
{
	npf_ext_log_t *meta;

	meta = kmem_zalloc(sizeof(npf_ext_log_t), KM_SLEEP);
	prop_dictionary_get_uint32(params, "log-interface", &meta->if_idx);
	npf_rproc_assign(rp, meta);
	return 0;
}

static void
npf_log_dtor(npf_rproc_t *rp, void *meta)
{
	kmem_free(meta, sizeof(npf_ext_log_t));
}

static bool
npf_log(npf_cache_t *npc, void *meta, int *decision)
{
	struct mbuf *m = nbuf_head_mbuf(npc->npc_nbuf);
	const npf_ext_log_t *log = meta;
	ifnet_t *ifp;
	int family;

	/* Set the address family. */
	if (npf_iscached(npc, NPC_IP4)) {
		family = AF_INET;
	} else if (npf_iscached(npc, NPC_IP6)) {
		family = AF_INET6;
	} else {
		family = AF_UNSPEC;
	}

	KERNEL_LOCK(1, NULL);

	/* Find a pseudo-interface to log. */
	ifp = if_byindex(log->if_idx);
	if (ifp == NULL) {
		/* No interface. */
		KERNEL_UNLOCK_ONE(NULL);
		return true;
	}

	/* Pass through BPF. */
	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len;
	bpf_mtap_af(ifp, family, m);
	KERNEL_UNLOCK_ONE(NULL);

	return true;
}

/*
 * Module interface.
 */
static int
npf_ext_log_modcmd(modcmd_t cmd, void *arg)
{
	static const npf_ext_ops_t npf_log_ops = {
		.version	= NPFEXT_LOG_VER,
		.ctx		= NULL,
		.ctor		= npf_log_ctor,
		.dtor		= npf_log_dtor,
		.proc		= npf_log
	};
	int error;

	switch (cmd) {
	case MODULE_CMD_INIT:
		/*
		 * Initialise the NPF logging extension.
		 */
		npf_ext_log_id = npf_ext_register("log", &npf_log_ops);
		if (!npf_ext_log_id) {
			return EEXIST;
		}
		break;

	case MODULE_CMD_FINI:
		error = npf_ext_unregister(npf_ext_log_id);
		if (error) {
			return error;
		}
		break;

	case MODULE_CMD_AUTOUNLOAD:
		/* Allow auto-unload only if NPF permits it. */
		return npf_autounload_p() ? 0 : EBUSY;

	default:
		return ENOTTY;
	}
	return 0;
}
