/*	$NetBSD: if_bridge.c,v 1.103 2015/10/07 08:48:04 ozaki-r Exp $	*/

/*
 * Copyright 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1999, 2000 Jason L. Wright (jason@thought.net)
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * OpenBSD: if_bridge.c,v 1.60 2001/06/15 03:38:33 itojun Exp
 */

/*
 * Network interface bridge support.
 *
 * TODO:
 *
 *	- Currently only supports Ethernet-like interfaces (Ethernet,
 *	  802.11, VLANs on Ethernet, etc.)  Figure out a nice way
 *	  to bridge other types of interfaces (FDDI-FDDI, and maybe
 *	  consider heterogenous bridges).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_bridge.c,v 1.103 2015/10/07 08:48:04 ozaki-r Exp $");

#ifdef _KERNEL_OPT
#include "opt_bridge_ipf.h"
#include "opt_inet.h"
#endif /* _KERNEL_OPT */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h> /* for softnet_lock */
#include <sys/sockio.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/pool.h>
#include <sys/kauth.h>
#include <sys/cpu.h>
#include <sys/cprng.h>
#include <sys/mutex.h>
#include <sys/kmem.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_llc.h>
#include <net/pktqueue.h>

#include <net/if_ether.h>
#include <net/if_bridgevar.h>

#if defined(BRIDGE_IPF)
/* Used for bridge_ip[6]_checkbasic */
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_private.h>		/* XXX */

#include <netinet/ip6.h>
#include <netinet6/in6_var.h>
#include <netinet6/ip6_var.h>
#include <netinet6/ip6_private.h>	/* XXX */
#endif /* BRIDGE_IPF */

/*
 * Size of the route hash table.  Must be a power of two.
 */
#ifndef BRIDGE_RTHASH_SIZE
#define	BRIDGE_RTHASH_SIZE		1024
#endif

#define	BRIDGE_RTHASH_MASK		(BRIDGE_RTHASH_SIZE - 1)

#include "carp.h"
#if NCARP > 0
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip_carp.h>
#endif

#include "ioconf.h"

__CTASSERT(sizeof(struct ifbifconf) == sizeof(struct ifbaconf));
__CTASSERT(offsetof(struct ifbifconf, ifbic_len) == offsetof(struct ifbaconf, ifbac_len));
__CTASSERT(offsetof(struct ifbifconf, ifbic_buf) == offsetof(struct ifbaconf, ifbac_buf));

/*
 * Maximum number of addresses to cache.
 */
#ifndef BRIDGE_RTABLE_MAX
#define	BRIDGE_RTABLE_MAX		100
#endif

/*
 * Spanning tree defaults.
 */
#define	BSTP_DEFAULT_MAX_AGE		(20 * 256)
#define	BSTP_DEFAULT_HELLO_TIME		(2 * 256)
#define	BSTP_DEFAULT_FORWARD_DELAY	(15 * 256)
#define	BSTP_DEFAULT_HOLD_TIME		(1 * 256)
#define	BSTP_DEFAULT_BRIDGE_PRIORITY	0x8000
#define	BSTP_DEFAULT_PORT_PRIORITY	0x80
#define	BSTP_DEFAULT_PATH_COST		55

/*
 * Timeout (in seconds) for entries learned dynamically.
 */
#ifndef BRIDGE_RTABLE_TIMEOUT
#define	BRIDGE_RTABLE_TIMEOUT		(20 * 60)	/* same as ARP */
#endif

/*
 * Number of seconds between walks of the route list.
 */
#ifndef BRIDGE_RTABLE_PRUNE_PERIOD
#define	BRIDGE_RTABLE_PRUNE_PERIOD	(5 * 60)
#endif

#define BRIDGE_RT_INTR_LOCK(_sc)	mutex_enter((_sc)->sc_rtlist_intr_lock)
#define BRIDGE_RT_INTR_UNLOCK(_sc)	mutex_exit((_sc)->sc_rtlist_intr_lock)
#define BRIDGE_RT_INTR_LOCKED(_sc)	mutex_owned((_sc)->sc_rtlist_intr_lock)

#define BRIDGE_RT_LOCK(_sc)	if ((_sc)->sc_rtlist_lock) \
					mutex_enter((_sc)->sc_rtlist_lock)
#define BRIDGE_RT_UNLOCK(_sc)	if ((_sc)->sc_rtlist_lock) \
					mutex_exit((_sc)->sc_rtlist_lock)
#define BRIDGE_RT_LOCKED(_sc)	(!(_sc)->sc_rtlist_lock || \
				 mutex_owned((_sc)->sc_rtlist_lock))

#define BRIDGE_RT_PSZ_PERFORM(_sc) \
				if ((_sc)->sc_rtlist_psz != NULL) \
					pserialize_perform((_sc)->sc_rtlist_psz);

#ifdef BRIDGE_MPSAFE
#define BRIDGE_RT_RENTER(__s)	do { \
					if (!cpu_intr_p()) \
						__s = pserialize_read_enter(); \
					else \
						__s = splhigh(); \
				} while (0)
#define BRIDGE_RT_REXIT(__s)	do { \
					if (!cpu_intr_p()) \
						pserialize_read_exit(__s); \
					else \
						splx(__s); \
				} while (0)
#else /* BRIDGE_MPSAFE */
#define BRIDGE_RT_RENTER(__s)	do { __s = 0; } while (0)
#define BRIDGE_RT_REXIT(__s)	do { (void)__s; } while (0)
#endif /* BRIDGE_MPSAFE */

int	bridge_rtable_prune_period = BRIDGE_RTABLE_PRUNE_PERIOD;

static struct pool bridge_rtnode_pool;
static struct work bridge_rtage_wk;

static int	bridge_clone_create(struct if_clone *, int);
static int	bridge_clone_destroy(struct ifnet *);

static int	bridge_ioctl(struct ifnet *, u_long, void *);
static int	bridge_init(struct ifnet *);
static void	bridge_stop(struct ifnet *, int);
static void	bridge_start(struct ifnet *);

static void	bridge_input(struct ifnet *, struct mbuf *);
static void	bridge_forward(void *);

static void	bridge_timer(void *);

static void	bridge_broadcast(struct bridge_softc *, struct ifnet *,
				 struct mbuf *);

static int	bridge_rtupdate(struct bridge_softc *, const uint8_t *,
				struct ifnet *, int, uint8_t);
static struct ifnet *bridge_rtlookup(struct bridge_softc *, const uint8_t *);
static void	bridge_rttrim(struct bridge_softc *);
static void	bridge_rtage(struct bridge_softc *);
static void	bridge_rtage_work(struct work *, void *);
static void	bridge_rtflush(struct bridge_softc *, int);
static int	bridge_rtdaddr(struct bridge_softc *, const uint8_t *);
static void	bridge_rtdelete(struct bridge_softc *, struct ifnet *ifp);

static void	bridge_rtable_init(struct bridge_softc *);
static void	bridge_rtable_fini(struct bridge_softc *);

static struct bridge_rtnode *bridge_rtnode_lookup(struct bridge_softc *,
						  const uint8_t *);
static int	bridge_rtnode_insert(struct bridge_softc *,
				     struct bridge_rtnode *);
static void	bridge_rtnode_remove(struct bridge_softc *,
				     struct bridge_rtnode *);
static void	bridge_rtnode_destroy(struct bridge_rtnode *);

static struct bridge_iflist *bridge_lookup_member(struct bridge_softc *,
						  const char *name);
static struct bridge_iflist *bridge_lookup_member_if(struct bridge_softc *,
						     struct ifnet *ifp);
static void	bridge_release_member(struct bridge_softc *, struct bridge_iflist *);
static void	bridge_delete_member(struct bridge_softc *,
				     struct bridge_iflist *);
static struct bridge_iflist *bridge_try_hold_bif(struct bridge_iflist *);

static int	bridge_ioctl_add(struct bridge_softc *, void *);
static int	bridge_ioctl_del(struct bridge_softc *, void *);
static int	bridge_ioctl_gifflags(struct bridge_softc *, void *);
static int	bridge_ioctl_sifflags(struct bridge_softc *, void *);
static int	bridge_ioctl_scache(struct bridge_softc *, void *);
static int	bridge_ioctl_gcache(struct bridge_softc *, void *);
static int	bridge_ioctl_gifs(struct bridge_softc *, void *);
static int	bridge_ioctl_rts(struct bridge_softc *, void *);
static int	bridge_ioctl_saddr(struct bridge_softc *, void *);
static int	bridge_ioctl_sto(struct bridge_softc *, void *);
static int	bridge_ioctl_gto(struct bridge_softc *, void *);
static int	bridge_ioctl_daddr(struct bridge_softc *, void *);
static int	bridge_ioctl_flush(struct bridge_softc *, void *);
static int	bridge_ioctl_gpri(struct bridge_softc *, void *);
static int	bridge_ioctl_spri(struct bridge_softc *, void *);
static int	bridge_ioctl_ght(struct bridge_softc *, void *);
static int	bridge_ioctl_sht(struct bridge_softc *, void *);
static int	bridge_ioctl_gfd(struct bridge_softc *, void *);
static int	bridge_ioctl_sfd(struct bridge_softc *, void *);
static int	bridge_ioctl_gma(struct bridge_softc *, void *);
static int	bridge_ioctl_sma(struct bridge_softc *, void *);
static int	bridge_ioctl_sifprio(struct bridge_softc *, void *);
static int	bridge_ioctl_sifcost(struct bridge_softc *, void *);
#if defined(BRIDGE_IPF)
static int	bridge_ioctl_gfilt(struct bridge_softc *, void *);
static int	bridge_ioctl_sfilt(struct bridge_softc *, void *);
static int	bridge_ipf(void *, struct mbuf **, struct ifnet *, int);
static int	bridge_ip_checkbasic(struct mbuf **mp);
# ifdef INET6
static int	bridge_ip6_checkbasic(struct mbuf **mp);
# endif /* INET6 */
#endif /* BRIDGE_IPF */

static void bridge_sysctl_fwdq_setup(struct sysctllog **clog,
    struct bridge_softc *sc);

struct bridge_control {
	int	(*bc_func)(struct bridge_softc *, void *);
	int	bc_argsize;
	int	bc_flags;
};

#define	BC_F_COPYIN		0x01	/* copy arguments in */
#define	BC_F_COPYOUT		0x02	/* copy arguments out */
#define	BC_F_SUSER		0x04	/* do super-user check */
#define BC_F_XLATEIN		0x08	/* xlate arguments in */
#define BC_F_XLATEOUT		0x10	/* xlate arguments out */

