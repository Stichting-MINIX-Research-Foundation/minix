/*	$NetBSD: in6_pcb.c,v 1.143 2015/08/24 22:21:27 pooka Exp $	*/
/*	$KAME: in6_pcb.c,v 1.84 2001/02/08 18:02:08 itojun Exp $	*/

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
 *	@(#)in_pcb.c	8.2 (Berkeley) 1/4/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: in6_pcb.c,v 1.143 2015/08/24 22:21:27 pooka Exp $");

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
#include <sys/proc.h>
#include <sys/kauth.h>
#include <sys/domain.h>
#include <sys/once.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip6.h>
#include <netinet/portalgo.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/scope6_var.h>
#include <netinet6/nd6.h>

#include "faith.h"

#ifdef IPSEC
#include <netipsec/ipsec.h>
#include <netipsec/ipsec6.h>
#include <netipsec/key.h>
#endif /* IPSEC */

#include <netinet/tcp_vtw.h>

const struct in6_addr zeroin6_addr;

#define	IN6PCBHASH_PORT(table, lport) \
	&(table)->inpt_porthashtbl[ntohs(lport) & (table)->inpt_porthash]
#define IN6PCBHASH_BIND(table, laddr, lport) \
	&(table)->inpt_bindhashtbl[ \
	    (((laddr)->s6_addr32[0] ^ (laddr)->s6_addr32[1] ^ \
	      (laddr)->s6_addr32[2] ^ (laddr)->s6_addr32[3]) + ntohs(lport)) & \
	    (table)->inpt_bindhash]
#define IN6PCBHASH_CONNECT(table, faddr, fport, laddr, lport) \
	&(table)->inpt_bindhashtbl[ \
	    ((((faddr)->s6_addr32[0] ^ (faddr)->s6_addr32[1] ^ \
	      (faddr)->s6_addr32[2] ^ (faddr)->s6_addr32[3]) + ntohs(fport)) + \
	     (((laddr)->s6_addr32[0] ^ (laddr)->s6_addr32[1] ^ \
	      (laddr)->s6_addr32[2] ^ (laddr)->s6_addr32[3]) + \
	      ntohs(lport))) & (table)->inpt_bindhash]

int ip6_anonportmin = IPV6PORT_ANONMIN;
int ip6_anonportmax = IPV6PORT_ANONMAX;
int ip6_lowportmin  = IPV6PORT_RESERVEDMIN;
int ip6_lowportmax  = IPV6PORT_RESERVEDMAX;

static struct pool in6pcb_pool;

static int
in6pcb_poolinit(void)
{

	pool_init(&in6pcb_pool, sizeof(struct in6pcb), 0, 0, 0, "in6pcbpl",
	    NULL, IPL_SOFTNET);
	return 0;
}

void
in6_pcbinit(struct inpcbtable *table, int bindhashsize, int connecthashsize)
{
	static ONCE_DECL(control);

	in_pcbinit(table, bindhashsize, connecthashsize);
	table->inpt_lastport = (u_int16_t)ip6_anonportmax;

	RUN_ONCE(&control, in6pcb_poolinit);
}

int
in6_pcballoc(struct socket *so, void *v)
{
	struct inpcbtable *table = v;
	struct in6pcb *in6p;
	int s;

	s = splnet();
	in6p = pool_get(&in6pcb_pool, PR_NOWAIT);
	splx(s);
	if (in6p == NULL)
		return (ENOBUFS);
	memset((void *)in6p, 0, sizeof(*in6p));
	in6p->in6p_af = AF_INET6;
	in6p->in6p_table = table;
	in6p->in6p_socket = so;
	in6p->in6p_hops = -1;	/* use kernel default */
	in6p->in6p_icmp6filt = NULL;
	in6p->in6p_portalgo = PORTALGO_DEFAULT;
	in6p->in6p_bindportonsend = false;
#if defined(IPSEC)
	if (ipsec_enabled) {
		int error = ipsec_init_pcbpolicy(so, &in6p->in6p_sp);
		if (error != 0) {
			s = splnet();
			pool_put(&in6pcb_pool, in6p);
			splx(s);
			return error;
		}
	}
#endif /* IPSEC */
	s = splnet();
	TAILQ_INSERT_HEAD(&table->inpt_queue, (struct inpcb_hdr*)in6p,
	    inph_queue);
	LIST_INSERT_HEAD(IN6PCBHASH_PORT(table, in6p->in6p_lport),
	    &in6p->in6p_head, inph_lhash);
	in6_pcbstate(in6p, IN6P_ATTACHED);
	splx(s);
	if (ip6_v6only)
		in6p->in6p_flags |= IN6P_IPV6_V6ONLY;
	so->so_pcb = (void *)in6p;
	return (0);
}

/*
 * Bind address from sin6 to in6p.
 */
static int
in6_pcbbind_addr(struct in6pcb *in6p, struct sockaddr_in6 *sin6, struct lwp *l)
{
	int error;

	/*
	 * We should check the family, but old programs
	 * incorrectly fail to intialize it.
	 */
	if (sin6->sin6_family != AF_INET6)
		return (EAFNOSUPPORT);

#ifndef INET
	if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr))
		return (EADDRNOTAVAIL);
