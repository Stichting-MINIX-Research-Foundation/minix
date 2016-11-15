/*	$NetBSD: udp6_usrreq.c,v 1.121 2015/08/24 22:21:27 pooka Exp $	*/
/*	$KAME: udp6_usrreq.c,v 1.86 2001/05/27 17:33:00 itojun Exp $	*/

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
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)udp_var.h	8.1 (Berkeley) 6/10/93
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: udp6_usrreq.c,v 1.121 2015/08/24 22:21:27 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#include "opt_inet_csum.h"
#include "opt_ipsec.h"
#endif

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include <sys/domain.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/in_offload.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/udp_private.h>

#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/ip6_private.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/udp6_var.h>
#include <netinet6/udp6_private.h>
#include <netinet6/ip6protosw.h>
#include <netinet6/scope6_var.h>

#ifdef IPSEC
#include <netipsec/ipsec.h>
#include <netipsec/ipsec_var.h>
#include <netipsec/ipsec_private.h>
#ifdef INET6
#include <netipsec/ipsec6.h>
#endif
#endif	/* IPSEC */

#include "faith.h"
#if defined(NFAITH) && NFAITH > 0
#include <net/if_faith.h>
#endif

/*
 * UDP protocol implementation.
 * Per RFC 768, August, 1980.
 */

extern struct inpcbtable udbtable;

percpu_t *udp6stat_percpu;

/* UDP on IP6 parameters */
static int	udp6_sendspace = 9216;	/* really max datagram size */
static int	udp6_recvspace = 40 * (1024 + sizeof(struct sockaddr_in6));
					/* 40 1K datagrams */

static	void udp6_notify(struct in6pcb *, int);
static	void sysctl_net_inet6_udp6_setup(struct sysctllog **);

#ifdef UDP_CSUM_COUNTERS
#include <sys/device.h>
struct evcnt udp6_hwcsum_bad = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "udp6", "hwcsum bad");
struct evcnt udp6_hwcsum_ok = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "udp6", "hwcsum ok");
struct evcnt udp6_hwcsum_data = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "udp6", "hwcsum data");
struct evcnt udp6_swcsum = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "udp6", "swcsum");

EVCNT_ATTACH_STATIC(udp6_hwcsum_bad);
EVCNT_ATTACH_STATIC(udp6_hwcsum_ok);
EVCNT_ATTACH_STATIC(udp6_hwcsum_data);
EVCNT_ATTACH_STATIC(udp6_swcsum);

#define	UDP_CSUM_COUNTER_INCR(ev)	(ev)->ev_count++
#else
#define	UDP_CSUM_COUNTER_INCR(ev)	/* nothing */
#endif

void
udp6_init(void)
{
	sysctl_net_inet6_udp6_setup(NULL);
	udp6stat_percpu = percpu_alloc(sizeof(uint64_t) * UDP6_NSTATS);

	udp_init_common();
}

/*
 * Notify a udp user of an asynchronous error;
 * just wake up so that he can collect error status.
 */
static	void
udp6_notify(struct in6pcb *in6p, int errno)
{
	in6p->in6p_socket->so_error = errno;
	sorwakeup(in6p->in6p_socket);
	sowwakeup(in6p->in6p_socket);
}

