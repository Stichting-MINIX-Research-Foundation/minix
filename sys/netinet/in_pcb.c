/*	$NetBSD: in_pcb.c,v 1.162 2015/08/24 22:21:26 pooka Exp $	*/

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

/*-
 * Copyright (c) 1998, 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Coyote Point Systems, Inc.
 * This code is derived from software contributed to The NetBSD Foundation
 * by Public Access Networks Corporation ("Panix").  It was developed under
 * contract to Panix by Eric Haszlakiewicz and Thor Lancelot Simon.
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
 * Copyright (c) 1982, 1986, 1991, 1993, 1995
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
 *	@(#)in_pcb.c	8.4 (Berkeley) 5/24/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: in_pcb.c,v 1.162 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#include "opt_ipsec.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/once.h>
#include <sys/pool.h>
#include <sys/proc.h>
#include <sys/kauth.h>
#include <sys/uidinfo.h>
#include <sys/domain.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet/portalgo.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_pcb.h>
#endif

#ifdef IPSEC
#include <netipsec/ipsec.h>
#include <netipsec/key.h>
#endif /* IPSEC */

#include <netinet/tcp_vtw.h>

struct	in_addr zeroin_addr;

#define	INPCBHASH_PORT(table, lport) \
	&(table)->inpt_porthashtbl[ntohs(lport) & (table)->inpt_porthash]
#define	INPCBHASH_BIND(table, laddr, lport) \
	&(table)->inpt_bindhashtbl[ \
	    ((ntohl((laddr).s_addr) + ntohs(lport))) & (table)->inpt_bindhash]
#define	INPCBHASH_CONNECT(table, faddr, fport, laddr, lport) \
	&(table)->inpt_connecthashtbl[ \
	    ((ntohl((faddr).s_addr) + ntohs(fport)) + \
	     (ntohl((laddr).s_addr) + ntohs(lport))) & (table)->inpt_connecthash]

int	anonportmin = IPPORT_ANONMIN;
int	anonportmax = IPPORT_ANONMAX;
int	lowportmin  = IPPORT_RESERVEDMIN;
int	lowportmax  = IPPORT_RESERVEDMAX;

static struct pool inpcb_pool;

static int
inpcb_poolinit(void)
{

	pool_init(&inpcb_pool, sizeof(struct inpcb), 0, 0, 0, "inpcbpl", NULL,
	    IPL_NET);
	return 0;
}

void
in_pcbinit(struct inpcbtable *table, int bindhashsize, int connecthashsize)
{
	static ONCE_DECL(control);

	TAILQ_INIT(&table->inpt_queue);
	table->inpt_porthashtbl = hashinit(bindhashsize, HASH_LIST, true,
	    &table->inpt_porthash);
	table->inpt_bindhashtbl = hashinit(bindhashsize, HASH_LIST, true,
	    &table->inpt_bindhash);
	table->inpt_connecthashtbl = hashinit(connecthashsize, HASH_LIST, true,
	    &table->inpt_connecthash);
	table->inpt_lastlow = IPPORT_RESERVEDMAX;
	table->inpt_lastport = (u_int16_t)anonportmax;

	RUN_ONCE(&control, inpcb_poolinit);
}

int
in_pcballoc(struct socket *so, void *v)
{
	struct inpcbtable *table = v;
	struct inpcb *inp;
	int s;

	s = splnet();
	inp = pool_get(&inpcb_pool, PR_NOWAIT);
	splx(s);
	if (inp == NULL)
		return (ENOBUFS);
	memset(inp, 0, sizeof(*inp));
	inp->inp_af = AF_INET;
	inp->inp_table = table;
	inp->inp_socket = so;
	inp->inp_errormtu = -1;
	inp->inp_portalgo = PORTALGO_DEFAULT;
	inp->inp_bindportonsend = false;
#if defined(IPSEC)
	if (ipsec_enabled) {
		int error = ipsec_init_pcbpolicy(so, &inp->inp_sp);
		if (error != 0) {
			s = splnet();
			pool_put(&inpcb_pool, inp);
			splx(s);
			return error;
		}
	}
#endif
	so->so_pcb = inp;
	s = splnet();
	TAILQ_INSERT_HEAD(&table->inpt_queue, &inp->inp_head, inph_queue);
	LIST_INSERT_HEAD(INPCBHASH_PORT(table, inp->inp_lport), &inp->inp_head,
	    inph_lhash);
	in_pcbstate(inp, INP_ATTACHED);
	splx(s);
	return (0);
}