#endif

	if ((error = sa6_embedscope(sin6, ip6_use_defzone)) != 0)
		return (error);

	if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
		if ((in6p->in6p_flags & IN6P_IPV6_V6ONLY) != 0)
			return (EINVAL);
		if (sin6->sin6_addr.s6_addr32[3]) {
			struct sockaddr_in sin;

			memset(&sin, 0, sizeof(sin));
			sin.sin_len = sizeof(sin);
			sin.sin_family = AF_INET;
			bcopy(&sin6->sin6_addr.s6_addr32[3],
			    &sin.sin_addr, sizeof(sin.sin_addr));
			if (!IN_MULTICAST(sin.sin_addr.s_addr) &&
			    ifa_ifwithaddr((struct sockaddr *)&sin) == NULL)
				return EADDRNOTAVAIL;
		}
	} else if (IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr)) {
		// succeed
	} else if (!IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
		struct ifaddr *ia = NULL;

		if ((in6p->in6p_flags & IN6P_FAITH) == 0 &&
		    (ia = ifa_ifwithaddr((struct sockaddr *)sin6)) == NULL)
			return (EADDRNOTAVAIL);

		/*
		 * bind to an anycast address might accidentally
		 * cause sending a packet with an anycast source
		 * address, so we forbid it.
		 *
		 * We should allow to bind to a deprecated address,
		 * since the application dare to use it.
		 * But, can we assume that they are careful enough
		 * to check if the address is deprecated or not?
		 * Maybe, as a safeguard, we should have a setsockopt
		 * flag to control the bind(2) behavior against
		 * deprecated addresses (default: forbid bind(2)).
		 */
		if (ia &&
		    ((struct in6_ifaddr *)ia)->ia6_flags &
		    (IN6_IFF_ANYCAST|IN6_IFF_NOTREADY|IN6_IFF_DETACHED))
			return (EADDRNOTAVAIL);
	}


	in6p->in6p_laddr = sin6->sin6_addr;


	return (0);
}

/*
 * Bind port from sin6 to in6p.
 */
static int
in6_pcbbind_port(struct in6pcb *in6p, struct sockaddr_in6 *sin6, struct lwp *l)
{
	struct inpcbtable *table = in6p->in6p_table;
	struct socket *so = in6p->in6p_socket;
	int wild = 0, reuseport = (so->so_options & SO_REUSEPORT);
	int error;

	if ((so->so_options & (SO_REUSEADDR|SO_REUSEPORT)) == 0 &&
	   ((so->so_proto->pr_flags & PR_CONNREQUIRED) == 0 ||
	    (so->so_options & SO_ACCEPTCONN) == 0))
		wild = 1;

	if (sin6->sin6_port != 0) {
		enum kauth_network_req req;

#ifndef IPNOPRIVPORTS
		if (ntohs(sin6->sin6_port) < IPV6PORT_RESERVED)
			req = KAUTH_REQ_NETWORK_BIND_PRIVPORT;
		else
#endif /* IPNOPRIVPORTS */
			req = KAUTH_REQ_NETWORK_BIND_PORT;

		error = kauth_authorize_network(l->l_cred, KAUTH_NETWORK_BIND,
		    req, so, sin6, NULL);
		if (error)
			return (EACCES);
	}

	if (IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr)) {
		/*
		 * Treat SO_REUSEADDR as SO_REUSEPORT for multicast;
		 * allow compepte duplication of binding if
		 * SO_REUSEPORT is set, or if SO_REUSEADDR is set
		 * and a multicast address is bound on both
		 * new and duplicated sockets.
		 */
		if (so->so_options & (SO_REUSEADDR | SO_REUSEPORT))
			reuseport = SO_REUSEADDR|SO_REUSEPORT;
	}

	if (sin6->sin6_port != 0) {
		if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
#ifdef INET
			struct inpcb *t;
			struct vestigial_inpcb vestige;

			t = in_pcblookup_port(table,
			    *(struct in_addr *)&sin6->sin6_addr.s6_addr32[3],
			    sin6->sin6_port, wild, &vestige);
			if (t && (reuseport & t->inp_socket->so_options) == 0)
				return (EADDRINUSE);
			if (!t
			    && vestige.valid
			    && !(reuseport && vestige.reuse_port))
			    return EADDRINUSE;
#else
			return (EADDRNOTAVAIL);
#endif
		}

		{
			struct in6pcb *t;
			struct vestigial_inpcb vestige;

			t = in6_pcblookup_port(table, &sin6->sin6_addr,
			    sin6->sin6_port, wild, &vestige);
			if (t && (reuseport & t->in6p_socket->so_options) == 0)
				return (EADDRINUSE);
			if (!t
			    && vestige.valid
			    && !(reuseport && vestige.reuse_port))
			    return EADDRINUSE;
		}
	}

	if (sin6->sin6_port == 0) {
		int e;
		e = in6_pcbsetport(sin6, in6p, l);
		if (e != 0)
			return (e);
	} else {
		in6p->in6p_lport = sin6->sin6_port;
		in6_pcbstate(in6p, IN6P_BOUND);
	}

	LIST_REMOVE(&in6p->in6p_head, inph_lhash);
	LIST_INSERT_HEAD(IN6PCBHASH_PORT(table, in6p->in6p_lport),
	    &in6p->in6p_head, inph_lhash);

	return (0);
}

