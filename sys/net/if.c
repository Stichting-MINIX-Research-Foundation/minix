/*	$NetBSD: if.c,v 1.318 2015/08/31 08:02:44 ozaki-r Exp $	*/

/*-
 * Copyright (c) 1999, 2000, 2001, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by William Studenmund and Jason R. Thorpe.
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
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

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
 *	@(#)if.c	8.5 (Berkeley) 1/9/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if.c,v 1.318 2015/08/31 08:02:44 ozaki-r Exp $");

#if defined(_KERNEL_OPT)
#include "opt_inet.h"

#include "opt_atalk.h"
#include "opt_natm.h"
#include "opt_wlan.h"
#include "opt_net_mpsafe.h"
#endif

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/kauth.h>
#include <sys/kmem.h>
#include <sys/xcall.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>
#include <net80211/ieee80211.h>
#include <net80211/ieee80211_ioctl.h>
#include <net/if_types.h>
#include <net/radix.h>
#include <net/route.h>
#include <net/netisr.h>
#include <sys/module.h>
#ifdef NETATALK
#include <netatalk/at_extern.h>
#include <netatalk/at.h>
#endif
#include <net/pfil.h>
#include <netinet/in.h>
#include <netinet/in_var.h>

#ifdef INET6
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#endif

#include "ether.h"
#include "fddi.h"
#include "token.h"

#include "carp.h"
#if NCARP > 0
#include <netinet/ip_carp.h>
#endif

#include <compat/sys/sockio.h>
#include <compat/sys/socket.h>

MALLOC_DEFINE(M_IFADDR, "ifaddr", "interface address");
MALLOC_DEFINE(M_IFMADDR, "ether_multi", "link-level multicast address");

/*
 * Global list of interfaces.
 */
struct ifnet_head		ifnet_list;
static ifnet_t **		ifindex2ifnet = NULL;

static u_int			if_index = 1;
static size_t			if_indexlim = 0;
static uint64_t			index_gen;
static kmutex_t			index_gen_mtx;
static kmutex_t			if_clone_mtx;

struct ifnet *lo0ifp;
int	ifqmaxlen = IFQ_MAXLEN;

static int	if_rt_walktree(struct rtentry *, void *);

static struct if_clone *if_clone_lookup(const char *, int *);

static LIST_HEAD(, if_clone) if_cloners = LIST_HEAD_INITIALIZER(if_cloners);
static int if_cloners_count;

/* Packet filtering hook for interfaces. */
pfil_head_t *	if_pfil;

static kauth_listener_t if_listener;

static int doifioctl(struct socket *, u_long, void *, struct lwp *);
static int ifioctl_attach(struct ifnet *);
static void ifioctl_detach(struct ifnet *);
static void ifnet_lock_enter(struct ifnet_lock *);
static void ifnet_lock_exit(struct ifnet_lock *);
static void if_detach_queues(struct ifnet *, struct ifqueue *);
static void sysctl_sndq_setup(struct sysctllog **, const char *,
    struct ifaltq *);
static void if_slowtimo(void *);
static void if_free_sadl(struct ifnet *);
static void if_attachdomain1(struct ifnet *);
static int ifconf(u_long, void *);
static int if_clone_create(const char *);
static int if_clone_destroy(const char *);

#if defined(INET) || defined(INET6)
static void sysctl_net_pktq_setup(struct sysctllog **, int);
#endif

static int
if_listener_cb(kauth_cred_t cred, kauth_action_t action, void *cookie,
    void *arg0, void *arg1, void *arg2, void *arg3)
{
	int result;
	enum kauth_network_req req;

	result = KAUTH_RESULT_DEFER;
	req = (enum kauth_network_req)arg1;

	if (action != KAUTH_NETWORK_INTERFACE)
		return result;

	if ((req == KAUTH_REQ_NETWORK_INTERFACE_GET) ||
	    (req == KAUTH_REQ_NETWORK_INTERFACE_SET))
		result = KAUTH_RESULT_ALLOW;

	return result;
}

/*
 * Network interface utility routines.
 *
 * Routines with ifa_ifwith* names take sockaddr *'s as
 * parameters.
 */
void
ifinit(void)
{
#if defined(INET)
	sysctl_net_pktq_setup(NULL, PF_INET);
#endif
#ifdef INET6
	if (in6_present)
		sysctl_net_pktq_setup(NULL, PF_INET6);
#endif

	if_listener = kauth_listen_scope(KAUTH_SCOPE_NETWORK,
	    if_listener_cb, NULL);

	/* interfaces are available, inform socket code */
	ifioctl = doifioctl;
}

/*
 * XXX Initialization before configure().
 * XXX hack to get pfil_add_hook working in autoconf.
 */
void
ifinit1(void)
{
	mutex_init(&index_gen_mtx, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&if_clone_mtx, MUTEX_DEFAULT, IPL_NONE);
	TAILQ_INIT(&ifnet_list);
	if_indexlim = 8;

	if_pfil = pfil_head_create(PFIL_TYPE_IFNET, NULL);
	KASSERT(if_pfil != NULL);

#if NETHER > 0 || NFDDI > 0 || defined(NETATALK) || NTOKEN > 0 || defined(WLAN)
	etherinit();
#endif
}

ifnet_t *
if_alloc(u_char type)
{
	return kmem_zalloc(sizeof(ifnet_t), KM_SLEEP);
}

void
if_free(ifnet_t *ifp)
{
	kmem_free(ifp, sizeof(ifnet_t));
}

void
if_initname(struct ifnet *ifp, const char *name, int unit)
{
	(void)snprintf(ifp->if_xname, sizeof(ifp->if_xname),
	    "%s%d", name, unit);
}

/*
 * Null routines used while an interface is going away.  These routines
 * just return an error.
 */

int
if_nulloutput(struct ifnet *ifp, struct mbuf *m,
    const struct sockaddr *so, struct rtentry *rt)
{

	return ENXIO;
}

void
if_nullinput(struct ifnet *ifp, struct mbuf *m)
{

	/* Nothing. */
}

void
if_nullstart(struct ifnet *ifp)
{

	/* Nothing. */
}

int
if_nullioctl(struct ifnet *ifp, u_long cmd, void *data)
{

	/* Wake ifioctl_detach(), who may wait for all threads to
	 * quit the critical section.
	 */
	cv_signal(&ifp->if_ioctl_lock->il_emptied);
	return ENXIO;
}

int
if_nullinit(struct ifnet *ifp)
{

	return ENXIO;
}

void
if_nullstop(struct ifnet *ifp, int disable)
{

	/* Nothing. */
}

void
if_nullslowtimo(struct ifnet *ifp)
{

	/* Nothing. */
}

void
if_nulldrain(struct ifnet *ifp)
{

	/* Nothing. */
}

void
if_set_sadl(struct ifnet *ifp, const void *lla, u_char addrlen, bool factory)
{
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;

	ifp->if_addrlen = addrlen;
	if_alloc_sadl(ifp);
	ifa = ifp->if_dl;
	sdl = satosdl(ifa->ifa_addr);

	(void)sockaddr_dl_setaddr(sdl, sdl->sdl_len, lla, ifp->if_addrlen);
	if (factory) {
		ifp->if_hwdl = ifp->if_dl;
		ifaref(ifp->if_hwdl);
	}
	/* TBD routing socket */
}

struct ifaddr *
if_dl_create(const struct ifnet *ifp, const struct sockaddr_dl **sdlp)
{
	unsigned socksize, ifasize;
	int addrlen, namelen;
	struct sockaddr_dl *mask, *sdl;
	struct ifaddr *ifa;

	namelen = strlen(ifp->if_xname);
	addrlen = ifp->if_addrlen;
	socksize = roundup(sockaddr_dl_measure(namelen, addrlen), sizeof(long));
	ifasize = sizeof(*ifa) + 2 * socksize;
	ifa = (struct ifaddr *)malloc(ifasize, M_IFADDR, M_WAITOK|M_ZERO);

	sdl = (struct sockaddr_dl *)(ifa + 1);
	mask = (struct sockaddr_dl *)(socksize + (char *)sdl);

	sockaddr_dl_init(sdl, socksize, ifp->if_index, ifp->if_type,
	    ifp->if_xname, namelen, NULL, addrlen);
	mask->sdl_len = sockaddr_dl_measure(namelen, 0);
	memset(&mask->sdl_data[0], 0xff, namelen);
	ifa->ifa_rtrequest = link_rtrequest;
	ifa->ifa_addr = (struct sockaddr *)sdl;
	ifa->ifa_netmask = (struct sockaddr *)mask;

	*sdlp = sdl;

	return ifa;
}

static void
if_sadl_setrefs(struct ifnet *ifp, struct ifaddr *ifa)
{
	const struct sockaddr_dl *sdl;

	ifp->if_dl = ifa;
	ifaref(ifa);
	sdl = satosdl(ifa->ifa_addr);
	ifp->if_sadl = sdl;
}

/*
 * Allocate the link level name for the specified interface.  This
 * is an attachment helper.  It must be called after ifp->if_addrlen
 * is initialized, which may not be the case when if_attach() is
 * called.
 */
void
if_alloc_sadl(struct ifnet *ifp)
{
	struct ifaddr *ifa;
	const struct sockaddr_dl *sdl;

	/*
	 * If the interface already has a link name, release it
	 * now.  This is useful for interfaces that can change
	 * link types, and thus switch link names often.
	 */
	if (ifp->if_sadl != NULL)
		if_free_sadl(ifp);

	ifa = if_dl_create(ifp, &sdl);

	ifa_insert(ifp, ifa);
	if_sadl_setrefs(ifp, ifa);
}