static int
in_pcbsetport(struct sockaddr_in *sin, struct inpcb *inp, kauth_cred_t cred)
{
	struct inpcbtable *table = inp->inp_table;
	struct socket *so = inp->inp_socket;
	u_int16_t *lastport;
	u_int16_t lport = 0;
	enum kauth_network_req req;
	int error;

	if (inp->inp_flags & INP_LOWPORT) {
#ifndef IPNOPRIVPORTS
		req = KAUTH_REQ_NETWORK_BIND_PRIVPORT;
#else
		req = KAUTH_REQ_NETWORK_BIND_PORT;
#endif

		lastport = &table->inpt_lastlow;
	} else {
		req = KAUTH_REQ_NETWORK_BIND_PORT;

		lastport = &table->inpt_lastport;
	}

	/* XXX-kauth: KAUTH_REQ_NETWORK_BIND_AUTOASSIGN_{,PRIV}PORT */
	error = kauth_authorize_network(cred, KAUTH_NETWORK_BIND, req, so, sin,
	    NULL);
	if (error)
		return (EACCES);

       /*
        * Use RFC6056 randomized port selection
        */
	error = portalgo_randport(&lport, &inp->inp_head, cred);
	if (error)
		return error;

	inp->inp_flags |= INP_ANONPORT;
	*lastport = lport;
	lport = htons(lport);
	inp->inp_lport = lport;
	in_pcbstate(inp, INP_BOUND);

	return (0);
}

static int
in_pcbbind_addr(struct inpcb *inp, struct sockaddr_in *sin, kauth_cred_t cred)
{
	if (sin->sin_family != AF_INET)
		return (EAFNOSUPPORT);

	if (IN_MULTICAST(sin->sin_addr.s_addr)) {
		/* Always succeed; port reuse handled in in_pcbbind_port(). */
	} else if (!in_nullhost(sin->sin_addr)) {
		struct in_ifaddr *ia = NULL;

		INADDR_TO_IA(sin->sin_addr, ia);
		/* check for broadcast addresses */
		if (ia == NULL)
			ia = ifatoia(ifa_ifwithaddr(sintosa(sin)));
		if (ia == NULL)
			return (EADDRNOTAVAIL);
		if (ia->ia4_flags & (IN_IFF_NOTREADY | IN_IFF_DETACHED))
			return (EADDRNOTAVAIL);
	}

	inp->inp_laddr = sin->sin_addr;

	return (0);
}