int
in6_pcbbind(void *v, struct sockaddr_in6 *sin6, struct lwp *l)
{
	struct in6pcb *in6p = v;
	struct sockaddr_in6 lsin6;
	int error;

	if (in6p->in6p_af != AF_INET6)
		return (EINVAL);

	/*
	 * If we already have a local port or a local address it means we're
	 * bounded.
	 */
	if (in6p->in6p_lport || !(IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_laddr) ||
	    (IN6_IS_ADDR_V4MAPPED(&in6p->in6p_laddr) &&
	      in6p->in6p_laddr.s6_addr32[3] == 0)))
		return (EINVAL);

	if (NULL != sin6) {
		/* We were provided a sockaddr_in6 to use. */
		if (sin6->sin6_len != sizeof(*sin6))
			return (EINVAL);
	} else {
		/* We always bind to *something*, even if it's "anything". */
		lsin6 = *((const struct sockaddr_in6 *)
		    in6p->in6p_socket->so_proto->pr_domain->dom_sa_any);
		sin6 = &lsin6;
	}

	/* Bind address. */
	error = in6_pcbbind_addr(in6p, sin6, l);
	if (error)
		return (error);

	/* Bind port. */
	error = in6_pcbbind_port(in6p, sin6, l);
	if (error) {
		/*
		 * Reset the address here to "any" so we don't "leak" the
		 * in6pcb.
		 */
		in6p->in6p_laddr = in6addr_any;

		return (error);
	}


#if 0
	in6p->in6p_flowinfo = 0;	/* XXX */
#endif
	return (0);
}

/*
 * Connect from a socket to a specified address.
 * Both address and port must be specified in argument sin6.
 * If don't have a local address for this socket yet,
 * then pick one.
 */
int
in6_pcbconnect(void *v, struct sockaddr_in6 *sin6, struct lwp *l)
{
	struct in6pcb *in6p = v;
	struct in6_addr *in6a = NULL;
	struct ifnet *ifp = NULL;	/* outgoing interface */
	int error = 0;
	int scope_ambiguous = 0;
#ifdef INET
	struct in6_addr mapped;
#endif
	struct sockaddr_in6 tmp;
	struct vestigial_inpcb vestige;

	(void)&in6a;				/* XXX fool gcc */

	if (in6p->in6p_af != AF_INET6)
		return (EINVAL);

	if (sin6->sin6_len != sizeof(*sin6))
		return (EINVAL);
	if (sin6->sin6_family != AF_INET6)
		return (EAFNOSUPPORT);
	if (sin6->sin6_port == 0)
		return (EADDRNOTAVAIL);

	if (IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr) &&
	    in6p->in6p_socket->so_type == SOCK_STREAM)
		return EADDRNOTAVAIL;

	if (sin6->sin6_scope_id == 0 && !ip6_use_defzone)
		scope_ambiguous = 1;
	if ((error = sa6_embedscope(sin6, ip6_use_defzone)) != 0)
		return(error);

	/* sanity check for mapped address case */
	if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
		if ((in6p->in6p_flags & IN6P_IPV6_V6ONLY) != 0)
			return EINVAL;
		if (IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_laddr))
			in6p->in6p_laddr.s6_addr16[5] = htons(0xffff);
		if (!IN6_IS_ADDR_V4MAPPED(&in6p->in6p_laddr))
			return EINVAL;
	} else
	{
		if (IN6_IS_ADDR_V4MAPPED(&in6p->in6p_laddr))
			return EINVAL;
	}

	/* protect *sin6 from overwrites */
	tmp = *sin6;
	sin6 = &tmp;

	/* Source address selection. */
	if (IN6_IS_ADDR_V4MAPPED(&in6p->in6p_laddr) &&
	    in6p->in6p_laddr.s6_addr32[3] == 0) {
#ifdef INET
		struct sockaddr_in sin, *sinp;

		memset(&sin, 0, sizeof(sin));
		sin.sin_len = sizeof(sin);
		sin.sin_family = AF_INET;
		memcpy(&sin.sin_addr, &sin6->sin6_addr.s6_addr32[3],
			sizeof(sin.sin_addr));
		sinp = in_selectsrc(&sin, &in6p->in6p_route,
			in6p->in6p_socket->so_options, NULL, &error);
		if (sinp == NULL) {
			if (error == 0)
				error = EADDRNOTAVAIL;
			return (error);
		}
		memset(&mapped, 0, sizeof(mapped));
		mapped.s6_addr16[5] = htons(0xffff);
		memcpy(&mapped.s6_addr32[3], &sinp->sin_addr, sizeof(sinp->sin_addr));
		in6a = &mapped;
#else
		return EADDRNOTAVAIL;
#endif
	} else {
		/*
		 * XXX: in6_selectsrc might replace the bound local address
		 * with the address specified by setsockopt(IPV6_PKTINFO).
		 * Is it the intended behavior?
		 */
		in6a = in6_selectsrc(sin6, in6p->in6p_outputopts,
				     in6p->in6p_moptions,
				     &in6p->in6p_route,
				     &in6p->in6p_laddr, &ifp, &error);
		if (ifp && scope_ambiguous &&
		    (error = in6_setscope(&sin6->sin6_addr, ifp, NULL)) != 0) {
			return(error);
		}

		if (in6a == NULL) {
			if (error == 0)
				error = EADDRNOTAVAIL;
			return (error);
		}
	}

	if (ifp != NULL)
		in6p->in6p_ip6.ip6_hlim = (u_int8_t)in6_selecthlim(in6p, ifp);
	else
		in6p->in6p_ip6.ip6_hlim = (u_int8_t)in6_selecthlim_rt(in6p);

	if (in6_pcblookup_connect(in6p->in6p_table, &sin6->sin6_addr,
	    sin6->sin6_port,
	    IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_laddr) ? in6a : &in6p->in6p_laddr,
				  in6p->in6p_lport, 0, &vestige)
		|| vestige.valid)
		return (EADDRINUSE);
	if (IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_laddr) ||
	    (IN6_IS_ADDR_V4MAPPED(&in6p->in6p_laddr) &&
	     in6p->in6p_laddr.s6_addr32[3] == 0))
	{
		if (in6p->in6p_lport == 0) {
			error = in6_pcbbind(in6p, NULL, l);
			if (error != 0)
				return error;
		}
		in6p->in6p_laddr = *in6a;
	}
	in6p->in6p_faddr = sin6->sin6_addr;
	in6p->in6p_fport = sin6->sin6_port;

        /* Late bind, if needed */
	if (in6p->in6p_bindportonsend) {
               struct sockaddr_in6 lsin = *((const struct sockaddr_in6 *)
		    in6p->in6p_socket->so_proto->pr_domain->dom_sa_any);
		lsin.sin6_addr = in6p->in6p_laddr;
		lsin.sin6_port = 0;

               if ((error = in6_pcbbind_port(in6p, &lsin, l)) != 0)
                       return error;
	}
	
	in6_pcbstate(in6p, IN6P_CONNECTED);
	in6p->in6p_flowinfo &= ~IPV6_FLOWLABEL_MASK;
	if (ip6_auto_flowlabel)
		in6p->in6p_flowinfo |=
		    (htonl(ip6_randomflowlabel()) & IPV6_FLOWLABEL_MASK);