void *
udp6_ctlinput(int cmd, const struct sockaddr *sa, void *d)
{
	struct udphdr uh;
	struct ip6_hdr *ip6;
	const struct sockaddr_in6 *sa6 = (const struct sockaddr_in6 *)sa;
	struct mbuf *m;
	int off;
	void *cmdarg;
	struct ip6ctlparam *ip6cp = NULL;
	const struct sockaddr_in6 *sa6_src = NULL;
	void (*notify)(struct in6pcb *, int) = udp6_notify;
	struct udp_portonly {
		u_int16_t uh_sport;
		u_int16_t uh_dport;
	} *uhp;

	if (sa->sa_family != AF_INET6 ||
	    sa->sa_len != sizeof(struct sockaddr_in6))
		return NULL;

	if ((unsigned)cmd >= PRC_NCMDS)
		return NULL;
	if (PRC_IS_REDIRECT(cmd))
		notify = in6_rtchange, d = NULL;
	else if (cmd == PRC_HOSTDEAD)
		d = NULL;
	else if (cmd == PRC_MSGSIZE) {
		/* special code is present, see below */
		notify = in6_rtchange;
	}
	else if (inet6ctlerrmap[cmd] == 0)
		return NULL;

	/* if the parameter is from icmp6, decode it. */
	if (d != NULL) {
		ip6cp = (struct ip6ctlparam *)d;
		m = ip6cp->ip6c_m;
		ip6 = ip6cp->ip6c_ip6;
		off = ip6cp->ip6c_off;
		cmdarg = ip6cp->ip6c_cmdarg;
		sa6_src = ip6cp->ip6c_src;
	} else {
		m = NULL;
		ip6 = NULL;
		cmdarg = NULL;
		sa6_src = &sa6_any;
		off = 0;
	}

	if (ip6) {
		/*
		 * XXX: We assume that when IPV6 is non NULL,
		 * M and OFF are valid.
		 */

		/* check if we can safely examine src and dst ports */
		if (m->m_pkthdr.len < off + sizeof(*uhp)) {
			if (cmd == PRC_MSGSIZE)
				icmp6_mtudisc_update((struct ip6ctlparam *)d, 0);
			return NULL;
		}

		memset(&uh, 0, sizeof(uh));
		m_copydata(m, off, sizeof(*uhp), (void *)&uh);

		if (cmd == PRC_MSGSIZE) {
			int valid = 0;

			/*
			 * Check to see if we have a valid UDP socket
			 * corresponding to the address in the ICMPv6 message
			 * payload.
			 */
			if (in6_pcblookup_connect(&udbtable, &sa6->sin6_addr,
			    uh.uh_dport, (const struct in6_addr *)&sa6_src->sin6_addr,
						  uh.uh_sport, 0, 0))
				valid++;
#if 0
			/*
			 * As the use of sendto(2) is fairly popular,
			 * we may want to allow non-connected pcb too.
			 * But it could be too weak against attacks...
			 * We should at least check if the local address (= s)
			 * is really ours.
			 */
			else if (in6_pcblookup_bind(&udbtable, &sa6->sin6_addr,
			    uh.uh_dport, 0))
				valid++;
#endif

			/*
			 * Depending on the value of "valid" and routing table
			 * size (mtudisc_{hi,lo}wat), we will:
			 * - recalculate the new MTU and create the
			 *   corresponding routing entry, or
			 * - ignore the MTU change notification.
			 */
			icmp6_mtudisc_update((struct ip6ctlparam *)d, valid);

			/*
			 * regardless of if we called
			 * icmp6_mtudisc_update(), we need to call
			 * in6_pcbnotify(), to notify path MTU change
			 * to the userland (RFC3542), because some
			 * unconnected sockets may share the same
			 * destination and want to know the path MTU.
			 */
		}

		(void) in6_pcbnotify(&udbtable, sa, uh.uh_dport,
		    (const struct sockaddr *)sa6_src, uh.uh_sport, cmd, cmdarg,
		    notify);
	} else {
		(void) in6_pcbnotify(&udbtable, sa, 0,
		    (const struct sockaddr *)sa6_src, 0, cmd, cmdarg, notify);
	}
	return NULL;
}

int
udp6_ctloutput(int op, struct socket *so, struct sockopt *sopt)
{
	int s;
	int error = 0;
	int family;

	family = so->so_proto->pr_domain->dom_family;

	s = splsoftnet();
	switch (family) {
#ifdef INET
	case PF_INET:
		if (sopt->sopt_level != IPPROTO_UDP) {
			error = ip_ctloutput(op, so, sopt);
			goto end;
		}
		break;
#endif
#ifdef INET6
	case PF_INET6:
		if (sopt->sopt_level != IPPROTO_UDP) {
			error = ip6_ctloutput(op, so, sopt);
			goto end;
		}
		break;
#endif
	default:
		error = EAFNOSUPPORT;
		goto end;
	}
	error = EINVAL;

end:
	splx(s);
	return error;
}

static void
udp6_sendup(struct mbuf *m, int off /* offset of data portion */,
	struct sockaddr *src, struct socket *so)
{
	struct mbuf *opts = NULL;
	struct mbuf *n;
	struct in6pcb *in6p = NULL;