static int
in_pcbbind_port(struct inpcb *inp, struct sockaddr_in *sin, kauth_cred_t cred)
{
	struct inpcbtable *table = inp->inp_table;
	struct socket *so = inp->inp_socket;
	int reuseport = (so->so_options & SO_REUSEPORT);
	int wild = 0, error;

	if (IN_MULTICAST(sin->sin_addr.s_addr)) {
		/*
		 * Treat SO_REUSEADDR as SO_REUSEPORT for multicast;
		 * allow complete duplication of binding if
		 * SO_REUSEPORT is set, or if SO_REUSEADDR is set
		 * and a multicast address is bound on both
		 * new and duplicated sockets.
		 */
		if (so->so_options & (SO_REUSEADDR | SO_REUSEPORT))
			reuseport = SO_REUSEADDR|SO_REUSEPORT;
	} 

	if (sin->sin_port == 0) {
		error = in_pcbsetport(sin, inp, cred);
		if (error)
			return (error);
	} else {
		struct inpcb *t;
		vestigial_inpcb_t vestige;
#ifdef INET6
		struct in6pcb *t6;
		struct in6_addr mapped;
#endif
		enum kauth_network_req req;

		if ((so->so_options & (SO_REUSEADDR|SO_REUSEPORT)) == 0)
			wild = 1;

#ifndef IPNOPRIVPORTS
		if (ntohs(sin->sin_port) < IPPORT_RESERVED)
			req = KAUTH_REQ_NETWORK_BIND_PRIVPORT;
		else
#endif /* !IPNOPRIVPORTS */
			req = KAUTH_REQ_NETWORK_BIND_PORT;

		error = kauth_authorize_network(cred, KAUTH_NETWORK_BIND, req,
		    so, sin, NULL);
		if (error)
			return (EACCES);

#ifdef INET6
		memset(&mapped, 0, sizeof(mapped));
		mapped.s6_addr16[5] = 0xffff;
		memcpy(&mapped.s6_addr32[3], &sin->sin_addr,
		    sizeof(mapped.s6_addr32[3]));
		t6 = in6_pcblookup_port(table, &mapped, sin->sin_port, wild, &vestige);
		if (t6 && (reuseport & t6->in6p_socket->so_options) == 0)
			return (EADDRINUSE);
		if (!t6 && vestige.valid) {
		    if (!!reuseport != !!vestige.reuse_port) {
			return EADDRINUSE;
		    }
		}
#endif

		/* XXX-kauth */
		if (so->so_uidinfo->ui_uid && !IN_MULTICAST(sin->sin_addr.s_addr)) {
			t = in_pcblookup_port(table, sin->sin_addr, sin->sin_port, 1, &vestige);
			/*
			 * XXX:	investigate ramifications of loosening this
			 *	restriction so that as long as both ports have
			 *	SO_REUSEPORT allow the bind
			 */
			if (t &&
			    (!in_nullhost(sin->sin_addr) ||
			     !in_nullhost(t->inp_laddr) ||
			     (t->inp_socket->so_options & SO_REUSEPORT) == 0)
			    && (so->so_uidinfo->ui_uid != t->inp_socket->so_uidinfo->ui_uid)) {
				return (EADDRINUSE);
			}
			if (!t && vestige.valid) {
				if ((!in_nullhost(sin->sin_addr)
				     || !in_nullhost(vestige.laddr.v4)
				     || !vestige.reuse_port)
				    && so->so_uidinfo->ui_uid != vestige.uid) {
					return EADDRINUSE;
				}
			}
		}
		t = in_pcblookup_port(table, sin->sin_addr, sin->sin_port, wild, &vestige);
		if (t && (reuseport & t->inp_socket->so_options) == 0)
			return (EADDRINUSE);
		if (!t
		    && vestige.valid
		    && !(reuseport && vestige.reuse_port))
			return EADDRINUSE;

		inp->inp_lport = sin->sin_port;
		in_pcbstate(inp, INP_BOUND);
	}

	LIST_REMOVE(&inp->inp_head, inph_lhash);
	LIST_INSERT_HEAD(INPCBHASH_PORT(table, inp->inp_lport), &inp->inp_head,
	    inph_lhash);

	return (0);
}

int
in_pcbbind(void *v, struct sockaddr_in *sin, struct lwp *l)
{
	struct inpcb *inp = v;
	struct sockaddr_in lsin;
	int error;

	if (inp->inp_af != AF_INET)
		return (EINVAL);

	if (TAILQ_FIRST(&in_ifaddrhead) == 0)
		return (EADDRNOTAVAIL);
	if (inp->inp_lport || !in_nullhost(inp->inp_laddr))
		return (EINVAL);

	if (NULL != sin) {
		if (sin->sin_len != sizeof(*sin))
			return (EINVAL);
	} else {
		lsin = *((const struct sockaddr_in *)
		    inp->inp_socket->so_proto->pr_domain->dom_sa_any);
		sin = &lsin;
	}

	/* Bind address. */
	error = in_pcbbind_addr(inp, sin, l->l_cred);
	if (error)
		return (error);

	/* Bind port. */
	error = in_pcbbind_port(inp, sin, l->l_cred);
	if (error) {
		inp->inp_laddr.s_addr = INADDR_ANY;

		return (error);
	}

	return (0);
}