#if defined(IPSEC)
	if (ipsec_enabled && in6p->in6p_socket->so_type == SOCK_STREAM)
		ipsec_pcbconn(in6p->in6p_sp);
#endif
	return (0);
}

void
in6_pcbdisconnect(struct in6pcb *in6p)
{
	memset((void *)&in6p->in6p_faddr, 0, sizeof(in6p->in6p_faddr));
	in6p->in6p_fport = 0;
	in6_pcbstate(in6p, IN6P_BOUND);
	in6p->in6p_flowinfo &= ~IPV6_FLOWLABEL_MASK;
#if defined(IPSEC)
	if (ipsec_enabled)
		ipsec_pcbdisconn(in6p->in6p_sp);
#endif
	if (in6p->in6p_socket->so_state & SS_NOFDREF)
		in6_pcbdetach(in6p);
}

void
in6_pcbdetach(struct in6pcb *in6p)
{
	struct socket *so = in6p->in6p_socket;
	int s;

	if (in6p->in6p_af != AF_INET6)
		return;

#if defined(IPSEC)
	if (ipsec_enabled)
		ipsec6_delete_pcbpolicy(in6p);
#endif
	so->so_pcb = NULL;

	s = splnet();
	in6_pcbstate(in6p, IN6P_ATTACHED);
	LIST_REMOVE(&in6p->in6p_head, inph_lhash);
	TAILQ_REMOVE(&in6p->in6p_table->inpt_queue, &in6p->in6p_head,
	    inph_queue);
	splx(s);

	if (in6p->in6p_options) {
		m_freem(in6p->in6p_options);
	}
	if (in6p->in6p_outputopts != NULL) {
		ip6_clearpktopts(in6p->in6p_outputopts, -1);
		free(in6p->in6p_outputopts, M_IP6OPT);
	}
	rtcache_free(&in6p->in6p_route);
	ip6_freemoptions(in6p->in6p_moptions);
	ip_freemoptions(in6p->in6p_v4moptions);
	sofree(so);				/* drops the socket's lock */

	pool_put(&in6pcb_pool, in6p);
	mutex_enter(softnet_lock);		/* reacquire it */
}

void
in6_setsockaddr(struct in6pcb *in6p, struct sockaddr_in6 *sin6)
{

	if (in6p->in6p_af != AF_INET6)
		return;

	sockaddr_in6_init(sin6, &in6p->in6p_laddr, in6p->in6p_lport, 0, 0);
	(void)sa6_recoverscope(sin6); /* XXX: should catch errors */
}