static void
if_deactivate_sadl(struct ifnet *ifp)
{
	struct ifaddr *ifa;

	KASSERT(ifp->if_dl != NULL);

	ifa = ifp->if_dl;

	ifp->if_sadl = NULL;

	ifp->if_dl = NULL;
	ifafree(ifa);
}

void
if_activate_sadl(struct ifnet *ifp, struct ifaddr *ifa,
    const struct sockaddr_dl *sdl)
{
	int s;

	s = splnet();

	if_deactivate_sadl(ifp);

	if_sadl_setrefs(ifp, ifa);
	IFADDR_FOREACH(ifa, ifp)
		rtinit(ifa, RTM_LLINFO_UPD, 0);
	splx(s);
}

/*
 * Free the link level name for the specified interface.  This is
 * a detach helper.  This is called from if_detach().
 */
static void
if_free_sadl(struct ifnet *ifp)
{
	struct ifaddr *ifa;
	int s;

	ifa = ifp->if_dl;
	if (ifa == NULL) {
		KASSERT(ifp->if_sadl == NULL);
		return;
	}

	KASSERT(ifp->if_sadl != NULL);

	s = splnet();
	rtinit(ifa, RTM_DELETE, 0);
	ifa_remove(ifp, ifa);
	if_deactivate_sadl(ifp);
	if (ifp->if_hwdl == ifa) {
		ifafree(ifa);
		ifp->if_hwdl = NULL;
	}
	splx(s);
}

static void
if_getindex(ifnet_t *ifp)
{
	bool hitlimit = false;

	mutex_enter(&index_gen_mtx);
	ifp->if_index_gen = index_gen++;
	mutex_exit(&index_gen_mtx);

	ifp->if_index = if_index;
	if (ifindex2ifnet == NULL) {
		if_index++;
		goto skip;
	}
	while (if_byindex(ifp->if_index)) {
		/*
		 * If we hit USHRT_MAX, we skip back to 0 since
		 * there are a number of places where the value
		 * of if_index or if_index itself is compared
		 * to or stored in an unsigned short.  By
		 * jumping back, we won't botch those assignments
		 * or comparisons.
		 */
		if (++if_index == 0) {
			if_index = 1;
		} else if (if_index == USHRT_MAX) {
			/*
			 * However, if we have to jump back to
			 * zero *twice* without finding an empty
			 * slot in ifindex2ifnet[], then there
			 * there are too many (>65535) interfaces.
			 */
			if (hitlimit) {
				panic("too many interfaces");
			}
			hitlimit = true;
			if_index = 1;
		}
		ifp->if_index = if_index;
	}
skip:
	/*
	 * ifindex2ifnet is indexed by if_index. Since if_index will
	 * grow dynamically, it should grow too.
	 */
	if (ifindex2ifnet == NULL || ifp->if_index >= if_indexlim) {
		size_t m, n, oldlim;
		void *q;

		oldlim = if_indexlim;
		while (ifp->if_index >= if_indexlim)
			if_indexlim <<= 1;

		/* grow ifindex2ifnet */
		m = oldlim * sizeof(struct ifnet *);
		n = if_indexlim * sizeof(struct ifnet *);
		q = malloc(n, M_IFADDR, M_WAITOK|M_ZERO);
		if (ifindex2ifnet != NULL) {
			memcpy(q, ifindex2ifnet, m);
			free(ifindex2ifnet, M_IFADDR);
		}
		ifindex2ifnet = (struct ifnet **)q;
	}
	ifindex2ifnet[ifp->if_index] = ifp;
}

/*
 * Initialize an interface and assign an index for it.
 *
 * It must be called prior to a device specific attach routine
 * (e.g., ether_ifattach and ieee80211_ifattach) or if_alloc_sadl,
 * and be followed by if_register:
 *
 *     if_initialize(ifp);
 *     ether_ifattach(ifp, enaddr);
 *     if_register(ifp);
 */
void
if_initialize(ifnet_t *ifp)
{
	KASSERT(if_indexlim > 0);
	TAILQ_INIT(&ifp->if_addrlist);

	/*
	 * Link level name is allocated later by a separate call to
	 * if_alloc_sadl().
	 */

	if (ifp->if_snd.ifq_maxlen == 0)
		ifp->if_snd.ifq_maxlen = ifqmaxlen;

	ifp->if_broadcastaddr = 0; /* reliably crash if used uninitialized */

	ifp->if_link_state = LINK_STATE_UNKNOWN;

	ifp->if_capenable = 0;
	ifp->if_csum_flags_tx = 0;
	ifp->if_csum_flags_rx = 0;

#ifdef ALTQ
	ifp->if_snd.altq_type = 0;
	ifp->if_snd.altq_disc = NULL;
	ifp->if_snd.altq_flags &= ALTQF_CANTCHANGE;
	ifp->if_snd.altq_tbr  = NULL;
	ifp->if_snd.altq_ifp  = ifp;
#endif

#ifdef NET_MPSAFE
	ifp->if_snd.ifq_lock = mutex_obj_alloc(MUTEX_DEFAULT, IPL_NET);
#else
	ifp->if_snd.ifq_lock = NULL;
#endif

	ifp->if_pfil = pfil_head_create(PFIL_TYPE_IFNET, ifp);
	(void)pfil_run_hooks(if_pfil,
	    (struct mbuf **)PFIL_IFNET_ATTACH, ifp, PFIL_IFNET);

	IF_AFDATA_LOCK_INIT(ifp);

	if_getindex(ifp);
}

/*
 * Register an interface to the list of "active" interfaces.
 */
void
if_register(ifnet_t *ifp)
{
	if (ifioctl_attach(ifp) != 0)
		panic("%s: ifioctl_attach() failed", __func__);

	sysctl_sndq_setup(&ifp->if_sysctl_log, ifp->if_xname, &ifp->if_snd);

	if (!STAILQ_EMPTY(&domains))
		if_attachdomain1(ifp);

	/* Announce the interface. */
	rt_ifannouncemsg(ifp, IFAN_ARRIVAL);

	if (ifp->if_slowtimo != NULL) {
		ifp->if_slowtimo_ch =
		    kmem_zalloc(sizeof(*ifp->if_slowtimo_ch), KM_SLEEP);
		callout_init(ifp->if_slowtimo_ch, 0);
		callout_setfunc(ifp->if_slowtimo_ch, if_slowtimo, ifp);
		if_slowtimo(ifp);
	}

	TAILQ_INSERT_TAIL(&ifnet_list, ifp, if_list);
}

/*
 * Deprecated. Use if_initialize and if_register instead.
 * See the above comment of if_initialize.
 */
void
if_attach(ifnet_t *ifp)
{
	if_initialize(ifp);
	if_register(ifp);
}

void
if_attachdomain(void)
{
	struct ifnet *ifp;
	int s;

	s = splnet();
	IFNET_FOREACH(ifp)
		if_attachdomain1(ifp);
	splx(s);
}

static void
if_attachdomain1(struct ifnet *ifp)
{
	struct domain *dp;
	int s;

	s = splnet();

	/* address family dependent data region */
	memset(ifp->if_afdata, 0, sizeof(ifp->if_afdata));
	DOMAIN_FOREACH(dp) {
		if (dp->dom_ifattach != NULL)
			ifp->if_afdata[dp->dom_family] =
			    (*dp->dom_ifattach)(ifp);
	}

	splx(s);
}

/*
 * Deactivate an interface.  This points all of the procedure
 * handles at error stubs.  May be called from interrupt context.
 */
void
if_deactivate(struct ifnet *ifp)
{
	int s;

	s = splnet();

	ifp->if_output	 = if_nulloutput;
	ifp->if_input	 = if_nullinput;
	ifp->if_start	 = if_nullstart;
	ifp->if_ioctl	 = if_nullioctl;
	ifp->if_init	 = if_nullinit;
	ifp->if_stop	 = if_nullstop;
	ifp->if_slowtimo = if_nullslowtimo;
	ifp->if_drain	 = if_nulldrain;

	/* No more packets may be enqueued. */
	ifp->if_snd.ifq_maxlen = 0;

	splx(s);
}

void
if_purgeaddrs(struct ifnet *ifp, int family, void (*purgeaddr)(struct ifaddr *))
{
	struct ifaddr *ifa, *nifa;

	IFADDR_FOREACH_SAFE(ifa, ifp, nifa) {
		if (ifa->ifa_addr->sa_family != family)
			continue;
		(*purgeaddr)(ifa);
	}
}

/*
 * Detach an interface from the list of "active" interfaces,
 * freeing any resources as we go along.
 *
 * NOTE: This routine must be called with a valid thread context,
 * as it may block.
 */