	if (!so)
		return;
	if (so->so_proto->pr_domain->dom_family != AF_INET6)
		return;
	in6p = sotoin6pcb(so);

#if defined(IPSEC)
	/* check AH/ESP integrity. */
	if (ipsec_used && so != NULL && ipsec6_in_reject_so(m, so)) {
		IPSEC6_STATINC(IPSEC_STAT_IN_POLVIO);
		if ((n = m_copypacket(m, M_DONTWAIT)) != NULL)
			icmp6_error(n, ICMP6_DST_UNREACH,
			    ICMP6_DST_UNREACH_ADMIN, 0);
		return;
	}
#endif /*IPSEC*/

	if ((n = m_copypacket(m, M_DONTWAIT)) != NULL) {
		if (in6p && (in6p->in6p_flags & IN6P_CONTROLOPTS
#ifdef SO_OTIMESTAMP
		    || in6p->in6p_socket->so_options & SO_OTIMESTAMP
#endif
		    || in6p->in6p_socket->so_options & SO_TIMESTAMP)) {
			struct ip6_hdr *ip6 = mtod(n, struct ip6_hdr *);
			ip6_savecontrol(in6p, &opts, ip6, n);
		}

		m_adj(n, off);
		if (sbappendaddr(&so->so_rcv, src, n, opts) == 0) {
			m_freem(n);
			if (opts)
				m_freem(opts);
			so->so_rcv.sb_overflowed++;
			UDP6_STATINC(UDP6_STAT_FULLSOCK);
		} else
			sorwakeup(so);
	}
}

int
udp6_realinput(int af, struct sockaddr_in6 *src, struct sockaddr_in6 *dst,
	struct mbuf *m, int off)
{
	u_int16_t sport, dport;
	int rcvcnt;
	struct in6_addr src6, *dst6;
	const struct in_addr *dst4;
	struct inpcb_hdr *inph;
	struct in6pcb *in6p;

	rcvcnt = 0;
	off += sizeof(struct udphdr);	/* now, offset of payload */

	if (af != AF_INET && af != AF_INET6)
		goto bad;
	if (src->sin6_family != AF_INET6 || dst->sin6_family != AF_INET6)
		goto bad;

	src6 = src->sin6_addr;
	if (sa6_recoverscope(src) != 0) {
		/* XXX: should be impossible. */
		goto bad;
	}
	sport = src->sin6_port;

	dport = dst->sin6_port;
	dst4 = (struct in_addr *)&dst->sin6_addr.s6_addr[12];
	dst6 = &dst->sin6_addr;

	if (IN6_IS_ADDR_MULTICAST(dst6) ||
	    (af == AF_INET && IN_MULTICAST(dst4->s_addr))) {
		/*
		 * Deliver a multicast or broadcast datagram to *all* sockets
		 * for which the local and remote addresses and ports match
		 * those of the incoming datagram.  This allows more than
		 * one process to receive multi/broadcasts on the same port.
		 * (This really ought to be done for unicast datagrams as
		 * well, but that would cause problems with existing
		 * applications that open both address-specific sockets and
		 * a wildcard socket listening to the same port -- they would
		 * end up receiving duplicates of every unicast datagram.
		 * Those applications open the multiple sockets to overcome an
		 * inadequacy of the UDP socket interface, but for backwards
		 * compatibility we avoid the problem here rather than
		 * fixing the interface.  Maybe 4.5BSD will remedy this?)
		 */

		/*
		 * KAME note: traditionally we dropped udpiphdr from mbuf here.
		 * we need udpiphdr for IPsec processing so we do that later.
		 */
		/*
		 * Locate pcb(s) for datagram.
		 */
		TAILQ_FOREACH(inph, &udbtable.inpt_queue, inph_queue) {
			in6p = (struct in6pcb *)inph;
			if (in6p->in6p_af != AF_INET6)
				continue;

			if (in6p->in6p_lport != dport)
				continue;
			if (!IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_laddr)) {
				if (!IN6_ARE_ADDR_EQUAL(&in6p->in6p_laddr,
				    dst6))
					continue;
			} else {
				if (IN6_IS_ADDR_V4MAPPED(dst6) &&
				    (in6p->in6p_flags & IN6P_IPV6_V6ONLY))
					continue;
			}
			if (!IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_faddr)) {
				if (!IN6_ARE_ADDR_EQUAL(&in6p->in6p_faddr,
				    &src6) || in6p->in6p_fport != sport)
					continue;
			} else {
				if (IN6_IS_ADDR_V4MAPPED(&src6) &&
				    (in6p->in6p_flags & IN6P_IPV6_V6ONLY))
					continue;
			}