void
in6_setpeeraddr(struct in6pcb *in6p, struct sockaddr_in6 *sin6)
{

	if (in6p->in6p_af != AF_INET6)
		return;

	sockaddr_in6_init(sin6, &in6p->in6p_faddr, in6p->in6p_fport, 0, 0);
	(void)sa6_recoverscope(sin6); /* XXX: should catch errors */
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
 *
 * Note: src (4th arg) carries the flowlabel value on the original IPv6
 * header, in sin6_flowinfo member.
 */
int
in6_pcbnotify(struct inpcbtable *table, const struct sockaddr *dst,
    u_int fport_arg, const struct sockaddr *src, u_int lport_arg, int cmd,
    void *cmdarg, void (*notify)(struct in6pcb *, int))
{
	struct rtentry *rt;
	struct inpcb_hdr *inph, *ninph;
	struct sockaddr_in6 sa6_src;
	const struct sockaddr_in6 *sa6_dst;
	u_int16_t fport = fport_arg, lport = lport_arg;
	int errno;
	int nmatch = 0;
	u_int32_t flowinfo;

	if ((unsigned)cmd >= PRC_NCMDS || dst->sa_family != AF_INET6)
		return 0;

	sa6_dst = (const struct sockaddr_in6 *)dst;
	if (IN6_IS_ADDR_UNSPECIFIED(&sa6_dst->sin6_addr))
		return 0;

	/*
	 * note that src can be NULL when we get notify by local fragmentation.
	 */
	sa6_src = (src == NULL) ? sa6_any : *(const struct sockaddr_in6 *)src;
	flowinfo = sa6_src.sin6_flowinfo;

	/*
	 * Redirects go to all references to the destination,
	 * and use in6_rtchange to invalidate the route cache.
	 * Dead host indications: also use in6_rtchange to invalidate
	 * the cache, and deliver the error to all the sockets.
	 * Otherwise, if we have knowledge of the local port and address,
	 * deliver only to that socket.
	 */
	if (PRC_IS_REDIRECT(cmd) || cmd == PRC_HOSTDEAD) {
		fport = 0;
		lport = 0;
		memset((void *)&sa6_src.sin6_addr, 0, sizeof(sa6_src.sin6_addr));

		if (cmd != PRC_HOSTDEAD)
			notify = in6_rtchange;
	}

	errno = inet6ctlerrmap[cmd];
	TAILQ_FOREACH_SAFE(inph, &table->inpt_queue, inph_queue, ninph) {
		struct in6pcb *in6p = (struct in6pcb *)inph;
		if (in6p->in6p_af != AF_INET6)
			continue;

		/*
		 * Under the following condition, notify of redirects
		 * to the pcb, without making address matches against inpcb.
		 * - redirect notification is arrived.
		 * - the inpcb is unconnected.
		 * - the inpcb is caching !RTF_HOST routing entry.
		 * - the ICMPv6 notification is from the gateway cached in the
		 *   inpcb.  i.e. ICMPv6 notification is from nexthop gateway
		 *   the inpcb used very recently.
		 *
		 * This is to improve interaction between netbsd/openbsd
		 * redirect handling code, and inpcb route cache code.
		 * without the clause, !RTF_HOST routing entry (which carries
		 * gateway used by inpcb right before the ICMPv6 redirect)
		 * will be cached forever in unconnected inpcb.
		 *
		 * There still is a question regarding to what is TRT:
		 * - On bsdi/freebsd, RTF_HOST (cloned) routing entry will be
		 *   generated on packet output.  inpcb will always cache
		 *   RTF_HOST routing entry so there's no need for the clause
		 *   (ICMPv6 redirect will update RTF_HOST routing entry,
		 *   and inpcb is caching it already).
		 *   However, bsdi/freebsd are vulnerable to local DoS attacks
		 *   due to the cloned routing entries.
		 * - Specwise, "destination cache" is mentioned in RFC2461.
		 *   Jinmei says that it implies bsdi/freebsd behavior, itojun
		 *   is not really convinced.
		 * - Having hiwat/lowat on # of cloned host route (redirect/
		 *   pmtud) may be a good idea.  netbsd/openbsd has it.  see
		 *   icmp6_mtudisc_update().
		 */
		if ((PRC_IS_REDIRECT(cmd) || cmd == PRC_HOSTDEAD) &&
		    IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_laddr) &&
		    (rt = rtcache_validate(&in6p->in6p_route)) != NULL &&
		    !(rt->rt_flags & RTF_HOST)) {
			const struct sockaddr_in6 *dst6;

			dst6 = (const struct sockaddr_in6 *)
			    rtcache_getdst(&in6p->in6p_route);
			if (dst6 == NULL)
				;
			else if (IN6_ARE_ADDR_EQUAL(&dst6->sin6_addr,
			    &sa6_dst->sin6_addr))
				goto do_notify;
		}

		/*
		 * If the error designates a new path MTU for a destination
		 * and the application (associated with this socket) wanted to
		 * know the value, notify. Note that we notify for all
		 * disconnected sockets if the corresponding application
		 * wanted. This is because some UDP applications keep sending
		 * sockets disconnected.
		 * XXX: should we avoid to notify the value to TCP sockets?
		 */
		if (cmd == PRC_MSGSIZE && (in6p->in6p_flags & IN6P_MTU) != 0 &&
		    (IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_faddr) ||
		     IN6_ARE_ADDR_EQUAL(&in6p->in6p_faddr, &sa6_dst->sin6_addr))) {
			ip6_notify_pmtu(in6p, (const struct sockaddr_in6 *)dst,
					(u_int32_t *)cmdarg);
		}

		/*
		 * Detect if we should notify the error. If no source and
		 * destination ports are specified, but non-zero flowinfo and
		 * local address match, notify the error. This is the case
		 * when the error is delivered with an encrypted buffer
		 * by ESP. Otherwise, just compare addresses and ports
		 * as usual.
		 */
		if (lport == 0 && fport == 0 && flowinfo &&
		    in6p->in6p_socket != NULL &&
		    flowinfo == (in6p->in6p_flowinfo & IPV6_FLOWLABEL_MASK) &&
		    IN6_ARE_ADDR_EQUAL(&in6p->in6p_laddr, &sa6_src.sin6_addr))
			goto do_notify;
		else if (!IN6_ARE_ADDR_EQUAL(&in6p->in6p_faddr,
					     &sa6_dst->sin6_addr) ||
		    in6p->in6p_socket == NULL ||
		    (lport && in6p->in6p_lport != lport) ||
		    (!IN6_IS_ADDR_UNSPECIFIED(&sa6_src.sin6_addr) &&
		     !IN6_ARE_ADDR_EQUAL(&in6p->in6p_laddr,
					 &sa6_src.sin6_addr)) ||
		    (fport && in6p->in6p_fport != fport))
			continue;

	  do_notify:
		if (notify)
			(*notify)(in6p, errno);
		nmatch++;
	}
	return nmatch;
}

