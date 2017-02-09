/*	$NetBSD: in6.c,v 1.190 2015/08/24 22:21:27 pooka Exp $	*/
/*	$KAME: in6.c,v 1.198 2001/07/18 09:12:38 itojun Exp $	*/

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
 * Copyright (c) 1982, 1986, 1991, 1993
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
 *	@(#)in.c	8.2 (Berkeley) 11/15/93
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: in6.c,v 1.190 2015/08/24 22:21:27 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/kauth.h>
#include <sys/cprng.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <net/pfil.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <net/if_ether.h>

#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet6/mld6_var.h>
#include <netinet6/ip6_mroute.h>
#include <netinet6/in6_ifattach.h>
#include <netinet6/scope6_var.h>

#include <net/net_osdep.h>

#ifdef COMPAT_50
#include <compat/netinet6/in6_var.h>
#endif

MALLOC_DEFINE(M_IP6OPT, "ip6_options", "IPv6 options");

/* enable backward compatibility code for obsoleted ioctls */
#define COMPAT_IN6IFIOCTL

#ifdef	IN6_DEBUG
#define	IN6_DPRINTF(__fmt, ...)	printf(__fmt, __VA_ARGS__)
#else
#define	IN6_DPRINTF(__fmt, ...)	do { } while (/*CONSTCOND*/0) 
#endif /* IN6_DEBUG */

/*
 * Definitions of some constant IP6 addresses.
 */
const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
const struct in6_addr in6addr_loopback = IN6ADDR_LOOPBACK_INIT;
const struct in6_addr in6addr_nodelocal_allnodes =
	IN6ADDR_NODELOCAL_ALLNODES_INIT;
const struct in6_addr in6addr_linklocal_allnodes =
	IN6ADDR_LINKLOCAL_ALLNODES_INIT;
const struct in6_addr in6addr_linklocal_allrouters =
	IN6ADDR_LINKLOCAL_ALLROUTERS_INIT;

const struct in6_addr in6mask0 = IN6MASK0;
const struct in6_addr in6mask32 = IN6MASK32;
const struct in6_addr in6mask64 = IN6MASK64;
const struct in6_addr in6mask96 = IN6MASK96;
const struct in6_addr in6mask128 = IN6MASK128;

const struct sockaddr_in6 sa6_any = {sizeof(sa6_any), AF_INET6,
				     0, 0, IN6ADDR_ANY_INIT, 0};

static int in6_lifaddr_ioctl(struct socket *, u_long, void *,
	struct ifnet *);
static int in6_ifinit(struct ifnet *, struct in6_ifaddr *,
	const struct sockaddr_in6 *, int);
static void in6_unlink_ifa(struct in6_ifaddr *, struct ifnet *);

/*
 * Add ownaddr as loopback rtentry.  We previously add the route only if
 * necessary (ex. on a p2p link).  However, since we now manage addresses
 * separately from prefixes, we should always add the route.  We can't
 * rely on the cloning mechanism from the corresponding interface route
 * any more.
 */
void
in6_ifaddlocal(struct ifaddr *ifa)
{

	if (IN6_ARE_ADDR_EQUAL(IFA_IN6(ifa), &in6addr_any) ||
	    (ifa->ifa_ifp->if_flags & IFF_POINTOPOINT &&
	    IN6_ARE_ADDR_EQUAL(IFA_IN6(ifa), IFA_DSTIN6(ifa))))
	{
		rt_newaddrmsg(RTM_NEWADDR, ifa, 0, NULL);
		return;
	}

	rt_ifa_addlocal(ifa);
}

/*
 * Remove loopback rtentry of ownaddr generated by in6_ifaddlocal(),
 * if it exists.
 */
void
in6_ifremlocal(struct ifaddr *ifa)
{
	struct in6_ifaddr *ia;
	struct ifaddr *alt_ifa = NULL;
	int ia_count = 0;

	/*
	 * Some of BSD variants do not remove cloned routes
	 * from an interface direct route, when removing the direct route
	 * (see comments in net/net_osdep.h).  Even for variants that do remove
	 * cloned routes, they could fail to remove the cloned routes when
	 * we handle multple addresses that share a common prefix.
	 * So, we should remove the route corresponding to the deleted address.
	 */

	/*
	 * Delete the entry only if exactly one ifaddr matches the
	 * address, ifa->ifa_addr.
	 *
	 * If more than one ifaddr matches, replace the ifaddr in
	 * the routing table, rt_ifa, with a different ifaddr than
	 * the one we are purging, ifa.  It is important to do
	 * this, or else the routing table can accumulate dangling
	 * pointers rt->rt_ifa->ifa_ifp to destroyed interfaces,
	 * which will lead to crashes, later.  (More than one ifaddr
	 * can match if we assign the same address to multiple---probably
	 * p2p---interfaces.)
	 *
	 * XXX An old comment at this place said, "we should avoid
	 * XXX such a configuration [i.e., interfaces with the same
	 * XXX addressed assigned --ed.] in IPv6...".  I do not
	 * XXX agree, especially now that I have fixed the dangling
	 * XXX ifp-pointers bug.
	 */
	for (ia = in6_ifaddr; ia; ia = ia->ia_next) {
		if (!IN6_ARE_ADDR_EQUAL(IFA_IN6(ifa), &ia->ia_addr.sin6_addr))
			continue;
		if (ia->ia_ifp != ifa->ifa_ifp)
			alt_ifa = &ia->ia_ifa;
		if (++ia_count > 1 && alt_ifa != NULL)
			break;
	}

	if (ia_count == 0)
		return;

	rt_ifa_remlocal(ifa, ia_count == 1 ? NULL : alt_ifa);
}

int
in6_mask2len(struct in6_addr *mask, u_char *lim0)
{
	int x = 0, y;
	u_char *lim = lim0, *p;

	/* ignore the scope_id part */
	if (lim0 == NULL || lim0 - (u_char *)mask > sizeof(*mask))
		lim = (u_char *)mask + sizeof(*mask);
	for (p = (u_char *)mask; p < lim; x++, p++) {
		if (*p != 0xff)
			break;
	}
	y = 0;
	if (p < lim) {
		for (y = 0; y < NBBY; y++) {
			if ((*p & (0x80 >> y)) == 0)
				break;
		}
	}

	/*
	 * when the limit pointer is given, do a stricter check on the
	 * remaining bits.
	 */
	if (p < lim) {
		if (y != 0 && (*p & (0x00ff >> y)) != 0)
			return -1;
		for (p = p + 1; p < lim; p++)
			if (*p != 0)
				return -1;
	}

	return x * NBBY + y;
}

#define ifa2ia6(ifa)	((struct in6_ifaddr *)(ifa))
#define ia62ifa(ia6)	(&((ia6)->ia_ifa))

static int
in6_control1(struct socket *so, u_long cmd, void *data, struct ifnet *ifp)
{
	struct	in6_ifreq *ifr = (struct in6_ifreq *)data;
	struct	in6_ifaddr *ia = NULL;
	struct	in6_aliasreq *ifra = (struct in6_aliasreq *)data;
	struct sockaddr_in6 *sa6;
	int error;

	switch (cmd) {
	/*
	 * XXX: Fix me, once we fix SIOCSIFADDR, SIOCIFDSTADDR, etc.
	 */
	case SIOCSIFADDR:
	case SIOCSIFDSTADDR:
	case SIOCSIFBRDADDR:
	case SIOCSIFNETMASK:
		return EOPNOTSUPP;
	case SIOCGETSGCNT_IN6:
	case SIOCGETMIFCNT_IN6:
		return mrt6_ioctl(cmd, data);
	case SIOCGIFADDRPREF:
	case SIOCSIFADDRPREF:
		if (ifp == NULL)
			return EINVAL;
		return ifaddrpref_ioctl(so, cmd, data, ifp);
	}

	if (ifp == NULL)
		return EOPNOTSUPP;

	switch (cmd) {
	case SIOCSNDFLUSH_IN6:
	case SIOCSPFXFLUSH_IN6:
	case SIOCSRTRFLUSH_IN6:
	case SIOCSDEFIFACE_IN6:
	case SIOCSIFINFO_FLAGS:
	case SIOCSIFINFO_IN6:
		/* Privileged. */
		/* FALLTHROUGH */
	case OSIOCGIFINFO_IN6:
	case SIOCGIFINFO_IN6:
	case SIOCGDRLST_IN6:
	case SIOCGPRLST_IN6:
	case SIOCGNBRINFO_IN6:
	case SIOCGDEFIFACE_IN6:
		return nd6_ioctl(cmd, data, ifp);
	}

	switch (cmd) {
	case SIOCSIFPREFIX_IN6:
	case SIOCDIFPREFIX_IN6:
	case SIOCAIFPREFIX_IN6:
	case SIOCCIFPREFIX_IN6:
	case SIOCSGIFPREFIX_IN6:
	case SIOCGIFPREFIX_IN6:
		log(LOG_NOTICE,
		    "prefix ioctls are now invalidated. "
		    "please use ifconfig.\n");
		return EOPNOTSUPP;
	}

	switch (cmd) {
	case SIOCALIFADDR:
	case SIOCDLIFADDR:
		/* Privileged. */
		/* FALLTHROUGH */
	case SIOCGLIFADDR:
		return in6_lifaddr_ioctl(so, cmd, data, ifp);
	}

	/*
	 * Find address for this interface, if it exists.
	 *
	 * In netinet code, we have checked ifra_addr in SIOCSIF*ADDR operation
	 * only, and used the first interface address as the target of other
	 * operations (without checking ifra_addr).  This was because netinet
	 * code/API assumed at most 1 interface address per interface.
	 * Since IPv6 allows a node to assign multiple addresses
	 * on a single interface, we almost always look and check the
	 * presence of ifra_addr, and reject invalid ones here.
	 * It also decreases duplicated code among SIOC*_IN6 operations.
	 */
	switch (cmd) {
	case SIOCAIFADDR_IN6:
#ifdef OSIOCAIFADDR_IN6
	case OSIOCAIFADDR_IN6:
#endif
#ifdef OSIOCSIFPHYADDR_IN6
	case OSIOCSIFPHYADDR_IN6:
#endif
	case SIOCSIFPHYADDR_IN6:
		sa6 = &ifra->ifra_addr;
		break;
	case SIOCSIFADDR_IN6:
	case SIOCGIFADDR_IN6:
	case SIOCSIFDSTADDR_IN6:
	case SIOCSIFNETMASK_IN6:
	case SIOCGIFDSTADDR_IN6:
	case SIOCGIFNETMASK_IN6:
	case SIOCDIFADDR_IN6:
	case SIOCGIFPSRCADDR_IN6:
	case SIOCGIFPDSTADDR_IN6:
	case SIOCGIFAFLAG_IN6:
	case SIOCSNDFLUSH_IN6:
	case SIOCSPFXFLUSH_IN6:
	case SIOCSRTRFLUSH_IN6:
	case SIOCGIFALIFETIME_IN6:
#ifdef OSIOCGIFALIFETIME_IN6
	case OSIOCGIFALIFETIME_IN6:
#endif
	case SIOCGIFSTAT_IN6:
	case SIOCGIFSTAT_ICMP6:
		sa6 = &ifr->ifr_addr;
		break;
	default:
		sa6 = NULL;
		break;
	}
	if (sa6 && sa6->sin6_family == AF_INET6) {
		if (sa6->sin6_scope_id != 0)
			error = sa6_embedscope(sa6, 0);
		else
			error = in6_setscope(&sa6->sin6_addr, ifp, NULL);
		if (error != 0)
			return error;
		ia = in6ifa_ifpwithaddr(ifp, &sa6->sin6_addr);
	} else
		ia = NULL;

	switch (cmd) {
	case SIOCSIFADDR_IN6:
	case SIOCSIFDSTADDR_IN6:
	case SIOCSIFNETMASK_IN6:
		/*
		 * Since IPv6 allows a node to assign multiple addresses
		 * on a single interface, SIOCSIFxxx ioctls are deprecated.
		 */
		return EINVAL;

	case SIOCDIFADDR_IN6:
		/*
		 * for IPv4, we look for existing in_ifaddr here to allow
		 * "ifconfig if0 delete" to remove the first IPv4 address on
		 * the interface.  For IPv6, as the spec allows multiple
		 * interface address from the day one, we consider "remove the
		 * first one" semantics to be not preferable.
		 */
		if (ia == NULL)
			return EADDRNOTAVAIL;
		/* FALLTHROUGH */
#ifdef OSIOCAIFADDR_IN6
	case OSIOCAIFADDR_IN6:
#endif
	case SIOCAIFADDR_IN6:
		/*
		 * We always require users to specify a valid IPv6 address for
		 * the corresponding operation.
		 */
		if (ifra->ifra_addr.sin6_family != AF_INET6 ||
		    ifra->ifra_addr.sin6_len != sizeof(struct sockaddr_in6))
			return EAFNOSUPPORT;
		/* Privileged. */

		break;

	case SIOCGIFADDR_IN6:
		/* This interface is basically deprecated. use SIOCGIFCONF. */
		/* FALLTHROUGH */
	case SIOCGIFAFLAG_IN6:
	case SIOCGIFNETMASK_IN6:
	case SIOCGIFDSTADDR_IN6:
	case SIOCGIFALIFETIME_IN6:
#ifdef OSIOCGIFALIFETIME_IN6
	case OSIOCGIFALIFETIME_IN6:
#endif
		/* must think again about its semantics */
		if (ia == NULL)
			return EADDRNOTAVAIL;
		break;
	}

	switch (cmd) {

	case SIOCGIFADDR_IN6:
		ifr->ifr_addr = ia->ia_addr;
		if ((error = sa6_recoverscope(&ifr->ifr_addr)) != 0)
			return error;
		break;

	case SIOCGIFDSTADDR_IN6:
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			return EINVAL;
		/*
		 * XXX: should we check if ifa_dstaddr is NULL and return
		 * an error?
		 */
		ifr->ifr_dstaddr = ia->ia_dstaddr;
		if ((error = sa6_recoverscope(&ifr->ifr_dstaddr)) != 0)
			return error;
		break;

	case SIOCGIFNETMASK_IN6:
		ifr->ifr_addr = ia->ia_prefixmask;
		break;

	case SIOCGIFAFLAG_IN6:
		ifr->ifr_ifru.ifru_flags6 = ia->ia6_flags;
		break;

	case SIOCGIFSTAT_IN6:
		if (ifp == NULL)
			return EINVAL;
		memset(&ifr->ifr_ifru.ifru_stat, 0,
		    sizeof(ifr->ifr_ifru.ifru_stat));
		ifr->ifr_ifru.ifru_stat =
		    *((struct in6_ifextra *)ifp->if_afdata[AF_INET6])->in6_ifstat;
		break;

	case SIOCGIFSTAT_ICMP6:
		if (ifp == NULL)
			return EINVAL;
		memset(&ifr->ifr_ifru.ifru_icmp6stat, 0,
		    sizeof(ifr->ifr_ifru.ifru_icmp6stat));
		ifr->ifr_ifru.ifru_icmp6stat =
		    *((struct in6_ifextra *)ifp->if_afdata[AF_INET6])->icmp6_ifstat;
		break;

#ifdef OSIOCGIFALIFETIME_IN6
	case OSIOCGIFALIFETIME_IN6:
#endif
	case SIOCGIFALIFETIME_IN6:
		ifr->ifr_ifru.ifru_lifetime = ia->ia6_lifetime;
		if (ia->ia6_lifetime.ia6t_vltime != ND6_INFINITE_LIFETIME) {
			time_t maxexpire;
			struct in6_addrlifetime *retlt =
			    &ifr->ifr_ifru.ifru_lifetime;

			/*
			 * XXX: adjust expiration time assuming time_t is
			 * signed.
			 */
			maxexpire = ((time_t)~0) &
			    ~((time_t)1 << ((sizeof(maxexpire) * NBBY) - 1));
			if (ia->ia6_lifetime.ia6t_vltime <
			    maxexpire - ia->ia6_updatetime) {
				retlt->ia6t_expire = ia->ia6_updatetime +
				    ia->ia6_lifetime.ia6t_vltime;
				retlt->ia6t_expire = retlt->ia6t_expire ?
				    time_mono_to_wall(retlt->ia6t_expire) :
				    0;
			} else
				retlt->ia6t_expire = maxexpire;
		}
		if (ia->ia6_lifetime.ia6t_pltime != ND6_INFINITE_LIFETIME) {
			time_t maxexpire;
			struct in6_addrlifetime *retlt =
			    &ifr->ifr_ifru.ifru_lifetime;

			/*
			 * XXX: adjust expiration time assuming time_t is
			 * signed.
			 */
			maxexpire = ((time_t)~0) &
			    ~((time_t)1 << ((sizeof(maxexpire) * NBBY) - 1));
			if (ia->ia6_lifetime.ia6t_pltime <
			    maxexpire - ia->ia6_updatetime) {
				retlt->ia6t_preferred = ia->ia6_updatetime +
				    ia->ia6_lifetime.ia6t_pltime;
				retlt->ia6t_preferred = retlt->ia6t_preferred ?
				    time_mono_to_wall(retlt->ia6t_preferred) :
				    0;
			} else
				retlt->ia6t_preferred = maxexpire;
		}
#ifdef OSIOCFIFALIFETIME_IN6
		if (cmd == OSIOCFIFALIFETIME_IN6)
			in6_addrlifetime_to_in6_addrlifetime50(
			    &ifr->ifru.ifru_lifetime);
#endif
		break;

#ifdef OSIOCAIFADDR_IN6
	case OSIOCAIFADDR_IN6:
		in6_aliasreq50_to_in6_aliasreq(ifra);
		/*FALLTHROUGH*/
#endif
	case SIOCAIFADDR_IN6:
	{
		int i;
		struct nd_prefixctl prc0;
		struct nd_prefix *pr;
		struct in6_addrlifetime *lt;

		/* reject read-only flags */
		if ((ifra->ifra_flags & IN6_IFF_DUPLICATED) != 0 ||
		    (ifra->ifra_flags & IN6_IFF_DETACHED) != 0 ||
		    (ifra->ifra_flags & IN6_IFF_TENTATIVE) != 0 ||
		    (ifra->ifra_flags & IN6_IFF_NODAD) != 0 ||
		    (ifra->ifra_flags & IN6_IFF_AUTOCONF) != 0) {
			return EINVAL;
		}
		/*
		 * ia6t_expire and ia6t_preferred won't be used for now,
		 * so just in case.
		 */
		lt = &ifra->ifra_lifetime;
		if (lt->ia6t_expire != 0)
			lt->ia6t_expire = time_wall_to_mono(lt->ia6t_expire);
		if (lt->ia6t_preferred != 0)
			lt->ia6t_preferred =
			    time_wall_to_mono(lt->ia6t_preferred);
		/*
		 * first, make or update the interface address structure,
		 * and link it to the list.
		 */
		if ((error = in6_update_ifa(ifp, ifra, ia, 0)) != 0)
			return error;
		if ((ia = in6ifa_ifpwithaddr(ifp, &ifra->ifra_addr.sin6_addr))
		    == NULL) {
		    	/*
			 * this can happen when the user specify the 0 valid
			 * lifetime.
			 */
			break;
		}

		/*
		 * then, make the prefix on-link on the interface.
		 * XXX: we'd rather create the prefix before the address, but
		 * we need at least one address to install the corresponding
		 * interface route, so we configure the address first.
		 */

		/*
		 * convert mask to prefix length (prefixmask has already
		 * been validated in in6_update_ifa().
		 */
		memset(&prc0, 0, sizeof(prc0));
		prc0.ndprc_ifp = ifp;
		prc0.ndprc_plen = in6_mask2len(&ifra->ifra_prefixmask.sin6_addr,
		    NULL);
		if (prc0.ndprc_plen == 128) {
			break;	/* we don't need to install a host route. */
		}
		prc0.ndprc_prefix = ifra->ifra_addr;
		/* apply the mask for safety. */
		for (i = 0; i < 4; i++) {
			prc0.ndprc_prefix.sin6_addr.s6_addr32[i] &=
			    ifra->ifra_prefixmask.sin6_addr.s6_addr32[i];
		}
		/*
		 * XXX: since we don't have an API to set prefix (not address)
		 * lifetimes, we just use the same lifetimes as addresses.
		 * The (temporarily) installed lifetimes can be overridden by
		 * later advertised RAs (when accept_rtadv is non 0), which is
		 * an intended behavior.
		 */
		prc0.ndprc_raf_onlink = 1; /* should be configurable? */
		prc0.ndprc_raf_auto =
		    ((ifra->ifra_flags & IN6_IFF_AUTOCONF) != 0);
		prc0.ndprc_vltime = ifra->ifra_lifetime.ia6t_vltime;
		prc0.ndprc_pltime = ifra->ifra_lifetime.ia6t_pltime;

		/* add the prefix if not yet. */
		if ((pr = nd6_prefix_lookup(&prc0)) == NULL) {
			/*
			 * nd6_prelist_add will install the corresponding
			 * interface route.
			 */
			if ((error = nd6_prelist_add(&prc0, NULL, &pr)) != 0)
				return error;
			if (pr == NULL) {
				log(LOG_ERR, "nd6_prelist_add succeeded but "
				    "no prefix\n");
				return EINVAL; /* XXX panic here? */
			}
		}

		/* relate the address to the prefix */
		if (ia->ia6_ndpr == NULL) {
			ia->ia6_ndpr = pr;
			pr->ndpr_refcnt++;

			/*
			 * If this is the first autoconf address from the
			 * prefix, create a temporary address as well
			 * (when required).
			 */
			if ((ia->ia6_flags & IN6_IFF_AUTOCONF) &&
			    ip6_use_tempaddr && pr->ndpr_refcnt == 1) {
				int e;
				if ((e = in6_tmpifadd(ia, 1, 0)) != 0) {
					log(LOG_NOTICE, "in6_control: failed "
					    "to create a temporary address, "
					    "errno=%d\n", e);
				}
			}
		}

		/*
		 * this might affect the status of autoconfigured addresses,
		 * that is, this address might make other addresses detached.
		 */
		pfxlist_onlink_check();

		(void)pfil_run_hooks(if_pfil, (struct mbuf **)SIOCAIFADDR_IN6,
		    ifp, PFIL_IFADDR);
		break;
	}

	case SIOCDIFADDR_IN6:
	{
		struct nd_prefix *pr;

		/*
		 * If the address being deleted is the only one that owns
		 * the corresponding prefix, expire the prefix as well.
		 * XXX: theoretically, we don't have to worry about such
		 * relationship, since we separate the address management
		 * and the prefix management.  We do this, however, to provide
		 * as much backward compatibility as possible in terms of
		 * the ioctl operation.
		 * Note that in6_purgeaddr() will decrement ndpr_refcnt.
		 */
		pr = ia->ia6_ndpr;
		in6_purgeaddr(&ia->ia_ifa);
		if (pr && pr->ndpr_refcnt == 0)
			prelist_remove(pr);
		(void)pfil_run_hooks(if_pfil, (struct mbuf **)SIOCDIFADDR_IN6,
		    ifp, PFIL_IFADDR);
		break;
	}

	default:
		return ENOTTY;
	}

	return 0;
}

int
in6_control(struct socket *so, u_long cmd, void *data, struct ifnet *ifp)
{
	int error, s;

	switch (cmd) {
	case SIOCSNDFLUSH_IN6:
	case SIOCSPFXFLUSH_IN6:
	case SIOCSRTRFLUSH_IN6:
	case SIOCSDEFIFACE_IN6:
	case SIOCSIFINFO_FLAGS:
	case SIOCSIFINFO_IN6:

	case SIOCALIFADDR:
	case SIOCDLIFADDR:

	case SIOCDIFADDR_IN6:
#ifdef OSIOCAIFADDR_IN6
	case OSIOCAIFADDR_IN6:
#endif
	case SIOCAIFADDR_IN6:
		if (kauth_authorize_network(curlwp->l_cred,
		    KAUTH_NETWORK_SOCKET,
		    KAUTH_REQ_NETWORK_SOCKET_SETPRIV,
		    so, NULL, NULL))
			return EPERM;
		break;
	}

	s = splnet();
	error = in6_control1(so , cmd, data, ifp);
	splx(s);
	return error;
}

/*
 * Update parameters of an IPv6 interface address.
 * If necessary, a new entry is created and linked into address chains.
 * This function is separated from in6_control().
 * XXX: should this be performed under splnet()?
 */
static int
in6_update_ifa1(struct ifnet *ifp, struct in6_aliasreq *ifra,
    struct in6_ifaddr *ia, int flags)
{
	int error = 0, hostIsNew = 0, plen = -1;
	struct in6_ifaddr *oia;
	struct sockaddr_in6 dst6;
	struct in6_addrlifetime *lt;
	struct in6_multi_mship *imm;
	struct in6_multi *in6m_sol;
	struct rtentry *rt;
	int dad_delay, was_tentative;

	in6m_sol = NULL;

	/* Validate parameters */
	if (ifp == NULL || ifra == NULL) /* this maybe redundant */
		return EINVAL;

	/*
	 * The destination address for a p2p link must have a family
	 * of AF_UNSPEC or AF_INET6.
	 */
	if ((ifp->if_flags & IFF_POINTOPOINT) != 0 &&
	    ifra->ifra_dstaddr.sin6_family != AF_INET6 &&
	    ifra->ifra_dstaddr.sin6_family != AF_UNSPEC)
		return EAFNOSUPPORT;
	/*
	 * validate ifra_prefixmask.  don't check sin6_family, netmask
	 * does not carry fields other than sin6_len.
	 */
	if (ifra->ifra_prefixmask.sin6_len > sizeof(struct sockaddr_in6))
		return EINVAL;
	/*
	 * Because the IPv6 address architecture is classless, we require
	 * users to specify a (non 0) prefix length (mask) for a new address.
	 * We also require the prefix (when specified) mask is valid, and thus
	 * reject a non-consecutive mask.
	 */
	if (ia == NULL && ifra->ifra_prefixmask.sin6_len == 0)
		return EINVAL;
	if (ifra->ifra_prefixmask.sin6_len != 0) {
		plen = in6_mask2len(&ifra->ifra_prefixmask.sin6_addr,
		    (u_char *)&ifra->ifra_prefixmask +
		    ifra->ifra_prefixmask.sin6_len);
		if (plen <= 0)
			return EINVAL;
	} else {
		/*
		 * In this case, ia must not be NULL.  We just use its prefix
		 * length.
		 */
		plen = in6_mask2len(&ia->ia_prefixmask.sin6_addr, NULL);
	}
	/*
	 * If the destination address on a p2p interface is specified,
	 * and the address is a scoped one, validate/set the scope
	 * zone identifier.
	 */
	dst6 = ifra->ifra_dstaddr;
	if ((ifp->if_flags & (IFF_POINTOPOINT|IFF_LOOPBACK)) != 0 &&
	    (dst6.sin6_family == AF_INET6)) {
		struct in6_addr in6_tmp;
		u_int32_t zoneid;

		in6_tmp = dst6.sin6_addr;
		if (in6_setscope(&in6_tmp, ifp, &zoneid))
			return EINVAL; /* XXX: should be impossible */

		if (dst6.sin6_scope_id != 0) {
			if (dst6.sin6_scope_id != zoneid)
				return EINVAL;
		} else		/* user omit to specify the ID. */
			dst6.sin6_scope_id = zoneid;

		/* convert into the internal form */
		if (sa6_embedscope(&dst6, 0))
			return EINVAL; /* XXX: should be impossible */
	}
	/*
	 * The destination address can be specified only for a p2p or a
	 * loopback interface.  If specified, the corresponding prefix length
	 * must be 128.
	 */
	if (ifra->ifra_dstaddr.sin6_family == AF_INET6) {
#ifdef FORCE_P2PPLEN
		int i;
#endif

		if ((ifp->if_flags & (IFF_POINTOPOINT|IFF_LOOPBACK)) == 0) {
			/* XXX: noisy message */
			nd6log((LOG_INFO, "in6_update_ifa: a destination can "
			    "be specified for a p2p or a loopback IF only\n"));
			return EINVAL;
		}
		if (plen != 128) {
			nd6log((LOG_INFO, "in6_update_ifa: prefixlen should "
			    "be 128 when dstaddr is specified\n"));
#ifdef FORCE_P2PPLEN
			/*
			 * To be compatible with old configurations,
			 * such as ifconfig gif0 inet6 2001::1 2001::2
			 * prefixlen 126, we override the specified
			 * prefixmask as if the prefix length was 128.
			 */
			ifra->ifra_prefixmask.sin6_len =
			    sizeof(struct sockaddr_in6);
			for (i = 0; i < 4; i++)
				ifra->ifra_prefixmask.sin6_addr.s6_addr32[i] =
				    0xffffffff;
			plen = 128;
#else
			return EINVAL;
#endif
		}
	}
	/* lifetime consistency check */
	lt = &ifra->ifra_lifetime;
	if (lt->ia6t_pltime > lt->ia6t_vltime)
		return EINVAL;
	if (lt->ia6t_vltime == 0) {
		/*
		 * the following log might be noisy, but this is a typical
		 * configuration mistake or a tool's bug.
		 */
		nd6log((LOG_INFO,
		    "in6_update_ifa: valid lifetime is 0 for %s\n",
		    ip6_sprintf(&ifra->ifra_addr.sin6_addr)));

		if (ia == NULL)
			return 0; /* there's nothing to do */
	}

	/*
	 * If this is a new address, allocate a new ifaddr and link it
	 * into chains.
	 */
	if (ia == NULL) {
		hostIsNew = 1;
		/*
		 * When in6_update_ifa() is called in a process of a received
		 * RA, it is called under an interrupt context.  So, we should
		 * call malloc with M_NOWAIT.
		 */
		ia = (struct in6_ifaddr *) malloc(sizeof(*ia), M_IFADDR,
		    M_NOWAIT);
		if (ia == NULL)
			return ENOBUFS;
		memset(ia, 0, sizeof(*ia));
		LIST_INIT(&ia->ia6_memberships);
		/* Initialize the address and masks, and put time stamp */
		ia->ia_ifa.ifa_addr = (struct sockaddr *)&ia->ia_addr;
		ia->ia_addr.sin6_family = AF_INET6;
		ia->ia_addr.sin6_len = sizeof(ia->ia_addr);
		ia->ia6_createtime = time_uptime;
		if ((ifp->if_flags & (IFF_POINTOPOINT | IFF_LOOPBACK)) != 0) {
			/*
			 * XXX: some functions expect that ifa_dstaddr is not
			 * NULL for p2p interfaces.
			 */
			ia->ia_ifa.ifa_dstaddr =
			    (struct sockaddr *)&ia->ia_dstaddr;
		} else {
			ia->ia_ifa.ifa_dstaddr = NULL;
		}
		ia->ia_ifa.ifa_netmask =
		    (struct sockaddr *)&ia->ia_prefixmask;

		ia->ia_ifp = ifp;
		if ((oia = in6_ifaddr) != NULL) {
			for ( ; oia->ia_next; oia = oia->ia_next)
				continue;
			oia->ia_next = ia;
		} else
			in6_ifaddr = ia;
		/* gain a refcnt for the link from in6_ifaddr */
		ifaref(&ia->ia_ifa);

		ifa_insert(ifp, &ia->ia_ifa);
	}

	/* update timestamp */
	ia->ia6_updatetime = time_uptime;

	/* set prefix mask */
	if (ifra->ifra_prefixmask.sin6_len) {
		/*
		 * We prohibit changing the prefix length of an existing
		 * address, because
		 * + such an operation should be rare in IPv6, and
		 * + the operation would confuse prefix management.
		 */
		if (ia->ia_prefixmask.sin6_len &&
		    in6_mask2len(&ia->ia_prefixmask.sin6_addr, NULL) != plen) {
			nd6log((LOG_INFO, "in6_update_ifa: the prefix length of an"
			    " existing (%s) address should not be changed\n",
			    ip6_sprintf(&ia->ia_addr.sin6_addr)));
			error = EINVAL;
			goto unlink;
		}
		ia->ia_prefixmask = ifra->ifra_prefixmask;
	}

	/*
	 * If a new destination address is specified, scrub the old one and
	 * install the new destination.  Note that the interface must be
	 * p2p or loopback (see the check above.)
	 */
	if (dst6.sin6_family == AF_INET6 &&
	    !IN6_ARE_ADDR_EQUAL(&dst6.sin6_addr, &ia->ia_dstaddr.sin6_addr)) {
		if ((ia->ia_flags & IFA_ROUTE) != 0 &&
		    rtinit(&(ia->ia_ifa), (int)RTM_DELETE, RTF_HOST) != 0) {
			nd6log((LOG_ERR, "in6_update_ifa: failed to remove "
			    "a route to the old destination: %s\n",
			    ip6_sprintf(&ia->ia_addr.sin6_addr)));
			/* proceed anyway... */
		} else
			ia->ia_flags &= ~IFA_ROUTE;
		ia->ia_dstaddr = dst6;
	}

	/*
	 * Set lifetimes.  We do not refer to ia6t_expire and ia6t_preferred
	 * to see if the address is deprecated or invalidated, but initialize
	 * these members for applications.
	 */
	ia->ia6_lifetime = ifra->ifra_lifetime;
	if (ia->ia6_lifetime.ia6t_vltime != ND6_INFINITE_LIFETIME) {
		ia->ia6_lifetime.ia6t_expire =
		    time_uptime + ia->ia6_lifetime.ia6t_vltime;
	} else
		ia->ia6_lifetime.ia6t_expire = 0;
	if (ia->ia6_lifetime.ia6t_pltime != ND6_INFINITE_LIFETIME) {
		ia->ia6_lifetime.ia6t_preferred =
		    time_uptime + ia->ia6_lifetime.ia6t_pltime;
	} else
		ia->ia6_lifetime.ia6t_preferred = 0;

	/*
	 * configure address flags.
	 * We need to preserve tentative state so DAD works if
	 * something adds the same address before DAD finishes.
	 */
	was_tentative = ia->ia6_flags & (IN6_IFF_TENTATIVE|IN6_IFF_DUPLICATED);
	ia->ia6_flags = ifra->ifra_flags;

	/*
	 * Make the address tentative before joining multicast addresses,
	 * so that corresponding MLD responses would not have a tentative
	 * source address.
	 */
	ia->ia6_flags &= ~IN6_IFF_DUPLICATED;	/* safety */
	if (ifp->if_link_state == LINK_STATE_DOWN) {
		ia->ia6_flags |= IN6_IFF_DETACHED;
		ia->ia6_flags &= ~IN6_IFF_TENTATIVE;
	} else if ((hostIsNew || was_tentative) && if_do_dad(ifp))
		ia->ia6_flags |= IN6_IFF_TENTATIVE;

	/*
	 * backward compatibility - if IN6_IFF_DEPRECATED is set from the
	 * userland, make it deprecated.
	 */
	if ((ifra->ifra_flags & IN6_IFF_DEPRECATED) != 0) {
		ia->ia6_lifetime.ia6t_pltime = 0;
		ia->ia6_lifetime.ia6t_preferred = time_uptime;
	}

	/* reset the interface and routing table appropriately. */
	if ((error = in6_ifinit(ifp, ia, &ifra->ifra_addr, hostIsNew)) != 0)
		goto unlink;
	/*
	 * We are done if we have simply modified an existing address.
	 */
	if (!hostIsNew)
		return error;

	/*
	 * Beyond this point, we should call in6_purgeaddr upon an error,
	 * not just go to unlink.
	 */

	/* join necessary multicast groups */
	if ((ifp->if_flags & IFF_MULTICAST) != 0) {
		struct sockaddr_in6 mltaddr, mltmask;
		struct in6_addr llsol;

		/* join solicited multicast addr for new host id */
		memset(&llsol, 0, sizeof(struct in6_addr));
		llsol.s6_addr16[0] = htons(0xff02);
		llsol.s6_addr32[1] = 0;
		llsol.s6_addr32[2] = htonl(1);
		llsol.s6_addr32[3] = ifra->ifra_addr.sin6_addr.s6_addr32[3];
		llsol.s6_addr8[12] = 0xff;
		if ((error = in6_setscope(&llsol, ifp, NULL)) != 0) {
			/* XXX: should not happen */
			log(LOG_ERR, "in6_update_ifa: "
			    "in6_setscope failed\n");
			goto cleanup;
		}
		dad_delay = 0;
		if ((flags & IN6_IFAUPDATE_DADDELAY)) {
			/*
			 * We need a random delay for DAD on the address
			 * being configured.  It also means delaying
			 * transmission of the corresponding MLD report to
			 * avoid report collision.
			 * [draft-ietf-ipv6-rfc2462bis-02.txt]
			 */
			dad_delay = cprng_fast32() %
			    (MAX_RTR_SOLICITATION_DELAY * hz);
		}

#define	MLTMASK_LEN  4	/* mltmask's masklen (=32bit=4octet) */
		/* join solicited multicast addr for new host id */
		imm = in6_joingroup(ifp, &llsol, &error, dad_delay);
		if (!imm) {
			nd6log((LOG_ERR,
			    "in6_update_ifa: addmulti "
			    "failed for %s on %s (errno=%d)\n",
			    ip6_sprintf(&llsol), if_name(ifp), error));
			goto cleanup;
		}
		LIST_INSERT_HEAD(&ia->ia6_memberships, imm, i6mm_chain);
		in6m_sol = imm->i6mm_maddr;

		sockaddr_in6_init(&mltmask, &in6mask32, 0, 0, 0);

		/*
		 * join link-local all-nodes address
		 */
		sockaddr_in6_init(&mltaddr, &in6addr_linklocal_allnodes,
		    0, 0, 0);
		if ((error = in6_setscope(&mltaddr.sin6_addr, ifp, NULL)) != 0)
			goto cleanup; /* XXX: should not fail */

		/*
		 * XXX: do we really need this automatic routes?
		 * We should probably reconsider this stuff.  Most applications
		 * actually do not need the routes, since they usually specify
		 * the outgoing interface.
		 */
		rt = rtalloc1((struct sockaddr *)&mltaddr, 0);
		if (rt) {
			if (memcmp(&mltaddr.sin6_addr,
			    &satocsin6(rt_getkey(rt))->sin6_addr,
			    MLTMASK_LEN)) {
				rtfree(rt);
				rt = NULL;
			} else if (rt->rt_ifp != ifp) {
				IN6_DPRINTF("%s: rt_ifp %p -> %p (%s) "
				    "network %04x:%04x::/32 = %04x:%04x::/32\n",
				    __func__, rt->rt_ifp, ifp, ifp->if_xname,
				    ntohs(mltaddr.sin6_addr.s6_addr16[0]),
				    ntohs(mltaddr.sin6_addr.s6_addr16[1]),
				    satocsin6(rt_getkey(rt))->sin6_addr.s6_addr16[0],
				    satocsin6(rt_getkey(rt))->sin6_addr.s6_addr16[1]);
				rt_replace_ifa(rt, &ia->ia_ifa);
				rt->rt_ifp = ifp;
			}
		}
		if (!rt) {
			struct rt_addrinfo info;

			memset(&info, 0, sizeof(info));
			info.rti_info[RTAX_DST] = (struct sockaddr *)&mltaddr;
			info.rti_info[RTAX_GATEWAY] =
			    (struct sockaddr *)&ia->ia_addr;
			info.rti_info[RTAX_NETMASK] =
			    (struct sockaddr *)&mltmask;
			info.rti_info[RTAX_IFA] =
			    (struct sockaddr *)&ia->ia_addr;
			/* XXX: we need RTF_CLONING to fake nd6_rtrequest */
			info.rti_flags = RTF_UP | RTF_CLONING;
			error = rtrequest1(RTM_ADD, &info, NULL);
			if (error)
				goto cleanup;
		} else {
			rtfree(rt);
		}
		imm = in6_joingroup(ifp, &mltaddr.sin6_addr, &error, 0);
		if (!imm) {
			nd6log((LOG_WARNING,
			    "in6_update_ifa: addmulti failed for "
			    "%s on %s (errno=%d)\n",
			    ip6_sprintf(&mltaddr.sin6_addr),
			    if_name(ifp), error));
			goto cleanup;
		}
		LIST_INSERT_HEAD(&ia->ia6_memberships, imm, i6mm_chain);

		/*
		 * join node information group address
		 */
		dad_delay = 0;
		if ((flags & IN6_IFAUPDATE_DADDELAY)) {
			/*
			 * The spec doesn't say anything about delay for this
			 * group, but the same logic should apply.
			 */
			dad_delay = cprng_fast32() %
			    (MAX_RTR_SOLICITATION_DELAY * hz);
		}
		if (in6_nigroup(ifp, hostname, hostnamelen, &mltaddr) != 0)
			;
		else if ((imm = in6_joingroup(ifp, &mltaddr.sin6_addr, &error,
		          dad_delay)) == NULL) { /* XXX jinmei */
			nd6log((LOG_WARNING, "in6_update_ifa: "
			    "addmulti failed for %s on %s (errno=%d)\n",
			    ip6_sprintf(&mltaddr.sin6_addr),
			    if_name(ifp), error));
			/* XXX not very fatal, go on... */
		} else {
			LIST_INSERT_HEAD(&ia->ia6_memberships, imm, i6mm_chain);
		}


		/*
		 * join interface-local all-nodes address.
		 * (ff01::1%ifN, and ff01::%ifN/32)
		 */
		mltaddr.sin6_addr = in6addr_nodelocal_allnodes;
		if ((error = in6_setscope(&mltaddr.sin6_addr, ifp, NULL)) != 0) 
			goto cleanup; /* XXX: should not fail */

		/* XXX: again, do we really need the route? */
		rt = rtalloc1((struct sockaddr *)&mltaddr, 0);
		if (rt) {
			/* 32bit came from "mltmask" */
			if (memcmp(&mltaddr.sin6_addr,
			    &satocsin6(rt_getkey(rt))->sin6_addr,
			    32 / NBBY)) {
				rtfree(rt);
				rt = NULL;
			} else if (rt->rt_ifp != ifp) {
				IN6_DPRINTF("%s: rt_ifp %p -> %p (%s) "
				    "network %04x:%04x::/32 = %04x:%04x::/32\n",
				    __func__, rt->rt_ifp, ifp, ifp->if_xname,
				    ntohs(mltaddr.sin6_addr.s6_addr16[0]),
				    ntohs(mltaddr.sin6_addr.s6_addr16[1]),
				    satocsin6(rt_getkey(rt))->sin6_addr.s6_addr16[0],
				    satocsin6(rt_getkey(rt))->sin6_addr.s6_addr16[1]);
				rt_replace_ifa(rt, &ia->ia_ifa);
				rt->rt_ifp = ifp;
			}
		}
		if (!rt) {
			struct rt_addrinfo info;

			memset(&info, 0, sizeof(info));
			info.rti_info[RTAX_DST] = (struct sockaddr *)&mltaddr;
			info.rti_info[RTAX_GATEWAY] =
			    (struct sockaddr *)&ia->ia_addr;
			info.rti_info[RTAX_NETMASK] =
			    (struct sockaddr *)&mltmask;
			info.rti_info[RTAX_IFA] =
			    (struct sockaddr *)&ia->ia_addr;
			info.rti_flags = RTF_UP | RTF_CLONING;
			error = rtrequest1(RTM_ADD, &info, NULL);
			if (error)
				goto cleanup;
#undef	MLTMASK_LEN
		} else {
			rtfree(rt);
		}
		imm = in6_joingroup(ifp, &mltaddr.sin6_addr, &error, 0);
		if (!imm) {
			nd6log((LOG_WARNING, "in6_update_ifa: "
			    "addmulti failed for %s on %s (errno=%d)\n",
			    ip6_sprintf(&mltaddr.sin6_addr),
			    if_name(ifp), error));
			goto cleanup;
		} else {
			LIST_INSERT_HEAD(&ia->ia6_memberships, imm, i6mm_chain);
		}
	}

	/*
	 * Perform DAD, if needed.
	 * XXX It may be of use, if we can administratively
	 * disable DAD.
	 */
	if (hostIsNew && if_do_dad(ifp) &&
	    ((ifra->ifra_flags & IN6_IFF_NODAD) == 0) &&
	    (ia->ia6_flags & IN6_IFF_TENTATIVE))
	{
		int mindelay, maxdelay;

		dad_delay = 0;
		if ((flags & IN6_IFAUPDATE_DADDELAY)) {
			/*
			 * We need to impose a delay before sending an NS
			 * for DAD.  Check if we also needed a delay for the
			 * corresponding MLD message.  If we did, the delay
			 * should be larger than the MLD delay (this could be
			 * relaxed a bit, but this simple logic is at least
			 * safe).
			 */
			mindelay = 0;
			if (in6m_sol != NULL &&
			    in6m_sol->in6m_state == MLD_REPORTPENDING) {
				mindelay = in6m_sol->in6m_timer;
			}
			maxdelay = MAX_RTR_SOLICITATION_DELAY * hz;
			if (maxdelay - mindelay == 0)
				dad_delay = 0;
			else {
				dad_delay =
				    (cprng_fast32() % (maxdelay - mindelay)) +
				    mindelay;
			}
		}
		/* +1 ensures callout is always used */
		nd6_dad_start(&ia->ia_ifa, dad_delay + 1);
	}

	return error;

  unlink:
	/*
	 * XXX: if a change of an existing address failed, keep the entry
	 * anyway.
	 */
	if (hostIsNew)
		in6_unlink_ifa(ia, ifp);
	return error;

  cleanup:
	in6_purgeaddr(&ia->ia_ifa);
	return error;
}

int
in6_update_ifa(struct ifnet *ifp, struct in6_aliasreq *ifra,
    struct in6_ifaddr *ia, int flags)
{
	int rc, s;

	s = splnet();
	rc = in6_update_ifa1(ifp, ifra, ia, flags);
	splx(s);
	return rc;
}

void
in6_purgeaddr(struct ifaddr *ifa)
{
	struct ifnet *ifp = ifa->ifa_ifp;
	struct in6_ifaddr *ia = (struct in6_ifaddr *) ifa;
	struct in6_multi_mship *imm;

	/* stop DAD processing */
	nd6_dad_stop(ifa);

	/*
	 * delete route to the destination of the address being purged.
	 * The interface must be p2p or loopback in this case.
	 */
	if ((ia->ia_flags & IFA_ROUTE) != 0 && ia->ia_dstaddr.sin6_len != 0) {
		int e;

		if ((e = rtinit(&(ia->ia_ifa), (int)RTM_DELETE, RTF_HOST))
		    != 0) {
			log(LOG_ERR, "in6_purgeaddr: failed to remove "
			    "a route to the p2p destination: %s on %s, "
			    "errno=%d\n",
			    ip6_sprintf(&ia->ia_addr.sin6_addr), if_name(ifp),
			    e);
			/* proceed anyway... */
		} else
			ia->ia_flags &= ~IFA_ROUTE;
	}

	/* Remove ownaddr's loopback rtentry, if it exists. */
	in6_ifremlocal(&(ia->ia_ifa));

	/*
	 * leave from multicast groups we have joined for the interface
	 */
	while ((imm = LIST_FIRST(&ia->ia6_memberships)) != NULL) {
		LIST_REMOVE(imm, i6mm_chain);
		in6_leavegroup(imm);
	}

	in6_unlink_ifa(ia, ifp);
}

static void
in6_unlink_ifa(struct in6_ifaddr *ia, struct ifnet *ifp)
{
	struct in6_ifaddr *oia;
	int	s = splnet();

	ifa_remove(ifp, &ia->ia_ifa);

	oia = ia;
	if (oia == (ia = in6_ifaddr))
		in6_ifaddr = ia->ia_next;
	else {
		while (ia->ia_next && (ia->ia_next != oia))
			ia = ia->ia_next;
		if (ia->ia_next)
			ia->ia_next = oia->ia_next;
		else {
			/* search failed */
			printf("Couldn't unlink in6_ifaddr from in6_ifaddr\n");
		}
	}

	/*
	 * XXX thorpej@NetBSD.org -- if the interface is going
	 * XXX away, don't save the multicast entries, delete them!
	 */
	if (LIST_EMPTY(&oia->ia6_multiaddrs))
		;
	else if (oia->ia_ifa.ifa_ifp->if_output == if_nulloutput) {
		struct in6_multi *in6m, *next;

		for (in6m = LIST_FIRST(&oia->ia6_multiaddrs); in6m != NULL;
		     in6m = next) {
			next = LIST_NEXT(in6m, in6m_entry);
			in6_delmulti(in6m);
		}
	} else
		in6_savemkludge(oia);

	/*
	 * Release the reference to the base prefix.  There should be a
	 * positive reference.
	 */
	if (oia->ia6_ndpr == NULL) {
		nd6log((LOG_NOTICE, "in6_unlink_ifa: autoconf'ed address "
		    "%p has no prefix\n", oia));
	} else {
		oia->ia6_ndpr->ndpr_refcnt--;
		oia->ia6_ndpr = NULL;
	}

	/*
	 * Also, if the address being removed is autoconf'ed, call
	 * pfxlist_onlink_check() since the release might affect the status of
	 * other (detached) addresses.
	 */
	if ((oia->ia6_flags & IN6_IFF_AUTOCONF) != 0)
		pfxlist_onlink_check();

	/*
	 * release another refcnt for the link from in6_ifaddr.
	 * Note that we should decrement the refcnt at least once for all *BSD.
	 */
	ifafree(&oia->ia_ifa);

	splx(s);
}

void
in6_purgeif(struct ifnet *ifp)
{
	if_purgeaddrs(ifp, AF_INET6, in6_purgeaddr);

	in6_ifdetach(ifp);
}

/*
 * SIOC[GAD]LIFADDR.
 *	SIOCGLIFADDR: get first address. (?)
 *	SIOCGLIFADDR with IFLR_PREFIX:
 *		get first address that matches the specified prefix.
 *	SIOCALIFADDR: add the specified address.
 *	SIOCALIFADDR with IFLR_PREFIX:
 *		add the specified prefix, filling hostid part from
 *		the first link-local address.  prefixlen must be <= 64.
 *	SIOCDLIFADDR: delete the specified address.
 *	SIOCDLIFADDR with IFLR_PREFIX:
 *		delete the first address that matches the specified prefix.
 * return values:
 *	EINVAL on invalid parameters
 *	EADDRNOTAVAIL on prefix match failed/specified address not found
 *	other values may be returned from in6_ioctl()
 *
 * NOTE: SIOCALIFADDR(with IFLR_PREFIX set) allows prefixlen less than 64.
 * this is to accommodate address naming scheme other than RFC2374,
 * in the future.
 * RFC2373 defines interface id to be 64bit, but it allows non-RFC2374
 * address encoding scheme. (see figure on page 8)
 */
static int
in6_lifaddr_ioctl(struct socket *so, u_long cmd, void *data, 
	struct ifnet *ifp)
{
	struct in6_ifaddr *ia;
	struct if_laddrreq *iflr = (struct if_laddrreq *)data;
	struct ifaddr *ifa;
	struct sockaddr *sa;

	/* sanity checks */
	if (!data || !ifp) {
		panic("invalid argument to in6_lifaddr_ioctl");
		/* NOTREACHED */
	}

	switch (cmd) {
	case SIOCGLIFADDR:
		/* address must be specified on GET with IFLR_PREFIX */
		if ((iflr->flags & IFLR_PREFIX) == 0)
			break;
		/* FALLTHROUGH */
	case SIOCALIFADDR:
	case SIOCDLIFADDR:
		/* address must be specified on ADD and DELETE */
		sa = (struct sockaddr *)&iflr->addr;
		if (sa->sa_family != AF_INET6)
			return EINVAL;
		if (sa->sa_len != sizeof(struct sockaddr_in6))
			return EINVAL;
		/* XXX need improvement */
		sa = (struct sockaddr *)&iflr->dstaddr;
		if (sa->sa_family && sa->sa_family != AF_INET6)
			return EINVAL;
		if (sa->sa_len && sa->sa_len != sizeof(struct sockaddr_in6))
			return EINVAL;
		break;
	default: /* shouldn't happen */
#if 0
		panic("invalid cmd to in6_lifaddr_ioctl");
		/* NOTREACHED */
#else
		return EOPNOTSUPP;
#endif
	}
	if (sizeof(struct in6_addr) * NBBY < iflr->prefixlen)
		return EINVAL;

	switch (cmd) {
	case SIOCALIFADDR:
	    {
		struct in6_aliasreq ifra;
		struct in6_addr *xhostid = NULL;
		int prefixlen;

		if ((iflr->flags & IFLR_PREFIX) != 0) {
			struct sockaddr_in6 *sin6;

			/*
			 * xhostid is to fill in the hostid part of the
			 * address.  xhostid points to the first link-local
			 * address attached to the interface.
			 */
			ia = in6ifa_ifpforlinklocal(ifp, 0);
			if (ia == NULL)
				return EADDRNOTAVAIL;
			xhostid = IFA_IN6(&ia->ia_ifa);

		 	/* prefixlen must be <= 64. */
			if (64 < iflr->prefixlen)
				return EINVAL;
			prefixlen = iflr->prefixlen;

			/* hostid part must be zero. */
			sin6 = (struct sockaddr_in6 *)&iflr->addr;
			if (sin6->sin6_addr.s6_addr32[2] != 0
			 || sin6->sin6_addr.s6_addr32[3] != 0) {
				return EINVAL;
			}
		} else
			prefixlen = iflr->prefixlen;

		/* copy args to in6_aliasreq, perform ioctl(SIOCAIFADDR_IN6). */
		memset(&ifra, 0, sizeof(ifra));
		memcpy(ifra.ifra_name, iflr->iflr_name, sizeof(ifra.ifra_name));

		memcpy(&ifra.ifra_addr, &iflr->addr,
		    ((struct sockaddr *)&iflr->addr)->sa_len);
		if (xhostid) {
			/* fill in hostid part */
			ifra.ifra_addr.sin6_addr.s6_addr32[2] =
			    xhostid->s6_addr32[2];
			ifra.ifra_addr.sin6_addr.s6_addr32[3] =
			    xhostid->s6_addr32[3];
		}

		if (((struct sockaddr *)&iflr->dstaddr)->sa_family) { /* XXX */
			memcpy(&ifra.ifra_dstaddr, &iflr->dstaddr,
			    ((struct sockaddr *)&iflr->dstaddr)->sa_len);
			if (xhostid) {
				ifra.ifra_dstaddr.sin6_addr.s6_addr32[2] =
				    xhostid->s6_addr32[2];
				ifra.ifra_dstaddr.sin6_addr.s6_addr32[3] =
				    xhostid->s6_addr32[3];
			}
		}

		ifra.ifra_prefixmask.sin6_len = sizeof(struct sockaddr_in6);
		in6_prefixlen2mask(&ifra.ifra_prefixmask.sin6_addr, prefixlen);

		ifra.ifra_lifetime.ia6t_vltime = ND6_INFINITE_LIFETIME;
		ifra.ifra_lifetime.ia6t_pltime = ND6_INFINITE_LIFETIME;
		ifra.ifra_flags = iflr->flags & ~IFLR_PREFIX;
		return in6_control(so, SIOCAIFADDR_IN6, &ifra, ifp);
	    }
	case SIOCGLIFADDR:
	case SIOCDLIFADDR:
	    {
		struct in6_addr mask, candidate, match;
		struct sockaddr_in6 *sin6;
		int cmp;

		memset(&mask, 0, sizeof(mask));
		if (iflr->flags & IFLR_PREFIX) {
			/* lookup a prefix rather than address. */
			in6_prefixlen2mask(&mask, iflr->prefixlen);

			sin6 = (struct sockaddr_in6 *)&iflr->addr;
			memcpy(&match, &sin6->sin6_addr, sizeof(match));
			match.s6_addr32[0] &= mask.s6_addr32[0];
			match.s6_addr32[1] &= mask.s6_addr32[1];
			match.s6_addr32[2] &= mask.s6_addr32[2];
			match.s6_addr32[3] &= mask.s6_addr32[3];

			/* if you set extra bits, that's wrong */
			if (memcmp(&match, &sin6->sin6_addr, sizeof(match)))
				return EINVAL;

			cmp = 1;
		} else {
			if (cmd == SIOCGLIFADDR) {
				/* on getting an address, take the 1st match */
				cmp = 0;	/* XXX */
			} else {
				/* on deleting an address, do exact match */
				in6_prefixlen2mask(&mask, 128);
				sin6 = (struct sockaddr_in6 *)&iflr->addr;
				memcpy(&match, &sin6->sin6_addr, sizeof(match));

				cmp = 1;
			}
		}

		IFADDR_FOREACH(ifa, ifp) {
			if (ifa->ifa_addr->sa_family != AF_INET6)
				continue;
			if (!cmp)
				break;

			/*
			 * XXX: this is adhoc, but is necessary to allow
			 * a user to specify fe80::/64 (not /10) for a
			 * link-local address.
			 */
			memcpy(&candidate, IFA_IN6(ifa), sizeof(candidate));
			in6_clearscope(&candidate);
			candidate.s6_addr32[0] &= mask.s6_addr32[0];
			candidate.s6_addr32[1] &= mask.s6_addr32[1];
			candidate.s6_addr32[2] &= mask.s6_addr32[2];
			candidate.s6_addr32[3] &= mask.s6_addr32[3];
			if (IN6_ARE_ADDR_EQUAL(&candidate, &match))
				break;
		}
		if (!ifa)
			return EADDRNOTAVAIL;
		ia = ifa2ia6(ifa);

		if (cmd == SIOCGLIFADDR) {
			int error;

			/* fill in the if_laddrreq structure */
			memcpy(&iflr->addr, &ia->ia_addr, ia->ia_addr.sin6_len);
			error = sa6_recoverscope(
			    (struct sockaddr_in6 *)&iflr->addr);
			if (error != 0)
				return error;

			if ((ifp->if_flags & IFF_POINTOPOINT) != 0) {
				memcpy(&iflr->dstaddr, &ia->ia_dstaddr,
				    ia->ia_dstaddr.sin6_len);
				error = sa6_recoverscope(
				    (struct sockaddr_in6 *)&iflr->dstaddr);
				if (error != 0)
					return error;
			} else
				memset(&iflr->dstaddr, 0, sizeof(iflr->dstaddr));

			iflr->prefixlen =
			    in6_mask2len(&ia->ia_prefixmask.sin6_addr, NULL);

			iflr->flags = ia->ia6_flags;	/* XXX */

			return 0;
		} else {
			struct in6_aliasreq ifra;

			/* fill in6_aliasreq and do ioctl(SIOCDIFADDR_IN6) */
			memset(&ifra, 0, sizeof(ifra));
			memcpy(ifra.ifra_name, iflr->iflr_name,
			    sizeof(ifra.ifra_name));

			memcpy(&ifra.ifra_addr, &ia->ia_addr,
			    ia->ia_addr.sin6_len);
			if ((ifp->if_flags & IFF_POINTOPOINT) != 0) {
				memcpy(&ifra.ifra_dstaddr, &ia->ia_dstaddr,
				    ia->ia_dstaddr.sin6_len);
			} else {
				memset(&ifra.ifra_dstaddr, 0,
				    sizeof(ifra.ifra_dstaddr));
			}
			memcpy(&ifra.ifra_dstaddr, &ia->ia_prefixmask,
			    ia->ia_prefixmask.sin6_len);

			ifra.ifra_flags = ia->ia6_flags;
			return in6_control(so, SIOCDIFADDR_IN6, &ifra, ifp);
		}
	    }
	}

	return EOPNOTSUPP;	/* just for safety */
}

/*
 * Initialize an interface's internet6 address
 * and routing table entry.
 */
static int
in6_ifinit(struct ifnet *ifp, struct in6_ifaddr *ia, 
	const struct sockaddr_in6 *sin6, int newhost)
{
	int	error = 0, plen, ifacount = 0;
	int	s = splnet();
	struct ifaddr *ifa;

	/*
	 * Give the interface a chance to initialize
	 * if this is its first address,
	 * and to validate the address if necessary.
	 */
	IFADDR_FOREACH(ifa, ifp) {
		if (ifa->ifa_addr == NULL)
			continue;	/* just for safety */
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		ifacount++;
	}

	ia->ia_addr = *sin6;

	if (ifacount <= 1 &&
	    (error = if_addr_init(ifp, &ia->ia_ifa, true)) != 0) {
		splx(s);
		return error;
	}
	splx(s);

	ia->ia_ifa.ifa_metric = ifp->if_metric;

	/* we could do in(6)_socktrim here, but just omit it at this moment. */

	/*
	 * Special case:
	 * If the destination address is specified for a point-to-point
	 * interface, install a route to the destination as an interface
	 * direct route.
	 */
	plen = in6_mask2len(&ia->ia_prefixmask.sin6_addr, NULL); /* XXX */
	if (plen == 128 && ia->ia_dstaddr.sin6_family == AF_INET6) {
		if ((error = rtinit(&ia->ia_ifa, RTM_ADD,
				    RTF_UP | RTF_HOST)) != 0)
			return error;
		ia->ia_flags |= IFA_ROUTE;
	}

	/* Add ownaddr as loopback rtentry, if necessary (ex. on p2p link). */
	if (newhost) {
		/* set the rtrequest function to create llinfo */
		if (ifp->if_flags & IFF_POINTOPOINT)
			ia->ia_ifa.ifa_rtrequest = p2p_rtrequest;
		else if ((ifp->if_flags & IFF_LOOPBACK) == 0)
			ia->ia_ifa.ifa_rtrequest = nd6_rtrequest;
		in6_ifaddlocal(&ia->ia_ifa);
	} else {
		/* Inform the routing socket of new flags/timings */
		rt_newaddrmsg(RTM_NEWADDR, &ia->ia_ifa, 0, NULL);
	}

	if (ifp->if_flags & IFF_MULTICAST)
		in6_restoremkludge(ia, ifp);

	return error;
}

static struct ifaddr *
bestifa(struct ifaddr *best_ifa, struct ifaddr *ifa)
{
	if (best_ifa == NULL || best_ifa->ifa_preference < ifa->ifa_preference)
		return ifa;
	return best_ifa;
}

/*
 * Find an IPv6 interface link-local address specific to an interface.
 */
struct in6_ifaddr *
in6ifa_ifpforlinklocal(const struct ifnet *ifp, const int ignoreflags)
{
	struct ifaddr *best_ifa = NULL, *ifa;

	IFADDR_FOREACH(ifa, ifp) {
		if (ifa->ifa_addr == NULL)
			continue;	/* just for safety */
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		if (!IN6_IS_ADDR_LINKLOCAL(IFA_IN6(ifa)))
			continue;
		if ((((struct in6_ifaddr *)ifa)->ia6_flags & ignoreflags) != 0)
			continue;
		best_ifa = bestifa(best_ifa, ifa);
	}

	return (struct in6_ifaddr *)best_ifa;
}


/*
 * find the internet address corresponding to a given interface and address.
 */
struct in6_ifaddr *
in6ifa_ifpwithaddr(const struct ifnet *ifp, const struct in6_addr *addr)
{
	struct ifaddr *best_ifa = NULL, *ifa;

	IFADDR_FOREACH(ifa, ifp) {
		if (ifa->ifa_addr == NULL)
			continue;	/* just for safety */
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		if (!IN6_ARE_ADDR_EQUAL(addr, IFA_IN6(ifa)))
			continue;
		best_ifa = bestifa(best_ifa, ifa);
	}

	return (struct in6_ifaddr *)best_ifa;
}

static struct in6_ifaddr *
bestia(struct in6_ifaddr *best_ia, struct in6_ifaddr *ia)
{
	if (best_ia == NULL ||
	    best_ia->ia_ifa.ifa_preference < ia->ia_ifa.ifa_preference)
		return ia;
	return best_ia;
}

/*
 * Convert IP6 address to printable (loggable) representation.
 */
char *
ip6_sprintf(const struct in6_addr *addr)
{
	static int ip6round = 0;
	static char ip6buf[8][INET6_ADDRSTRLEN];
	char *cp = ip6buf[ip6round++ & 7];

	in6_print(cp, INET6_ADDRSTRLEN, addr);
	return cp;
}

/*
 * Determine if an address is on a local network.
 */
int
in6_localaddr(const struct in6_addr *in6)
{
	struct in6_ifaddr *ia;

	if (IN6_IS_ADDR_LOOPBACK(in6) || IN6_IS_ADDR_LINKLOCAL(in6))
		return 1;

	for (ia = in6_ifaddr; ia; ia = ia->ia_next)
		if (IN6_ARE_MASKED_ADDR_EQUAL(in6, &ia->ia_addr.sin6_addr,
					      &ia->ia_prefixmask.sin6_addr))
			return 1;

	return 0;
}

int
in6_is_addr_deprecated(struct sockaddr_in6 *sa6)
{
	struct in6_ifaddr *ia;

	for (ia = in6_ifaddr; ia; ia = ia->ia_next) {
		if (IN6_ARE_ADDR_EQUAL(&ia->ia_addr.sin6_addr,
		    &sa6->sin6_addr) &&
#ifdef SCOPEDROUTING
		    ia->ia_addr.sin6_scope_id == sa6->sin6_scope_id &&
#endif
		    (ia->ia6_flags & IN6_IFF_DEPRECATED) != 0)
			return 1; /* true */

		/* XXX: do we still have to go thru the rest of the list? */
	}

	return 0;		/* false */
}

/*
 * return length of part which dst and src are equal
 * hard coding...
 */
int
in6_matchlen(struct in6_addr *src, struct in6_addr *dst)
{
	int match = 0;
	u_char *s = (u_char *)src, *d = (u_char *)dst;
	u_char *lim = s + 16, r;

	while (s < lim)
		if ((r = (*d++ ^ *s++)) != 0) {
			while (r < 128) {
				match++;
				r <<= 1;
			}
			break;
		} else
			match += NBBY;
	return match;
}

/* XXX: to be scope conscious */
int
in6_are_prefix_equal(struct in6_addr *p1, struct in6_addr *p2, int len)
{
	int bytelen, bitlen;

	/* sanity check */
	if (len < 0 || len > 128) {
		log(LOG_ERR, "in6_are_prefix_equal: invalid prefix length(%d)\n",
		    len);
		return 0;
	}

	bytelen = len / NBBY;
	bitlen = len % NBBY;

	if (memcmp(&p1->s6_addr, &p2->s6_addr, bytelen))
		return 0;
	if (bitlen != 0 &&
	    p1->s6_addr[bytelen] >> (NBBY - bitlen) !=
	    p2->s6_addr[bytelen] >> (NBBY - bitlen))
		return 0;

	return 1;
}

void
in6_prefixlen2mask(struct in6_addr *maskp, int len)
{
	static const u_char maskarray[NBBY] = {0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff};
	int bytelen, bitlen, i;

	/* sanity check */
	if (len < 0 || len > 128) {
		log(LOG_ERR, "in6_prefixlen2mask: invalid prefix length(%d)\n",
		    len);
		return;
	}

	memset(maskp, 0, sizeof(*maskp));
	bytelen = len / NBBY;
	bitlen = len % NBBY;
	for (i = 0; i < bytelen; i++)
		maskp->s6_addr[i] = 0xff;
	if (bitlen)
		maskp->s6_addr[bytelen] = maskarray[bitlen - 1];
}

/*
 * return the best address out of the same scope. if no address was
 * found, return the first valid address from designated IF.
 */
struct in6_ifaddr *
in6_ifawithifp(struct ifnet *ifp, struct in6_addr *dst)
{
	int dst_scope =	in6_addrscope(dst), blen = -1, tlen;
	struct ifaddr *ifa;
	struct in6_ifaddr *best_ia = NULL, *ia;
	struct in6_ifaddr *dep[2];	/* last-resort: deprecated */

	dep[0] = dep[1] = NULL;

	/*
	 * We first look for addresses in the same scope.
	 * If there is one, return it.
	 * If two or more, return one which matches the dst longest.
	 * If none, return one of global addresses assigned other ifs.
	 */
	IFADDR_FOREACH(ifa, ifp) {
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		ia = (struct in6_ifaddr *)ifa;
		if (ia->ia6_flags & IN6_IFF_ANYCAST)
			continue; /* XXX: is there any case to allow anycast? */
		if (ia->ia6_flags & IN6_IFF_NOTREADY)
			continue; /* don't use this interface */
		if (ia->ia6_flags & IN6_IFF_DETACHED)
			continue;
		if (ia->ia6_flags & IN6_IFF_DEPRECATED) {
			if (ip6_use_deprecated)
				dep[0] = ia;
			continue;
		}

		if (dst_scope != in6_addrscope(IFA_IN6(ifa)))
			continue;
		/*
		 * call in6_matchlen() as few as possible
		 */
		if (best_ia == NULL) {
			best_ia = ia;
			continue;
		}
		if (blen == -1)
			blen = in6_matchlen(&best_ia->ia_addr.sin6_addr, dst);
		tlen = in6_matchlen(IFA_IN6(ifa), dst);
		if (tlen > blen) {
			blen = tlen;
			best_ia = ia;
		} else if (tlen == blen)
			best_ia = bestia(best_ia, ia);
	}
	if (best_ia != NULL)
		return best_ia;

	IFADDR_FOREACH(ifa, ifp) {
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		ia = (struct in6_ifaddr *)ifa;
		if (ia->ia6_flags & IN6_IFF_ANYCAST)
			continue; /* XXX: is there any case to allow anycast? */
		if (ia->ia6_flags & IN6_IFF_NOTREADY)
			continue; /* don't use this interface */
		if (ia->ia6_flags & IN6_IFF_DETACHED)
			continue;
		if (ia->ia6_flags & IN6_IFF_DEPRECATED) {
			if (ip6_use_deprecated)
				dep[1] = (struct in6_ifaddr *)ifa;
			continue;
		}

		best_ia = bestia(best_ia, ia);
	}
	if (best_ia != NULL)
		return best_ia;

	/* use the last-resort values, that are, deprecated addresses */
	if (dep[0])
		return dep[0];
	if (dep[1])
		return dep[1];

	return NULL;
}

/*
 * perform DAD when interface becomes IFF_UP.
 */
void
in6_if_link_up(struct ifnet *ifp)
{
	struct ifaddr *ifa;
	struct in6_ifaddr *ia;

	/* Ensure it's sane to run DAD */
	if (ifp->if_link_state == LINK_STATE_DOWN)
		return;
	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING))
		return;

	IFADDR_FOREACH(ifa, ifp) {
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		ia = (struct in6_ifaddr *)ifa;

		/* If detached then mark as tentative */
		if (ia->ia6_flags & IN6_IFF_DETACHED) {
			ia->ia6_flags &= ~IN6_IFF_DETACHED;
			if (if_do_dad(ifp)) {
				ia->ia6_flags |= IN6_IFF_TENTATIVE;
				nd6log((LOG_ERR, "in6_if_up: "
				    "%s marked tentative\n",
				    ip6_sprintf(&ia->ia_addr.sin6_addr)));
			} else if ((ia->ia6_flags & IN6_IFF_TENTATIVE) == 0)
				rt_newaddrmsg(RTM_NEWADDR, ifa, 0, NULL);
		}

		if (ia->ia6_flags & IN6_IFF_TENTATIVE) {
			int rand_delay;

			/* Clear the duplicated flag as we're starting DAD. */
			ia->ia6_flags &= ~IN6_IFF_DUPLICATED;

			/*
			 * The TENTATIVE flag was likely set by hand
			 * beforehand, implicitly indicating the need for DAD.
			 * We may be able to skip the random delay in this
			 * case, but we impose delays just in case.
			 */
			rand_delay = cprng_fast32() %
			    (MAX_RTR_SOLICITATION_DELAY * hz);
			/* +1 ensures callout is always used */
			nd6_dad_start(ifa, rand_delay + 1);
		}
	}

	/* Restore any detached prefixes */
	pfxlist_onlink_check();
}