			udp6_sendup(m, off, (struct sockaddr *)src,
				in6p->in6p_socket);
			rcvcnt++;

			/*
			 * Don't look for additional matches if this one does
			 * not have either the SO_REUSEPORT or SO_REUSEADDR
			 * socket options set.  This heuristic avoids searching
			 * through all pcbs in the common case of a non-shared
			 * port.  It assumes that an application will never
			 * clear these options after setting them.
			 */
			if ((in6p->in6p_socket->so_options &
			    (SO_REUSEPORT|SO_REUSEADDR)) == 0)
				break;
		}
	} else {
		/*
		 * Locate pcb for datagram.
		 */
		in6p = in6_pcblookup_connect(&udbtable, &src6, sport, dst6,
					     dport, 0, 0);
		if (in6p == 0) {
			UDP_STATINC(UDP_STAT_PCBHASHMISS);
			in6p = in6_pcblookup_bind(&udbtable, dst6, dport, 0);
			if (in6p == 0)
				return rcvcnt;
		}

		udp6_sendup(m, off, (struct sockaddr *)src, in6p->in6p_socket);
		rcvcnt++;
	}

bad:
	return rcvcnt;
}

int
udp6_input_checksum(struct mbuf *m, const struct udphdr *uh, int off, int len)
{

	/*
	 * XXX it's better to record and check if this mbuf is
	 * already checked.
	 */

	if (__predict_false((m->m_flags & M_LOOP) && !udp_do_loopback_cksum)) {
		goto good;
	}
	if (uh->uh_sum == 0) {
		UDP6_STATINC(UDP6_STAT_NOSUM);
		goto bad;
	}

	switch (m->m_pkthdr.csum_flags &
	    ((m->m_pkthdr.rcvif->if_csum_flags_rx & M_CSUM_UDPv6) |
	    M_CSUM_TCP_UDP_BAD | M_CSUM_DATA)) {
	case M_CSUM_UDPv6|M_CSUM_TCP_UDP_BAD:
		UDP_CSUM_COUNTER_INCR(&udp6_hwcsum_bad);
		UDP6_STATINC(UDP6_STAT_BADSUM);
		goto bad;

#if 0 /* notyet */
	case M_CSUM_UDPv6|M_CSUM_DATA:
#endif

	case M_CSUM_UDPv6:
		/* Checksum was okay. */
		UDP_CSUM_COUNTER_INCR(&udp6_hwcsum_ok);
		break;

	default:
		/*
		 * Need to compute it ourselves.  Maybe skip checksum
		 * on loopback interfaces.
		 */
		UDP_CSUM_COUNTER_INCR(&udp6_swcsum);
		if (in6_cksum(m, IPPROTO_UDP, off, len) != 0) {
			UDP6_STATINC(UDP6_STAT_BADSUM);
			goto bad;
		}
	}

good:
	return 0;
bad:
	return -1;
}