/*
 * Connect from a socket to a specified address.
 * Both address and port must be specified in argument sin.
 * If don't have a local address for this socket yet,
 * then pick one.
 */
int
in_pcbconnect(void *v, struct sockaddr_in *sin, struct lwp *l)
{
	struct inpcb *inp = v;
	struct in_ifaddr *ia = NULL;
	struct sockaddr_in *ifaddr = NULL;
	vestigial_inpcb_t vestige;
	int error;

	if (inp->inp_af != AF_INET)
		return (EINVAL);

	if (sin->sin_len != sizeof (*sin))
		return (EINVAL);
	if (sin->sin_family != AF_INET)
		return (EAFNOSUPPORT);
	if (sin->sin_port == 0)
		return (EADDRNOTAVAIL);

	if (IN_MULTICAST(sin->sin_addr.s_addr) &&
	    inp->inp_socket->so_type == SOCK_STREAM)
		return EADDRNOTAVAIL;

	if (TAILQ_FIRST(&in_ifaddrhead) != 0) {
		/*
		 * If the destination address is INADDR_ANY,
		 * use any local address (likely loopback).
		 * If the supplied address is INADDR_BROADCAST,
		 * use the broadcast address of an interface
		 * which supports broadcast. (loopback does not)
		 */

		if (in_nullhost(sin->sin_addr)) {
			sin->sin_addr =
			    TAILQ_FIRST(&in_ifaddrhead)->ia_addr.sin_addr;
		} else if (sin->sin_addr.s_addr == INADDR_BROADCAST) {
			TAILQ_FOREACH(ia, &in_ifaddrhead, ia_list) {
				if (ia->ia_ifp->if_flags & IFF_BROADCAST) {
					sin->sin_addr =
					    ia->ia_broadaddr.sin_addr;
					break;
				}
			}
		}
	}
	/*
	 * If we haven't bound which network number to use as ours,
	 * we will use the number of the outgoing interface.
	 * This depends on having done a routing lookup, which
	 * we will probably have to do anyway, so we might
	 * as well do it now.  On the other hand if we are
	 * sending to multiple destinations we may have already
	 * done the lookup, so see if we can use the route
	 * from before.  In any case, we only
	 * chose a port number once, even if sending to multiple
	 * destinations.
	 */
	if (in_nullhost(inp->inp_laddr)) {
		int xerror;
		ifaddr = in_selectsrc(sin, &inp->inp_route,
		    inp->inp_socket->so_options, inp->inp_moptions, &xerror);
		if (ifaddr == NULL) {
			if (xerror == 0)
				xerror = EADDRNOTAVAIL;
			return xerror;
		}
		INADDR_TO_IA(ifaddr->sin_addr, ia);
		if (ia == NULL)
			return (EADDRNOTAVAIL);
	}
	if (in_pcblookup_connect(inp->inp_table, sin->sin_addr, sin->sin_port,
	    !in_nullhost(inp->inp_laddr) ? inp->inp_laddr : ifaddr->sin_addr,
				 inp->inp_lport, &vestige) != 0
	    || vestige.valid)
		return (EADDRINUSE);
	if (in_nullhost(inp->inp_laddr)) {
		if (inp->inp_lport == 0) {
			error = in_pcbbind(inp, NULL, l);
			/*
			 * This used to ignore the return value
			 * completely, but we need to check for
			 * ephemeral port shortage.
			 * And attempts to request low ports if not root.
			 */
			if (error != 0)
				return (error);
		}
		inp->inp_laddr = ifaddr->sin_addr;
	}
	inp->inp_faddr = sin->sin_addr;
	inp->inp_fport = sin->sin_port;

        /* Late bind, if needed */
	if (inp->inp_bindportonsend) {
               struct sockaddr_in lsin = *((const struct sockaddr_in *)
		    inp->inp_socket->so_proto->pr_domain->dom_sa_any);
		lsin.sin_addr = inp->inp_laddr;
		lsin.sin_port = 0;

               if ((error = in_pcbbind_port(inp, &lsin, l->l_cred)) != 0)
                       return error;
	}

	in_pcbstate(inp, INP_CONNECTED);
#if defined(IPSEC)
	if (ipsec_enabled && inp->inp_socket->so_type == SOCK_STREAM)
		ipsec_pcbconn(inp->inp_sp);
#endif
	return (0);
}