void
if_detach(struct ifnet *ifp)
{
	struct socket so;
	struct ifaddr *ifa;
#ifdef IFAREF_DEBUG
	struct ifaddr *last_ifa = NULL;
#endif
	struct domain *dp;
	const struct protosw *pr;
	int s, i, family, purged;
	uint64_t xc;

	/*
	 * XXX It's kind of lame that we have to have the
	 * XXX socket structure...
	 */
	memset(&so, 0, sizeof(so));

	s = splnet();

	if (ifp->if_slowtimo != NULL) {
		ifp->if_slowtimo = NULL;
		callout_halt(ifp->if_slowtimo_ch, NULL);
		callout_destroy(ifp->if_slowtimo_ch);
		kmem_free(ifp->if_slowtimo_ch, sizeof(*ifp->if_slowtimo_ch));
	}

	/*
	 * Do an if_down() to give protocols a chance to do something.
	 */
	if_down(ifp);

#ifdef ALTQ
	if (ALTQ_IS_ENABLED(&ifp->if_snd))
		altq_disable(&ifp->if_snd);
	if (ALTQ_IS_ATTACHED(&ifp->if_snd))
		altq_detach(&ifp->if_snd);
#endif

	if (ifp->if_snd.ifq_lock)
		mutex_obj_free(ifp->if_snd.ifq_lock);

	sysctl_teardown(&ifp->if_sysctl_log);

#if NCARP > 0
	/* Remove the interface from any carp group it is a part of.  */
	if (ifp->if_carp != NULL && ifp->if_type != IFT_CARP)
		carp_ifdetach(ifp);
#endif

	/*
	 * Rip all the addresses off the interface.  This should make
	 * all of the routes go away.
	 *
	 * pr_usrreq calls can remove an arbitrary number of ifaddrs
	 * from the list, including our "cursor", ifa.  For safety,
	 * and to honor the TAILQ abstraction, I just restart the
	 * loop after each removal.  Note that the loop will exit
	 * when all of the remaining ifaddrs belong to the AF_LINK
	 * family.  I am counting on the historical fact that at
	 * least one pr_usrreq in each address domain removes at
	 * least one ifaddr.
	 */
again:
	IFADDR_FOREACH(ifa, ifp) {
		family = ifa->ifa_addr->sa_family;
#ifdef IFAREF_DEBUG
		printf("if_detach: ifaddr %p, family %d, refcnt %d\n",
		    ifa, family, ifa->ifa_refcnt);
		if (last_ifa != NULL && ifa == last_ifa)
			panic("if_detach: loop detected");
		last_ifa = ifa;
#endif
		if (family == AF_LINK)
			continue;
		dp = pffinddomain(family);
#ifdef DIAGNOSTIC
		if (dp == NULL)
			panic("if_detach: no domain for AF %d",
			    family);
#endif
		/*
		 * XXX These PURGEIF calls are redundant with the
		 * purge-all-families calls below, but are left in for
		 * now both to make a smaller change, and to avoid
		 * unplanned interactions with clearing of
		 * ifp->if_addrlist.
		 */
		purged = 0;
		for (pr = dp->dom_protosw;
		     pr < dp->dom_protoswNPROTOSW; pr++) {
			so.so_proto = pr;
			if (pr->pr_usrreqs) {
				(void) (*pr->pr_usrreqs->pr_purgeif)(&so, ifp);
				purged = 1;
			}
		}
		if (purged == 0) {
			/*
			 * XXX What's really the best thing to do
			 * XXX here?  --thorpej@NetBSD.org
			 */
			printf("if_detach: WARNING: AF %d not purged\n",
			    family);
			ifa_remove(ifp, ifa);
		}
		goto again;
	}

	if_free_sadl(ifp);

	/* Walk the routing table looking for stragglers. */
	for (i = 0; i <= AF_MAX; i++) {
		while (rt_walktree(i, if_rt_walktree, ifp) == ERESTART)
			continue;
	}

	DOMAIN_FOREACH(dp) {
		if (dp->dom_ifdetach != NULL && ifp->if_afdata[dp->dom_family])
		{
			void *p = ifp->if_afdata[dp->dom_family];
			if (p) {
				ifp->if_afdata[dp->dom_family] = NULL;
				(*dp->dom_ifdetach)(ifp, p);
			}
		}

		/*
		 * One would expect multicast memberships (INET and
		 * INET6) on UDP sockets to be purged by the PURGEIF
		 * calls above, but if all addresses were removed from
		 * the interface prior to destruction, the calls will
		 * not be made (e.g. ppp, for which pppd(8) generally
		 * removes addresses before destroying the interface).
		 * Because there is no invariant that multicast
		 * memberships only exist for interfaces with IPv4
		 * addresses, we must call PURGEIF regardless of
		 * addresses.  (Protocols which might store ifnet
		 * pointers are marked with PR_PURGEIF.)
		 */
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++) {
			so.so_proto = pr;
			if (pr->pr_usrreqs && pr->pr_flags & PR_PURGEIF)
				(void)(*pr->pr_usrreqs->pr_purgeif)(&so, ifp);
		}
	}

	(void)pfil_run_hooks(if_pfil,
	    (struct mbuf **)PFIL_IFNET_DETACH, ifp, PFIL_IFNET);
	(void)pfil_head_destroy(ifp->if_pfil);

	/* Announce that the interface is gone. */
	rt_ifannouncemsg(ifp, IFAN_DEPARTURE);

	ifindex2ifnet[ifp->if_index] = NULL;

	TAILQ_REMOVE(&ifnet_list, ifp, if_list);

	ifioctl_detach(ifp);

	/*
	 * remove packets that came from ifp, from software interrupt queues.
	 */
	DOMAIN_FOREACH(dp) {
		for (i = 0; i < __arraycount(dp->dom_ifqueues); i++) {
			struct ifqueue *iq = dp->dom_ifqueues[i];
			if (iq == NULL)
				break;
			dp->dom_ifqueues[i] = NULL;
			if_detach_queues(ifp, iq);
		}
	}

	/*
	 * IP queues have to be processed separately: net-queue barrier
	 * ensures that the packets are dequeued while a cross-call will
	 * ensure that the interrupts have completed. FIXME: not quite..
	 */
#ifdef INET
	pktq_barrier(ip_pktq);
#endif
#ifdef INET6
	if (in6_present)
		pktq_barrier(ip6_pktq);
#endif
	xc = xc_broadcast(0, (xcfunc_t)nullop, NULL, NULL);
	xc_wait(xc);

	splx(s);
}

static void
if_detach_queues(struct ifnet *ifp, struct ifqueue *q)
{
	struct mbuf *m, *prev, *next;

	prev = NULL;
	for (m = q->ifq_head; m != NULL; m = next) {
		KASSERT((m->m_flags & M_PKTHDR) != 0);

		next = m->m_nextpkt;
		if (m->m_pkthdr.rcvif != ifp) {
			prev = m;
			continue;
		}

		if (prev != NULL)
			prev->m_nextpkt = m->m_nextpkt;
		else
			q->ifq_head = m->m_nextpkt;
		if (q->ifq_tail == m)
			q->ifq_tail = prev;
		q->ifq_len--;

		m->m_nextpkt = NULL;
		m_freem(m);
		IF_DROP(q);
	}
}

/*
 * Callback for a radix tree walk to delete all references to an
 * ifnet.
 */
static int
if_rt_walktree(struct rtentry *rt, void *v)
{
	struct ifnet *ifp = (struct ifnet *)v;
	int error;
	struct rtentry *retrt;

	if (rt->rt_ifp != ifp)
		return 0;

	/* Delete the entry. */
	error = rtrequest(RTM_DELETE, rt_getkey(rt), rt->rt_gateway,
	    rt_mask(rt), rt->rt_flags, &retrt);
	if (error == 0) {
		KASSERT(retrt == rt);
		KASSERT((retrt->rt_flags & RTF_UP) == 0);
		retrt->rt_ifp = NULL;
		rtfree(retrt);
	} else {
		printf("%s: warning: unable to delete rtentry @ %p, "
		    "error = %d\n", ifp->if_xname, rt, error);
	}
	return ERESTART;
}

/*
 * Create a clone network interface.
 */
static int
if_clone_create(const char *name)
{
	struct if_clone *ifc;
	int unit;

	ifc = if_clone_lookup(name, &unit);
	if (ifc == NULL)
		return EINVAL;

	if (ifunit(name) != NULL)
		return EEXIST;

	return (*ifc->ifc_create)(ifc, unit);
}

/*
 * Destroy a clone network interface.
 */
static int
if_clone_destroy(const char *name)
{
	struct if_clone *ifc;
	struct ifnet *ifp;

	ifc = if_clone_lookup(name, NULL);
	if (ifc == NULL)
		return EINVAL;

	ifp = ifunit(name);
	if (ifp == NULL)
		return ENXIO;

	if (ifc->ifc_destroy == NULL)
		return EOPNOTSUPP;

	return (*ifc->ifc_destroy)(ifp);
}

/*
 * Look up a network interface cloner.
 */
static struct if_clone *
if_clone_lookup(const char *name, int *unitp)
{
	struct if_clone *ifc;
	const char *cp;
	char *dp, ifname[IFNAMSIZ + 3];
	int unit;

	strcpy(ifname, "if_");
	/* separate interface name from unit */
	for (dp = ifname + 3, cp = name; cp - name < IFNAMSIZ &&
	    *cp && (*cp < '0' || *cp > '9');)
		*dp++ = *cp++;

	if (cp == name || cp - name == IFNAMSIZ || !*cp)
		return NULL;	/* No name or unit number */
	*dp++ = '\0';

again:
	LIST_FOREACH(ifc, &if_cloners, ifc_list) {
		if (strcmp(ifname + 3, ifc->ifc_name) == 0)
			break;
	}

	if (ifc == NULL) {
		if (*ifname == '\0' ||
		    module_autoload(ifname, MODULE_CLASS_DRIVER))
			return NULL;
		*ifname = '\0';
		goto again;
	}

	unit = 0;
	while (cp - name < IFNAMSIZ && *cp) {
		if (*cp < '0' || *cp > '9' || unit >= INT_MAX / 10) {
			/* Bogus unit number. */
			return NULL;
		}
		unit = (unit * 10) + (*cp++ - '0');
	}

	if (unitp != NULL)
		*unitp = unit;
	return ifc;
}

/*
 * Register a network interface cloner.
 */
void
if_clone_attach(struct if_clone *ifc)
{

	LIST_INSERT_HEAD(&if_cloners, ifc, ifc_list);
	if_cloners_count++;
}