void
in6_if_up(struct ifnet *ifp)
{

	/*
	 * special cases, like 6to4, are handled in in6_ifattach
	 */
	in6_ifattach(ifp, NULL);

	/* interface may not support link state, so bring it up also */
	in6_if_link_up(ifp);
}

/*
 * Mark all addresses as detached.
 */
void
in6_if_link_down(struct ifnet *ifp)
{
	struct ifaddr *ifa;
	struct in6_ifaddr *ia;

	/* Any prefixes on this interface should be detached as well */
	pfxlist_onlink_check();

	IFADDR_FOREACH(ifa, ifp) {
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		ia = (struct in6_ifaddr *)ifa;

		/* Stop DAD processing */
		nd6_dad_stop(ifa);

		/*
		 * Mark the address as detached.
		 * This satisfies RFC4862 Section 5.3, but we should apply
		 * this logic to all addresses to be a good citizen and
		 * avoid potential duplicated addresses.
		 * When the interface comes up again, detached addresses
		 * are marked tentative and DAD commences.
		 */
		if (!(ia->ia6_flags & IN6_IFF_DETACHED)) {
			nd6log((LOG_DEBUG, "in6_if_down: "
			    "%s marked detached\n",
			    ip6_sprintf(&ia->ia_addr.sin6_addr)));
			ia->ia6_flags |= IN6_IFF_DETACHED;
			ia->ia6_flags &=
			    ~(IN6_IFF_TENTATIVE | IN6_IFF_DUPLICATED);
			rt_newaddrmsg(RTM_NEWADDR, ifa, 0, NULL);
		}
	}
}