void
in_pcbdisconnect(void *v)
{
	struct inpcb *inp = v;

	if (inp->inp_af != AF_INET)
		return;

	inp->inp_faddr = zeroin_addr;
	inp->inp_fport = 0;
	in_pcbstate(inp, INP_BOUND);
#if defined(IPSEC)
	if (ipsec_enabled)
		ipsec_pcbdisconn(inp->inp_sp);
#endif
	if (inp->inp_socket->so_state & SS_NOFDREF)
		in_pcbdetach(inp);
}

void
in_pcbdetach(void *v)
{
	struct inpcb *inp = v;
	struct socket *so = inp->inp_socket;
	int s;

	if (inp->inp_af != AF_INET)
		return;

#if defined(IPSEC)
	if (ipsec_enabled)
		ipsec4_delete_pcbpolicy(inp);
#endif
	so->so_pcb = NULL;

	s = splnet();
	in_pcbstate(inp, INP_ATTACHED);
	LIST_REMOVE(&inp->inp_head, inph_lhash);
	TAILQ_REMOVE(&inp->inp_table->inpt_queue, &inp->inp_head, inph_queue);
	splx(s);

	if (inp->inp_options) {
		m_free(inp->inp_options);
	}
	rtcache_free(&inp->inp_route);
	ip_freemoptions(inp->inp_moptions);
	sofree(so);			/* drops the socket's lock */

	pool_put(&inpcb_pool, inp);
	mutex_enter(softnet_lock);	/* reacquire the softnet_lock */
}

void
in_setsockaddr(struct inpcb *inp, struct sockaddr_in *sin)
{

	if (inp->inp_af != AF_INET)
		return;

	sockaddr_in_init(sin, &inp->inp_laddr, inp->inp_lport);
}

void
in_setpeeraddr(struct inpcb *inp, struct sockaddr_in *sin)
{

	if (inp->inp_af != AF_INET)
		return;

	sockaddr_in_init(sin, &inp->inp_faddr, inp->inp_fport);
}

/*
 * Pass some notification to all connections of a protocol
 * associated with address dst.  The local address and/or port numbers
 * may be specified to limit the search.  The "usual action" will be
 * taken, depending on the ctlinput cmd.  The caller must filter any
 * cmds that are uninteresting (e.g., no error in the map).
 * Call the protocol specific routine (if any) to report
 * any errors for each matching socket.
 *
 * Must be called at splsoftnet.
 */
int
in_pcbnotify(struct inpcbtable *table, struct in_addr faddr, u_int fport_arg,
    struct in_addr laddr, u_int lport_arg, int errno,
    void (*notify)(struct inpcb *, int))
{
	struct inpcbhead *head;
	struct inpcb *inp, *ninp;
	u_int16_t fport = fport_arg, lport = lport_arg;
	int nmatch;

	if (in_nullhost(faddr) || notify == 0)
		return (0);

	nmatch = 0;
	head = INPCBHASH_CONNECT(table, faddr, fport, laddr, lport);
	for (inp = (struct inpcb *)LIST_FIRST(head); inp != NULL; inp = ninp) {
		ninp = (struct inpcb *)LIST_NEXT(inp, inp_hash);
		if (inp->inp_af != AF_INET)
			continue;
		if (in_hosteq(inp->inp_faddr, faddr) &&
		    inp->inp_fport == fport &&
		    inp->inp_lport == lport &&
		    in_hosteq(inp->inp_laddr, laddr)) {
			(*notify)(inp, errno);
			nmatch++;
		}
	}
	return (nmatch);
}

