/*	$NetBSD: if_npflog.c,v 1.3 2013/03/13 13:15:47 christos Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: if_npflog.c,v 1.3 2013/03/13 13:15:47 christos Exp $");

#include <sys/types.h>
#include <sys/module.h>

#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/bpf.h>

MODULE(MODULE_CLASS_DRIVER, if_npflog, NULL);

typedef struct npflog_softc {
	LIST_ENTRY(npflog_softc)	sc_entry;
	kmutex_t			sc_lock;
	ifnet_t				sc_if;
	int				sc_unit;
} npflog_softc_t;

static int	npflog_clone_create(struct if_clone *, int);
static int	npflog_clone_destroy(ifnet_t *);

static LIST_HEAD(, npflog_softc)	npflog_if_list	__cacheline_aligned;
static struct if_clone			npflog_cloner =
    IF_CLONE_INITIALIZER("npflog", npflog_clone_create, npflog_clone_destroy);

static void
npflogattach(int nunits)
{

	LIST_INIT(&npflog_if_list);
	if_clone_attach(&npflog_cloner);
}

static void
npflogdetach(void)
{
	npflog_softc_t *sc;

	while ((sc = LIST_FIRST(&npflog_if_list)) != NULL) {
		npflog_clone_destroy(&sc->sc_if);
	}
	if_clone_detach(&npflog_cloner);
}

static int
npflog_ioctl(ifnet_t *ifp, u_long cmd, void *data)
{
	npflog_softc_t *sc = ifp->if_softc;
	int error = 0;

	mutex_enter(&sc->sc_lock);
	switch (cmd) {
	case SIOCINITIFADDR:
		ifp->if_flags |= (IFF_UP | IFF_RUNNING);
		break;
	default:
		error = ifioctl_common(ifp, cmd, data);
		break;
	}
	mutex_exit(&sc->sc_lock);
	return error;
}

static int
npflog_clone_create(struct if_clone *ifc, int unit)
{
	npflog_softc_t *sc;
	ifnet_t *ifp;

	sc = kmem_zalloc(sizeof(npflog_softc_t), KM_SLEEP);
	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_SOFTNET);

	ifp = &sc->sc_if;
	ifp->if_softc = sc;

	if_initname(ifp, "npflog", unit);
	ifp->if_type = IFT_OTHER;
	ifp->if_dlt = DLT_NULL;
	ifp->if_ioctl = npflog_ioctl;

	KERNEL_LOCK(1, NULL);
	if_attach(ifp);
	if_alloc_sadl(ifp);
	bpf_attach(ifp, DLT_NULL, 0);
	LIST_INSERT_HEAD(&npflog_if_list, sc, sc_entry);
	KERNEL_UNLOCK_ONE(NULL);

	return 0;
}

static int
npflog_clone_destroy(ifnet_t *ifp)
{
	npflog_softc_t *sc = ifp->if_softc;

	KERNEL_LOCK(1, NULL);
	LIST_REMOVE(sc, sc_entry);
	bpf_detach(ifp);
	if_detach(ifp);
	KERNEL_UNLOCK_ONE(NULL);

	mutex_destroy(&sc->sc_lock);
	kmem_free(sc, sizeof(npflog_softc_t));
	return 0;
}

/*
 * Module interface.
 */
static int
if_npflog_modcmd(modcmd_t cmd, void *arg)
{
	switch (cmd) {
	case MODULE_CMD_INIT:
		npflogattach(1);
		break;

	case MODULE_CMD_FINI:
		npflogdetach();
		break;

	case MODULE_CMD_AUTOUNLOAD:
		return EBUSY;

	default:
		return ENOTTY;
	}
	return 0;
}