/*
 * Unregister a network interface cloner.
 */
void
if_clone_detach(struct if_clone *ifc)
{

	LIST_REMOVE(ifc, ifc_list);
	if_cloners_count--;
}

/*
 * Provide list of interface cloners to userspace.
 */
int
if_clone_list(int buf_count, char *buffer, int *total)
{
	char outbuf[IFNAMSIZ], *dst;
	struct if_clone *ifc;
	int count, error = 0;

	*total = if_cloners_count;
	if ((dst = buffer) == NULL) {
		/* Just asking how many there are. */
		return 0;
	}

	if (buf_count < 0)
		return EINVAL;

	count = (if_cloners_count < buf_count) ?
	    if_cloners_count : buf_count;

	for (ifc = LIST_FIRST(&if_cloners); ifc != NULL && count != 0;
	     ifc = LIST_NEXT(ifc, ifc_list), count--, dst += IFNAMSIZ) {
		(void)strncpy(outbuf, ifc->ifc_name, sizeof(outbuf));
		if (outbuf[sizeof(outbuf) - 1] != '\0')
			return ENAMETOOLONG;
		error = copyout(outbuf, dst, sizeof(outbuf));
		if (error != 0)
			break;
	}

	return error;
}

void
ifaref(struct ifaddr *ifa)
{
	ifa->ifa_refcnt++;
}

void
ifafree(struct ifaddr *ifa)
{
	KASSERT(ifa != NULL);
	KASSERT(ifa->ifa_refcnt > 0);

	if (--ifa->ifa_refcnt == 0) {
		free(ifa, M_IFADDR);
	}
}

void
ifa_insert(struct ifnet *ifp, struct ifaddr *ifa)
{
	ifa->ifa_ifp = ifp;
	TAILQ_INSERT_TAIL(&ifp->if_addrlist, ifa, ifa_list);
	ifaref(ifa);
}

void
ifa_remove(struct ifnet *ifp, struct ifaddr *ifa)
{
	KASSERT(ifa->ifa_ifp == ifp);
	TAILQ_REMOVE(&ifp->if_addrlist, ifa, ifa_list);
	ifafree(ifa);
}

static inline int
equal(const struct sockaddr *sa1, const struct sockaddr *sa2)
{
	return sockaddr_cmp(sa1, sa2) == 0;
}

/*
 * Locate an interface based on a complete address.
 */
/*ARGSUSED*/
struct ifaddr *
ifa_ifwithaddr(const struct sockaddr *addr)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;

	IFNET_FOREACH(ifp) {
		if (ifp->if_output == if_nulloutput)
			continue;
		IFADDR_FOREACH(ifa, ifp) {
			if (ifa->ifa_addr->sa_family != addr->sa_family)
				continue;
			if (equal(addr, ifa->ifa_addr))
				return ifa;
			if ((ifp->if_flags & IFF_BROADCAST) &&
			    ifa->ifa_broadaddr &&
			    /* IP6 doesn't have broadcast */
			    ifa->ifa_broadaddr->sa_len != 0 &&
			    equal(ifa->ifa_broadaddr, addr))
				return ifa;
		}
	}
	return NULL;
}

/*
 * Locate the point to point interface with a given destination address.
 */
/*ARGSUSED*/
struct ifaddr *
ifa_ifwithdstaddr(const struct sockaddr *addr)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;

	IFNET_FOREACH(ifp) {
		if (ifp->if_output == if_nulloutput)
			continue;
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			continue;
		IFADDR_FOREACH(ifa, ifp) {
			if (ifa->ifa_addr->sa_family != addr->sa_family ||
			    ifa->ifa_dstaddr == NULL)
				continue;
			if (equal(addr, ifa->ifa_dstaddr))
				return ifa;
		}
	}
	return NULL;
}

/*
 * Find an interface on a specific network.  If many, choice
 * is most specific found.
 */
struct ifaddr *
ifa_ifwithnet(const struct sockaddr *addr)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;
	const struct sockaddr_dl *sdl;
	struct ifaddr *ifa_maybe = 0;
	u_int af = addr->sa_family;
	const char *addr_data = addr->sa_data, *cplim;

	if (af == AF_LINK) {
		sdl = satocsdl(addr);
		if (sdl->sdl_index && sdl->sdl_index < if_indexlim &&
		    ifindex2ifnet[sdl->sdl_index] &&
		    ifindex2ifnet[sdl->sdl_index]->if_output != if_nulloutput) {
			return ifindex2ifnet[sdl->sdl_index]->if_dl;
		}
	}
#ifdef NETATALK
	if (af == AF_APPLETALK) {
		const struct sockaddr_at *sat, *sat2;
		sat = (const struct sockaddr_at *)addr;
		IFNET_FOREACH(ifp) {
			if (ifp->if_output == if_nulloutput)
				continue;
			ifa = at_ifawithnet((const struct sockaddr_at *)addr, ifp);
			if (ifa == NULL)
				continue;
			sat2 = (struct sockaddr_at *)ifa->ifa_addr;
			if (sat2->sat_addr.s_net == sat->sat_addr.s_net)
				return ifa; /* exact match */
			if (ifa_maybe == NULL) {
				/* else keep the if with the right range */
				ifa_maybe = ifa;
			}
		}
		return ifa_maybe;
	}
#endif
	IFNET_FOREACH(ifp) {
		if (ifp->if_output == if_nulloutput)
			continue;
		IFADDR_FOREACH(ifa, ifp) {
			const char *cp, *cp2, *cp3;

			if (ifa->ifa_addr->sa_family != af ||
			    ifa->ifa_netmask == NULL)
 next:				continue;
			cp = addr_data;
			cp2 = ifa->ifa_addr->sa_data;
			cp3 = ifa->ifa_netmask->sa_data;
			cplim = (const char *)ifa->ifa_netmask +
			    ifa->ifa_netmask->sa_len;
			while (cp3 < cplim) {
				if ((*cp++ ^ *cp2++) & *cp3++) {
					/* want to continue for() loop */
					goto next;
				}
			}
			if (ifa_maybe == NULL ||
			    rn_refines((void *)ifa->ifa_netmask,
			    (void *)ifa_maybe->ifa_netmask))
				ifa_maybe = ifa;
		}
	}
	return ifa_maybe;
}

/*
 * Find the interface of the addresss.
 */
struct ifaddr *
ifa_ifwithladdr(const struct sockaddr *addr)
{
	struct ifaddr *ia;

	if ((ia = ifa_ifwithaddr(addr)) || (ia = ifa_ifwithdstaddr(addr)) ||
	    (ia = ifa_ifwithnet(addr)))
		return ia;
	return NULL;
}

/*
 * Find an interface using a specific address family
 */
struct ifaddr *
ifa_ifwithaf(int af)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;

	IFNET_FOREACH(ifp) {
		if (ifp->if_output == if_nulloutput)
			continue;
		IFADDR_FOREACH(ifa, ifp) {
			if (ifa->ifa_addr->sa_family == af)
				return ifa;
		}
	}
	return NULL;
}

/*
 * Find an interface address specific to an interface best matching
 * a given address.
 */
struct ifaddr *
ifaof_ifpforaddr(const struct sockaddr *addr, struct ifnet *ifp)
{
	struct ifaddr *ifa;
	const char *cp, *cp2, *cp3;
	const char *cplim;
	struct ifaddr *ifa_maybe = 0;
	u_int af = addr->sa_family;

	if (ifp->if_output == if_nulloutput)
		return NULL;

	if (af >= AF_MAX)
		return NULL;

	IFADDR_FOREACH(ifa, ifp) {
		if (ifa->ifa_addr->sa_family != af)
			continue;
		ifa_maybe = ifa;
		if (ifa->ifa_netmask == NULL) {
			if (equal(addr, ifa->ifa_addr) ||
			    (ifa->ifa_dstaddr &&
			     equal(addr, ifa->ifa_dstaddr)))
				return ifa;
			continue;
		}
		cp = addr->sa_data;
		cp2 = ifa->ifa_addr->sa_data;
		cp3 = ifa->ifa_netmask->sa_data;
		cplim = ifa->ifa_netmask->sa_len + (char *)ifa->ifa_netmask;
		for (; cp3 < cplim; cp3++) {
			if ((*cp++ ^ *cp2++) & *cp3)
				break;
		}
		if (cp3 == cplim)
			return ifa;
	}
	return ifa_maybe;
}

/*
 * Default action when installing a route with a Link Level gateway.
 * Lookup an appropriate real ifa to point to.
 * This should be moved to /sys/net/link.c eventually.
 */
void
link_rtrequest(int cmd, struct rtentry *rt, const struct rt_addrinfo *info)
{
	struct ifaddr *ifa;
	const struct sockaddr *dst;
	struct ifnet *ifp;

	if (cmd != RTM_ADD || (ifa = rt->rt_ifa) == NULL ||
	    (ifp = ifa->ifa_ifp) == NULL || (dst = rt_getkey(rt)) == NULL)
		return;
	if ((ifa = ifaof_ifpforaddr(dst, ifp)) != NULL) {
		rt_replace_ifa(rt, ifa);
		if (ifa->ifa_rtrequest && ifa->ifa_rtrequest != link_rtrequest)
			ifa->ifa_rtrequest(cmd, rt, info);
	}
}

/*
 * Handle a change in the interface link state.
 * XXX: We should listen to the routing socket in-kernel rather
 * than calling in6_if_link_* functions directly from here.
 */
