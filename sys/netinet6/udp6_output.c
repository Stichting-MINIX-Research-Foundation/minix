/*	$NetBSD: udp6_output.c,v 1.50 2015/08/24 22:21:27 pooka Exp $	*/
/*	$KAME: udp6_output.c,v 1.43 2001/10/15 09:19:52 itojun Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: udp6_output.c,v 1.50 2015/08/24 22:21:27 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#endif

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include <sys/kauth.h>
#include <sys/domain.h>

#include <net/if.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/udp_private.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/udp6_var.h>
#include <netinet6/udp6_private.h>
#include <netinet/icmp6.h>
#include <netinet6/ip6protosw.h>
#include <netinet6/scope6_var.h>

#include "faith.h"

#include <net/net_osdep.h>

/*
 * UDP protocol inplementation.
 * Per RFC 768, August, 1980.
 */

int
udp6_output(struct in6pcb * const in6p, struct mbuf *m,
    struct sockaddr_in6 * const addr6, struct mbuf * const control,
    struct lwp * const l)
{
	u_int32_t ulen = m->m_pkthdr.len;
	u_int32_t plen = sizeof(struct udphdr) + ulen;
	struct ip6_hdr *ip6;
	struct udphdr *udp6;
	struct in6_addr *laddr, *faddr;
	struct in6_addr laddr_mapped; /* XXX ugly */
	struct sockaddr_in6 *sin6 = NULL;
	struct ifnet *oifp = NULL;
	int scope_ambiguous = 0;
	u_int16_t fport;
	int error = 0;
	struct ip6_pktopts *optp = NULL;
	struct ip6_pktopts opt;
	int af = AF_INET6, hlen = sizeof(struct ip6_hdr);
#ifdef INET
	struct ip *ip;
	struct udpiphdr *ui;
	int flags = 0;
#endif
	struct sockaddr_in6 tmp;

	if (addr6) {
		sin6 = addr6;
		if (sin6->sin6_family != AF_INET6) {
			error = EAFNOSUPPORT;
			goto release;
		}

		/* protect *sin6 from overwrites */
		tmp = *sin6;
		sin6 = &tmp;

		/*
		 * Application should provide a proper zone ID or the use of
		 * default zone IDs should be enabled.  Unfortunately, some
		 * applications do not behave as it should, so we need a
		 * workaround.  Even if an appropriate ID is not determined,
		 * we'll see if we can determine the outgoing interface.  If we
		 * can, determine the zone ID based on the interface below.
		 */
		if (sin6->sin6_scope_id == 0 && !ip6_use_defzone)
			scope_ambiguous = 1;
		if ((error = sa6_embedscope(sin6, ip6_use_defzone)) != 0)
			goto release;
	}

	if (control) {
		if ((error = ip6_setpktopts(control, &opt,
		    in6p->in6p_outputopts, l->l_cred, IPPROTO_UDP)) != 0)
			goto release;
		optp = &opt;
	} else
		optp = in6p->in6p_outputopts;


	if (sin6) {
		/*
		 * Slightly different than v4 version in that we call
		 * in6_selectsrc and in6_pcbsetport to fill in the local
		 * address and port rather than in_pcbconnect. in_pcbconnect
		 * sets in6p_faddr which causes EISCONN below to be hit on
		 * subsequent sendto.
		 */
		if (sin6->sin6_port == 0) {
			error = EADDRNOTAVAIL;
			goto release;
		}

		if (!IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_faddr)) {
			/* how about ::ffff:0.0.0.0 case? */
			error = EISCONN;
			goto release;
		}


		faddr = &sin6->sin6_addr;
		fport = sin6->sin6_port; /* allow 0 port */