void
in6_pcbpurgeif0(struct inpcbtable *table, struct ifnet *ifp)
{
	struct inpcb_hdr *inph, *ninph;
	struct ip6_moptions *im6o;
	struct in6_multi_mship *imm, *nimm;

	TAILQ_FOREACH_SAFE(inph, &table->inpt_queue, inph_queue, ninph) {
		struct in6pcb *in6p = (struct in6pcb *)inph;
		if (in6p->in6p_af != AF_INET6)
			continue;

		im6o = in6p->in6p_moptions;
		if (im6o) {
			/*
			 * Unselect the outgoing interface if it is being
			 * detached.
			 */
			if (im6o->im6o_multicast_ifp == ifp)
				im6o->im6o_multicast_ifp = NULL;

			/*
			 * Drop multicast group membership if we joined
			 * through the interface being detached.
			 * XXX controversial - is it really legal for kernel
			 * to force this?
			 */
			for (imm = im6o->im6o_memberships.lh_first;
			     imm != NULL; imm = nimm) {
				nimm = imm->i6mm_chain.le_next;
				if (imm->i6mm_maddr->in6m_ifp == ifp) {
					LIST_REMOVE(imm, i6mm_chain);
					in6_leavegroup(imm);
				}
			}
		}
		in_purgeifmcast(in6p->in6p_v4moptions, ifp);
	}
}

void
in6_pcbpurgeif(struct inpcbtable *table, struct ifnet *ifp)
{
	struct rtentry *rt;
	struct inpcb_hdr *inph, *ninph;

	TAILQ_FOREACH_SAFE(inph, &table->inpt_queue, inph_queue, ninph) {
		struct in6pcb *in6p = (struct in6pcb *)inph;
		if (in6p->in6p_af != AF_INET6)
			continue;
		if ((rt = rtcache_validate(&in6p->in6p_route)) != NULL &&
		    rt->rt_ifp == ifp)
			in6_rtchange(in6p, 0);
	}
}

/*
 * Check for alternatives when higher level complains
 * about service problems.  For now, invalidate cached
 * routing information.  If the route was created dynamically
 * (by a redirect), time to try a default gateway again.
 */
void
in6_losing(struct in6pcb *in6p)
{
	struct rtentry *rt;
	struct rt_addrinfo info;

	if (in6p->in6p_af != AF_INET6)
		return;

	if ((rt = rtcache_validate(&in6p->in6p_route)) == NULL)
		return;

	memset(&info, 0, sizeof(info));
	info.rti_info[RTAX_DST] = rtcache_getdst(&in6p->in6p_route);
	info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
	info.rti_info[RTAX_NETMASK] = rt_mask(rt);
	rt_missmsg(RTM_LOSING, &info, rt->rt_flags, 0);
	if (rt->rt_flags & RTF_DYNAMIC) {
		(void)rtrequest(RTM_DELETE, rt_getkey(rt),
		    rt->rt_gateway, rt_mask(rt), rt->rt_flags, NULL);
	}
	/*
	 * A new route can be allocated
	 * the next time output is attempted.
	 */
	rtcache_free(&in6p->in6p_route);
}

/*
 * After a routing change, flush old routing.  A new route can be
 * allocated the next time output is attempted.
 */
void
in6_rtchange(struct in6pcb *in6p, int errno)
{
	if (in6p->in6p_af != AF_INET6)
		return;

	rtcache_free(&in6p->in6p_route);
	/*
	 * A new route can be allocated the next time
	 * output is attempted.
	 */
}