void
if_link_state_change(struct ifnet *ifp, int link_state)
{
	int s;
	int old_link_state;
	struct domain *dp;

	s = splnet();
	if (ifp->if_link_state == link_state) {
		splx(s);
		return;
	}

	old_link_state = ifp->if_link_state;
	ifp->if_link_state = link_state;
#ifdef DEBUG
	log(LOG_DEBUG, "%s: link state %s (was %s)\n", ifp->if_xname,
		link_state == LINK_STATE_UP ? "UP" :
		link_state == LINK_STATE_DOWN ? "DOWN" :
		"UNKNOWN",
		 old_link_state == LINK_STATE_UP ? "UP" :
		old_link_state == LINK_STATE_DOWN ? "DOWN" :
		"UNKNOWN");
#endif

	/*
	 * When going from UNKNOWN to UP, we need to mark existing
	 * addresses as tentative and restart DAD as we may have
	 * erroneously not found a duplicate.
	 *
	 * This needs to happen before rt_ifmsg to avoid a race where
	 * listeners would have an address and expect it to work right
	 * away.
	 */
	if (link_state == LINK_STATE_UP &&
	    old_link_state == LINK_STATE_UNKNOWN)
	{
		DOMAIN_FOREACH(dp) {
			if (dp->dom_if_link_state_change != NULL)
				dp->dom_if_link_state_change(ifp,
				    LINK_STATE_DOWN);
		}
	}

	/* Notify that the link state has changed. */
	rt_ifmsg(ifp);

#if NCARP > 0
	if (ifp->if_carp)
		carp_carpdev_state(ifp);
#endif

	DOMAIN_FOREACH(dp) {
		if (dp->dom_if_link_state_change != NULL)
			dp->dom_if_link_state_change(ifp, link_state);
	}

	splx(s);
}

/*
 * Default action when installing a local route on a point-to-point
 * interface.
 */
void
p2p_rtrequest(int req, struct rtentry *rt,
    __unused const struct rt_addrinfo *info)
{
	struct ifnet *ifp = rt->rt_ifp;
	struct ifaddr *ifa, *lo0ifa;

	switch (req) {
	case RTM_ADD:
		if ((rt->rt_flags & RTF_LOCAL) == 0)
			break;

		IFADDR_FOREACH(ifa, ifp) {
			if (equal(rt_getkey(rt), ifa->ifa_addr))
				break;
		}
		if (ifa == NULL)
			break;

		/*
		 * Ensure lo0 has an address of the same family.
		 */
		IFADDR_FOREACH(lo0ifa, lo0ifp) {
			if (lo0ifa->ifa_addr->sa_family ==
			    ifa->ifa_addr->sa_family)
				break;
		}
		if (lo0ifa == NULL)
			break;

		rt->rt_ifp = lo0ifp;
		rt->rt_flags &= ~RTF_LLINFO;

		/*
		 * Make sure to set rt->rt_ifa to the interface
		 * address we are using, otherwise we will have trouble
		 * with source address selection.
		 */
		if (ifa != rt->rt_ifa)
			rt_replace_ifa(rt, ifa);
		break;
	case RTM_DELETE:
	case RTM_RESOLVE:
	default:
		break;
	}
}

/*
 * Mark an interface down and notify protocols of
 * the transition.
 * NOTE: must be called at splsoftnet or equivalent.
 */
void
if_down(struct ifnet *ifp)
{
	struct ifaddr *ifa;
	struct domain *dp;

	ifp->if_flags &= ~IFF_UP;
	nanotime(&ifp->if_lastchange);
	IFADDR_FOREACH(ifa, ifp)
		pfctlinput(PRC_IFDOWN, ifa->ifa_addr);
	IFQ_PURGE(&ifp->if_snd);
#if NCARP > 0
	if (ifp->if_carp)
		carp_carpdev_state(ifp);
#endif
	rt_ifmsg(ifp);
	DOMAIN_FOREACH(dp) {
		if (dp->dom_if_down)
			dp->dom_if_down(ifp);
	}
}

/*
 * Mark an interface up and notify protocols of
 * the transition.
 * NOTE: must be called at splsoftnet or equivalent.
 */
void
if_up(struct ifnet *ifp)
{
#ifdef notyet
	struct ifaddr *ifa;
#endif
	struct domain *dp;

	ifp->if_flags |= IFF_UP;
	nanotime(&ifp->if_lastchange);
#ifdef notyet
	/* this has no effect on IP, and will kill all ISO connections XXX */
	IFADDR_FOREACH(ifa, ifp)
		pfctlinput(PRC_IFUP, ifa->ifa_addr);
#endif
#if NCARP > 0
	if (ifp->if_carp)
		carp_carpdev_state(ifp);
#endif
	rt_ifmsg(ifp);
	DOMAIN_FOREACH(dp) {
		if (dp->dom_if_up)
			dp->dom_if_up(ifp);
	}
}

/*
 * Handle interface slowtimo timer routine.  Called
 * from softclock, we decrement timer (if set) and
 * call the appropriate interface routine on expiration.
 */
static void
if_slowtimo(void *arg)
{
	void (*slowtimo)(struct ifnet *);
	struct ifnet *ifp = arg;
	int s;

	slowtimo = ifp->if_slowtimo;
	if (__predict_false(slowtimo == NULL))
		return;

	s = splnet();
	if (ifp->if_timer != 0 && --ifp->if_timer == 0)
		(*slowtimo)(ifp);

	splx(s);

	if (__predict_true(ifp->if_slowtimo != NULL))
		callout_schedule(ifp->if_slowtimo_ch, hz / IFNET_SLOWHZ);
}

/*
 * Set/clear promiscuous mode on interface ifp based on the truth value
 * of pswitch.  The calls are reference counted so that only the first
 * "on" request actually has an effect, as does the final "off" request.
 * Results are undefined if the "off" and "on" requests are not matched.
 */
int
ifpromisc(struct ifnet *ifp, int pswitch)
{
	int pcount, ret;
	short nflags;

	pcount = ifp->if_pcount;
	if (pswitch) {
		/*
		 * Allow the device to be "placed" into promiscuous
		 * mode even if it is not configured up.  It will
		 * consult IFF_PROMISC when it is brought up.
		 */
		if (ifp->if_pcount++ != 0)
			return 0;
		nflags = ifp->if_flags | IFF_PROMISC;
	} else {
		if (--ifp->if_pcount > 0)
			return 0;
		nflags = ifp->if_flags & ~IFF_PROMISC;
	}
	ret = if_flags_set(ifp, nflags);
	/* Restore interface state if not successful. */
	if (ret != 0) {
		ifp->if_pcount = pcount;
	}
	return ret;
}

/*
 * Map interface name to
 * interface structure pointer.
 */
struct ifnet *
ifunit(const char *name)
{
	struct ifnet *ifp;
	const char *cp = name;
	u_int unit = 0;
	u_int i;

	/*
	 * If the entire name is a number, treat it as an ifindex.
	 */
	for (i = 0; i < IFNAMSIZ && *cp >= '0' && *cp <= '9'; i++, cp++) {
		unit = unit * 10 + (*cp - '0');
	}

	/*
	 * If the number took all of the name, then it's a valid ifindex.
	 */
	if (i == IFNAMSIZ || (cp != name && *cp == '\0')) {
		if (unit >= if_indexlim)
			return NULL;
		ifp = ifindex2ifnet[unit];
		if (ifp == NULL || ifp->if_output == if_nulloutput)
			return NULL;
		return ifp;
	}

	IFNET_FOREACH(ifp) {
		if (ifp->if_output == if_nulloutput)
			continue;
	 	if (strcmp(ifp->if_xname, name) == 0)
			return ifp;
	}
	return NULL;
}

ifnet_t *
if_byindex(u_int idx)
{
	return (idx < if_indexlim) ? ifindex2ifnet[idx] : NULL;
}