		if (IN6_IS_ADDR_V4MAPPED(faddr)) {
			if ((in6p->in6p_flags & IN6P_IPV6_V6ONLY))
			{
				/*
				 * I believe we should explicitly discard the
				 * packet when mapped addresses are disabled,
				 * rather than send the packet as an IPv6 one.
				 * If we chose the latter approach, the packet
				 * might be sent out on the wire based on the
				 * default route, the situation which we'd
				 * probably want to avoid.
				 * (20010421 jinmei@kame.net)
				 */
				error = EINVAL;
				goto release;
			}
			if (!IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_laddr)
			    && !IN6_IS_ADDR_V4MAPPED(&in6p->in6p_laddr)) {
				/*
				 * when remote addr is an IPv4-mapped address,
				 * local addr should not be an IPv6 address,
				 * since you cannot determine how to map IPv6
				 * source address to IPv4.
				 */
				error = EINVAL;
				goto release;
			}

			af = AF_INET;
		}

		if (!IN6_IS_ADDR_V4MAPPED(faddr)) {
			laddr = in6_selectsrc(sin6, optp,
			    in6p->in6p_moptions,
			    &in6p->in6p_route,
			    &in6p->in6p_laddr, &oifp, &error);
			if (oifp && scope_ambiguous &&
			    (error = in6_setscope(&sin6->sin6_addr,
			    oifp, NULL))) {
				goto release;
			}
		} else {
			/*
			 * XXX: freebsd[34] does not have in_selectsrc, but
			 * we can omit the whole part because freebsd4 calls
			 * udp_output() directly in this case, and thus we'll
			 * never see this path.
			 */
			if (IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_laddr)) {
				struct sockaddr_in *sinp, sin_dst;
				struct in_addr ina;

				memcpy(&ina, &faddr->s6_addr[12], sizeof(ina));
				sockaddr_in_init(&sin_dst, &ina, 0);
				sinp = in_selectsrc(&sin_dst, &in6p->in6p_route,
				    in6p->in6p_socket->so_options, NULL,
				    &error);
				if (sinp == NULL) {
					if (error == 0)
						error = EADDRNOTAVAIL;
					goto release;
				}
				memset(&laddr_mapped, 0, sizeof(laddr_mapped));
				laddr_mapped.s6_addr16[5] = 0xffff; /* ugly */
				memcpy(&laddr_mapped.s6_addr[12],
				      &sinp->sin_addr,
				      sizeof(sinp->sin_addr));
				laddr = &laddr_mapped;
			} else
			{
				laddr = &in6p->in6p_laddr;	/* XXX */
			}
		}
		if (laddr == NULL) {
			if (error == 0)
				error = EADDRNOTAVAIL;
			goto release;
		}
		if (in6p->in6p_lport == 0) {
			/*
			 * Craft a sockaddr_in6 for the local endpoint. Use the
			 * "any" as a base, set the address, and recover the
			 * scope.
			 */
			struct sockaddr_in6 lsin6 =
			    *((const struct sockaddr_in6 *)in6p->in6p_socket->so_proto->pr_domain->dom_sa_any);
			lsin6.sin6_addr = *laddr;
			error = sa6_recoverscope(&lsin6);
			if (error)
				goto release;

			error = in6_pcbsetport(&lsin6, in6p, l);

			if (error) {
				in6p->in6p_laddr = in6addr_any;
				goto release;
			}
		}
	} else {
		if (IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_faddr)) {
			error = ENOTCONN;
			goto release;
		}
		if (IN6_IS_ADDR_V4MAPPED(&in6p->in6p_faddr)) {
			if ((in6p->in6p_flags & IN6P_IPV6_V6ONLY))
			{
				/*
				 * XXX: this case would happen when the
				 * application sets the V6ONLY flag after
				 * connecting the foreign address.
				 * Such applications should be fixed,
				 * so we bark here.
				 */
				log(LOG_INFO, "udp6_output: IPV6_V6ONLY "
				    "option was set for a connected socket\n");
				error = EINVAL;
				goto release;
			} else
				af = AF_INET;
		}
		laddr = &in6p->in6p_laddr;
		faddr = &in6p->in6p_faddr;
		fport = in6p->in6p_fport;
	}

	if (af == AF_INET)
		hlen = sizeof(struct ip);

	/*
	 * Calculate data length and get a mbuf
	 * for UDP and IP6 headers.
	 */
	M_PREPEND(m, hlen + sizeof(struct udphdr), M_DONTWAIT);
	if (m == 0) {
		error = ENOBUFS;
		goto release;
	}

	/*
	 * Stuff checksum and output datagram.
	 */
	udp6 = (struct udphdr *)(mtod(m, char *) + hlen);
	udp6->uh_sport = in6p->in6p_lport; /* lport is always set in the PCB */
	udp6->uh_dport = fport;
	if (plen <= 0xffff)
		udp6->uh_ulen = htons((u_int16_t)plen);
	else
		udp6->uh_ulen = 0;
	udp6->uh_sum = 0;

	switch (af) {
	case AF_INET6:
		ip6 = mtod(m, struct ip6_hdr *);
		ip6->ip6_flow	= in6p->in6p_flowinfo & IPV6_FLOWINFO_MASK;
		ip6->ip6_vfc 	&= ~IPV6_VERSION_MASK;
		ip6->ip6_vfc 	|= IPV6_VERSION;
#if 0				/* ip6_plen will be filled in ip6_output. */
		ip6->ip6_plen	= htons((u_int16_t)plen);
#endif
		ip6->ip6_nxt	= IPPROTO_UDP;
		ip6->ip6_hlim	= in6_selecthlim_rt(in6p);
		ip6->ip6_src	= *laddr;
		ip6->ip6_dst	= *faddr;

		udp6->uh_sum = in6_cksum_phdr(laddr, faddr,
		    htonl(plen), htonl(IPPROTO_UDP));
		m->m_pkthdr.csum_flags = M_CSUM_UDPv6;
		m->m_pkthdr.csum_data = offsetof(struct udphdr, uh_sum);

		UDP6_STATINC(UDP6_STAT_OPACKETS);
		error = ip6_output(m, optp, &in6p->in6p_route, 0,
		    in6p->in6p_moptions, in6p->in6p_socket, NULL);
		break;
	case AF_INET:
#ifdef INET
		/* can't transmit jumbogram over IPv4 */
		if (plen > 0xffff) {
			error = EMSGSIZE;
			goto release;
		}

		ip = mtod(m, struct ip *);
		ui = (struct udpiphdr *)ip;
		memset(ui->ui_x1, 0, sizeof(ui->ui_x1));
		ui->ui_pr = IPPROTO_UDP;
		ui->ui_len = htons(plen);
		memcpy(&ui->ui_src, &laddr->s6_addr[12], sizeof(ui->ui_src));
		ui->ui_ulen = ui->ui_len;

		flags = (in6p->in6p_socket->so_options &
			 (SO_DONTROUTE | SO_BROADCAST));
		memcpy(&ui->ui_dst, &faddr->s6_addr[12], sizeof(ui->ui_dst));

		udp6->uh_sum = in_cksum(m, hlen + plen);
		if (udp6->uh_sum == 0)
			udp6->uh_sum = 0xffff;

		ip->ip_len = htons(hlen + plen);
		ip->ip_ttl = in6_selecthlim(in6p, NULL); /* XXX */
		ip->ip_tos = 0;	/* XXX */

		UDP_STATINC(UDP_STAT_OPACKETS);
		error = ip_output(m, NULL, &in6p->in6p_route, flags /* XXX */,
		    in6p->in6p_v4moptions, (struct socket *)in6p->in6p_socket);
		break;
#else
		error = EAFNOSUPPORT;
		goto release;
#endif
	}
	goto releaseopt;

release:
	m_freem(m);

releaseopt:
	if (control) {
		if (optp == &opt)
			ip6_clearpktopts(&opt, -1);
		m_freem(control);
	}
	return (error);
}