void
in6_if_down(struct ifnet *ifp)
{

	in6_if_link_down(ifp);
}

void
in6_if_link_state_change(struct ifnet *ifp, int link_state)
{

	switch (link_state) {
	case LINK_STATE_DOWN:
		in6_if_link_down(ifp);
		break;
	case LINK_STATE_UP:
		in6_if_link_up(ifp);
		break;
	}
}

/*
 * Calculate max IPv6 MTU through all the interfaces and store it
 * to in6_maxmtu.
 */
void
in6_setmaxmtu(void)
{
	unsigned long maxmtu = 0;
	struct ifnet *ifp;

	IFNET_FOREACH(ifp) {
		/* this function can be called during ifnet initialization */
		if (!ifp->if_afdata[AF_INET6])
			continue;
		if ((ifp->if_flags & IFF_LOOPBACK) == 0 &&
		    IN6_LINKMTU(ifp) > maxmtu)
			maxmtu = IN6_LINKMTU(ifp);
	}
	if (maxmtu)	     /* update only when maxmtu is positive */
		in6_maxmtu = maxmtu;
}

/*
 * Provide the length of interface identifiers to be used for the link attached
 * to the given interface.  The length should be defined in "IPv6 over
 * xxx-link" document.  Note that address architecture might also define
 * the length for a particular set of address prefixes, regardless of the
 * link type.  As clarified in rfc2462bis, those two definitions should be
 * consistent, and those really are as of August 2004.
 */