/* common */
int
ifioctl_common(struct ifnet *ifp, u_long cmd, void *data)
{
	int s;
	struct ifreq *ifr;
	struct ifcapreq *ifcr;
	struct ifdatareq *ifdr;

	switch (cmd) {
	case SIOCSIFCAP:
		ifcr = data;
		if ((ifcr->ifcr_capenable & ~ifp->if_capabilities) != 0)
			return EINVAL;

		if (ifcr->ifcr_capenable == ifp->if_capenable)
			return 0;

		ifp->if_capenable = ifcr->ifcr_capenable;

		/* Pre-compute the checksum flags mask. */
		ifp->if_csum_flags_tx = 0;
		ifp->if_csum_flags_rx = 0;
		if (ifp->if_capenable & IFCAP_CSUM_IPv4_Tx) {
			ifp->if_csum_flags_tx |= M_CSUM_IPv4;
		}
		if (ifp->if_capenable & IFCAP_CSUM_IPv4_Rx) {
			ifp->if_csum_flags_rx |= M_CSUM_IPv4;
		}

		if (ifp->if_capenable & IFCAP_CSUM_TCPv4_Tx) {
			ifp->if_csum_flags_tx |= M_CSUM_TCPv4;
		}
		if (ifp->if_capenable & IFCAP_CSUM_TCPv4_Rx) {
			ifp->if_csum_flags_rx |= M_CSUM_TCPv4;
		}

		if (ifp->if_capenable & IFCAP_CSUM_UDPv4_Tx) {
			ifp->if_csum_flags_tx |= M_CSUM_UDPv4;
		}
		if (ifp->if_capenable & IFCAP_CSUM_UDPv4_Rx) {
			ifp->if_csum_flags_rx |= M_CSUM_UDPv4;
		}

		if (ifp->if_capenable & IFCAP_CSUM_TCPv6_Tx) {
			ifp->if_csum_flags_tx |= M_CSUM_TCPv6;
		}
		if (ifp->if_capenable & IFCAP_CSUM_TCPv6_Rx) {
			ifp->if_csum_flags_rx |= M_CSUM_TCPv6;
		}

		if (ifp->if_capenable & IFCAP_CSUM_UDPv6_Tx) {
			ifp->if_csum_flags_tx |= M_CSUM_UDPv6;
		}
		if (ifp->if_capenable & IFCAP_CSUM_UDPv6_Rx) {
			ifp->if_csum_flags_rx |= M_CSUM_UDPv6;
		}
		if (ifp->if_flags & IFF_UP)
			return ENETRESET;
		return 0;
	case SIOCSIFFLAGS:
		ifr = data;
		if (ifp->if_flags & IFF_UP && (ifr->ifr_flags & IFF_UP) == 0) {
			s = splnet();
			if_down(ifp);
			splx(s);
		}
		if (ifr->ifr_flags & IFF_UP && (ifp->if_flags & IFF_UP) == 0) {
			s = splnet();
			if_up(ifp);
			splx(s);
		}
		ifp->if_flags = (ifp->if_flags & IFF_CANTCHANGE) |
			(ifr->ifr_flags &~ IFF_CANTCHANGE);
		break;
	case SIOCGIFFLAGS:
		ifr = data;
		ifr->ifr_flags = ifp->if_flags;
		break;

	case SIOCGIFMETRIC:
		ifr = data;
		ifr->ifr_metric = ifp->if_metric;
		break;

	case SIOCGIFMTU:
		ifr = data;
		ifr->ifr_mtu = ifp->if_mtu;
		break;

	case SIOCGIFDLT:
		ifr = data;
		ifr->ifr_dlt = ifp->if_dlt;
		break;

	case SIOCGIFCAP:
		ifcr = data;
		ifcr->ifcr_capabilities = ifp->if_capabilities;
		ifcr->ifcr_capenable = ifp->if_capenable;
		break;

	case SIOCSIFMETRIC:
		ifr = data;
		ifp->if_metric = ifr->ifr_metric;
		break;

	case SIOCGIFDATA:
		ifdr = data;
		ifdr->ifdr_data = ifp->if_data;
		break;

	case SIOCGIFINDEX:
		ifr = data;
		ifr->ifr_index = ifp->if_index;
		break;

	case SIOCZIFDATA:
		ifdr = data;
		ifdr->ifdr_data = ifp->if_data;
		/*
		 * Assumes that the volatile counters that can be
		 * zero'ed are at the end of if_data.
		 */
		memset(&ifp->if_data.ifi_ipackets, 0, sizeof(ifp->if_data) -
		    offsetof(struct if_data, ifi_ipackets));
		/*
		 * The memset() clears to the bottm of if_data. In the area,
		 * if_lastchange is included. Please be careful if new entry
		 * will be added into if_data or rewite this.
		 *
		 * And also, update if_lastchnage.
		 */
		getnanotime(&ifp->if_lastchange);
		break;
	case SIOCSIFMTU:
		ifr = data;
		if (ifp->if_mtu == ifr->ifr_mtu)
			break;
		ifp->if_mtu = ifr->ifr_mtu;
		/*
		 * If the link MTU changed, do network layer specific procedure.
		 */
#ifdef INET6
		if (in6_present)
			nd6_setmtu(ifp);
#endif
		return ENETRESET;
	default:
		return ENOTTY;
	}
	return 0;
}

