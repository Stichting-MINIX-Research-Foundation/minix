/*	$NetBSD: if_agrsubr.c,v 1.10 2015/08/24 22:21:26 pooka Exp $	*/

/*-
 * Copyright (c)2005 YAMAMOTO Takashi,
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_agrsubr.c,v 1.10 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#endif

#include <sys/param.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/sockio.h>

#include <net/if.h>

#include <net/agr/if_agrvar_impl.h>
#include <net/agr/if_agrsubr.h>

struct agr_mc_entry {
	TAILQ_ENTRY(agr_mc_entry) ame_q;
	int ame_refcnt;
	struct agr_ifreq ame_ifr; /* XXX waste */
};

static struct agr_mc_entry *agr_mc_lookup(struct agr_multiaddrs *,
    const struct sockaddr *);
static int agrport_mc_add_callback(struct agr_port *, void *);
static int agrport_mc_del_callback(struct agr_port *, void *);
static int agrmc_mc_add_callback(struct agr_mc_entry *, void *);
static int agrmc_mc_del_callback(struct agr_mc_entry *, void *);

static int agr_mc_add(struct agr_multiaddrs *, const struct sockaddr *);
static int agr_mc_del(struct agr_multiaddrs *, const struct sockaddr *);

int
agr_mc_purgeall(struct agr_softc *sc, struct agr_multiaddrs *ama)
{
	struct agr_mc_entry *ame;
	int error = 0;

	while ((ame = TAILQ_FIRST(&ama->ama_addrs)) != NULL) {
		error = agr_port_foreach(sc,
		    agrport_mc_del_callback, &ame->ame_ifr);
		if (error) {
			/* XXX XXX */
			printf("%s: error %d\n", __func__, error);
		}
		TAILQ_REMOVE(&ama->ama_addrs, ame, ame_q);
		free(ame, M_DEVBUF);
	}

	return error;
}

int
agr_mc_init(struct agr_softc *sc, struct agr_multiaddrs *ama)
{

	TAILQ_INIT(&ama->ama_addrs);

	return 0;
}

/* ==================== */

static struct agr_mc_entry *
agr_mc_lookup(struct agr_multiaddrs *ama, const struct sockaddr *sa)
{
	struct agr_mc_entry *ame;

	TAILQ_FOREACH(ame, &ama->ama_addrs, ame_q) {
		if (!memcmp(&ame->ame_ifr.ifr_ss, sa, sa->sa_len))
			return ame;
	}

	return NULL;
}

int
agr_mc_foreach(struct agr_multiaddrs *ama,
    int (*func)(struct agr_mc_entry *, void *), void *arg)
{
	struct agr_mc_entry *ame;
	int error = 0;

	TAILQ_FOREACH(ame, &ama->ama_addrs, ame_q) {
		error = (*func)(ame, arg);
		if (error) {
			/*
			 * XXX how to recover?
			 * we can try to restore setting, but it can also fail..
			 */
			break;
		}
	}

	return error;
}

static int
agr_mc_add(struct agr_multiaddrs *ama, const struct sockaddr *sa)
{
	struct agr_mc_entry *ame;

	ame = agr_mc_lookup(ama, sa);
	if (ame) {
		ame->ame_refcnt++;
		return 0;
	}

	ame = malloc(sizeof(*ame), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (ame == NULL)
		return ENOMEM;

	memcpy(&ame->ame_ifr.ifr_ss, sa, sa->sa_len);
	ame->ame_refcnt = 1;
	TAILQ_INSERT_TAIL(&ama->ama_addrs, ame, ame_q);

	return ENETRESET;
}

static int
agr_mc_del(struct agr_multiaddrs *ama, const struct sockaddr *sa)
{
	struct agr_mc_entry *ame;

	ame = agr_mc_lookup(ama, sa);
	if (ame == NULL)
		return ENOENT;

	ame->ame_refcnt--;
	if (ame->ame_refcnt > 0)
		return 0;

	TAILQ_REMOVE(&ama->ama_addrs, ame, ame_q);
	free(ame, M_DEVBUF);

	return ENETRESET;
}

/* ==================== */

int
agr_port_foreach(struct agr_softc *sc,
    int (*func)(struct agr_port *, void *), void *arg)
{
	struct agr_port *port;
	int error = 0;

	TAILQ_FOREACH(port, &sc->sc_ports, port_q) {
		if ((port->port_flags & (AGRPORT_LARVAL | AGRPORT_DETACHING))) {
			continue;
		}
		error = (func)(port, arg);
		if (error) {
			/*
			 * XXX how to recover?
			 * we can try to restore setting, but it can also fail..
			 */
			break;
		}
	}

	return error;
}

/* ==================== */

static int
agrmc_mc_add_callback(struct agr_mc_entry *ame, void *arg)
{

	return agrport_mc_add_callback(arg, &ame->ame_ifr);
}

static int
agrmc_mc_del_callback(struct agr_mc_entry *ame, void *arg)
{

	return agrport_mc_del_callback(arg, &ame->ame_ifr);
}

int
agr_configmulti_port(struct agr_multiaddrs *ama, struct agr_port *port,
    bool add)
{

	return agr_mc_foreach(ama,
	    add ? agrmc_mc_add_callback : agrmc_mc_del_callback, port);
}

/* -------------------- */

static int
agrport_mc_add_callback(struct agr_port *port, void *arg)
{

	return agrport_ioctl(port, SIOCADDMULTI, arg);
}

static int
agrport_mc_del_callback(struct agr_port *port, void *arg)
{

	return agrport_ioctl(port, SIOCDELMULTI, arg);
}

int
agr_configmulti_ifreq(struct agr_softc *sc, struct agr_multiaddrs *ama,
    struct ifreq *ifr, bool add)
{
	int error;

	if (add)
		error = agr_mc_add(ama, ifreq_getaddr(SIOCADDMULTI, ifr));
	else
		error = agr_mc_del(ama, ifreq_getaddr(SIOCDELMULTI, ifr));

	if (error != ENETRESET)
		return error;

	return agr_port_foreach(sc,
	    add ? agrport_mc_add_callback : agrport_mc_del_callback, ifr);
}

/* ==================== */

int
agr_port_getmedia(struct agr_port *port, u_int *media, u_int *status)
{
	struct ifmediareq ifmr;
	int error;

	memset(&ifmr, 0, sizeof(ifmr));
	ifmr.ifm_count = 0;
	error = agrport_ioctl(port, SIOCGIFMEDIA, (void *)&ifmr);

	if (error == 0) {
		*media = ifmr.ifm_active;
		*status = ifmr.ifm_status;
	}

	return error;
}
