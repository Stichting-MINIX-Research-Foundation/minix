/*	$NetBSD: npf_if.c,v 1.5 2015/07/12 23:51:53 rmind Exp $	*/

/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Mindaugas Rasiukevicius.
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
 * NPF network interface handling module.
 *
 * NPF uses its own interface IDs (npf-if-id).  When NPF configuration is
 * (re)loaded, each required interface name is registered and a matching
 * network interface gets an ID assigned.  If an interface is not present,
 * it gets an ID on attach.
 *
 * IDs start from 1.  Zero is reserved to indicate "no interface" case or
 * an interface of no interest (i.e. not registered).
 *
 * The IDs are mapped synchronously based on interface events which are
 * monitored using pfil(9) hooks.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: npf_if.c,v 1.5 2015/07/12 23:51:53 rmind Exp $");

#ifdef _KERNEL_OPT
#include "pf.h"
#if NPF > 0
#error "NPF and PF are mutually exclusive; please select one"
#endif
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kmem.h>

#include <net/if.h>

#include "npf_impl.h"

typedef struct {
	char		n_ifname[IFNAMSIZ];
} npf_ifmap_t;

static npf_ifmap_t	npf_ifmap[NPF_MAX_IFMAP]	__read_mostly;
static u_int		npf_ifmap_cnt			__read_mostly;

static u_int
npf_ifmap_new(void)
{
	KASSERT(npf_config_locked_p());

	for (u_int i = 0; i < npf_ifmap_cnt; i++)
		if (npf_ifmap[i].n_ifname[0] == '\0')
			return i + 1;

	if (npf_ifmap_cnt == NPF_MAX_IFMAP) {
		printf("npf_ifmap_new: out of slots; bump NPF_MAX_IFMAP\n");
		return 0;
	}
	return ++npf_ifmap_cnt;
}

static u_int
npf_ifmap_lookup(const char *ifname)
{
	KASSERT(npf_config_locked_p());

	for (u_int i = 0; i < npf_ifmap_cnt; i++) {
		npf_ifmap_t *nim = &npf_ifmap[i];

		if (nim->n_ifname[0] && strcmp(nim->n_ifname, ifname) == 0)
			return i + 1;
	}
	return 0;
}

u_int
npf_ifmap_register(const char *ifname)
{
	npf_ifmap_t *nim;
	ifnet_t *ifp;
	u_int i;

	npf_config_enter();
	if ((i = npf_ifmap_lookup(ifname)) != 0) {
		goto out;
	}
	if ((i = npf_ifmap_new()) == 0) {
		goto out;
	}
	nim = &npf_ifmap[i - 1];
	strlcpy(nim->n_ifname, ifname, IFNAMSIZ);

	KERNEL_LOCK(1, NULL);
	if ((ifp = ifunit(ifname)) != NULL) {
		ifp->if_pf_kif = (void *)(uintptr_t)i;
	}
	KERNEL_UNLOCK_ONE(NULL);
out:
	npf_config_exit();
	return i;
}

void
npf_ifmap_flush(void)
{
	ifnet_t *ifp;

	KASSERT(npf_config_locked_p());

	for (u_int i = 0; i < npf_ifmap_cnt; i++) {
		npf_ifmap[i].n_ifname[0] = '\0';
	}
	npf_ifmap_cnt = 0;

	KERNEL_LOCK(1, NULL);
	IFNET_FOREACH(ifp) {
		ifp->if_pf_kif = (void *)(uintptr_t)0;
	}
	KERNEL_UNLOCK_ONE(NULL);
}

u_int
npf_ifmap_getid(const ifnet_t *ifp)
{
	const u_int i = (uintptr_t)ifp->if_pf_kif;
	KASSERT(i <= npf_ifmap_cnt);
	return i;
}

const char *
npf_ifmap_getname(const u_int id)
{
	const char *ifname;

	KASSERT(npf_config_locked_p());
	KASSERT(id > 0 && id <= npf_ifmap_cnt);

	ifname = npf_ifmap[id - 1].n_ifname;
	KASSERT(ifname[0] != '\0');
	return ifname;
}

void
npf_ifmap_attach(ifnet_t *ifp)
{
	npf_config_enter();
	ifp->if_pf_kif = (void *)(uintptr_t)npf_ifmap_lookup(ifp->if_xname);
	npf_config_exit();
}

void
npf_ifmap_detach(ifnet_t *ifp)
{
	/* Diagnostic. */
	npf_config_enter();
	ifp->if_pf_kif = (void *)(uintptr_t)0;
	npf_config_exit();
}