static const struct bridge_control bridge_control_table[] = {
[BRDGADD] = {bridge_ioctl_add, sizeof(struct ifbreq), BC_F_COPYIN|BC_F_SUSER},
[BRDGDEL] = {bridge_ioctl_del, sizeof(struct ifbreq), BC_F_COPYIN|BC_F_SUSER}, 

[BRDGGIFFLGS] = {bridge_ioctl_gifflags, sizeof(struct ifbreq), BC_F_COPYIN|BC_F_COPYOUT}, 
[BRDGSIFFLGS] = {bridge_ioctl_sifflags, sizeof(struct ifbreq), BC_F_COPYIN|BC_F_SUSER}, 

[BRDGSCACHE] = {bridge_ioctl_scache, sizeof(struct ifbrparam), BC_F_COPYIN|BC_F_SUSER}, 
[BRDGGCACHE] = {bridge_ioctl_gcache, sizeof(struct ifbrparam), BC_F_COPYOUT}, 

[OBRDGGIFS] = {bridge_ioctl_gifs, sizeof(struct ifbifconf), BC_F_COPYIN|BC_F_COPYOUT}, 
[OBRDGRTS] = {bridge_ioctl_rts, sizeof(struct ifbaconf), BC_F_COPYIN|BC_F_COPYOUT}, 

[BRDGSADDR] = {bridge_ioctl_saddr, sizeof(struct ifbareq), BC_F_COPYIN|BC_F_SUSER}, 

[BRDGSTO] = {bridge_ioctl_sto, sizeof(struct ifbrparam), BC_F_COPYIN|BC_F_SUSER}, 
[BRDGGTO] = {bridge_ioctl_gto, sizeof(struct ifbrparam), BC_F_COPYOUT}, 

[BRDGDADDR] = {bridge_ioctl_daddr, sizeof(struct ifbareq), BC_F_COPYIN|BC_F_SUSER}, 

[BRDGFLUSH] = {bridge_ioctl_flush, sizeof(struct ifbreq), BC_F_COPYIN|BC_F_SUSER}, 

[BRDGGPRI] = {bridge_ioctl_gpri, sizeof(struct ifbrparam), BC_F_COPYOUT}, 
[BRDGSPRI] = {bridge_ioctl_spri, sizeof(struct ifbrparam), BC_F_COPYIN|BC_F_SUSER}, 

[BRDGGHT] = {bridge_ioctl_ght, sizeof(struct ifbrparam), BC_F_COPYOUT}, 
[BRDGSHT] = {bridge_ioctl_sht, sizeof(struct ifbrparam), BC_F_COPYIN|BC_F_SUSER}, 

[BRDGGFD] = {bridge_ioctl_gfd, sizeof(struct ifbrparam), BC_F_COPYOUT}, 
[BRDGSFD] = {bridge_ioctl_sfd, sizeof(struct ifbrparam), BC_F_COPYIN|BC_F_SUSER}, 

[BRDGGMA] = {bridge_ioctl_gma, sizeof(struct ifbrparam), BC_F_COPYOUT}, 
[BRDGSMA] = {bridge_ioctl_sma, sizeof(struct ifbrparam), BC_F_COPYIN|BC_F_SUSER}, 

[BRDGSIFPRIO] = {bridge_ioctl_sifprio, sizeof(struct ifbreq), BC_F_COPYIN|BC_F_SUSER}, 

[BRDGSIFCOST] = {bridge_ioctl_sifcost, sizeof(struct ifbreq), BC_F_COPYIN|BC_F_SUSER}, 
#if defined(BRIDGE_IPF)
[BRDGGFILT] = {bridge_ioctl_gfilt, sizeof(struct ifbrparam), BC_F_COPYOUT},
[BRDGSFILT] = {bridge_ioctl_sfilt, sizeof(struct ifbrparam), BC_F_COPYIN|BC_F_SUSER},
#endif /* BRIDGE_IPF */
[BRDGGIFS] = {bridge_ioctl_gifs, sizeof(struct ifbifconf), BC_F_XLATEIN|BC_F_XLATEOUT},
[BRDGRTS] = {bridge_ioctl_rts, sizeof(struct ifbaconf), BC_F_XLATEIN|BC_F_XLATEOUT},
};

static const int bridge_control_table_size = __arraycount(bridge_control_table);

static LIST_HEAD(, bridge_softc) bridge_list;
static kmutex_t bridge_list_lock;

static struct if_clone bridge_cloner =
    IF_CLONE_INITIALIZER("bridge", bridge_clone_create, bridge_clone_destroy);

/*
 * bridgeattach:
 *
 *	Pseudo-device attach routine.
 */
void
bridgeattach(int n)
{

	pool_init(&bridge_rtnode_pool, sizeof(struct bridge_rtnode),
	    0, 0, 0, "brtpl", NULL, IPL_NET);

	LIST_INIT(&bridge_list);
	mutex_init(&bridge_list_lock, MUTEX_DEFAULT, IPL_NET);
	if_clone_attach(&bridge_cloner);
}

/*
 * bridge_clone_create:
 *
 *	Create a new bridge instance.
 */
static int
bridge_clone_create(struct if_clone *ifc, int unit)
{
	struct bridge_softc *sc;
	struct ifnet *ifp;
	int error, flags;

	sc = kmem_zalloc(sizeof(*sc),  KM_SLEEP);
	ifp = &sc->sc_if;

	sc->sc_brtmax = BRIDGE_RTABLE_MAX;
	sc->sc_brttimeout = BRIDGE_RTABLE_TIMEOUT;
	sc->sc_bridge_max_age = BSTP_DEFAULT_MAX_AGE;
	sc->sc_bridge_hello_time = BSTP_DEFAULT_HELLO_TIME;
	sc->sc_bridge_forward_delay = BSTP_DEFAULT_FORWARD_DELAY;
	sc->sc_bridge_priority = BSTP_DEFAULT_BRIDGE_PRIORITY;
	sc->sc_hold_time = BSTP_DEFAULT_HOLD_TIME;
	sc->sc_filter_flags = 0;

	/* Initialize our routing table. */
	bridge_rtable_init(sc);

#ifdef BRIDGE_MPSAFE
	flags = WQ_MPSAFE;
#else
	flags = 0;
#endif
	error = workqueue_create(&sc->sc_rtage_wq, "bridge_rtage",
	    bridge_rtage_work, sc, PRI_SOFTNET, IPL_SOFTNET, flags);
	if (error)
		panic("%s: workqueue_create %d\n", __func__, error);

	callout_init(&sc->sc_brcallout, 0);
	callout_init(&sc->sc_bstpcallout, 0);

	LIST_INIT(&sc->sc_iflist);
#ifdef BRIDGE_MPSAFE
	sc->sc_iflist_intr_lock = mutex_obj_alloc(MUTEX_DEFAULT, IPL_NET);
	sc->sc_iflist_psz = pserialize_create();
	sc->sc_iflist_lock = mutex_obj_alloc(MUTEX_DEFAULT, IPL_SOFTNET);
#else
	sc->sc_iflist_intr_lock = NULL;
	sc->sc_iflist_psz = NULL;
	sc->sc_iflist_lock = NULL;
#endif
	cv_init(&sc->sc_iflist_cv, "if_bridge_cv");

	if_initname(ifp, ifc->ifc_name, unit);
	ifp->if_softc = sc;
	ifp->if_mtu = ETHERMTU;
	ifp->if_ioctl = bridge_ioctl;
	ifp->if_output = bridge_output;
	ifp->if_start = bridge_start;
	ifp->if_stop = bridge_stop;
	ifp->if_init = bridge_init;
	ifp->if_type = IFT_BRIDGE;
	ifp->if_addrlen = 0;
	ifp->if_dlt = DLT_EN10MB;
	ifp->if_hdrlen = ETHER_HDR_LEN;

	sc->sc_fwd_pktq = pktq_create(IFQ_MAXLEN, bridge_forward, sc);
	KASSERT(sc->sc_fwd_pktq != NULL);

	bridge_sysctl_fwdq_setup(&ifp->if_sysctl_log, sc);

	if_attach(ifp);

	if_alloc_sadl(ifp);

	mutex_enter(&bridge_list_lock);
	LIST_INSERT_HEAD(&bridge_list, sc, sc_list);
	mutex_exit(&bridge_list_lock);

	return (0);
}

/*
 * bridge_clone_destroy:
 *
 *	Destroy a bridge instance.
 */
static int
bridge_clone_destroy(struct ifnet *ifp)
{
	struct bridge_softc *sc = ifp->if_softc;
	struct bridge_iflist *bif;
	int s;

	/* Must be called during IFF_RUNNING, i.e., before bridge_stop */
	pktq_barrier(sc->sc_fwd_pktq);

	s = splnet();

	bridge_stop(ifp, 1);

	BRIDGE_LOCK(sc);
	while ((bif = LIST_FIRST(&sc->sc_iflist)) != NULL)
		bridge_delete_member(sc, bif);
	BRIDGE_UNLOCK(sc);

	mutex_enter(&bridge_list_lock);
	LIST_REMOVE(sc, sc_list);
	mutex_exit(&bridge_list_lock);

	splx(s);

	if_detach(ifp);

	/* Should be called after if_detach for safe */
	pktq_flush(sc->sc_fwd_pktq);
	pktq_destroy(sc->sc_fwd_pktq);

	/* Tear down the routing table. */
	bridge_rtable_fini(sc);

	cv_destroy(&sc->sc_iflist_cv);
	if (sc->sc_iflist_intr_lock)
		mutex_obj_free(sc->sc_iflist_intr_lock);

	if (sc->sc_iflist_psz)
		pserialize_destroy(sc->sc_iflist_psz);
	if (sc->sc_iflist_lock)
		mutex_obj_free(sc->sc_iflist_lock);

	workqueue_destroy(sc->sc_rtage_wq);

	kmem_free(sc, sizeof(*sc));

	return (0);
}

static int
bridge_sysctl_fwdq_maxlen(SYSCTLFN_ARGS)
{
	struct sysctlnode node = *rnode;
	const struct bridge_softc *sc =	node.sysctl_data;
	return sysctl_pktq_maxlen(SYSCTLFN_CALL(rnode), sc->sc_fwd_pktq);
}

#define	SYSCTL_BRIDGE_PKTQ(cn, c)					\
	static int							\
	bridge_sysctl_fwdq_##cn(SYSCTLFN_ARGS)				\
	{								\
		struct sysctlnode node = *rnode;			\
		const struct bridge_softc *sc =	node.sysctl_data;	\
		return sysctl_pktq_count(SYSCTLFN_CALL(rnode),		\
					 sc->sc_fwd_pktq, c);		\
	}

SYSCTL_BRIDGE_PKTQ(items, PKTQ_NITEMS)
SYSCTL_BRIDGE_PKTQ(drops, PKTQ_DROPS)