int
in6_if2idlen(struct ifnet *ifp)
{
	switch (ifp->if_type) {
	case IFT_ETHER:		/* RFC2464 */
	case IFT_PROPVIRTUAL:	/* XXX: no RFC. treat it as ether */
	case IFT_L2VLAN:	/* ditto */
	case IFT_IEEE80211:	/* ditto */
	case IFT_FDDI:		/* RFC2467 */
	case IFT_ISO88025:	/* RFC2470 (IPv6 over Token Ring) */
	case IFT_PPP:		/* RFC2472 */
	case IFT_ARCNET:	/* RFC2497 */
	case IFT_FRELAY:	/* RFC2590 */
	case IFT_IEEE1394:	/* RFC3146 */
	case IFT_GIF:		/* draft-ietf-v6ops-mech-v2-07 */
	case IFT_LOOP:		/* XXX: is this really correct? */
		return 64;
	default:
		/*
		 * Unknown link type:
		 * It might be controversial to use the today's common constant
		 * of 64 for these cases unconditionally.  For full compliance,
		 * we should return an error in this case.  On the other hand,
		 * if we simply miss the standard for the link type or a new
		 * standard is defined for a new link type, the IFID length
		 * is very likely to be the common constant.  As a compromise,
		 * we always use the constant, but make an explicit notice
		 * indicating the "unknown" case.
		 */
		printf("in6_if2idlen: unknown link type (%d)\n", ifp->if_type);
		return 64;
	}
}

