/*	$NetBSD: in_proto.c,v 1.114 2015/08/31 08:05:20 ozaki-r Exp $	*/

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
 * Copyright (c) 1982, 1986, 1993
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
 *	@(#)in_proto.c	8.2 (Berkeley) 2/9/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: in_proto.c,v 1.114 2015/08/31 08:05:20 ozaki-r Exp $");

#ifdef _KERNEL_OPT
#include "opt_mrouting.h"
#include "opt_inet.h"
#include "opt_ipsec.h"
#include "opt_pim.h"
#include "opt_gateway.h"
#include "opt_dccp.h"
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/mbuf.h>

#include <net/if.h>
#include <net/radix.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/in_ifattach.h>
#include <netinet/in_pcb.h>
#include <netinet/in_proto.h>

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet/ip6.h>
#endif

#include <netinet/igmp_var.h>
#ifdef PIM
#include <netinet/pim_var.h>
#endif
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_debug.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/ip_encap.h>

#ifdef DCCP
#include <netinet/dccp.h>
#include <netinet/dccp_var.h>
#endif

/*
 * TCP/IP protocol family: IP, ICMP, UDP, TCP.
 */

#ifdef IPSEC
#include <netipsec/ipsec.h>
#include <netipsec/key.h>
#endif	/* IPSEC */

#include "carp.h"
#if NCARP > 0
#include <netinet/ip_carp.h>
#endif

#include "pfsync.h"
#if NPFSYNC > 0
#include <net/pfvar.h>
#include <net/if_pfsync.h>
#endif

#include "etherip.h"
#if NETHERIP > 0
#include <netinet/ip_etherip.h>
#endif

DOMAIN_DEFINE(inetdomain);	/* forward declare and add to link set */

/* Wrappers to acquire kernel_lock. */

PR_WRAP_CTLINPUT(rip_ctlinput)
PR_WRAP_CTLINPUT(udp_ctlinput)
PR_WRAP_CTLINPUT(tcp_ctlinput)

#define	rip_ctlinput	rip_ctlinput_wrapper
#define	udp_ctlinput	udp_ctlinput_wrapper
#define	tcp_ctlinput	tcp_ctlinput_wrapper

PR_WRAP_CTLOUTPUT(rip_ctloutput)
PR_WRAP_CTLOUTPUT(udp_ctloutput)
PR_WRAP_CTLOUTPUT(tcp_ctloutput)

#define	rip_ctloutput	rip_ctloutput_wrapper
#define	udp_ctloutput	udp_ctloutput_wrapper
#define	tcp_ctloutput	tcp_ctloutput_wrapper

#ifdef DCCP
PR_WRAP_CTLINPUT(dccp_ctlinput)
PR_WRAP_CTLOUTPUT(dccp_ctloutput)

#define dccp_ctlinput	dccp_ctlinput_wrapper
#define dccp_ctloutput	dccp_ctloutput_wrapper
#endif

#if defined(IPSEC)
PR_WRAP_CTLINPUT(ah4_ctlinput)

#define	ah4_ctlinput	ah4_ctlinput_wrapper
PR_WRAP_CTLINPUT(esp4_ctlinput)

#define	esp4_ctlinput	esp4_ctlinput_wrapper
#endif

const struct protosw inetsw[] = {
{	.pr_domain = &inetdomain,
	.pr_init = ip_init,
	.pr_output = ip_output,
	.pr_fasttimo = ip_fasttimo,
	.pr_slowtimo = ip_slowtimo,
	.pr_drain = ip_drainstub,
},
{	.pr_type = SOCK_DGRAM,
	.pr_domain = &inetdomain,
	.pr_protocol = IPPROTO_UDP,
	.pr_flags = PR_ATOMIC|PR_ADDR|PR_PURGEIF,
	.pr_input = udp_input,
	.pr_ctlinput = udp_ctlinput,
	.pr_ctloutput = udp_ctloutput,
	.pr_usrreqs = &udp_usrreqs,
	.pr_init = udp_init,
},
{	.pr_type = SOCK_STREAM,
	.pr_domain = &inetdomain,
	.pr_protocol = IPPROTO_TCP,
	.pr_flags = PR_CONNREQUIRED|PR_WANTRCVD|PR_LISTEN|PR_ABRTACPTDIS|PR_PURGEIF,
	.pr_input = tcp_input,
	.pr_ctlinput = tcp_ctlinput,
	.pr_ctloutput = tcp_ctloutput,
	.pr_usrreqs = &tcp_usrreqs,
	.pr_init = tcp_init,
	.pr_fasttimo = tcp_fasttimo,
	.pr_drain = tcp_drainstub,
},
#ifdef DCCP
{	.pr_type = SOCK_CONN_DGRAM,
	.pr_domain = &inetdomain,
	.pr_protocol = IPPROTO_DCCP,
	.pr_flags = PR_CONNREQUIRED|PR_WANTRCVD|PR_ATOMIC|PR_LISTEN|PR_ABRTACPTDIS,
	.pr_input = dccp_input,
	.pr_ctlinput = dccp_ctlinput,
	.pr_ctloutput = dccp_ctloutput,
	.pr_usrreqs = &dccp_usrreqs,
	.pr_init = dccp_init,
},
#endif
{	.pr_type = SOCK_RAW,
	.pr_domain = &inetdomain,
	.pr_protocol = IPPROTO_RAW,
	.pr_flags = PR_ATOMIC|PR_ADDR|PR_PURGEIF,
	.pr_input = rip_input,
	.pr_output = rip_output,
	.pr_ctlinput = rip_ctlinput,
	.pr_ctloutput = rip_ctloutput,
	.pr_usrreqs = &rip_usrreqs,
},
{	.pr_type = SOCK_RAW,
	.pr_domain = &inetdomain,
	.pr_protocol = IPPROTO_ICMP,
	.pr_flags = PR_ATOMIC|PR_ADDR|PR_LASTHDR,
	.pr_input = icmp_input,
	.pr_output = rip_output,
	.pr_ctlinput = rip_ctlinput,
	.pr_ctloutput = rip_ctloutput,
	.pr_usrreqs = &rip_usrreqs,
	.pr_init = icmp_init,
},
#ifdef GATEWAY
{	.pr_domain = &inetdomain,
	.pr_protocol = IPPROTO_IP,
	.pr_slowtimo = ipflow_slowtimo,
	.pr_init = ipflow_poolinit,
},
#endif /* GATEWAY */
#ifdef IPSEC
{	.pr_type = SOCK_RAW,
	.pr_domain = &inetdomain,
	.pr_protocol = IPPROTO_AH,
	.pr_flags = PR_ATOMIC|PR_ADDR,
	.pr_input = ipsec4_common_input,
	.pr_ctlinput = ah4_ctlinput,
},
{	.pr_type = SOCK_RAW,
	.pr_domain = &inetdomain,
	.pr_protocol = IPPROTO_ESP,
	.pr_flags = PR_ATOMIC|PR_ADDR,
	.pr_input = ipsec4_common_input,
	.pr_ctlinput = esp4_ctlinput,
},
{	.pr_type = SOCK_RAW,
	.pr_domain = &inetdomain,
	.pr_protocol = IPPROTO_IPCOMP,
	.pr_flags = PR_ATOMIC|PR_ADDR,
	.pr_input = ipsec4_common_input,
},
#endif /* IPSEC */
{	.pr_type = SOCK_RAW,
	.pr_domain = &inetdomain,
	.pr_protocol = IPPROTO_IPV4,
	.pr_flags = PR_ATOMIC|PR_ADDR|PR_LASTHDR,
	.pr_input = encap4_input,
	.pr_output = rip_output,
	.pr_ctlinput = rip_ctlinput,
	.pr_ctloutput = rip_ctloutput,
	.pr_usrreqs = &rip_usrreqs,
	.pr_init = encap_init,
},
#ifdef INET6
{	.pr_type = SOCK_RAW,
	.pr_domain = &inetdomain,
	.pr_protocol = IPPROTO_IPV6,
	.pr_flags = PR_ATOMIC|PR_ADDR|PR_LASTHDR,
	.pr_input = encap4_input,
	.pr_output = rip_output,
	.pr_ctlinput = rip_ctlinput,
	.pr_ctloutput = rip_ctloutput,
	.pr_usrreqs = &rip_usrreqs,
	.pr_init = encap_init,
},
#endif /* INET6 */
#if NETHERIP > 0
{	.pr_type = SOCK_RAW,
	.pr_domain = &inetdomain,
	.pr_protocol = IPPROTO_ETHERIP,
	.pr_flags = PR_ATOMIC|PR_ADDR|PR_LASTHDR,
	.pr_input = ip_etherip_input,
	.pr_output = rip_output,
	.pr_ctlinput = rip_ctlinput,
	.pr_ctloutput = rip_ctloutput,
	.pr_usrreqs = &rip_usrreqs,
},
#endif /* NETHERIP > 0 */
#if NCARP > 0
{	.pr_type = SOCK_RAW,
	.pr_domain = &inetdomain,
	.pr_protocol = IPPROTO_CARP,
	.pr_flags = PR_ATOMIC|PR_ADDR,
	.pr_input = carp_proto_input,
	.pr_output = rip_output,
	.pr_ctloutput = rip_ctloutput,
	.pr_usrreqs = &rip_usrreqs,
	.pr_init = carp_init,
},
#endif /* NCARP > 0 */
#if NPFSYNC > 0
{	.pr_type = SOCK_RAW,
	.pr_domain = &inetdomain,
	.pr_protocol = IPPROTO_PFSYNC,
	.pr_flags	 = PR_ATOMIC|PR_ADDR,
	.pr_input	 = pfsync_input,
	.pr_output	 = rip_output,
	.pr_ctloutput = rip_ctloutput,
	.pr_usrreqs	 = &rip_usrreqs,
},
#endif /* NPFSYNC > 0 */
{	.pr_type = SOCK_RAW,
	.pr_domain = &inetdomain,
	.pr_protocol = IPPROTO_IGMP,
	.pr_flags = PR_ATOMIC|PR_ADDR|PR_LASTHDR,
	.pr_input = igmp_input, 
	.pr_output = rip_output,
	.pr_ctloutput = rip_ctloutput,
	.pr_ctlinput = rip_ctlinput,
	.pr_usrreqs = &rip_usrreqs,
	.pr_fasttimo = igmp_fasttimo,
	.pr_slowtimo = igmp_slowtimo,
	.pr_init = igmp_init,
},
#ifdef PIM
{	.pr_type = SOCK_RAW,
	.pr_domain = &inetdomain,
	.pr_protocol = IPPROTO_PIM,
	.pr_flags = PR_ATOMIC|PR_ADDR|PR_LASTHDR,
	.pr_input = pim_input, 
	.pr_output = rip_output,
	.pr_ctloutput = rip_ctloutput,
	.pr_ctlinput = rip_ctlinput,
	.pr_usrreqs = &rip_usrreqs,
},
#endif /* PIM */
/* raw wildcard */
{	.pr_type = SOCK_RAW,
	.pr_domain = &inetdomain,
	.pr_flags = PR_ATOMIC|PR_ADDR|PR_LASTHDR,
	.pr_input = rip_input, 
	.pr_output = rip_output,
	.pr_ctloutput = rip_ctloutput,
	.pr_ctlinput = rip_ctlinput,
	.pr_usrreqs = &rip_usrreqs,
	.pr_init = rip_init,
},
};

const struct sockaddr_in in_any = {
	  .sin_len = sizeof(struct sockaddr_in)
	, .sin_family = AF_INET
	, .sin_port = 0
	, .sin_addr = {.s_addr = 0 /* INADDR_ANY */}
};

struct domain inetdomain = {
	.dom_family = PF_INET, .dom_name = "internet", .dom_init = NULL,
	.dom_externalize = NULL, .dom_dispose = NULL,
	.dom_protosw = inetsw,
	.dom_protoswNPROTOSW = &inetsw[__arraycount(inetsw)],
	.dom_rtattach = rt_inithead,
	.dom_rtoffset = 32,
	.dom_maxrtkey = sizeof(struct ip_pack4),
	.dom_if_up = in_if_up,
	.dom_if_down = in_if_down,
	.dom_ifattach = in_domifattach,
	.dom_ifdetach = in_domifdetach,
	.dom_if_link_state_change = in_if_link_state_change,
	.dom_ifqueues = { NULL, NULL },
	.dom_link = { NULL },
	.dom_mowner = MOWNER_INIT("",""),
	.dom_sa_cmpofs = offsetof(struct sockaddr_in, sin_addr),
	.dom_sa_cmplen = sizeof(struct in_addr),
	.dom_sa_any = (const struct sockaddr *)&in_any,
	.dom_sockaddr_const_addr = sockaddr_in_const_addr,
	.dom_sockaddr_addr = sockaddr_in_addr,
	.dom_rtcache = LIST_HEAD_INITIALIZER(inetdomain.dom_rtcache)
};

u_char	ip_protox[IPPROTO_MAX];

int icmperrppslim = 100;			/* 100pps */

static void
sockaddr_in_addrlen(const struct sockaddr *sa, socklen_t *slenp)
{
	socklen_t slen;

	if (slenp == NULL)
		return;

	slen = sockaddr_getlen(sa);
	*slenp = (socklen_t)MIN(sizeof(struct in_addr),
	    slen - MIN(slen, offsetof(struct sockaddr_in, sin_addr)));
}

const void *
sockaddr_in_const_addr(const struct sockaddr *sa, socklen_t *slenp)
{
	const struct sockaddr_in *sin;

	sockaddr_in_addrlen(sa, slenp);
	sin = (const struct sockaddr_in *)sa;
	return &sin->sin_addr;
}

void *
sockaddr_in_addr(struct sockaddr *sa, socklen_t *slenp)
{
	struct sockaddr_in *sin;

	sockaddr_in_addrlen(sa, slenp);
	sin = (struct sockaddr_in *)sa;
	return &sin->sin_addr;
}

int
sockaddr_in_cmp(const struct sockaddr *sa1, const struct sockaddr *sa2)
{
	uint_fast8_t len;
	const uint_fast8_t addrofs = offsetof(struct sockaddr_in, sin_addr),
			   addrend = addrofs + sizeof(struct in_addr);
	int rc;
	const struct sockaddr_in *sin1, *sin2;

	sin1 = satocsin(sa1);
	sin2 = satocsin(sa2);

	len = MIN(addrend, MIN(sin1->sin_len, sin2->sin_len));

	if (len > addrofs &&
	     (rc = memcmp(&sin1->sin_addr, &sin2->sin_addr,
	                  len - addrofs)) != 0)
		return rc;

	return sin1->sin_len - sin2->sin_len;
}