struct in6pcb *
in6_pcblookup_port(struct inpcbtable *table, struct in6_addr *laddr6, 
		   u_int lport_arg, int lookup_wildcard, struct vestigial_inpcb *vp)
{
	struct inpcbhead *head;
	struct inpcb_hdr *inph;
	struct in6pcb *in6p, *match = NULL;
	int matchwild = 3, wildcard;
	u_int16_t lport = lport_arg;

	if (vp)
		vp->valid = 0;

	head = IN6PCBHASH_PORT(table, lport);
	LIST_FOREACH(inph, head, inph_lhash) {
		in6p = (struct in6pcb *)inph;
		if (in6p->in6p_af != AF_INET6)
			continue;

		if (in6p->in6p_lport != lport)
			continue;
		wildcard = 0;
		if (IN6_IS_ADDR_V4MAPPED(&in6p->in6p_faddr)) {
			if ((in6p->in6p_flags & IN6P_IPV6_V6ONLY) != 0)
				continue;
		}
		if (!IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_faddr))
			wildcard++;
		if (IN6_IS_ADDR_V4MAPPED(&in6p->in6p_laddr)) {
			if ((in6p->in6p_flags & IN6P_IPV6_V6ONLY) != 0)
				continue;
			if (!IN6_IS_ADDR_V4MAPPED(laddr6))
				continue;

			/* duplicate of IPv4 logic */
			wildcard = 0;
			if (IN6_IS_ADDR_V4MAPPED(&in6p->in6p_faddr) &&
			    in6p->in6p_faddr.s6_addr32[3])
				wildcard++;
			if (!in6p->in6p_laddr.s6_addr32[3]) {
				if (laddr6->s6_addr32[3])
					wildcard++;
			} else {
				if (!laddr6->s6_addr32[3])
					wildcard++;
				else {
					if (in6p->in6p_laddr.s6_addr32[3] !=
					    laddr6->s6_addr32[3])
						continue;
				}
			}
		} else if (IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_laddr)) {
			if (IN6_IS_ADDR_V4MAPPED(laddr6)) {
				if ((in6p->in6p_flags & IN6P_IPV6_V6ONLY) != 0)
					continue;
			}
			if (!IN6_IS_ADDR_UNSPECIFIED(laddr6))
				wildcard++;
		} else {
			if (IN6_IS_ADDR_V4MAPPED(laddr6)) {
				if ((in6p->in6p_flags & IN6P_IPV6_V6ONLY) != 0)
					continue;
			}
			if (IN6_IS_ADDR_UNSPECIFIED(laddr6))
				wildcard++;
			else {
				if (!IN6_ARE_ADDR_EQUAL(&in6p->in6p_laddr,
				    laddr6))
					continue;
			}
		}
		if (wildcard && !lookup_wildcard)
			continue;
		if (wildcard < matchwild) {
			match = in6p;
			matchwild = wildcard;
			if (matchwild == 0)
				break;
		}
	}
	if (match && matchwild == 0)
		return match;

	if (vp && table->vestige && table->vestige->init_ports6) {
		struct vestigial_inpcb better;
		void *state;

		state = (*table->vestige->init_ports6)(laddr6,
						       lport_arg,
						       lookup_wildcard);
		while (table->vestige
		       && (*table->vestige->next_port6)(state, vp)) {

			if (vp->lport != lport)
				continue;
			wildcard = 0;
			if (!IN6_IS_ADDR_UNSPECIFIED(&vp->faddr.v6))
				wildcard++;
			if (IN6_IS_ADDR_UNSPECIFIED(&vp->laddr.v6)) {
				if (!IN6_IS_ADDR_UNSPECIFIED(laddr6))
					wildcard++;
			} else {
				if (IN6_IS_ADDR_V4MAPPED(laddr6)) {
					if (vp->v6only)
						continue;
				}
				if (IN6_IS_ADDR_UNSPECIFIED(laddr6))
					wildcard++;
				else {
					if (!IN6_ARE_ADDR_EQUAL(&vp->laddr.v6, laddr6))
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

/*
 * WARNING: return value (rtentry) could be IPv4 one if in6pcb is connected to
 * IPv4 mapped address.
 */
struct rtentry *
in6_pcbrtentry(struct in6pcb *in6p)
{
	struct rtentry *rt;
	struct route *ro;
	union {
		const struct sockaddr *sa;
		const struct sockaddr_in6 *sa6;
#ifdef INET
		const struct sockaddr_in *sa4;
#endif
	} cdst;

	ro = &in6p->in6p_route;

	if (in6p->in6p_af != AF_INET6)
		return (NULL);

	cdst.sa = rtcache_getdst(ro);
	if (cdst.sa == NULL)
		;
#ifdef INET
	else if (cdst.sa->sa_family == AF_INET) {
		KASSERT(IN6_IS_ADDR_V4MAPPED(&in6p->in6p_faddr));
		if (cdst.sa4->sin_addr.s_addr != in6p->in6p_faddr.s6_addr32[3])
			rtcache_free(ro);
	}
#endif
	else {
		if (!IN6_ARE_ADDR_EQUAL(&cdst.sa6->sin6_addr,
					&in6p->in6p_faddr))
			rtcache_free(ro);
	}
	if ((rt = rtcache_validate(ro)) == NULL)
		rt = rtcache_update(ro, 1);
#ifdef INET
	if (rt == NULL && IN6_IS_ADDR_V4MAPPED(&in6p->in6p_faddr)) {
		union {
			struct sockaddr		dst;
			struct sockaddr_in	dst4;
		} u;
		struct in_addr addr;

		addr.s_addr = in6p->in6p_faddr.s6_addr32[3];

		sockaddr_in_init(&u.dst4, &addr, 0);
		if (rtcache_setdst(ro, &u.dst) != 0)
			return NULL;

		rt = rtcache_init(ro);
	} else
#endif
	if (rt == NULL && !IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_faddr)) {
		union {
			struct sockaddr		dst;
			struct sockaddr_in6	dst6;
		} u;

		sockaddr_in6_init(&u.dst6, &in6p->in6p_faddr, 0, 0, 0);
		if (rtcache_setdst(ro, &u.dst) != 0)
			return NULL;

		rt = rtcache_init(ro);
	}
	return rt;
}

struct in6pcb *
in6_pcblookup_connect(struct inpcbtable *table, const struct in6_addr *faddr6,
		      u_int fport_arg, const struct in6_addr *laddr6, u_int lport_arg,
		      int faith,
		      struct vestigial_inpcb *vp)
{
	struct inpcbhead *head;
	struct inpcb_hdr *inph;
	struct in6pcb *in6p;
	u_int16_t fport = fport_arg, lport = lport_arg;

	if (vp)
		vp->valid = 0;

	head = IN6PCBHASH_CONNECT(table, faddr6, fport, laddr6, lport);
	LIST_FOREACH(inph, head, inph_hash) {
		in6p = (struct in6pcb *)inph;
		if (in6p->in6p_af != AF_INET6)
			continue;

		/* find exact match on both source and dest */
		if (in6p->in6p_fport != fport)
			continue;
		if (in6p->in6p_lport != lport)
			continue;
		if (IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_faddr))
			continue;
		if (!IN6_ARE_ADDR_EQUAL(&in6p->in6p_faddr, faddr6))
			continue;
		if (IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_laddr))
			continue;
		if (!IN6_ARE_ADDR_EQUAL(&in6p->in6p_laddr, laddr6))
			continue;
		if ((IN6_IS_ADDR_V4MAPPED(laddr6) ||
		     IN6_IS_ADDR_V4MAPPED(faddr6)) &&
		    (in6p->in6p_flags & IN6P_IPV6_V6ONLY))
			continue;
		return in6p;
	}
	if (vp && table->vestige) {
		if ((*table->vestige->lookup6)(faddr6, fport_arg,
					       laddr6, lport_arg, vp))
			return NULL;
	}

	return NULL;
}

struct in6pcb *
in6_pcblookup_bind(struct inpcbtable *table, const struct in6_addr *laddr6, 
	u_int lport_arg, int faith)
{
	struct inpcbhead *head;
	struct inpcb_hdr *inph;
	struct in6pcb *in6p;
	u_int16_t lport = lport_arg;
#ifdef INET
	struct in6_addr zero_mapped;
#endif

	head = IN6PCBHASH_BIND(table, laddr6, lport);
	LIST_FOREACH(inph, head, inph_hash) {
		in6p = (struct in6pcb *)inph;
		if (in6p->in6p_af != AF_INET6)
			continue;

		if (faith && (in6p->in6p_flags & IN6P_FAITH) == 0)
			continue;
		if (in6p->in6p_fport != 0)
			continue;
		if (in6p->in6p_lport != lport)
			continue;
		if (IN6_IS_ADDR_V4MAPPED(laddr6) &&
		    (in6p->in6p_flags & IN6P_IPV6_V6ONLY) != 0)
			continue;
		if (IN6_ARE_ADDR_EQUAL(&in6p->in6p_laddr, laddr6))
			goto out;
	}
#ifdef INET
	if (IN6_IS_ADDR_V4MAPPED(laddr6)) {
		memset(&zero_mapped, 0, sizeof(zero_mapped));
		zero_mapped.s6_addr16[5] = 0xffff;
		head = IN6PCBHASH_BIND(table, &zero_mapped, lport);
		LIST_FOREACH(inph, head, inph_hash) {
			in6p = (struct in6pcb *)inph;
			if (in6p->in6p_af != AF_INET6)
				continue;

			if (faith && (in6p->in6p_flags & IN6P_FAITH) == 0)
				continue;
			if (in6p->in6p_fport != 0)
				continue;
			if (in6p->in6p_lport != lport)
				continue;
			if ((in6p->in6p_flags & IN6P_IPV6_V6ONLY) != 0)
				continue;
			if (IN6_ARE_ADDR_EQUAL(&in6p->in6p_laddr, &zero_mapped))
				goto out;
		}
	}
#endif
	head = IN6PCBHASH_BIND(table, &zeroin6_addr, lport);
	LIST_FOREACH(inph, head, inph_hash) {
		in6p = (struct in6pcb *)inph;
		if (in6p->in6p_af != AF_INET6)
			continue;

		if (faith && (in6p->in6p_flags & IN6P_FAITH) == 0)
			continue;
		if (in6p->in6p_fport != 0)
			continue;
		if (in6p->in6p_lport != lport)
			continue;
		if (IN6_IS_ADDR_V4MAPPED(laddr6) &&
		    (in6p->in6p_flags & IN6P_IPV6_V6ONLY) != 0)
			continue;
		if (IN6_ARE_ADDR_EQUAL(&in6p->in6p_laddr, &zeroin6_addr))
			goto out;
	}
	return (NULL);

out:
	inph = &in6p->in6p_head;
	if (inph != LIST_FIRST(head)) {
		LIST_REMOVE(inph, inph_hash);
		LIST_INSERT_HEAD(head, inph, inph_hash);
	}
	return in6p;
}

void
in6_pcbstate(struct in6pcb *in6p, int state)
{

	if (in6p->in6p_af != AF_INET6)
		return;

	if (in6p->in6p_state > IN6P_ATTACHED)
		LIST_REMOVE(&in6p->in6p_head, inph_hash);

	switch (state) {
	case IN6P_BOUND:
		LIST_INSERT_HEAD(IN6PCBHASH_BIND(in6p->in6p_table,
		    &in6p->in6p_laddr, in6p->in6p_lport), &in6p->in6p_head,
		    inph_hash);
		break;
	case IN6P_CONNECTED:
		LIST_INSERT_HEAD(IN6PCBHASH_CONNECT(in6p->in6p_table,
		    &in6p->in6p_faddr, in6p->in6p_fport,
		    &in6p->in6p_laddr, in6p->in6p_lport), &in6p->in6p_head,
		    inph_hash);
		break;
	}

	in6p->in6p_state = state;
}