static void
bridge_sysctl_fwdq_setup(struct sysctllog **clog, struct bridge_softc *sc)
{
	const struct sysctlnode *cnode, *rnode;
	sysctlfn len_func = NULL, maxlen_func = NULL, drops_func = NULL;
	const char *ifname = sc->sc_if.if_xname;

	len_func = bridge_sysctl_fwdq_items;
	maxlen_func = bridge_sysctl_fwdq_maxlen;
	drops_func = bridge_sysctl_fwdq_drops;

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
			   CTLTYPE_NODE, "fwdq",
			   SYSCTL_DESCR("Protocol input queue controls"),
			   NULL, 0, NULL, 0,
			   CTL_CREATE, CTL_EOL) != 0)
		goto bad;

	if (sysctl_createv(clog, 0, &rnode, &cnode,
			   CTLFLAG_PERMANENT,
			   CTLTYPE_INT, "len",
			   SYSCTL_DESCR("Current forwarding queue length"),
			   len_func, 0, (void *)sc, 0,
			   CTL_CREATE, IFQCTL_LEN, CTL_EOL) != 0)
		goto bad;

	if (sysctl_createv(clog, 0, &rnode, &cnode,
			   CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
			   CTLTYPE_INT, "maxlen",
			   SYSCTL_DESCR("Maximum allowed forwarding queue length"),
			   maxlen_func, 0, (void *)sc, 0,
			   CTL_CREATE, IFQCTL_MAXLEN, CTL_EOL) != 0)
		goto bad;

	if (sysctl_createv(clog, 0, &rnode, &cnode,
			   CTLFLAG_PERMANENT,
			   CTLTYPE_INT, "drops",
			   SYSCTL_DESCR("Packets dropped due to full forwarding queue"),
			   drops_func, 0, (void *)sc, 0,
			   CTL_CREATE, IFQCTL_DROPS, CTL_EOL) != 0)
		goto bad;

	return;
bad:
	aprint_error("%s: could not attach sysctl nodes\n", ifname);
	return;
}

/*
 * bridge_ioctl:
 *
 *	Handle a control request from the operator.
 */
static int
bridge_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct bridge_softc *sc = ifp->if_softc;
	struct lwp *l = curlwp;	/* XXX */
	union {
		struct ifbreq ifbreq;
		struct ifbifconf ifbifconf;
		struct ifbareq ifbareq;
		struct ifbaconf ifbaconf;
		struct ifbrparam ifbrparam;
	} args;
	struct ifdrv *ifd = (struct ifdrv *) data;
	const struct bridge_control *bc = NULL; /* XXXGCC */
	int s, error = 0;

	/* Authorize command before calling splnet(). */
	switch (cmd) {
	case SIOCGDRVSPEC:
	case SIOCSDRVSPEC:
		if (ifd->ifd_cmd >= bridge_control_table_size
		    || (bc = &bridge_control_table[ifd->ifd_cmd]) == NULL) {
			error = EINVAL;
			return error;
		}

		/* We only care about BC_F_SUSER at this point. */
		if ((bc->bc_flags & BC_F_SUSER) == 0)
			break;

		error = kauth_authorize_network(l->l_cred,
		    KAUTH_NETWORK_INTERFACE_BRIDGE,
		    cmd == SIOCGDRVSPEC ?
		     KAUTH_REQ_NETWORK_INTERFACE_BRIDGE_GETPRIV :
		     KAUTH_REQ_NETWORK_INTERFACE_BRIDGE_SETPRIV,
		     ifd, NULL, NULL);
		if (error)
			return (error);

		break;
	}

	s = splnet();

	switch (cmd) {
	case SIOCGDRVSPEC:
	case SIOCSDRVSPEC:
		KASSERT(bc != NULL);
		if (cmd == SIOCGDRVSPEC &&
		    (bc->bc_flags & (BC_F_COPYOUT|BC_F_XLATEOUT)) == 0) {
			error = EINVAL;
			break;
		}
		else if (cmd == SIOCSDRVSPEC &&
		    (bc->bc_flags & (BC_F_COPYOUT|BC_F_XLATEOUT)) != 0) {
			error = EINVAL;
			break;
		}

		/* BC_F_SUSER is checked above, before splnet(). */

		if ((bc->bc_flags & (BC_F_XLATEIN|BC_F_XLATEOUT)) == 0
		    && (ifd->ifd_len != bc->bc_argsize
			|| ifd->ifd_len > sizeof(args))) {
			error = EINVAL;
			break;
		}

		memset(&args, 0, sizeof(args));
		if (bc->bc_flags & BC_F_COPYIN) {
			error = copyin(ifd->ifd_data, &args, ifd->ifd_len);
			if (error)
				break;
		} else if (bc->bc_flags & BC_F_XLATEIN) {
			args.ifbifconf.ifbic_len = ifd->ifd_len;
			args.ifbifconf.ifbic_buf = ifd->ifd_data;
		}

		error = (*bc->bc_func)(sc, &args);
		if (error)
			break;

		if (bc->bc_flags & BC_F_COPYOUT) {
			error = copyout(&args, ifd->ifd_data, ifd->ifd_len);
		} else if (bc->bc_flags & BC_F_XLATEOUT) {
			ifd->ifd_len = args.ifbifconf.ifbic_len;
			ifd->ifd_data = args.ifbifconf.ifbic_buf;
		}
		break;

	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		switch (ifp->if_flags & (IFF_UP|IFF_RUNNING)) {
		case IFF_RUNNING:
			/*
			 * If interface is marked down and it is running,
			 * then stop and disable it.
			 */
			(*ifp->if_stop)(ifp, 1);
			break;
		case IFF_UP:
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			error = (*ifp->if_init)(ifp);
			break;
		default:
			break;
		}
		break;

	case SIOCSIFMTU:
		if ((error = ifioctl_common(ifp, cmd, data)) == ENETRESET)
			error = 0;
		break;

	default:
		error = ifioctl_common(ifp, cmd, data);
		break;
	}

	splx(s);

	return (error);
}

/*
 * bridge_lookup_member:
 *
 *	Lookup a bridge member interface.
 */
static struct bridge_iflist *
bridge_lookup_member(struct bridge_softc *sc, const char *name)
{
	struct bridge_iflist *bif;
	struct ifnet *ifp;
	int s;

	BRIDGE_PSZ_RENTER(s);

	LIST_FOREACH(bif, &sc->sc_iflist, bif_next) {
		ifp = bif->bif_ifp;
		if (strcmp(ifp->if_xname, name) == 0)
			break;
	}
	bif = bridge_try_hold_bif(bif);

	BRIDGE_PSZ_REXIT(s);

	return bif;
}

/*
 * bridge_lookup_member_if:
 *
 *	Lookup a bridge member interface by ifnet*.
 */
static struct bridge_iflist *
bridge_lookup_member_if(struct bridge_softc *sc, struct ifnet *member_ifp)
{
	struct bridge_iflist *bif;
	int s;

	BRIDGE_PSZ_RENTER(s);

	bif = member_ifp->if_bridgeif;
	bif = bridge_try_hold_bif(bif);

	BRIDGE_PSZ_REXIT(s);

	return bif;
}

static struct bridge_iflist *
bridge_try_hold_bif(struct bridge_iflist *bif)
{
#ifdef BRIDGE_MPSAFE
	if (bif != NULL) {
		if (bif->bif_waiting)
			bif = NULL;
		else
			atomic_inc_32(&bif->bif_refs);
	}
#endif
	return bif;
}

/*
 * bridge_release_member:
 *
 *	Release the specified member interface.
 */
static void
bridge_release_member(struct bridge_softc *sc, struct bridge_iflist *bif)
{
#ifdef BRIDGE_MPSAFE
	uint32_t refs;

	refs = atomic_dec_uint_nv(&bif->bif_refs);
	if (__predict_false(refs == 0 && bif->bif_waiting)) {
		BRIDGE_INTR_LOCK(sc);
		cv_broadcast(&sc->sc_iflist_cv);
		BRIDGE_INTR_UNLOCK(sc);
	}
#else
	(void)sc;
	(void)bif;
#endif
}

/*
 * bridge_delete_member:
 *
 *	Delete the specified member interface.
 */
static void
bridge_delete_member(struct bridge_softc *sc, struct bridge_iflist *bif)
{
	struct ifnet *ifs = bif->bif_ifp;

	KASSERT(BRIDGE_LOCKED(sc));

	ifs->if_input = ether_input;
	ifs->if_bridge = NULL;
	ifs->if_bridgeif = NULL;

	LIST_REMOVE(bif, bif_next);

	BRIDGE_PSZ_PERFORM(sc);

	BRIDGE_UNLOCK(sc);

#ifdef BRIDGE_MPSAFE
	BRIDGE_INTR_LOCK(sc);
	bif->bif_waiting = true;
	membar_sync();
	while (bif->bif_refs > 0) {
		aprint_debug("%s: cv_wait on iflist\n", __func__);
		cv_wait(&sc->sc_iflist_cv, sc->sc_iflist_intr_lock);
	}
	bif->bif_waiting = false;
	BRIDGE_INTR_UNLOCK(sc);
#endif

	kmem_free(bif, sizeof(*bif));

	BRIDGE_LOCK(sc);
}

static int
bridge_ioctl_add(struct bridge_softc *sc, void *arg)
{
	struct ifbreq *req = arg;
	struct bridge_iflist *bif = NULL;
	struct ifnet *ifs;
	int error = 0;

	ifs = ifunit(req->ifbr_ifsname);
	if (ifs == NULL)
		return (ENOENT);

	if (sc->sc_if.if_mtu != ifs->if_mtu)
		return (EINVAL);

	if (ifs->if_bridge == sc)
		return (EEXIST);

	if (ifs->if_bridge != NULL)
		return (EBUSY);

	if (ifs->if_input != ether_input)
		return EINVAL;

	/* FIXME: doesn't work with non-IFF_SIMPLEX interfaces */
	if ((ifs->if_flags & IFF_SIMPLEX) == 0)
		return EINVAL;

	bif = kmem_alloc(sizeof(*bif), KM_SLEEP);

	switch (ifs->if_type) {
	case IFT_ETHER:
		/*
		 * Place the interface into promiscuous mode.
		 */
		error = ifpromisc(ifs, 1);
		if (error)
			goto out;
		break;
	default:
		error = EINVAL;
		goto out;
	}

	bif->bif_ifp = ifs;
	bif->bif_flags = IFBIF_LEARNING | IFBIF_DISCOVER;
	bif->bif_priority = BSTP_DEFAULT_PORT_PRIORITY;
	bif->bif_path_cost = BSTP_DEFAULT_PATH_COST;
	bif->bif_refs = 0;
	bif->bif_waiting = false;

	BRIDGE_LOCK(sc);

	ifs->if_bridge = sc;
	ifs->if_bridgeif = bif;
	LIST_INSERT_HEAD(&sc->sc_iflist, bif, bif_next);
	ifs->if_input = bridge_input;

	BRIDGE_UNLOCK(sc);

	if (sc->sc_if.if_flags & IFF_RUNNING)
		bstp_initialization(sc);
	else
		bstp_stop(sc);

 out:
	if (error) {
		if (bif != NULL)
			kmem_free(bif, sizeof(*bif));
	}
	return (error);
}