int
udp6_input(struct mbuf **mp, int *offp, int proto)
{
	struct mbuf *m = *mp;
	int off = *offp;
	struct sockaddr_in6 src, dst;
	struct ip6_hdr *ip6;
	struct udphdr *uh;
	u_int32_t plen, ulen;

	ip6 = mtod(m, struct ip6_hdr *);

#if defined(NFAITH) && 0 < NFAITH
	if (faithprefix(&ip6->ip6_dst)) {
		/* send icmp6 host unreach? */
		m_freem(m);
		return IPPROTO_DONE;
	}
#endif

	UDP6_STATINC(UDP6_STAT_IPACKETS);

	/* check for jumbogram is done in ip6_input.  we can trust pkthdr.len */
	plen = m->m_pkthdr.len - off;
	IP6_EXTHDR_GET(uh, struct udphdr *, m, off, sizeof(struct udphdr));
	if (uh == NULL) {
		IP6_STATINC(IP6_STAT_TOOSHORT);
		return IPPROTO_DONE;
	}
	KASSERT(UDP_HDR_ALIGNED_P(uh));
	ulen = ntohs((u_short)uh->uh_ulen);
	/*
	 * RFC2675 section 4: jumbograms will have 0 in the UDP header field,
	 * iff payload length > 0xffff.
	 */
	if (ulen == 0 && plen > 0xffff)
		ulen = plen;

	if (plen != ulen) {
		UDP6_STATINC(UDP6_STAT_BADLEN);
		goto bad;
	}

	/* destination port of 0 is illegal, based on RFC768. */
	if (uh->uh_dport == 0)
		goto bad;

	/* Be proactive about malicious use of IPv4 mapped address */
	if (IN6_IS_ADDR_V4MAPPED(&ip6->ip6_src) ||
	    IN6_IS_ADDR_V4MAPPED(&ip6->ip6_dst)) {
		/* XXX stat */
		goto bad;
	}

	/*
	 * Checksum extended UDP header and data.  Maybe skip checksum
	 * on loopback interfaces.
	 */
	if (udp6_input_checksum(m, uh, off, ulen))
		goto bad;

	/*
	 * Construct source and dst sockaddrs.
	 */
	memset(&src, 0, sizeof(src));
	src.sin6_family = AF_INET6;
	src.sin6_len = sizeof(struct sockaddr_in6);
	src.sin6_addr = ip6->ip6_src;
	src.sin6_port = uh->uh_sport;
	memset(&dst, 0, sizeof(dst));
	dst.sin6_family = AF_INET6;
	dst.sin6_len = sizeof(struct sockaddr_in6);
	dst.sin6_addr = ip6->ip6_dst;
	dst.sin6_port = uh->uh_dport;

	if (udp6_realinput(AF_INET6, &src, &dst, m, off) == 0) {
		if (m->m_flags & M_MCAST) {
			UDP6_STATINC(UDP6_STAT_NOPORTMCAST);
			goto bad;
		}
		UDP6_STATINC(UDP6_STAT_NOPORT);
		icmp6_error(m, ICMP6_DST_UNREACH, ICMP6_DST_UNREACH_NOPORT, 0);
		m = NULL;
	}

bad:
	if (m)
		m_freem(m);
	return IPPROTO_DONE;
}

static int
udp6_attach(struct socket *so, int proto)
{
	struct in6pcb *in6p;
	int s, error;

	KASSERT(sotoin6pcb(so) == NULL);
	sosetlock(so);

	/*
	 * MAPPED_ADDR implementation spec:
	 *  Always attach for IPv6, and only when necessary for IPv4.
	 */
	s = splsoftnet();
	error = in6_pcballoc(so, &udbtable);
	splx(s);
	if (error) {
		return error;
	}
	error = soreserve(so, udp6_sendspace, udp6_recvspace);
	if (error) {
		return error;
	}
	in6p = sotoin6pcb(so);
	in6p->in6p_cksum = -1;	/* just to be sure */

	KASSERT(solocked(so));
	return 0;
}

static void
udp6_detach(struct socket *so)
{
	struct in6pcb *in6p = sotoin6pcb(so);
	int s;

	KASSERT(solocked(so));
	KASSERT(in6p != NULL);

	s = splsoftnet();
	in6_pcbdetach(in6p);
	splx(s);
}

static int
udp6_accept(struct socket *so, struct sockaddr *nam)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
udp6_bind(struct socket *so, struct sockaddr *nam, struct lwp *l)
{
	struct in6pcb *in6p = sotoin6pcb(so);
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)nam;
	int error = 0;
	int s;

	KASSERT(solocked(so));
	KASSERT(in6p != NULL);

	s = splsoftnet();
	error = in6_pcbbind(in6p, sin6, l);
	splx(s);
	return error;
}

static int
udp6_listen(struct socket *so, struct lwp *l)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
udp6_connect(struct socket *so, struct sockaddr *nam, struct lwp *l)
{
	struct in6pcb *in6p = sotoin6pcb(so);
	int error = 0;
	int s;

	KASSERT(solocked(so));
	KASSERT(in6p != NULL);

	if (!IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_faddr))
		return EISCONN;
	s = splsoftnet();
	error = in6_pcbconnect(in6p, (struct sockaddr_in6 *)nam, l);
	splx(s);
	if (error == 0)
		soisconnected(so);

	return error;
}