void *
in6_domifattach(struct ifnet *ifp)
{
	struct in6_ifextra *ext;

	ext = malloc(sizeof(*ext), M_IFADDR, M_WAITOK|M_ZERO);

	ext->in6_ifstat = malloc(sizeof(struct in6_ifstat),
	    M_IFADDR, M_WAITOK|M_ZERO);

	ext->icmp6_ifstat = malloc(sizeof(struct icmp6_ifstat),
	    M_IFADDR, M_WAITOK|M_ZERO);

	ext->nd_ifinfo = nd6_ifattach(ifp);
	ext->scope6_id = scope6_ifattach(ifp);
	ext->nprefixes = 0;
	ext->ndefrouters = 0;
	return ext;
}

void
in6_domifdetach(struct ifnet *ifp, void *aux)
{
	struct in6_ifextra *ext = (struct in6_ifextra *)aux;

	nd6_ifdetach(ifp, ext);
	free(ext->in6_ifstat, M_IFADDR);
	free(ext->icmp6_ifstat, M_IFADDR);
	scope6_ifdetach(ext->scope6_id);
	free(ext, M_IFADDR);
}

/*
 * Convert sockaddr_in6 to sockaddr_in.  Original sockaddr_in6 must be
 * v4 mapped addr or v4 compat addr
 */