void
in_pcbnotifyall(struct inpcbtable *table, struct in_addr faddr, int errno,
    void (*notify)(struct inpcb *, int))
{
	struct inpcb_hdr *inph, *ninph;

	if (in_nullhost(faddr) || notify == 0)
		return;

	TAILQ_FOREACH_SAFE(inph, &table->inpt_queue, inph_queue, ninph) {
		struct inpcb *inp = (struct inpcb *)inph;
		if (inp->inp_af != AF_INET)
			continue;
		if (in_hosteq(inp->inp_faddr, faddr))
			(*notify)(inp, errno);
	}
}

void
in_purgeifmcast(struct ip_moptions *imo, struct ifnet *ifp)
{
	int i, gap;

	if (imo == NULL)
		return;

	/*
	 * Unselect the outgoing interface if it is being
	 * detached.
	 */
	if (imo->imo_multicast_ifp == ifp)
		imo->imo_multicast_ifp = NULL;

	/*
	 * Drop multicast group membership if we joined
	 * through the interface being detached.
	 */
	for (i = 0, gap = 0; i < imo->imo_num_memberships; i++) {
		if (imo->imo_membership[i]->inm_ifp == ifp) {
			in_delmulti(imo->imo_membership[i]);
			gap++;
		} else if (gap != 0)
			imo->imo_membership[i - gap] = imo->imo_membership[i];
	}
	imo->imo_num_memberships -= gap;
}

void
in_pcbpurgeif0(struct inpcbtable *table, struct ifnet *ifp)
{
	struct inpcb_hdr *inph, *ninph;

	TAILQ_FOREACH_SAFE(inph, &table->inpt_queue, inph_queue, ninph) {
		struct inpcb *inp = (struct inpcb *)inph;
		if (inp->inp_af != AF_INET)
			continue;
		in_purgeifmcast(inp->inp_moptions, ifp);
	}
}

void
in_pcbpurgeif(struct inpcbtable *table, struct ifnet *ifp)
{
	struct rtentry *rt;
	struct inpcb_hdr *inph, *ninph;

	TAILQ_FOREACH_SAFE(inph, &table->inpt_queue, inph_queue, ninph) {
		struct inpcb *inp = (struct inpcb *)inph;
		if (inp->inp_af != AF_INET)
			continue;
		if ((rt = rtcache_validate(&inp->inp_route)) != NULL &&
		    rt->rt_ifp == ifp)
			in_rtchange(inp, 0);
	}
}

/*
 * Check for alternatives when higher level complains
 * about service problems.  For now, invalidate cached
 * routing information.  If the route was created dynamically
 * (by a redirect), time to try a default gateway again.
 */
void
in_losing(struct inpcb *inp)
{
	struct rtentry *rt;
	struct rt_addrinfo info;

	if (inp->inp_af != AF_INET)
		return;

	if ((rt = rtcache_validate(&inp->inp_route)) == NULL)
		return;

	memset(&info, 0, sizeof(info));
	info.rti_info[RTAX_DST] = rtcache_getdst(&inp->inp_route);
	info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
	info.rti_info[RTAX_NETMASK] = rt_mask(rt);
	rt_missmsg(RTM_LOSING, &info, rt->rt_flags, 0);
	if (rt->rt_flags & RTF_DYNAMIC)
		(void) rtrequest(RTM_DELETE, rt_getkey(rt),
			rt->rt_gateway, rt_mask(rt), rt->rt_flags,
			NULL);
	/*
	 * A new route can be allocated
	 * the next time output is attempted.
	 */
	rtcache_free(&inp->inp_route);
}