static int
udp6_connect2(struct socket *so, struct socket *so2)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
udp6_disconnect(struct socket *so)
{
	struct in6pcb *in6p = sotoin6pcb(so);
	int s;

	KASSERT(solocked(so));
	KASSERT(in6p != NULL);

	if (IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_faddr))
		return ENOTCONN;

	s = splsoftnet();
	in6_pcbdisconnect(in6p);
	memset((void *)&in6p->in6p_laddr, 0, sizeof(in6p->in6p_laddr));
	splx(s);

	so->so_state &= ~SS_ISCONNECTED;	/* XXX */
	in6_pcbstate(in6p, IN6P_BOUND);		/* XXX */
	return 0;
}

static int
udp6_shutdown(struct socket *so)
{
	int s;

	s = splsoftnet();
	socantsendmore(so);
	splx(s);

	return 0;
}

static int
udp6_abort(struct socket *so)
{
	int s;

	KASSERT(solocked(so));
	KASSERT(sotoin6pcb(so) != NULL);

	s = splsoftnet();
	soisdisconnected(so);
	in6_pcbdetach(sotoin6pcb(so));
	splx(s);

	return 0;
}

static int
udp6_ioctl(struct socket *so, u_long cmd, void *addr6, struct ifnet *ifp)
{
	/*
	 * MAPPED_ADDR implementation info:
	 *  Mapped addr support for PRU_CONTROL is not necessary.
	 *  Because typical user of PRU_CONTROL is such as ifconfig,
	 *  and they don't associate any addr to their socket.  Then
	 *  socket family is only hint about the PRU_CONTROL'ed address
	 *  family, especially when getting addrs from kernel.
	 *  So AF_INET socket need to be used to control AF_INET addrs,
	 *  and AF_INET6 socket for AF_INET6 addrs.
	 */
	return in6_control(so, cmd, addr6, ifp);
}

static int
udp6_stat(struct socket *so, struct stat *ub)
{
	KASSERT(solocked(so));

	/* stat: don't bother with a blocksize */
	return 0;
}

static int
udp6_peeraddr(struct socket *so, struct sockaddr *nam)
{
	KASSERT(solocked(so));
	KASSERT(sotoin6pcb(so) != NULL);
	KASSERT(nam != NULL);

	in6_setpeeraddr(sotoin6pcb(so), (struct sockaddr_in6 *)nam);
	return 0;
}

static int
udp6_sockaddr(struct socket *so, struct sockaddr *nam)
{
	KASSERT(solocked(so));
	KASSERT(sotoin6pcb(so) != NULL);
	KASSERT(nam != NULL);

	in6_setsockaddr(sotoin6pcb(so), (struct sockaddr_in6 *)nam);
	return 0;
}

static int
udp6_rcvd(struct socket *so, int flags, struct lwp *l)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
udp6_recvoob(struct socket *so, struct mbuf *m, int flags)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
udp6_send(struct socket *so, struct mbuf *m, struct sockaddr *nam,
    struct mbuf *control, struct lwp *l)
{
	struct in6pcb *in6p = sotoin6pcb(so);
	int error = 0;
	int s;

	KASSERT(solocked(so));
	KASSERT(in6p != NULL);
	KASSERT(m != NULL);

	s = splsoftnet();
	error = udp6_output(in6p, m, (struct sockaddr_in6 *)nam, control, l);
	splx(s);

	return error;
}

static int
udp6_sendoob(struct socket *so, struct mbuf *m, struct mbuf *control)
{
	KASSERT(solocked(so));

	if (m)
		m_freem(m);
	if (control)
		m_freem(control);

	return EOPNOTSUPP;
}

static int
udp6_purgeif(struct socket *so, struct ifnet *ifp)
{

	mutex_enter(softnet_lock);
	in6_pcbpurgeif0(&udbtable, ifp);
	in6_purgeif(ifp);
	in6_pcbpurgeif(&udbtable, ifp);
	mutex_exit(softnet_lock);

	return 0;
}