int
ifaddrpref_ioctl(struct socket *so, u_long cmd, void *data, struct ifnet *ifp)
{
	struct if_addrprefreq *ifap = (struct if_addrprefreq *)data;
	struct ifaddr *ifa;
	const struct sockaddr *any, *sa;
	union {
		struct sockaddr sa;
		struct sockaddr_storage ss;
	} u, v;

	switch (cmd) {
	case SIOCSIFADDRPREF:
		if (kauth_authorize_network(curlwp->l_cred, KAUTH_NETWORK_INTERFACE,
		    KAUTH_REQ_NETWORK_INTERFACE_SETPRIV, ifp, (void *)cmd,
		    NULL) != 0)
			return EPERM;
	case SIOCGIFADDRPREF:
		break;
	default:
		return EOPNOTSUPP;
	}

	/* sanity checks */
	if (data == NULL || ifp == NULL) {
		panic("invalid argument to %s", __func__);
		/*NOTREACHED*/
	}

	/* address must be specified on ADD and DELETE */
	sa = sstocsa(&ifap->ifap_addr);
	if (sa->sa_family != sofamily(so))
		return EINVAL;
	if ((any = sockaddr_any(sa)) == NULL || sa->sa_len != any->sa_len)
		return EINVAL;

	sockaddr_externalize(&v.sa, sizeof(v.ss), sa);

	IFADDR_FOREACH(ifa, ifp) {
		if (ifa->ifa_addr->sa_family != sa->sa_family)
			continue;
		sockaddr_externalize(&u.sa, sizeof(u.ss), ifa->ifa_addr);
		if (sockaddr_cmp(&u.sa, &v.sa) == 0)
			break;
	}
	if (ifa == NULL)
		return EADDRNOTAVAIL;

	switch (cmd) {
	case SIOCSIFADDRPREF:
		ifa->ifa_preference = ifap->ifap_preference;
		return 0;
	case SIOCGIFADDRPREF:
		/* fill in the if_laddrreq structure */
		(void)sockaddr_copy(sstosa(&ifap->ifap_addr),
		    sizeof(ifap->ifap_addr), ifa->ifa_addr);
		ifap->ifap_preference = ifa->ifa_preference;
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

static void
ifnet_lock_enter(struct ifnet_lock *il)
{
	uint64_t *nenter;

	/* Before trying to acquire the mutex, increase the count of threads
	 * who have entered or who wait to enter the critical section.
	 * Avoid one costly locked memory transaction by keeping a count for
	 * each CPU.
	 */
	nenter = percpu_getref(il->il_nenter);
	(*nenter)++;
	percpu_putref(il->il_nenter);
	mutex_enter(&il->il_lock);
}

static void
ifnet_lock_exit(struct ifnet_lock *il)
{
	/* Increase the count of threads who have exited the critical
	 * section.  Increase while we still hold the lock.
	 */
	il->il_nexit++;
	mutex_exit(&il->il_lock);
}

/*
 * Interface ioctls.
 */
static int
doifioctl(struct socket *so, u_long cmd, void *data, struct lwp *l)
{
	struct ifnet *ifp;
	struct ifreq *ifr;
	int error = 0;
#if defined(COMPAT_OSOCK) || defined(COMPAT_OIFREQ)
	u_long ocmd = cmd;
#endif
	short oif_flags;
#ifdef COMPAT_OIFREQ
	struct ifreq ifrb;
	struct oifreq *oifr = NULL;
#endif
	int r;

	switch (cmd) {
#ifdef COMPAT_OIFREQ
	case OSIOCGIFCONF:
	case OOSIOCGIFCONF:
		return compat_ifconf(cmd, data);
#endif
#ifdef COMPAT_OIFDATA
	case OSIOCGIFDATA:
	case OSIOCZIFDATA:
		return compat_ifdatareq(l, cmd, data);
#endif
	case SIOCGIFCONF:
		return ifconf(cmd, data);
	case SIOCINITIFADDR:
		return EPERM;
	}

#ifdef COMPAT_OIFREQ
	cmd = compat_cvtcmd(cmd);
	if (cmd != ocmd) {
		oifr = data;
		data = ifr = &ifrb;
		ifreqo2n(oifr, ifr);
	} else
#endif
		ifr = data;

	ifp = ifunit(ifr->ifr_name);

	switch (cmd) {
	case SIOCIFCREATE:
	case SIOCIFDESTROY:
		if (l != NULL) {
			error = kauth_authorize_network(l->l_cred,
			    KAUTH_NETWORK_INTERFACE,
			    KAUTH_REQ_NETWORK_INTERFACE_SETPRIV, ifp,
			    (void *)cmd, NULL);
			if (error != 0)
				return error;
		}
		mutex_enter(&if_clone_mtx);
		r = (cmd == SIOCIFCREATE) ?
			if_clone_create(ifr->ifr_name) :
			if_clone_destroy(ifr->ifr_name);
		mutex_exit(&if_clone_mtx);
		return r;

	case SIOCIFGCLONERS:
		{
			struct if_clonereq *req = (struct if_clonereq *)data;
			return if_clone_list(req->ifcr_count, req->ifcr_buffer,
			    &req->ifcr_total);
		}
	}

	if (ifp == NULL)
		return ENXIO;

	switch (cmd) {
	case SIOCALIFADDR:
	case SIOCDLIFADDR:
	case SIOCSIFADDRPREF:
	case SIOCSIFFLAGS:
	case SIOCSIFCAP:
	case SIOCSIFMETRIC:
	case SIOCZIFDATA:
	case SIOCSIFMTU:
	case SIOCSIFPHYADDR:
	case SIOCDIFPHYADDR:
#ifdef INET6
	case SIOCSIFPHYADDR_IN6:
#endif
	case SIOCSLIFPHYADDR:
	case SIOCADDMULTI:
	case SIOCDELMULTI:
	case SIOCSIFMEDIA:
	case SIOCSDRVSPEC:
	case SIOCG80211:
	case SIOCS80211:
	case SIOCS80211NWID:
	case SIOCS80211NWKEY:
	case SIOCS80211POWER:
	case SIOCS80211BSSID:
	case SIOCS80211CHANNEL:
	case SIOCSLINKSTR:
		if (l != NULL) {
			error = kauth_authorize_network(l->l_cred,
			    KAUTH_NETWORK_INTERFACE,
			    KAUTH_REQ_NETWORK_INTERFACE_SETPRIV, ifp,
			    (void *)cmd, NULL);
			if (error != 0)
				return error;
		}
	}

	oif_flags = ifp->if_flags;

	ifnet_lock_enter(ifp->if_ioctl_lock);
	error = (*ifp->if_ioctl)(ifp, cmd, data);
	if (error != ENOTTY)
		;
	else if (so->so_proto == NULL)
		error = EOPNOTSUPP;
	else {
#ifdef COMPAT_OSOCK
		error = compat_ifioctl(so, ocmd, cmd, data, l);
#else
		error = (*so->so_proto->pr_usrreqs->pr_ioctl)(so,
		    cmd, data, ifp);
#endif
	}

	if (((oif_flags ^ ifp->if_flags) & IFF_UP) != 0) {
		if ((ifp->if_flags & IFF_UP) != 0) {
			int s = splnet();
			if_up(ifp);
			splx(s);
		}
	}
#ifdef COMPAT_OIFREQ
	if (cmd != ocmd)
		ifreqn2o(oifr, ifr);
#endif

	ifnet_lock_exit(ifp->if_ioctl_lock);
	return error;
}

/* This callback adds to the sum in `arg' the number of
 * threads on `ci' who have entered or who wait to enter the
 * critical section.
 */
static void
ifnet_lock_sum(void *p, void *arg, struct cpu_info *ci)
{
	uint64_t *sum = arg, *nenter = p;

	*sum += *nenter;
}

/* Return the number of threads who have entered or who wait
 * to enter the critical section on all CPUs.
 */
static uint64_t
ifnet_lock_entrances(struct ifnet_lock *il)
{
	uint64_t sum = 0;

	percpu_foreach(il->il_nenter, ifnet_lock_sum, &sum);

	return sum;
}

static int
ifioctl_attach(struct ifnet *ifp)
{
	struct ifnet_lock *il;

	/* If the driver has not supplied its own if_ioctl, then
	 * supply the default.
	 */
	if (ifp->if_ioctl == NULL)
		ifp->if_ioctl = ifioctl_common;

	/* Create an ifnet_lock for synchronizing ifioctls. */
	if ((il = kmem_zalloc(sizeof(*il), KM_SLEEP)) == NULL)
		return ENOMEM;

	il->il_nenter = percpu_alloc(sizeof(uint64_t));
	if (il->il_nenter == NULL) {
		kmem_free(il, sizeof(*il));
		return ENOMEM;
	}

	mutex_init(&il->il_lock, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&il->il_emptied, ifp->if_xname);

	ifp->if_ioctl_lock = il;

	return 0;
}

/*
 * This must not be called until after `ifp' has been withdrawn from the
 * ifnet tables so that ifioctl() cannot get a handle on it by calling
 * ifunit().
 */
static void
ifioctl_detach(struct ifnet *ifp)
{
	struct ifnet_lock *il;

	il = ifp->if_ioctl_lock;
	mutex_enter(&il->il_lock);
	/* Install if_nullioctl to make sure that any thread that
	 * subsequently enters the critical section will quit it
	 * immediately and signal the condition variable that we
	 * wait on, below.
	 */
	ifp->if_ioctl = if_nullioctl;
	/* Sleep while threads are still in the critical section or
	 * wait to enter it.
	 */
	while (ifnet_lock_entrances(il) != il->il_nexit)
		cv_wait(&il->il_emptied, &il->il_lock);
	/* At this point, we are the only thread still in the critical
	 * section, and no new thread can get a handle on the ifioctl
	 * lock, so it is safe to free its memory.
	 */
	mutex_exit(&il->il_lock);
	ifp->if_ioctl_lock = NULL;
	percpu_free(il->il_nenter, sizeof(uint64_t));
	il->il_nenter = NULL;
	cv_destroy(&il->il_emptied);
	mutex_destroy(&il->il_lock);
	kmem_free(il, sizeof(*il));
}

/*
 * Return interface configuration
 * of system.  List may be used
 * in later ioctl's (above) to get
 * other information.
 *
 * Each record is a struct ifreq.  Before the addition of
 * sockaddr_storage, the API rule was that sockaddr flavors that did
 * not fit would extend beyond the struct ifreq, with the next struct
 * ifreq starting sa_len beyond the struct sockaddr.  Because the
 * union in struct ifreq includes struct sockaddr_storage, every kind
 * of sockaddr must fit.  Thus, there are no longer any overlength
 * records.
 *
 * Records are added to the user buffer if they fit, and ifc_len is
 * adjusted to the length that was written.  Thus, the user is only
 * assured of getting the complete list if ifc_len on return is at
 * least sizeof(struct ifreq) less than it was on entry.
 *
 * If the user buffer pointer is NULL, this routine copies no data and
 * returns the amount of space that would be needed.
 *
 * Invariants:
 * ifrp points to the next part of the user's buffer to be used.  If
 * ifrp != NULL, space holds the number of bytes remaining that we may
 * write at ifrp.  Otherwise, space holds the number of bytes that
 * would have been written had there been adequate space.
 */
/*ARGSUSED*/
static int
ifconf(u_long cmd, void *data)
{
	struct ifconf *ifc = (struct ifconf *)data;
	struct ifnet *ifp;
	struct ifaddr *ifa;
	struct ifreq ifr, *ifrp = NULL;
	int space = 0, error = 0;
	const int sz = (int)sizeof(struct ifreq);
	const bool docopy = ifc->ifc_req != NULL;

	if (docopy) {
		space = ifc->ifc_len;
		ifrp = ifc->ifc_req;
	}

	IFNET_FOREACH(ifp) {
		(void)strncpy(ifr.ifr_name, ifp->if_xname,
		    sizeof(ifr.ifr_name));
		if (ifr.ifr_name[sizeof(ifr.ifr_name) - 1] != '\0')
			return ENAMETOOLONG;
		if (IFADDR_EMPTY(ifp)) {
			/* Interface with no addresses - send zero sockaddr. */
			memset(&ifr.ifr_addr, 0, sizeof(ifr.ifr_addr));
			if (!docopy) {
				space += sz;
				continue;
			}
			if (space >= sz) {
				error = copyout(&ifr, ifrp, sz);
				if (error != 0)
					return error;
				ifrp++;
				space -= sz;
			}
		}

		IFADDR_FOREACH(ifa, ifp) {
			struct sockaddr *sa = ifa->ifa_addr;
			/* all sockaddrs must fit in sockaddr_storage */
			KASSERT(sa->sa_len <= sizeof(ifr.ifr_ifru));

			if (!docopy) {
				space += sz;
				continue;
			}
			memcpy(&ifr.ifr_space, sa, sa->sa_len);
			if (space >= sz) {
				error = copyout(&ifr, ifrp, sz);
				if (error != 0)
					return (error);
				ifrp++; space -= sz;
			}
		}
	}
	if (docopy) {
		KASSERT(0 <= space && space <= ifc->ifc_len);
		ifc->ifc_len -= space;
	} else {
		KASSERT(space >= 0);
		ifc->ifc_len = space;
	}
	return (0);
}

int
ifreq_setaddr(u_long cmd, struct ifreq *ifr, const struct sockaddr *sa)
{
	uint8_t len;
#ifdef COMPAT_OIFREQ
	struct ifreq ifrb;
	struct oifreq *oifr = NULL;
	u_long ocmd = cmd;
	cmd = compat_cvtcmd(cmd);
	if (cmd != ocmd) {
		oifr = (struct oifreq *)(void *)ifr;
		ifr = &ifrb;
		ifreqo2n(oifr, ifr);
		len = sizeof(oifr->ifr_addr);
	} else
#endif
		len = sizeof(ifr->ifr_ifru.ifru_space);

	if (len < sa->sa_len)
		return EFBIG;

	memset(&ifr->ifr_addr, 0, len);
	sockaddr_copy(&ifr->ifr_addr, len, sa);

#ifdef COMPAT_OIFREQ
	if (cmd != ocmd)
		ifreqn2o(oifr, ifr);
#endif
	return 0;
}

/*
 * Queue message on interface, and start output if interface
 * not yet active.
 */
int
ifq_enqueue(struct ifnet *ifp, struct mbuf *m
    ALTQ_COMMA ALTQ_DECL(struct altq_pktattr *pktattr))
{
	int len = m->m_pkthdr.len;
	int mflags = m->m_flags;
	int s = splnet();
	int error;

	IFQ_ENQUEUE(&ifp->if_snd, m, pktattr, error);
	if (error != 0)
		goto out;
	ifp->if_obytes += len;
	if (mflags & M_MCAST)
		ifp->if_omcasts++;
	if ((ifp->if_flags & IFF_OACTIVE) == 0)
		(*ifp->if_start)(ifp);
out:
	splx(s);
	return error;
}

/*
 * Queue message on interface, possibly using a second fast queue
 */
int
ifq_enqueue2(struct ifnet *ifp, struct ifqueue *ifq, struct mbuf *m
    ALTQ_COMMA ALTQ_DECL(struct altq_pktattr *pktattr))
{
	int error = 0;

	if (ifq != NULL
#ifdef ALTQ
	    && ALTQ_IS_ENABLED(&ifp->if_snd) == 0
#endif
	    ) {
		if (IF_QFULL(ifq)) {
			IF_DROP(&ifp->if_snd);
			m_freem(m);
			if (error == 0)
				error = ENOBUFS;
		} else
			IF_ENQUEUE(ifq, m);
	} else
		IFQ_ENQUEUE(&ifp->if_snd, m, pktattr, error);
	if (error != 0) {
		++ifp->if_oerrors;
		return error;
	}
	return 0;
}

int
if_addr_init(ifnet_t *ifp, struct ifaddr *ifa, const bool src)
{
	int rc;

	if (ifp->if_initaddr != NULL)
		rc = (*ifp->if_initaddr)(ifp, ifa, src);
	else if (src ||
	         (rc = (*ifp->if_ioctl)(ifp, SIOCSIFDSTADDR, ifa)) == ENOTTY)
		rc = (*ifp->if_ioctl)(ifp, SIOCINITIFADDR, ifa);

	return rc;
}

int
if_do_dad(struct ifnet *ifp)
{
	if ((ifp->if_flags & IFF_LOOPBACK) != 0)
		return 0;

	switch (ifp->if_type) {
	case IFT_FAITH:
		/*
		 * These interfaces do not have the IFF_LOOPBACK flag,
		 * but loop packets back.  We do not have to do DAD on such
		 * interfaces.  We should even omit it, because loop-backed
		 * responses would confuse the DAD procedure.
		 */
		return 0;
	default:
		/*
		 * Our DAD routine requires the interface up and running.
		 * However, some interfaces can be up before the RUNNING
		 * status.  Additionaly, users may try to assign addresses
		 * before the interface becomes up (or running).
		 * We simply skip DAD in such a case as a work around.
		 * XXX: we should rather mark "tentative" on such addresses,
		 * and do DAD after the interface becomes ready.
		 */
		if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) !=
		    (IFF_UP|IFF_RUNNING))
			return 0;

		return 1;
	}
}