/*
 * After a routing change, flush old routing.  A new route can be
 * allocated the next time output is attempted.
 */
void
in_rtchange(struct inpcb *inp, int errno)
{

	if (inp->inp_af != AF_INET)
		return;

	rtcache_free(&inp->inp_route);

	/* XXX SHOULD NOTIFY HIGHER-LEVEL PROTOCOLS */
}

struct inpcb *
in_pcblookup_port(struct inpcbtable *table, struct in_addr laddr,
		  u_int lport_arg, int lookup_wildcard, vestigial_inpcb_t *vp)
{
	struct inpcbhead *head;
	struct inpcb_hdr *inph;
	struct inpcb *match = NULL;
	int matchwild = 3;
	int wildcard;
	u_int16_t lport = lport_arg;

	if (vp)
		vp->valid = 0;

	head = INPCBHASH_PORT(table, lport);
	LIST_FOREACH(inph, head, inph_lhash) {
		struct inpcb * const inp = (struct inpcb *)inph;

		if (inp->inp_af != AF_INET)
			continue;
		if (inp->inp_lport != lport)
			continue;
		/*
		 * check if inp's faddr and laddr match with ours.
		 * our faddr is considered null.
		 * count the number of wildcard matches. (0 - 2)
		 *
		 *	null	null	match
		 *	A	null	wildcard match
		 *	null	B	wildcard match
		 *	A	B	non match
		 *	A	A	match
		 */
		wildcard = 0;
		if (!in_nullhost(inp->inp_faddr))
			wildcard++;
		if (in_nullhost(inp->inp_laddr)) {
			if (!in_nullhost(laddr))
				wildcard++;
		} else {
			if (in_nullhost(laddr))
				wildcard++;
			else {
				if (!in_hosteq(inp->inp_laddr, laddr))
					continue;
			}
		}
		if (wildcard && !lookup_wildcard)
			continue;
		/*
		 * prefer an address with less wildcards.
		 */
		if (wildcard < matchwild) {
			match = inp;
			matchwild = wildcard;
			if (matchwild == 0)
				break;
		}
	}
	if (match && matchwild == 0)
		return match;

	if (vp && table->vestige) {
		void	*state = (*table->vestige->init_ports4)(laddr, lport_arg, lookup_wildcard);
		vestigial_inpcb_t better;

		while (table->vestige
		       && (*table->vestige->next_port4)(state, vp)) {

			if (vp->lport != lport)
				continue;
			wildcard = 0;
			if (!in_nullhost(vp->faddr.v4))
				wildcard++;
			if (in_nullhost(vp->laddr.v4)) {
				if (!in_nullhost(laddr))
					wildcard++;
			} else {
				if (in_nullhost(laddr))
					wildcard++;
				else {
					if (!in_hosteq(vp->laddr.v4, laddr))
						continue;
				}
			}
			if (wildcard && !lookup_wildcard)
				continue;
			if (wildcard < matchwild) {
				better = *vp;
				match  = (void*)&better;

				matchwild = wildcard;
				if (matchwild == 0)
					break;
			}
		}

		if (match) {
			if (match != (void*)&better)
				return match;
			else {
				*vp = better;
				return 0;
			}
		}
	}

	return (match);
}

#ifdef DIAGNOSTIC
int	in_pcbnotifymiss = 0;
#endif