static int
sysctl_net_inet6_udp6_stats(SYSCTLFN_ARGS)
{

	return (NETSTAT_SYSCTL(udp6stat_percpu, UDP6_NSTATS));
}

static void
sysctl_net_inet6_udp6_setup(struct sysctllog **clog)
{

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "inet6", NULL,
		       NULL, 0, NULL, 0,
		       CTL_NET, PF_INET6, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "udp6",
		       SYSCTL_DESCR("UDPv6 related settings"),
		       NULL, 0, NULL, 0,
		       CTL_NET, PF_INET6, IPPROTO_UDP, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "sendspace",
		       SYSCTL_DESCR("Default UDP send buffer size"),
		       NULL, 0, &udp6_sendspace, 0,
		       CTL_NET, PF_INET6, IPPROTO_UDP, UDP6CTL_SENDSPACE,
		       CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "recvspace",
		       SYSCTL_DESCR("Default UDP receive buffer size"),
		       NULL, 0, &udp6_recvspace, 0,
		       CTL_NET, PF_INET6, IPPROTO_UDP, UDP6CTL_RECVSPACE,
		       CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "do_loopback_cksum",
		       SYSCTL_DESCR("Perform UDP checksum on loopback"),
		       NULL, 0, &udp_do_loopback_cksum, 0,
		       CTL_NET, PF_INET6, IPPROTO_UDP, UDP6CTL_LOOPBACKCKSUM,
		       CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "pcblist",
		       SYSCTL_DESCR("UDP protocol control block list"),
		       sysctl_inpcblist, 0, &udbtable, 0,
		       CTL_NET, PF_INET6, IPPROTO_UDP, CTL_CREATE,
		       CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "stats",
		       SYSCTL_DESCR("UDPv6 statistics"),
		       sysctl_net_inet6_udp6_stats, 0, NULL, 0,
		       CTL_NET, PF_INET6, IPPROTO_UDP, UDP6CTL_STATS,
		       CTL_EOL);
}

void
udp6_statinc(u_int stat)
{

	KASSERT(stat < UDP6_NSTATS);
	UDP6_STATINC(stat);
}

PR_WRAP_USRREQS(udp6)
#define	udp6_attach	udp6_attach_wrapper
#define	udp6_detach	udp6_detach_wrapper
#define	udp6_accept	udp6_accept_wrapper
#define	udp6_bind	udp6_bind_wrapper
#define	udp6_listen	udp6_listen_wrapper
#define	udp6_connect	udp6_connect_wrapper
#define	udp6_connect2	udp6_connect2_wrapper
#define	udp6_disconnect	udp6_disconnect_wrapper
#define	udp6_shutdown	udp6_shutdown_wrapper
#define	udp6_abort	udp6_abort_wrapper
#define	udp6_ioctl	udp6_ioctl_wrapper
#define	udp6_stat	udp6_stat_wrapper
#define	udp6_peeraddr	udp6_peeraddr_wrapper
#define	udp6_sockaddr	udp6_sockaddr_wrapper
#define	udp6_rcvd	udp6_rcvd_wrapper
#define	udp6_recvoob	udp6_recvoob_wrapper
#define	udp6_send	udp6_send_wrapper
#define	udp6_sendoob	udp6_sendoob_wrapper
#define	udp6_purgeif	udp6_purgeif_wrapper

const struct pr_usrreqs udp6_usrreqs = {
	.pr_attach	= udp6_attach,
	.pr_detach	= udp6_detach,
	.pr_accept	= udp6_accept,
	.pr_bind	= udp6_bind,
	.pr_listen	= udp6_listen,
	.pr_connect	= udp6_connect,
	.pr_connect2	= udp6_connect2,
	.pr_disconnect	= udp6_disconnect,
	.pr_shutdown	= udp6_shutdown,
	.pr_abort	= udp6_abort,
	.pr_ioctl	= udp6_ioctl,
	.pr_stat	= udp6_stat,
	.pr_peeraddr	= udp6_peeraddr,
	.pr_sockaddr	= udp6_sockaddr,
	.pr_rcvd	= udp6_rcvd,
	.pr_recvoob	= udp6_recvoob,
	.pr_send	= udp6_send,
	.pr_sendoob	= udp6_sendoob,
	.pr_purgeif	= udp6_purgeif,
};