void
in6_sin6_2_sin(struct sockaddr_in *sin, struct sockaddr_in6 *sin6)
{
	memset(sin, 0, sizeof(*sin));
	sin->sin_len = sizeof(struct sockaddr_in);
	sin->sin_family = AF_INET;
	sin->sin_port = sin6->sin6_port;
	sin->sin_addr.s_addr = sin6->sin6_addr.s6_addr32[3];
}

/* Convert sockaddr_in to sockaddr_in6 in v4 mapped addr format. */
void
in6_sin_2_v4mapsin6(const struct sockaddr_in *sin, struct sockaddr_in6 *sin6)
{
	memset(sin6, 0, sizeof(*sin6));
	sin6->sin6_len = sizeof(struct sockaddr_in6);
	sin6->sin6_family = AF_INET6;
	sin6->sin6_port = sin->sin_port;
	sin6->sin6_addr.s6_addr32[0] = 0;
	sin6->sin6_addr.s6_addr32[1] = 0;
	sin6->sin6_addr.s6_addr32[2] = IPV6_ADDR_INT32_SMP;
	sin6->sin6_addr.s6_addr32[3] = sin->sin_addr.s_addr;
}

/* Convert sockaddr_in6 into sockaddr_in. */
void
in6_sin6_2_sin_in_sock(struct sockaddr *nam)
{
	struct sockaddr_in *sin_p;
	struct sockaddr_in6 sin6;

	/*
	 * Save original sockaddr_in6 addr and convert it
	 * to sockaddr_in.
	 */
	sin6 = *(struct sockaddr_in6 *)nam;
	sin_p = (struct sockaddr_in *)nam;
	in6_sin6_2_sin(sin_p, &sin6);
}

/* Convert sockaddr_in into sockaddr_in6 in v4 mapped addr format. */
void
in6_sin_2_v4mapsin6_in_sock(struct sockaddr **nam)
{
	struct sockaddr_in *sin_p;
	struct sockaddr_in6 *sin6_p;

	sin6_p = malloc(sizeof(*sin6_p), M_SONAME, M_WAITOK);
	sin_p = (struct sockaddr_in *)*nam;
	in6_sin_2_v4mapsin6(sin_p, sin6_p);
	free(*nam, M_SONAME);
	*nam = (struct sockaddr *)sin6_p;
}