struct inpcb *
in_pcblookup_connect(struct inpcbtable *table,
    struct in_addr faddr, u_int fport_arg,
    struct in_addr laddr, u_int lport_arg,
    vestigial_inpcb_t *vp)
{
	struct inpcbhead *head;
	struct inpcb_hdr *inph;
	struct inpcb *inp;
	u_int16_t fport = fport_arg, lport = lport_arg;

	if (vp)
		vp->valid = 0;

	head = INPCBHASH_CONNECT(table, faddr, fport, laddr, lport);
	LIST_FOREACH(inph, head, inph_hash) {
		inp = (struct inpcb *)inph;
		if (inp->inp_af != AF_INET)
			continue;

		if (in_hosteq(inp->inp_faddr, faddr) &&
		    inp->inp_fport == fport &&
		    inp->inp_lport == lport &&
		    in_hosteq(inp->inp_laddr, laddr))
			goto out;
	}
	if (vp && table->vestige) {
		if ((*table->vestige->lookup4)(faddr, fport_arg,
					       laddr, lport_arg, vp))
			return 0;
	}

#ifdef DIAGNOSTIC
	if (in_pcbnotifymiss) {
		printf("in_pcblookup_connect: faddr=%08x fport=%d laddr=%08x lport=%d\n",
		    ntohl(faddr.s_addr), ntohs(fport),
		    ntohl(laddr.s_addr), ntohs(lport));
	}
#endif
	return (0);

out:
	/* Move this PCB to the head of hash chain. */
	inph = &inp->inp_head;
	if (inph != LIST_FIRST(head)) {
		LIST_REMOVE(inph, inph_hash);
		LIST_INSERT_HEAD(head, inph, inph_hash);
	}
	return (inp);
}

struct inpcb *
in_pcblookup_bind(struct inpcbtable *table,
    struct in_addr laddr, u_int lport_arg)
{
	struct inpcbhead *head;
	struct inpcb_hdr *inph;
	struct inpcb *inp;
	u_int16_t lport = lport_arg;

	head = INPCBHASH_BIND(table, laddr, lport);
	LIST_FOREACH(inph, head, inph_hash) {
		inp = (struct inpcb *)inph;
		if (inp->inp_af != AF_INET)
			continue;

		if (inp->inp_lport == lport &&
		    in_hosteq(inp->inp_laddr, laddr))
			goto out;
	}
	head = INPCBHASH_BIND(table, zeroin_addr, lport);
	LIST_FOREACH(inph, head, inph_hash) {
		inp = (struct inpcb *)inph;
		if (inp->inp_af != AF_INET)
			continue;

		if (inp->inp_lport == lport &&
		    in_hosteq(inp->inp_laddr, zeroin_addr))
			goto out;
	}
#ifdef DIAGNOSTIC
	if (in_pcbnotifymiss) {
		printf("in_pcblookup_bind: laddr=%08x lport=%d\n",
		    ntohl(laddr.s_addr), ntohs(lport));
	}
#endif
	return (0);

out:
	/* Move this PCB to the head of hash chain. */
	inph = &inp->inp_head;
	if (inph != LIST_FIRST(head)) {
		LIST_REMOVE(inph, inph_hash);
		LIST_INSERT_HEAD(head, inph, inph_hash);
	}
	return (inp);
}

void
in_pcbstate(struct inpcb *inp, int state)
{

	if (inp->inp_af != AF_INET)
		return;

	if (inp->inp_state > INP_ATTACHED)
		LIST_REMOVE(&inp->inp_head, inph_hash);

	switch (state) {
	case INP_BOUND:
		LIST_INSERT_HEAD(INPCBHASH_BIND(inp->inp_table,
		    inp->inp_laddr, inp->inp_lport), &inp->inp_head,
		    inph_hash);
		break;
	case INP_CONNECTED:
		LIST_INSERT_HEAD(INPCBHASH_CONNECT(inp->inp_table,
		    inp->inp_faddr, inp->inp_fport,
		    inp->inp_laddr, inp->inp_lport), &inp->inp_head,
		    inph_hash);
		break;
	}

	inp->inp_state = state;
}

struct rtentry *
in_pcbrtentry(struct inpcb *inp)
{
	struct route *ro;
	union {
		struct sockaddr		dst;
		struct sockaddr_in	dst4;
	} u;

	if (inp->inp_af != AF_INET)
		return (NULL);

	ro = &inp->inp_route;

	sockaddr_in_init(&u.dst4, &inp->inp_faddr, 0);
	return rtcache_lookup(ro, &u.dst);
}