int
if_flags_set(ifnet_t *ifp, const short flags)
{
	int rc;

	if (ifp->if_setflags != NULL)
		rc = (*ifp->if_setflags)(ifp, flags);
	else {
		short cantflags, chgdflags;
		struct ifreq ifr;

		chgdflags = ifp->if_flags ^ flags;
		cantflags = chgdflags & IFF_CANTCHANGE;

		if (cantflags != 0)
			ifp->if_flags ^= cantflags;

                /* Traditionally, we do not call if_ioctl after
                 * setting/clearing only IFF_PROMISC if the interface
                 * isn't IFF_UP.  Uphold that tradition.
		 */
		if (chgdflags == IFF_PROMISC && (ifp->if_flags & IFF_UP) == 0)
			return 0;

		memset(&ifr, 0, sizeof(ifr));

		ifr.ifr_flags = flags & ~IFF_CANTCHANGE;
		rc = (*ifp->if_ioctl)(ifp, SIOCSIFFLAGS, &ifr);

		if (rc != 0 && cantflags != 0)
			ifp->if_flags ^= cantflags;
	}

	return rc;
}

int
if_mcast_op(ifnet_t *ifp, const unsigned long cmd, const struct sockaddr *sa)
{
	int rc;
	struct ifreq ifr;

	if (ifp->if_mcastop != NULL)
		rc = (*ifp->if_mcastop)(ifp, cmd, sa);
	else {
		ifreq_setaddr(cmd, &ifr, sa);
		rc = (*ifp->if_ioctl)(ifp, cmd, &ifr);
	}

	return rc;
}

static void
sysctl_sndq_setup(struct sysctllog **clog, const char *ifname,
    struct ifaltq *ifq)
{
	const struct sysctlnode *cnode, *rnode;

	if (sysctl_createv(clog, 0, NULL, &rnode,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "interfaces",
		       SYSCTL_DESCR("Per-interface controls"),
		       NULL, 0, NULL, 0,
		       CTL_NET, CTL_CREATE, CTL_EOL) != 0)
		goto bad;

	if (sysctl_createv(clog, 0, &rnode, &rnode,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, ifname,
		       SYSCTL_DESCR("Interface controls"),
		       NULL, 0, NULL, 0,
		       CTL_CREATE, CTL_EOL) != 0)
		goto bad;

	if (sysctl_createv(clog, 0, &rnode, &rnode,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "sndq",
		       SYSCTL_DESCR("Interface output queue controls"),
		       NULL, 0, NULL, 0,
		       CTL_CREATE, CTL_EOL) != 0)
		goto bad;

	if (sysctl_createv(clog, 0, &rnode, &cnode,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_INT, "len",
		       SYSCTL_DESCR("Current output queue length"),
		       NULL, 0, &ifq->ifq_len, 0,
		       CTL_CREATE, CTL_EOL) != 0)
		goto bad;

	if (sysctl_createv(clog, 0, &rnode, &cnode,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "maxlen",
		       SYSCTL_DESCR("Maximum allowed output queue length"),
		       NULL, 0, &ifq->ifq_maxlen, 0,
		       CTL_CREATE, CTL_EOL) != 0)
		goto bad;

	if (sysctl_createv(clog, 0, &rnode, &cnode,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_INT, "drops",
		       SYSCTL_DESCR("Packets dropped due to full output queue"),
		       NULL, 0, &ifq->ifq_drops, 0,
		       CTL_CREATE, CTL_EOL) != 0)
		goto bad;

	return;
bad:
	printf("%s: could not attach sysctl nodes\n", ifname);
	return;
}

#if defined(INET) || defined(INET6)

#define	SYSCTL_NET_PKTQ(q, cn, c)					\
	static int							\
	sysctl_net_##q##_##cn(SYSCTLFN_ARGS)				\
	{								\
		return sysctl_pktq_count(SYSCTLFN_CALL(rnode), q, c);	\
	}

#if defined(INET)
static int
sysctl_net_ip_pktq_maxlen(SYSCTLFN_ARGS)
{
	return sysctl_pktq_maxlen(SYSCTLFN_CALL(rnode), ip_pktq);
}
SYSCTL_NET_PKTQ(ip_pktq, items, PKTQ_NITEMS)
SYSCTL_NET_PKTQ(ip_pktq, drops, PKTQ_DROPS)
#endif

#if defined(INET6)
static int
sysctl_net_ip6_pktq_maxlen(SYSCTLFN_ARGS)
{
	return sysctl_pktq_maxlen(SYSCTLFN_CALL(rnode), ip6_pktq);
}
SYSCTL_NET_PKTQ(ip6_pktq, items, PKTQ_NITEMS)
SYSCTL_NET_PKTQ(ip6_pktq, drops, PKTQ_DROPS)
#endif

static void
sysctl_net_pktq_setup(struct sysctllog **clog, int pf)
{
	sysctlfn len_func = NULL, maxlen_func = NULL, drops_func = NULL;
	const char *pfname = NULL, *ipname = NULL;
	int ipn = 0, qid = 0;

	switch (pf) {
#if defined(INET)
	case PF_INET:
		len_func = sysctl_net_ip_pktq_items;
		maxlen_func = sysctl_net_ip_pktq_maxlen;
		drops_func = sysctl_net_ip_pktq_drops;
		pfname = "inet", ipn = IPPROTO_IP;
		ipname = "ip", qid = IPCTL_IFQ;
		break;
#endif
#if defined(INET6)
	case PF_INET6:
		len_func = sysctl_net_ip6_pktq_items;
		maxlen_func = sysctl_net_ip6_pktq_maxlen;
		drops_func = sysctl_net_ip6_pktq_drops;
		pfname = "inet6", ipn = IPPROTO_IPV6;
		ipname = "ip6", qid = IPV6CTL_IFQ;
		break;
#endif
	default:
		KASSERT(false);
	}

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, pfname, NULL,
		       NULL, 0, NULL, 0,
		       CTL_NET, pf, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, ipname, NULL,
		       NULL, 0, NULL, 0,
		       CTL_NET, pf, ipn, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "ifq",
		       SYSCTL_DESCR("Protocol input queue controls"),
		       NULL, 0, NULL, 0,
		       CTL_NET, pf, ipn, qid, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_INT, "len",
		       SYSCTL_DESCR("Current input queue length"),
		       len_func, 0, NULL, 0,
		       CTL_NET, pf, ipn, qid, IFQCTL_LEN, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "maxlen",
		       SYSCTL_DESCR("Maximum allowed input queue length"),
		       maxlen_func, 0, NULL, 0,
		       CTL_NET, pf, ipn, qid, IFQCTL_MAXLEN, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_INT, "drops",
		       SYSCTL_DESCR("Packets dropped due to full input queue"),
		       drops_func, 0, NULL, 0,
		       CTL_NET, pf, ipn, qid, IFQCTL_DROPS, CTL_EOL);
}
#endif /* INET || INET6 */

static int
if_sdl_sysctl(SYSCTLFN_ARGS)
{
	struct ifnet *ifp;
	const struct sockaddr_dl *sdl;

	if (namelen != 1)
		return EINVAL;

	ifp = if_byindex(name[0]);
	if (ifp == NULL)
		return ENODEV;

	sdl = ifp->if_sadl;
	if (sdl == NULL) {
		*oldlenp = 0;
		return 0;
	}

	if (oldp == NULL) {
		*oldlenp = sdl->sdl_alen;
		return 0;
	}

	if (*oldlenp >= sdl->sdl_alen)
		*oldlenp = sdl->sdl_alen;
	return sysctl_copyout(l, &sdl->sdl_data[sdl->sdl_nlen], oldp, *oldlenp);
}

SYSCTL_SETUP(sysctl_net_sdl_setup, "sysctl net.sdl subtree setup")
{
	const struct sysctlnode *rnode = NULL;

	sysctl_createv(clog, 0, NULL, &rnode,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "sdl",
		       SYSCTL_DESCR("Get active link-layer address"),
		       if_sdl_sysctl, 0, NULL, 0,
		       CTL_NET, CTL_CREATE, CTL_EOL);
}