static int
bridge_ioctl_del(struct bridge_softc *sc, void *arg)
{
	struct ifbreq *req = arg;
	const char *name = req->ifbr_ifsname;
	struct bridge_iflist *bif;
	struct ifnet *ifs;

	BRIDGE_LOCK(sc);

	/*
	 * Don't use bridge_lookup_member. We want to get a member
	 * with bif_refs == 0.
	 */
	LIST_FOREACH(bif, &sc->sc_iflist, bif_next) {
		ifs = bif->bif_ifp;
		if (strcmp(ifs->if_xname, name) == 0)
			break;
	}

	if (bif == NULL) {
		BRIDGE_UNLOCK(sc);
		return ENOENT;
	}

	bridge_delete_member(sc, bif);

	BRIDGE_UNLOCK(sc);

	switch (ifs->if_type) {
	case IFT_ETHER:
		/*
		 * Take the interface out of promiscuous mode.
		 * Don't call it with holding a spin lock.
		 */
		(void) ifpromisc(ifs, 0);
		break;
	default:
#ifdef DIAGNOSTIC
		panic("bridge_delete_member: impossible");
#endif
		break;
	}

	bridge_rtdelete(sc, ifs);

	if (sc->sc_if.if_flags & IFF_RUNNING)
		bstp_initialization(sc);

	return 0;
}

static int
bridge_ioctl_gifflags(struct bridge_softc *sc, void *arg)
{
	struct ifbreq *req = arg;
	struct bridge_iflist *bif;

	bif = bridge_lookup_member(sc, req->ifbr_ifsname);
	if (bif == NULL)
		return (ENOENT);

	req->ifbr_ifsflags = bif->bif_flags;
	req->ifbr_state = bif->bif_state;
	req->ifbr_priority = bif->bif_priority;
	req->ifbr_path_cost = bif->bif_path_cost;
	req->ifbr_portno = bif->bif_ifp->if_index & 0xff;

	bridge_release_member(sc, bif);

	return (0);
}

static int
bridge_ioctl_sifflags(struct bridge_softc *sc, void *arg)
{
	struct ifbreq *req = arg;
	struct bridge_iflist *bif;

	bif = bridge_lookup_member(sc, req->ifbr_ifsname);
	if (bif == NULL)
		return (ENOENT);

	if (req->ifbr_ifsflags & IFBIF_STP) {
		switch (bif->bif_ifp->if_type) {
		case IFT_ETHER:
			/* These can do spanning tree. */
			break;

		default:
			/* Nothing else can. */
			bridge_release_member(sc, bif);
			return (EINVAL);
		}
	}

	bif->bif_flags = req->ifbr_ifsflags;

	bridge_release_member(sc, bif);

	if (sc->sc_if.if_flags & IFF_RUNNING)
		bstp_initialization(sc);

	return (0);
}

static int
bridge_ioctl_scache(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	sc->sc_brtmax = param->ifbrp_csize;
	bridge_rttrim(sc);

	return (0);
}

static int
bridge_ioctl_gcache(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	param->ifbrp_csize = sc->sc_brtmax;

	return (0);
}

static int
bridge_ioctl_gifs(struct bridge_softc *sc, void *arg)
{
	struct ifbifconf *bifc = arg;
	struct bridge_iflist *bif;
	struct ifbreq *breqs;
	int i, count, error = 0;

retry:
	BRIDGE_LOCK(sc);
	count = 0;
	LIST_FOREACH(bif, &sc->sc_iflist, bif_next)
		count++;
	BRIDGE_UNLOCK(sc);

	if (count == 0) {
		bifc->ifbic_len = 0;
		return 0;
	}

	if (bifc->ifbic_len == 0 || bifc->ifbic_len < (sizeof(*breqs) * count)) {
		/* Tell that a larger buffer is needed */
		bifc->ifbic_len = sizeof(*breqs) * count;
		return 0;
	}

	breqs = kmem_alloc(sizeof(*breqs) * count, KM_SLEEP);

	BRIDGE_LOCK(sc);

	i = 0;
	LIST_FOREACH(bif, &sc->sc_iflist, bif_next)
		i++;
	if (i > count) {
		/*
		 * The number of members has been increased.
		 * We need more memory!
		 */
		BRIDGE_UNLOCK(sc);
		kmem_free(breqs, sizeof(*breqs) * count);
		goto retry;
	}

	i = 0;
	LIST_FOREACH(bif, &sc->sc_iflist, bif_next) {
		struct ifbreq *breq = &breqs[i++];
		memset(breq, 0, sizeof(*breq));

		strlcpy(breq->ifbr_ifsname, bif->bif_ifp->if_xname,
		    sizeof(breq->ifbr_ifsname));
		breq->ifbr_ifsflags = bif->bif_flags;
		breq->ifbr_state = bif->bif_state;
		breq->ifbr_priority = bif->bif_priority;
		breq->ifbr_path_cost = bif->bif_path_cost;
		breq->ifbr_portno = bif->bif_ifp->if_index & 0xff;
	}

	/* Don't call copyout with holding the mutex */
	BRIDGE_UNLOCK(sc);

	for (i = 0; i < count; i++) {
		error = copyout(&breqs[i], bifc->ifbic_req + i, sizeof(*breqs));
		if (error)
			break;
	}
	bifc->ifbic_len = sizeof(*breqs) * i;

	kmem_free(breqs, sizeof(*breqs) * count);

	return error;
}

static int
bridge_ioctl_rts(struct bridge_softc *sc, void *arg)
{
	struct ifbaconf *bac = arg;
	struct bridge_rtnode *brt;
	struct ifbareq bareq;
	int count = 0, error = 0, len;

	if (bac->ifbac_len == 0)
		return (0);

	BRIDGE_RT_INTR_LOCK(sc);

	len = bac->ifbac_len;
	LIST_FOREACH(brt, &sc->sc_rtlist, brt_list) {
		if (len < sizeof(bareq))
			goto out;
		memset(&bareq, 0, sizeof(bareq));
		strlcpy(bareq.ifba_ifsname, brt->brt_ifp->if_xname,
		    sizeof(bareq.ifba_ifsname));
		memcpy(bareq.ifba_dst, brt->brt_addr, sizeof(brt->brt_addr));
		if ((brt->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC) {
			bareq.ifba_expire = brt->brt_expire - time_uptime;
		} else
			bareq.ifba_expire = 0;
		bareq.ifba_flags = brt->brt_flags;

		error = copyout(&bareq, bac->ifbac_req + count, sizeof(bareq));
		if (error)
			goto out;
		count++;
		len -= sizeof(bareq);
	}
 out:
	BRIDGE_RT_INTR_UNLOCK(sc);

	bac->ifbac_len = sizeof(bareq) * count;
	return (error);
}

static int
bridge_ioctl_saddr(struct bridge_softc *sc, void *arg)
{
	struct ifbareq *req = arg;
	struct bridge_iflist *bif;
	int error;

	bif = bridge_lookup_member(sc, req->ifba_ifsname);
	if (bif == NULL)
		return (ENOENT);

	error = bridge_rtupdate(sc, req->ifba_dst, bif->bif_ifp, 1,
	    req->ifba_flags);

	bridge_release_member(sc, bif);

	return (error);
}

static int
bridge_ioctl_sto(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	sc->sc_brttimeout = param->ifbrp_ctime;

	return (0);
}

static int
bridge_ioctl_gto(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	param->ifbrp_ctime = sc->sc_brttimeout;

	return (0);
}

static int
bridge_ioctl_daddr(struct bridge_softc *sc, void *arg)
{
	struct ifbareq *req = arg;

	return (bridge_rtdaddr(sc, req->ifba_dst));
}

static int
bridge_ioctl_flush(struct bridge_softc *sc, void *arg)
{
	struct ifbreq *req = arg;

	bridge_rtflush(sc, req->ifbr_ifsflags);

	return (0);
}

static int
bridge_ioctl_gpri(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	param->ifbrp_prio = sc->sc_bridge_priority;

	return (0);
}

static int
bridge_ioctl_spri(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	sc->sc_bridge_priority = param->ifbrp_prio;

	if (sc->sc_if.if_flags & IFF_RUNNING)
		bstp_initialization(sc);

	return (0);
}

static int
bridge_ioctl_ght(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	param->ifbrp_hellotime = sc->sc_bridge_hello_time >> 8;

	return (0);
}

static int
bridge_ioctl_sht(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	if (param->ifbrp_hellotime == 0)
		return (EINVAL);
	sc->sc_bridge_hello_time = param->ifbrp_hellotime << 8;

	if (sc->sc_if.if_flags & IFF_RUNNING)
		bstp_initialization(sc);

	return (0);
}

static int
bridge_ioctl_gfd(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	param->ifbrp_fwddelay = sc->sc_bridge_forward_delay >> 8;

	return (0);
}

static int
bridge_ioctl_sfd(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	if (param->ifbrp_fwddelay == 0)
		return (EINVAL);
	sc->sc_bridge_forward_delay = param->ifbrp_fwddelay << 8;

	if (sc->sc_if.if_flags & IFF_RUNNING)
		bstp_initialization(sc);

	return (0);
}

static int
bridge_ioctl_gma(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	param->ifbrp_maxage = sc->sc_bridge_max_age >> 8;

	return (0);
}

static int
bridge_ioctl_sma(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	if (param->ifbrp_maxage == 0)
		return (EINVAL);
	sc->sc_bridge_max_age = param->ifbrp_maxage << 8;

	if (sc->sc_if.if_flags & IFF_RUNNING)
		bstp_initialization(sc);

	return (0);
}

static int
bridge_ioctl_sifprio(struct bridge_softc *sc, void *arg)
{
	struct ifbreq *req = arg;
	struct bridge_iflist *bif;

	bif = bridge_lookup_member(sc, req->ifbr_ifsname);
	if (bif == NULL)
		return (ENOENT);

	bif->bif_priority = req->ifbr_priority;

	if (sc->sc_if.if_flags & IFF_RUNNING)
		bstp_initialization(sc);

	bridge_release_member(sc, bif);

	return (0);
}

#if defined(BRIDGE_IPF)
static int
bridge_ioctl_gfilt(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;

	param->ifbrp_filter = sc->sc_filter_flags;

	return (0);
}

static int
bridge_ioctl_sfilt(struct bridge_softc *sc, void *arg)
{
	struct ifbrparam *param = arg;
	uint32_t nflags, oflags;

	if (param->ifbrp_filter & ~IFBF_FILT_MASK)
		return (EINVAL);

	nflags = param->ifbrp_filter;
	oflags = sc->sc_filter_flags;

	if ((nflags & IFBF_FILT_USEIPF) && !(oflags & IFBF_FILT_USEIPF)) {
		pfil_add_hook((void *)bridge_ipf, NULL, PFIL_IN|PFIL_OUT,
			sc->sc_if.if_pfil);
	}
	if (!(nflags & IFBF_FILT_USEIPF) && (oflags & IFBF_FILT_USEIPF)) {
		pfil_remove_hook((void *)bridge_ipf, NULL, PFIL_IN|PFIL_OUT,
			sc->sc_if.if_pfil);
	}

	sc->sc_filter_flags = nflags;

	return (0);
}
#endif /* BRIDGE_IPF */

static int
bridge_ioctl_sifcost(struct bridge_softc *sc, void *arg)
{
	struct ifbreq *req = arg;
	struct bridge_iflist *bif;

	bif = bridge_lookup_member(sc, req->ifbr_ifsname);
	if (bif == NULL)
		return (ENOENT);

	bif->bif_path_cost = req->ifbr_path_cost;

	if (sc->sc_if.if_flags & IFF_RUNNING)
		bstp_initialization(sc);

	bridge_release_member(sc, bif);

	return (0);
}

/*
 * bridge_ifdetach:
 *
 *	Detach an interface from a bridge.  Called when a member
 *	interface is detaching.
 */
void
bridge_ifdetach(struct ifnet *ifp)
{
	struct bridge_softc *sc = ifp->if_bridge;
	struct ifbreq breq;

	/* ioctl_lock should prevent this from happening */
	KASSERT(sc != NULL);

	memset(&breq, 0, sizeof(breq));
	strlcpy(breq.ifbr_ifsname, ifp->if_xname, sizeof(breq.ifbr_ifsname));

	(void) bridge_ioctl_del(sc, &breq);
}

/*
 * bridge_init:
 *
 *	Initialize a bridge interface.
 */
static int
bridge_init(struct ifnet *ifp)
{
	struct bridge_softc *sc = ifp->if_softc;

	if (ifp->if_flags & IFF_RUNNING)
		return (0);

	callout_reset(&sc->sc_brcallout, bridge_rtable_prune_period * hz,
	    bridge_timer, sc);

	ifp->if_flags |= IFF_RUNNING;
	bstp_initialization(sc);
	return (0);
}

/*
 * bridge_stop:
 *
 *	Stop the bridge interface.
 */
static void
bridge_stop(struct ifnet *ifp, int disable)
{
	struct bridge_softc *sc = ifp->if_softc;

	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;

	callout_stop(&sc->sc_brcallout);
	bstp_stop(sc);

	bridge_rtflush(sc, IFBF_FLUSHDYN);

	ifp->if_flags &= ~IFF_RUNNING;
}

/*
 * bridge_enqueue:
 *
 *	Enqueue a packet on a bridge member interface.
 */
void
bridge_enqueue(struct bridge_softc *sc, struct ifnet *dst_ifp, struct mbuf *m,
    int runfilt)
{
	ALTQ_DECL(struct altq_pktattr pktattr;)
	int len, error;
	short mflags;

	/*
	 * Clear any in-bound checksum flags for this packet.
	 */
	m->m_pkthdr.csum_flags = 0;

	if (runfilt) {
		if (pfil_run_hooks(sc->sc_if.if_pfil, &m,
		    dst_ifp, PFIL_OUT) != 0) {
			if (m != NULL)
				m_freem(m);
			return;
		}
		if (m == NULL)
			return;
	}

#ifdef ALTQ
	/*
	 * If ALTQ is enabled on the member interface, do
	 * classification; the queueing discipline might
	 * not require classification, but might require
	 * the address family/header pointer in the pktattr.
	 */
	if (ALTQ_IS_ENABLED(&dst_ifp->if_snd)) {
		/* XXX IFT_ETHER */
		altq_etherclassify(&dst_ifp->if_snd, m, &pktattr);
	}
#endif /* ALTQ */

	len = m->m_pkthdr.len;
	mflags = m->m_flags;

	IFQ_ENQUEUE(&dst_ifp->if_snd, m, &pktattr, error);

	if (error) {
		/* mbuf is already freed */
		sc->sc_if.if_oerrors++;
		return;
	}

	sc->sc_if.if_opackets++;
	sc->sc_if.if_obytes += len;

	dst_ifp->if_obytes += len;

	if (mflags & M_MCAST) {
		sc->sc_if.if_omcasts++;
		dst_ifp->if_omcasts++;
	}

	if ((dst_ifp->if_flags & IFF_OACTIVE) == 0)
		(*dst_ifp->if_start)(dst_ifp);
}

/*
 * bridge_output:
 *
 *	Send output from a bridge member interface.  This
 *	performs the bridging function for locally originated
 *	packets.
 *
 *	The mbuf has the Ethernet header already attached.  We must
 *	enqueue or free the mbuf before returning.
 */
int
bridge_output(struct ifnet *ifp, struct mbuf *m, const struct sockaddr *sa,
    struct rtentry *rt)
{
	struct ether_header *eh;
	struct ifnet *dst_if;
	struct bridge_softc *sc;
#ifndef BRIDGE_MPSAFE
	int s;
#endif

	if (m->m_len < ETHER_HDR_LEN) {
		m = m_pullup(m, ETHER_HDR_LEN);
		if (m == NULL)
			return (0);
	}

	eh = mtod(m, struct ether_header *);
	sc = ifp->if_bridge;

#ifndef BRIDGE_MPSAFE
	s = splnet();
#endif

	/*
	 * If bridge is down, but the original output interface is up,
	 * go ahead and send out that interface.  Otherwise, the packet
	 * is dropped below.
	 */
	if (__predict_false(sc == NULL) ||
	    (sc->sc_if.if_flags & IFF_RUNNING) == 0) {
		dst_if = ifp;
		goto sendunicast;
	}

	/*
	 * If the packet is a multicast, or we don't know a better way to
	 * get there, send to all interfaces.
	 */
	if (ETHER_IS_MULTICAST(eh->ether_dhost))
		dst_if = NULL;
	else
		dst_if = bridge_rtlookup(sc, eh->ether_dhost);
	if (dst_if == NULL) {
		struct bridge_iflist *bif;
		struct mbuf *mc;
		int used = 0;
		int ss;

		BRIDGE_PSZ_RENTER(ss);
		LIST_FOREACH(bif, &sc->sc_iflist, bif_next) {
			bif = bridge_try_hold_bif(bif);
			if (bif == NULL)
				continue;
			BRIDGE_PSZ_REXIT(ss);

			dst_if = bif->bif_ifp;
			if ((dst_if->if_flags & IFF_RUNNING) == 0)
				goto next;

			/*
			 * If this is not the original output interface,
			 * and the interface is participating in spanning
			 * tree, make sure the port is in a state that
			 * allows forwarding.
			 */
			if (dst_if != ifp &&
			    (bif->bif_flags & IFBIF_STP) != 0) {
				switch (bif->bif_state) {
				case BSTP_IFSTATE_BLOCKING:
				case BSTP_IFSTATE_LISTENING:
				case BSTP_IFSTATE_DISABLED:
					goto next;
				}
			}

			if (LIST_NEXT(bif, bif_next) == NULL) {
				used = 1;
				mc = m;
			} else {
				mc = m_copym(m, 0, M_COPYALL, M_NOWAIT);
				if (mc == NULL) {
					sc->sc_if.if_oerrors++;
					goto next;
				}
			}

			bridge_enqueue(sc, dst_if, mc, 0);
next:
			bridge_release_member(sc, bif);
			BRIDGE_PSZ_RENTER(ss);
		}
		BRIDGE_PSZ_REXIT(ss);

		if (used == 0)
			m_freem(m);
#ifndef BRIDGE_MPSAFE
		splx(s);
#endif
		return (0);
	}

 sendunicast:
	/*
	 * XXX Spanning tree consideration here?
	 */

	if ((dst_if->if_flags & IFF_RUNNING) == 0) {
		m_freem(m);
#ifndef BRIDGE_MPSAFE
		splx(s);
#endif
		return (0);
	}

	bridge_enqueue(sc, dst_if, m, 0);

#ifndef BRIDGE_MPSAFE
	splx(s);
#endif
	return (0);
}

/*
 * bridge_start:
 *
 *	Start output on a bridge.
 *
 *	NOTE: This routine should never be called in this implementation.
 */
static void
bridge_start(struct ifnet *ifp)
{

	printf("%s: bridge_start() called\n", ifp->if_xname);
}

/*
 * bridge_forward:
 *
 *	The forwarding function of the bridge.
 */
static void
bridge_forward(void *v)
{
	struct bridge_softc *sc = v;
	struct mbuf *m;
	struct bridge_iflist *bif;
	struct ifnet *src_if, *dst_if;
	struct ether_header *eh;
#ifndef BRIDGE_MPSAFE
	int s;

	KERNEL_LOCK(1, NULL);
	mutex_enter(softnet_lock);
#endif

	if ((sc->sc_if.if_flags & IFF_RUNNING) == 0) {
#ifndef BRIDGE_MPSAFE
		mutex_exit(softnet_lock);
		KERNEL_UNLOCK_ONE(NULL);
#endif
		return;
	}

#ifndef BRIDGE_MPSAFE
	s = splnet();
#endif
	while ((m = pktq_dequeue(sc->sc_fwd_pktq)) != NULL) {
		src_if = m->m_pkthdr.rcvif;

		sc->sc_if.if_ipackets++;
		sc->sc_if.if_ibytes += m->m_pkthdr.len;

		/*
		 * Look up the bridge_iflist.
		 */
		bif = bridge_lookup_member_if(sc, src_if);
		if (bif == NULL) {
			/* Interface is not a bridge member (anymore?) */
			m_freem(m);
			continue;
		}

		if (bif->bif_flags & IFBIF_STP) {
			switch (bif->bif_state) {
			case BSTP_IFSTATE_BLOCKING:
			case BSTP_IFSTATE_LISTENING:
			case BSTP_IFSTATE_DISABLED:
				m_freem(m);
				bridge_release_member(sc, bif);
				continue;
			}
		}

		eh = mtod(m, struct ether_header *);

		/*
		 * If the interface is learning, and the source
		 * address is valid and not multicast, record
		 * the address.
		 */
		if ((bif->bif_flags & IFBIF_LEARNING) != 0 &&
		    ETHER_IS_MULTICAST(eh->ether_shost) == 0 &&
		    (eh->ether_shost[0] == 0 &&
		     eh->ether_shost[1] == 0 &&
		     eh->ether_shost[2] == 0 &&
		     eh->ether_shost[3] == 0 &&
		     eh->ether_shost[4] == 0 &&
		     eh->ether_shost[5] == 0) == 0) {
			(void) bridge_rtupdate(sc, eh->ether_shost,
			    src_if, 0, IFBAF_DYNAMIC);
		}

		if ((bif->bif_flags & IFBIF_STP) != 0 &&
		    bif->bif_state == BSTP_IFSTATE_LEARNING) {
			m_freem(m);
			bridge_release_member(sc, bif);
			continue;
		}

		bridge_release_member(sc, bif);

		/*
		 * At this point, the port either doesn't participate
		 * in spanning tree or it is in the forwarding state.
		 */

		/*
		 * If the packet is unicast, destined for someone on
		 * "this" side of the bridge, drop it.
		 */
		if ((m->m_flags & (M_BCAST|M_MCAST)) == 0) {
			dst_if = bridge_rtlookup(sc, eh->ether_dhost);
			if (src_if == dst_if) {
				m_freem(m);
				continue;
			}
		} else {
			/* ...forward it to all interfaces. */
			sc->sc_if.if_imcasts++;
			dst_if = NULL;
		}

		if (pfil_run_hooks(sc->sc_if.if_pfil, &m,
		    m->m_pkthdr.rcvif, PFIL_IN) != 0) {
			if (m != NULL)
				m_freem(m);
			continue;
		}
		if (m == NULL)
			continue;

		if (dst_if == NULL) {
			bridge_broadcast(sc, src_if, m);
			continue;
		}

		/*
		 * At this point, we're dealing with a unicast frame
		 * going to a different interface.
		 */
		if ((dst_if->if_flags & IFF_RUNNING) == 0) {
			m_freem(m);
			continue;
		}

		bif = bridge_lookup_member_if(sc, dst_if);
		if (bif == NULL) {
			/* Not a member of the bridge (anymore?) */
			m_freem(m);
			continue;
		}

		if (bif->bif_flags & IFBIF_STP) {
			switch (bif->bif_state) {
			case BSTP_IFSTATE_DISABLED:
			case BSTP_IFSTATE_BLOCKING:
				m_freem(m);
				bridge_release_member(sc, bif);
				continue;
			}
		}

		bridge_release_member(sc, bif);

		bridge_enqueue(sc, dst_if, m, 1);
	}
#ifndef BRIDGE_MPSAFE
	splx(s);
	mutex_exit(softnet_lock);
	KERNEL_UNLOCK_ONE(NULL);
#endif
}

static bool
bstp_state_before_learning(struct bridge_iflist *bif)
{
	if (bif->bif_flags & IFBIF_STP) {
		switch (bif->bif_state) {
		case BSTP_IFSTATE_BLOCKING:
		case BSTP_IFSTATE_LISTENING:
		case BSTP_IFSTATE_DISABLED:
			return true;
		}
	}
	return false;
}

static bool
bridge_ourether(struct bridge_iflist *bif, struct ether_header *eh, int src)
{
	uint8_t *ether = src ? eh->ether_shost : eh->ether_dhost;

	if (memcmp(CLLADDR(bif->bif_ifp->if_sadl), ether, ETHER_ADDR_LEN) == 0
#if NCARP > 0
	    || (bif->bif_ifp->if_carp &&
	        carp_ourether(bif->bif_ifp->if_carp, eh, IFT_ETHER, src) != NULL)
#endif /* NCARP > 0 */
	    )
		return true;

	return false;
}

/*
 * bridge_input:
 *
 *	Receive input from a member interface.  Queue the packet for
 *	bridging if it is not for us.
 */
static void
bridge_input(struct ifnet *ifp, struct mbuf *m)
{
	struct bridge_softc *sc = ifp->if_bridge;
	struct bridge_iflist *bif;
	struct ether_header *eh;

	if (__predict_false(sc == NULL) ||
	    (sc->sc_if.if_flags & IFF_RUNNING) == 0) {
		ether_input(ifp, m);
		return;
	}

	bif = bridge_lookup_member_if(sc, ifp);
	if (bif == NULL) {
		ether_input(ifp, m);
		return;
	}

	eh = mtod(m, struct ether_header *);

	if (ETHER_IS_MULTICAST(eh->ether_dhost)) {
		if (memcmp(etherbroadcastaddr,
		    eh->ether_dhost, ETHER_ADDR_LEN) == 0)
			m->m_flags |= M_BCAST;
		else
			m->m_flags |= M_MCAST;
	}

	/*
	 * A 'fast' path for packets addressed to interfaces that are
	 * part of this bridge.
	 */
	if (!(m->m_flags & (M_BCAST|M_MCAST)) &&
	    !bstp_state_before_learning(bif)) {
		struct bridge_iflist *_bif;
		struct ifnet *_ifp = NULL;
		int s;

		BRIDGE_PSZ_RENTER(s);
		LIST_FOREACH(_bif, &sc->sc_iflist, bif_next) {
			/* It is destined for us. */
			if (bridge_ourether(_bif, eh, 0)) {
				_bif = bridge_try_hold_bif(_bif);
				BRIDGE_PSZ_REXIT(s);
				if (_bif == NULL)
					goto out;
				if (_bif->bif_flags & IFBIF_LEARNING)
					(void) bridge_rtupdate(sc,
					    eh->ether_shost, ifp, 0, IFBAF_DYNAMIC);
				_ifp = m->m_pkthdr.rcvif = _bif->bif_ifp;
				bridge_release_member(sc, _bif);
				goto out;
			}

			/* We just received a packet that we sent out. */
			if (bridge_ourether(_bif, eh, 1))
				break;
		}
		BRIDGE_PSZ_REXIT(s);
out:

		if (_bif != NULL) {
			bridge_release_member(sc, bif);
			if (_ifp != NULL) {
				m->m_flags &= ~M_PROMISC;
				ether_input(_ifp, m);
			} else
				m_freem(m);
			return;
		}
	}

	/* Tap off 802.1D packets; they do not get forwarded. */
	if (bif->bif_flags & IFBIF_STP &&
	    memcmp(eh->ether_dhost, bstp_etheraddr, ETHER_ADDR_LEN) == 0) {
		bstp_input(sc, bif, m);
		bridge_release_member(sc, bif);
		return;
	}

	/*
	 * A normal switch would discard the packet here, but that's not what
	 * we've done historically. This also prevents some obnoxious behaviour.
	 */
	if (bstp_state_before_learning(bif)) {
		bridge_release_member(sc, bif);
		ether_input(ifp, m);
		return;
	}

	bridge_release_member(sc, bif);

	/* Queue the packet for bridge forwarding. */
	{
		/*
		 * Force to enqueue to curcpu's pktq (RX can run on a CPU
		 * other than CPU#0). XXX need fundamental solution.
		 */
		const unsigned hash = curcpu()->ci_index;

		if (__predict_false(!pktq_enqueue(sc->sc_fwd_pktq, m, hash)))
			m_freem(m);
	}
}

/*
 * bridge_broadcast:
 *
 *	Send a frame to all interfaces that are members of
 *	the bridge, except for the one on which the packet
 *	arrived.
 */
static void
bridge_broadcast(struct bridge_softc *sc, struct ifnet *src_if,
    struct mbuf *m)
{
	struct bridge_iflist *bif;
	struct mbuf *mc;
	struct ifnet *dst_if;
	bool bmcast;
	int s;

	bmcast = m->m_flags & (M_BCAST|M_MCAST);

	BRIDGE_PSZ_RENTER(s);
	LIST_FOREACH(bif, &sc->sc_iflist, bif_next) {
		bif = bridge_try_hold_bif(bif);
		if (bif == NULL)
			continue;
		BRIDGE_PSZ_REXIT(s);

		dst_if = bif->bif_ifp;

		if (bif->bif_flags & IFBIF_STP) {
			switch (bif->bif_state) {
			case BSTP_IFSTATE_BLOCKING:
			case BSTP_IFSTATE_DISABLED:
				goto next;
			}
		}

		if ((bif->bif_flags & IFBIF_DISCOVER) == 0 && !bmcast)
			goto next;

		if ((dst_if->if_flags & IFF_RUNNING) == 0)
			goto next;

		if (dst_if != src_if) {
			mc = m_copym(m, 0, M_COPYALL, M_DONTWAIT);
			if (mc == NULL) {
				sc->sc_if.if_oerrors++;
				goto next;
			}
			bridge_enqueue(sc, dst_if, mc, 1);
		}

		if (bmcast) {
			mc = m_copym(m, 0, M_COPYALL, M_DONTWAIT);
			if (mc == NULL) {
				sc->sc_if.if_oerrors++;
				goto next;
			}

			mc->m_pkthdr.rcvif = dst_if;
			mc->m_flags &= ~M_PROMISC;
			ether_input(dst_if, mc);
		}
next:
		bridge_release_member(sc, bif);
		BRIDGE_PSZ_RENTER(s);
	}
	BRIDGE_PSZ_REXIT(s);

	m_freem(m);
}

static int
bridge_rtalloc(struct bridge_softc *sc, const uint8_t *dst,
    struct bridge_rtnode **brtp)
{
	struct bridge_rtnode *brt;
	int error;

	if (sc->sc_brtcnt >= sc->sc_brtmax)
		return ENOSPC;

	/*
	 * Allocate a new bridge forwarding node, and
	 * initialize the expiration time and Ethernet
	 * address.
	 */
	brt = pool_get(&bridge_rtnode_pool, PR_NOWAIT);
	if (brt == NULL)
		return ENOMEM;

	memset(brt, 0, sizeof(*brt));
	brt->brt_expire = time_uptime + sc->sc_brttimeout;
	brt->brt_flags = IFBAF_DYNAMIC;
	memcpy(brt->brt_addr, dst, ETHER_ADDR_LEN);

	BRIDGE_RT_INTR_LOCK(sc);
	error = bridge_rtnode_insert(sc, brt);
	BRIDGE_RT_INTR_UNLOCK(sc);

	if (error != 0) {
		pool_put(&bridge_rtnode_pool, brt);
		return error;
	}

	*brtp = brt;
	return 0;
}

/*
 * bridge_rtupdate:
 *
 *	Add a bridge routing entry.
 */
static int
bridge_rtupdate(struct bridge_softc *sc, const uint8_t *dst,
    struct ifnet *dst_if, int setflags, uint8_t flags)
{
	struct bridge_rtnode *brt;
	int s;

again:
	/*
	 * A route for this destination might already exist.  If so,
	 * update it, otherwise create a new one.
	 */
	BRIDGE_RT_RENTER(s);
	brt = bridge_rtnode_lookup(sc, dst);

	if (brt != NULL) {
		brt->brt_ifp = dst_if;
		if (setflags) {
			brt->brt_flags = flags;
			if (flags & IFBAF_STATIC)
				brt->brt_expire = 0;
			else
				brt->brt_expire = time_uptime + sc->sc_brttimeout;
		} else {
			if ((brt->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC)
				brt->brt_expire = time_uptime + sc->sc_brttimeout;
		}
	}
	BRIDGE_RT_REXIT(s);

	if (brt == NULL) {
		int r;

		r = bridge_rtalloc(sc, dst, &brt);
		if (r != 0)
			return r;
		goto again;
	}

	return 0;
}

/*
 * bridge_rtlookup:
 *
 *	Lookup the destination interface for an address.
 */
static struct ifnet *
bridge_rtlookup(struct bridge_softc *sc, const uint8_t *addr)
{
	struct bridge_rtnode *brt;
	struct ifnet *ifs = NULL;
	int s;

	BRIDGE_RT_RENTER(s);
	brt = bridge_rtnode_lookup(sc, addr);
	if (brt != NULL)
		ifs = brt->brt_ifp;
	BRIDGE_RT_REXIT(s);

	return ifs;
}

typedef bool (*bridge_iterate_cb_t)
    (struct bridge_softc *, struct bridge_rtnode *, bool *, void *);

/*
 * bridge_rtlist_iterate_remove:
 *
 *	It iterates on sc->sc_rtlist and removes rtnodes of it which func
 *	callback judges to remove. Removals of rtnodes are done in a manner
 *	of pserialize. To this end, all kmem_* operations are placed out of
 *	mutexes.
 */
static void
bridge_rtlist_iterate_remove(struct bridge_softc *sc, bridge_iterate_cb_t func, void *arg)
{
	struct bridge_rtnode *brt, *nbrt;
	struct bridge_rtnode **brt_list;
	int i, count;

retry:
	count = sc->sc_brtcnt;
	if (count == 0)
		return;
	brt_list = kmem_alloc(sizeof(struct bridge_rtnode *) * count, KM_SLEEP);

	BRIDGE_RT_LOCK(sc);
	BRIDGE_RT_INTR_LOCK(sc);
	if (__predict_false(sc->sc_brtcnt > count)) {
		/* The rtnodes increased, we need more memory */
		BRIDGE_RT_INTR_UNLOCK(sc);
		BRIDGE_RT_UNLOCK(sc);
		kmem_free(brt_list, sizeof(*brt_list) * count);
		goto retry;
	}

	i = 0;
	LIST_FOREACH_SAFE(brt, &sc->sc_rtlist, brt_list, nbrt) {
		bool need_break = false;
		if (func(sc, brt, &need_break, arg)) {
			bridge_rtnode_remove(sc, brt);
			brt_list[i++] = brt;
		}
		if (need_break)
			break;
	}
	BRIDGE_RT_INTR_UNLOCK(sc);

	if (i > 0)
		BRIDGE_RT_PSZ_PERFORM(sc);
	BRIDGE_RT_UNLOCK(sc);

	while (--i >= 0)
		bridge_rtnode_destroy(brt_list[i]);

	kmem_free(brt_list, sizeof(*brt_list) * count);
}

static bool
bridge_rttrim0_cb(struct bridge_softc *sc, struct bridge_rtnode *brt,
    bool *need_break, void *arg)
{
	if ((brt->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC) {
		/* Take into account of the subsequent removal */
		if ((sc->sc_brtcnt - 1) <= sc->sc_brtmax)
			*need_break = true;
		return true;
	} else
		return false;
}

static void
bridge_rttrim0(struct bridge_softc *sc)
{
	bridge_rtlist_iterate_remove(sc, bridge_rttrim0_cb, NULL);
}

/*
 * bridge_rttrim:
 *
 *	Trim the routine table so that we have a number
 *	of routing entries less than or equal to the
 *	maximum number.
 */
static void
bridge_rttrim(struct bridge_softc *sc)
{

	/* Make sure we actually need to do this. */
	if (sc->sc_brtcnt <= sc->sc_brtmax)
		return;

	/* Force an aging cycle; this might trim enough addresses. */
	bridge_rtage(sc);
	if (sc->sc_brtcnt <= sc->sc_brtmax)
		return;

	bridge_rttrim0(sc);

	return;
}

/*
 * bridge_timer:
 *
 *	Aging timer for the bridge.
 */
static void
bridge_timer(void *arg)
{
	struct bridge_softc *sc = arg;

	workqueue_enqueue(sc->sc_rtage_wq, &bridge_rtage_wk, NULL);
}

static void
bridge_rtage_work(struct work *wk, void *arg)
{
	struct bridge_softc *sc = arg;

	KASSERT(wk == &bridge_rtage_wk);

	bridge_rtage(sc);

	if (sc->sc_if.if_flags & IFF_RUNNING)
		callout_reset(&sc->sc_brcallout,
		    bridge_rtable_prune_period * hz, bridge_timer, sc);
}

static bool
bridge_rtage_cb(struct bridge_softc *sc, struct bridge_rtnode *brt,
    bool *need_break, void *arg)
{
	if ((brt->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC &&
	    time_uptime >= brt->brt_expire)
		return true;
	else
		return false;
}

/*
 * bridge_rtage:
 *
 *	Perform an aging cycle.
 */
static void
bridge_rtage(struct bridge_softc *sc)
{
	bridge_rtlist_iterate_remove(sc, bridge_rtage_cb, NULL);
}


static bool
bridge_rtflush_cb(struct bridge_softc *sc, struct bridge_rtnode *brt,
    bool *need_break, void *arg)
{
	int full = *(int*)arg;

	if (full || (brt->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC)
		return true;
	else
		return false;
}

/*
 * bridge_rtflush:
 *
 *	Remove all dynamic addresses from the bridge.
 */
static void
bridge_rtflush(struct bridge_softc *sc, int full)
{
	bridge_rtlist_iterate_remove(sc, bridge_rtflush_cb, &full);
}

/*
 * bridge_rtdaddr:
 *
 *	Remove an address from the table.
 */
static int
bridge_rtdaddr(struct bridge_softc *sc, const uint8_t *addr)
{
	struct bridge_rtnode *brt;

	BRIDGE_RT_LOCK(sc);
	BRIDGE_RT_INTR_LOCK(sc);
	if ((brt = bridge_rtnode_lookup(sc, addr)) == NULL) {
		BRIDGE_RT_INTR_UNLOCK(sc);
		BRIDGE_RT_UNLOCK(sc);
		return ENOENT;
	}
	bridge_rtnode_remove(sc, brt);
	BRIDGE_RT_INTR_UNLOCK(sc);
	BRIDGE_RT_PSZ_PERFORM(sc);
	BRIDGE_RT_UNLOCK(sc);

	bridge_rtnode_destroy(brt);

	return 0;
}

/*
 * bridge_rtdelete:
 *
 *	Delete routes to a speicifc member interface.
 */
static void
bridge_rtdelete(struct bridge_softc *sc, struct ifnet *ifp)
{
	struct bridge_rtnode *brt, *nbrt;

	BRIDGE_RT_LOCK(sc);
	BRIDGE_RT_INTR_LOCK(sc);
	LIST_FOREACH_SAFE(brt, &sc->sc_rtlist, brt_list, nbrt) {
		if (brt->brt_ifp == ifp)
			break;
	}
	if (brt == NULL) {
		BRIDGE_RT_INTR_UNLOCK(sc);
		BRIDGE_RT_UNLOCK(sc);
		return;
	}
	bridge_rtnode_remove(sc, brt);
	BRIDGE_RT_INTR_UNLOCK(sc);
	BRIDGE_RT_PSZ_PERFORM(sc);
	BRIDGE_RT_UNLOCK(sc);

	bridge_rtnode_destroy(brt);
}

/*
 * bridge_rtable_init:
 *
 *	Initialize the route table for this bridge.
 */
static void
bridge_rtable_init(struct bridge_softc *sc)
{
	int i;

	sc->sc_rthash = kmem_alloc(sizeof(*sc->sc_rthash) * BRIDGE_RTHASH_SIZE,
	    KM_SLEEP);

	for (i = 0; i < BRIDGE_RTHASH_SIZE; i++)
		LIST_INIT(&sc->sc_rthash[i]);

	sc->sc_rthash_key = cprng_fast32();

	LIST_INIT(&sc->sc_rtlist);

	sc->sc_rtlist_intr_lock = mutex_obj_alloc(MUTEX_DEFAULT, IPL_NET);
#ifdef BRIDGE_MPSAFE
	sc->sc_rtlist_psz = pserialize_create();
	sc->sc_rtlist_lock = mutex_obj_alloc(MUTEX_DEFAULT, IPL_SOFTNET);
#else
	sc->sc_rtlist_psz = NULL;
	sc->sc_rtlist_lock = NULL;
#endif
}

/*
 * bridge_rtable_fini:
 *
 *	Deconstruct the route table for this bridge.
 */
static void
bridge_rtable_fini(struct bridge_softc *sc)
{

	kmem_free(sc->sc_rthash, sizeof(*sc->sc_rthash) * BRIDGE_RTHASH_SIZE);
	if (sc->sc_rtlist_intr_lock)
		mutex_obj_free(sc->sc_rtlist_intr_lock);
	if (sc->sc_rtlist_lock)
		mutex_obj_free(sc->sc_rtlist_lock);
	if (sc->sc_rtlist_psz)
		pserialize_destroy(sc->sc_rtlist_psz);
}

/*
 * The following hash function is adapted from "Hash Functions" by Bob Jenkins
 * ("Algorithm Alley", Dr. Dobbs Journal, September 1997).
 */
#define	mix(a, b, c)							\
do {									\
	a -= b; a -= c; a ^= (c >> 13);					\
	b -= c; b -= a; b ^= (a << 8);					\
	c -= a; c -= b; c ^= (b >> 13);					\
	a -= b; a -= c; a ^= (c >> 12);					\
	b -= c; b -= a; b ^= (a << 16);					\
	c -= a; c -= b; c ^= (b >> 5);					\
	a -= b; a -= c; a ^= (c >> 3);					\
	b -= c; b -= a; b ^= (a << 10);					\
	c -= a; c -= b; c ^= (b >> 15);					\
} while (/*CONSTCOND*/0)

static inline uint32_t
bridge_rthash(struct bridge_softc *sc, const uint8_t *addr)
{
	uint32_t a = 0x9e3779b9, b = 0x9e3779b9, c = sc->sc_rthash_key;

	b += addr[5] << 8;
	b += addr[4];
	a += addr[3] << 24;
	a += addr[2] << 16;
	a += addr[1] << 8;
	a += addr[0];

	mix(a, b, c);

	return (c & BRIDGE_RTHASH_MASK);
}

#undef mix

/*
 * bridge_rtnode_lookup:
 *
 *	Look up a bridge route node for the specified destination.
 */
static struct bridge_rtnode *
bridge_rtnode_lookup(struct bridge_softc *sc, const uint8_t *addr)
{
	struct bridge_rtnode *brt;
	uint32_t hash;
	int dir;

	hash = bridge_rthash(sc, addr);
	LIST_FOREACH(brt, &sc->sc_rthash[hash], brt_hash) {
		dir = memcmp(addr, brt->brt_addr, ETHER_ADDR_LEN);
		if (dir == 0)
			return (brt);
		if (dir > 0)
			return (NULL);
	}

	return (NULL);
}

/*
 * bridge_rtnode_insert:
 *
 *	Insert the specified bridge node into the route table.  We
 *	assume the entry is not already in the table.
 */
static int
bridge_rtnode_insert(struct bridge_softc *sc, struct bridge_rtnode *brt)
{
	struct bridge_rtnode *lbrt;
	uint32_t hash;
	int dir;

	KASSERT(BRIDGE_RT_INTR_LOCKED(sc));

	hash = bridge_rthash(sc, brt->brt_addr);

	lbrt = LIST_FIRST(&sc->sc_rthash[hash]);
	if (lbrt == NULL) {
		LIST_INSERT_HEAD(&sc->sc_rthash[hash], brt, brt_hash);
		goto out;
	}

	do {
		dir = memcmp(brt->brt_addr, lbrt->brt_addr, ETHER_ADDR_LEN);
		if (dir == 0)
			return (EEXIST);
		if (dir > 0) {
			LIST_INSERT_BEFORE(lbrt, brt, brt_hash);
			goto out;
		}
		if (LIST_NEXT(lbrt, brt_hash) == NULL) {
			LIST_INSERT_AFTER(lbrt, brt, brt_hash);
			goto out;
		}
		lbrt = LIST_NEXT(lbrt, brt_hash);
	} while (lbrt != NULL);

#ifdef DIAGNOSTIC
	panic("bridge_rtnode_insert: impossible");
#endif

 out:
	LIST_INSERT_HEAD(&sc->sc_rtlist, brt, brt_list);
	sc->sc_brtcnt++;

	return (0);
}

/*
 * bridge_rtnode_remove:
 *
 *	Remove a bridge rtnode from the rthash and the rtlist of a bridge.
 */
static void
bridge_rtnode_remove(struct bridge_softc *sc, struct bridge_rtnode *brt)
{

	KASSERT(BRIDGE_RT_INTR_LOCKED(sc));

	LIST_REMOVE(brt, brt_hash);
	LIST_REMOVE(brt, brt_list);
	sc->sc_brtcnt--;
}

/*
 * bridge_rtnode_destroy:
 *
 *	Destroy a bridge rtnode.
 */
static void
bridge_rtnode_destroy(struct bridge_rtnode *brt)
{

	pool_put(&bridge_rtnode_pool, brt);
}

#if defined(BRIDGE_IPF)
extern pfil_head_t *inet_pfil_hook;                 /* XXX */
extern pfil_head_t *inet6_pfil_hook;                /* XXX */

/*
 * Send bridge packets through IPF if they are one of the types IPF can deal
 * with, or if they are ARP or REVARP.  (IPF will pass ARP and REVARP without
 * question.)
 */
static int
bridge_ipf(void *arg, struct mbuf **mp, struct ifnet *ifp, int dir)
{
	int snap, error;
	struct ether_header *eh1, eh2;
	struct llc llc1;
	uint16_t ether_type;

	snap = 0;
	error = -1;	/* Default error if not error == 0 */
	eh1 = mtod(*mp, struct ether_header *);
	ether_type = ntohs(eh1->ether_type);

	/*
	 * Check for SNAP/LLC.
	 */
        if (ether_type < ETHERMTU) {
                struct llc *llc2 = (struct llc *)(eh1 + 1);

                if ((*mp)->m_len >= ETHER_HDR_LEN + 8 &&
                    llc2->llc_dsap == LLC_SNAP_LSAP &&
                    llc2->llc_ssap == LLC_SNAP_LSAP &&
                    llc2->llc_control == LLC_UI) {
                	ether_type = htons(llc2->llc_un.type_snap.ether_type);
			snap = 1;
                }
        }

	/*
	 * If we're trying to filter bridge traffic, don't look at anything
	 * other than IP and ARP traffic.  If the filter doesn't understand
	 * IPv6, don't allow IPv6 through the bridge either.  This is lame
	 * since if we really wanted, say, an AppleTalk filter, we are hosed,
	 * but of course we don't have an AppleTalk filter to begin with.
	 * (Note that since IPF doesn't understand ARP it will pass *ALL*
	 * ARP traffic.)
	 */
	switch (ether_type) {
		case ETHERTYPE_ARP:
		case ETHERTYPE_REVARP:
			return 0; /* Automatically pass */
		case ETHERTYPE_IP:
# ifdef INET6
		case ETHERTYPE_IPV6:
# endif /* INET6 */
			break;
		default:
			goto bad;
	}

	/* Strip off the Ethernet header and keep a copy. */
	m_copydata(*mp, 0, ETHER_HDR_LEN, (void *) &eh2);
	m_adj(*mp, ETHER_HDR_LEN);

	/* Strip off snap header, if present */
	if (snap) {
		m_copydata(*mp, 0, sizeof(struct llc), (void *) &llc1);
		m_adj(*mp, sizeof(struct llc));
	}

	/*
	 * Check basic packet sanity and run IPF through pfil.
	 */
	KASSERT(!cpu_intr_p());
	switch (ether_type)
	{
	case ETHERTYPE_IP :
		error = (dir == PFIL_IN) ? bridge_ip_checkbasic(mp) : 0;
		if (error == 0)
			error = pfil_run_hooks(inet_pfil_hook, mp, ifp, dir);
		break;
# ifdef INET6
	case ETHERTYPE_IPV6 :
		error = (dir == PFIL_IN) ? bridge_ip6_checkbasic(mp) : 0;
		if (error == 0)
			error = pfil_run_hooks(inet6_pfil_hook, mp, ifp, dir);
		break;
# endif
	default :
		error = 0;
		break;
	}

	if (*mp == NULL)
		return error;
	if (error != 0)
		goto bad;

	error = -1;

	/*
	 * Finally, put everything back the way it was and return
	 */
	if (snap) {
		M_PREPEND(*mp, sizeof(struct llc), M_DONTWAIT);
		if (*mp == NULL)
			return error;
		bcopy(&llc1, mtod(*mp, void *), sizeof(struct llc));
	}

	M_PREPEND(*mp, ETHER_HDR_LEN, M_DONTWAIT);
	if (*mp == NULL)
		return error;
	bcopy(&eh2, mtod(*mp, void *), ETHER_HDR_LEN);

	return 0;

    bad:
	m_freem(*mp);
	*mp = NULL;
	return error;
}

/*
 * Perform basic checks on header size since
 * IPF assumes ip_input has already processed
 * it for it.  Cut-and-pasted from ip_input.c.
 * Given how simple the IPv6 version is,
 * does the IPv4 version really need to be
 * this complicated?
 *
 * XXX Should we update ipstat here, or not?
 * XXX Right now we update ipstat but not
 * XXX csum_counter.
 */
static int
bridge_ip_checkbasic(struct mbuf **mp)
{
	struct mbuf *m = *mp;
	struct ip *ip;
	int len, hlen;

	if (*mp == NULL)
		return -1;

	if (IP_HDR_ALIGNED_P(mtod(m, void *)) == 0) {
		if ((m = m_copyup(m, sizeof(struct ip),
			(max_linkhdr + 3) & ~3)) == NULL) {
			/* XXXJRT new stat, please */
			ip_statinc(IP_STAT_TOOSMALL);
			goto bad;
		}
	} else if (__predict_false(m->m_len < sizeof (struct ip))) {
		if ((m = m_pullup(m, sizeof (struct ip))) == NULL) {
			ip_statinc(IP_STAT_TOOSMALL);
			goto bad;
		}
	}
	ip = mtod(m, struct ip *);
	if (ip == NULL) goto bad;

	if (ip->ip_v != IPVERSION) {
		ip_statinc(IP_STAT_BADVERS);
		goto bad;
	}
	hlen = ip->ip_hl << 2;
	if (hlen < sizeof(struct ip)) { /* minimum header length */
		ip_statinc(IP_STAT_BADHLEN);
		goto bad;
	}
	if (hlen > m->m_len) {
		if ((m = m_pullup(m, hlen)) == 0) {
			ip_statinc(IP_STAT_BADHLEN);
			goto bad;
		}
		ip = mtod(m, struct ip *);
		if (ip == NULL) goto bad;
	}

        switch (m->m_pkthdr.csum_flags &
                ((m->m_pkthdr.rcvif->if_csum_flags_rx & M_CSUM_IPv4) |
                 M_CSUM_IPv4_BAD)) {
        case M_CSUM_IPv4|M_CSUM_IPv4_BAD:
                /* INET_CSUM_COUNTER_INCR(&ip_hwcsum_bad); */
                goto bad;

        case M_CSUM_IPv4:
                /* Checksum was okay. */
                /* INET_CSUM_COUNTER_INCR(&ip_hwcsum_ok); */
                break;

        default:
                /* Must compute it ourselves. */
                /* INET_CSUM_COUNTER_INCR(&ip_swcsum); */
                if (in_cksum(m, hlen) != 0)
                        goto bad;
                break;
        }

        /* Retrieve the packet length. */
        len = ntohs(ip->ip_len);

        /*
         * Check for additional length bogosity
         */
        if (len < hlen) {
		ip_statinc(IP_STAT_BADLEN);
                goto bad;
        }

        /*
         * Check that the amount of data in the buffers
         * is as at least much as the IP header would have us expect.
         * Drop packet if shorter than we expect.
         */
        if (m->m_pkthdr.len < len) {
		ip_statinc(IP_STAT_TOOSHORT);
                goto bad;
        }

	/* Checks out, proceed */
	*mp = m;
	return 0;

    bad:
	*mp = m;
	return -1;
}

# ifdef INET6
/*
 * Same as above, but for IPv6.
 * Cut-and-pasted from ip6_input.c.
 * XXX Should we update ip6stat, or not?
 */
static int
bridge_ip6_checkbasic(struct mbuf **mp)
{
	struct mbuf *m = *mp;
	struct ip6_hdr *ip6;

        /*
         * If the IPv6 header is not aligned, slurp it up into a new
         * mbuf with space for link headers, in the event we forward
         * it.  Otherwise, if it is aligned, make sure the entire base
         * IPv6 header is in the first mbuf of the chain.
         */
        if (IP6_HDR_ALIGNED_P(mtod(m, void *)) == 0) {
                struct ifnet *inifp = m->m_pkthdr.rcvif;
                if ((m = m_copyup(m, sizeof(struct ip6_hdr),
                                  (max_linkhdr + 3) & ~3)) == NULL) {
                        /* XXXJRT new stat, please */
			ip6_statinc(IP6_STAT_TOOSMALL);
                        in6_ifstat_inc(inifp, ifs6_in_hdrerr);
                        goto bad;
                }
        } else if (__predict_false(m->m_len < sizeof(struct ip6_hdr))) {
                struct ifnet *inifp = m->m_pkthdr.rcvif;
                if ((m = m_pullup(m, sizeof(struct ip6_hdr))) == NULL) {
			ip6_statinc(IP6_STAT_TOOSMALL);
                        in6_ifstat_inc(inifp, ifs6_in_hdrerr);
                        goto bad;
                }
        }

        ip6 = mtod(m, struct ip6_hdr *);

        if ((ip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION) {
		ip6_statinc(IP6_STAT_BADVERS);
                in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_hdrerr);
                goto bad;
        }

	/* Checks out, proceed */
	*mp = m;
	return 0;

    bad:
	*mp = m;
	return -1;
}
# endif /* INET6 */
#endif /* BRIDGE_IPF */
