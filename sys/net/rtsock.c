/*	$NetBSD: rtsock.c,v 1.173 2015/08/07 08:11:33 ozaki-r Exp $	*/

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
 * Copyright (c) 1988, 1991, 1993
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
 *	@(#)rtsock.c	8.7 (Berkeley) 10/12/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rtsock.c,v 1.173 2015/08/07 08:11:33 ozaki-r Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#include "opt_mpls.h"
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/sysctl.h>
#include <sys/kauth.h>
#include <sys/kmem.h>
#include <sys/intr.h>
#ifdef RTSOCK_DEBUG
#include <netinet/in.h>
#endif /* RTSOCK_DEBUG */

#include <net/if.h>
#include <net/route.h>
#include <net/raw_cb.h>

#include <netmpls/mpls.h>

#if defined(COMPAT_14) || defined(COMPAT_50)
#include <compat/net/if.h>
#include <compat/net/route.h>
#endif
#ifdef COMPAT_RTSOCK
#define	RTM_XVERSION	RTM_OVERSION
#define	RT_XADVANCE(a,b) RT_OADVANCE(a,b)
#define	RT_XROUNDUP(n)	RT_OROUNDUP(n)
#define	PF_XROUTE	PF_OROUTE
#define	rt_xmsghdr	rt_msghdr50
#define	if_xmsghdr	if_msghdr	/* if_msghdr50 is for RTM_OIFINFO */
#define	ifa_xmsghdr	ifa_msghdr50
#define	if_xannouncemsghdr	if_announcemsghdr50
#define	COMPATNAME(x)	compat_50_ ## x
#define	DOMAINNAME	"oroute"
CTASSERT(sizeof(struct ifa_xmsghdr) == 20);
DOMAIN_DEFINE(compat_50_routedomain); /* forward declare and add to link set */
#else /* COMPAT_RTSOCK */
#define	RTM_XVERSION	RTM_VERSION
#define	RT_XADVANCE(a,b) RT_ADVANCE(a,b)
#define	RT_XROUNDUP(n)	RT_ROUNDUP(n)
#define	PF_XROUTE	PF_ROUTE
#define	rt_xmsghdr	rt_msghdr
#define	if_xmsghdr	if_msghdr
#define	ifa_xmsghdr	ifa_msghdr
#define	if_xannouncemsghdr	if_announcemsghdr
#define	COMPATNAME(x)	x
#define	DOMAINNAME	"route"
CTASSERT(sizeof(struct ifa_xmsghdr) == 24);
#ifdef COMPAT_50
#define	COMPATCALL(name, args)	compat_50_ ## name args
#endif
DOMAIN_DEFINE(routedomain); /* forward declare and add to link set */
#undef COMPAT_50
#undef COMPAT_14
#endif /* COMPAT_RTSOCK */

#ifndef COMPATCALL
#define	COMPATCALL(name, args)	do { } while (/*CONSTCOND*/ 0)
#endif

#ifdef RTSOCK_DEBUG
#define RT_IN_PRINT(b, a) (in_print((b), sizeof(b), \
    &((const struct sockaddr_in *)info.rti_info[(a)])->sin_addr), (b))
#endif /* RTSOCK_DEBUG */

struct route_info COMPATNAME(route_info) = {
	.ri_dst = { .sa_len = 2, .sa_family = PF_XROUTE, },
	.ri_src = { .sa_len = 2, .sa_family = PF_XROUTE, },
	.ri_maxqlen = IFQ_MAXLEN,
};

#define	PRESERVED_RTF	(RTF_UP | RTF_GATEWAY | RTF_HOST | RTF_DONE | RTF_MASK)

static void COMPATNAME(route_init)(void);
static int COMPATNAME(route_output)(struct mbuf *, ...);

static int rt_msg2(int, struct rt_addrinfo *, void *, struct rt_walkarg *, int *);
static int rt_xaddrs(u_char, const char *, const char *, struct rt_addrinfo *);
static struct mbuf *rt_makeifannouncemsg(struct ifnet *, int, int,
    struct rt_addrinfo *);
static void rt_setmetrics(int, const struct rt_xmsghdr *, struct rtentry *);
static void rtm_setmetrics(const struct rtentry *, struct rt_xmsghdr *);
static void sysctl_net_route_setup(struct sysctllog **);
static int sysctl_dumpentry(struct rtentry *, void *);
static int sysctl_iflist(int, struct rt_walkarg *, int);
static int sysctl_rtable(SYSCTLFN_PROTO);
static void rt_adjustcount(int, int);

static void
rt_adjustcount(int af, int cnt)
{
	struct route_cb * const cb = &COMPATNAME(route_info).ri_cb;

	cb->any_count += cnt;

	switch (af) {
	case AF_INET:
		cb->ip_count += cnt;
		return;
#ifdef INET6
	case AF_INET6:
		cb->ip6_count += cnt;
		return;
#endif
	case AF_MPLS:
		cb->mpls_count += cnt;
		return;
	}
}

static int
COMPATNAME(route_attach)(struct socket *so, int proto)
{
	struct rawcb *rp;
	int s, error;

	KASSERT(sotorawcb(so) == NULL);
	rp = kmem_zalloc(sizeof(*rp), KM_SLEEP);
	rp->rcb_len = sizeof(*rp);
	so->so_pcb = rp;

	s = splsoftnet();
	if ((error = raw_attach(so, proto)) == 0) {
		rt_adjustcount(rp->rcb_proto.sp_protocol, 1);
		rp->rcb_laddr = &COMPATNAME(route_info).ri_src;
		rp->rcb_faddr = &COMPATNAME(route_info).ri_dst;
	}
	splx(s);

	if (error) {
		kmem_free(rp, sizeof(*rp));
		so->so_pcb = NULL;
		return error;
	}

	soisconnected(so);
	so->so_options |= SO_USELOOPBACK;
	KASSERT(solocked(so));

	return error;
}

static void
COMPATNAME(route_detach)(struct socket *so)
{
	struct rawcb *rp = sotorawcb(so);
	int s;

	KASSERT(rp != NULL);
	KASSERT(solocked(so));

	s = splsoftnet();
	rt_adjustcount(rp->rcb_proto.sp_protocol, -1);
	raw_detach(so);
	splx(s);
}

static int
COMPATNAME(route_accept)(struct socket *so, struct sockaddr *nam)
{
	KASSERT(solocked(so));

	panic("route_accept");

	return EOPNOTSUPP;
}

static int
COMPATNAME(route_bind)(struct socket *so, struct sockaddr *nam, struct lwp *l)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
COMPATNAME(route_listen)(struct socket *so, struct lwp *l)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
COMPATNAME(route_connect)(struct socket *so, struct sockaddr *nam, struct lwp *l)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
COMPATNAME(route_connect2)(struct socket *so, struct socket *so2)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
COMPATNAME(route_disconnect)(struct socket *so)
{
	struct rawcb *rp = sotorawcb(so);
	int s;

	KASSERT(solocked(so));
	KASSERT(rp != NULL);

	s = splsoftnet();
	soisdisconnected(so);
	raw_disconnect(rp);
	splx(s);

	return 0;
}

static int
COMPATNAME(route_shutdown)(struct socket *so)
{
	int s;

	KASSERT(solocked(so));

	/*
	 * Mark the connection as being incapable of further input.
	 */
	s = splsoftnet();
	socantsendmore(so);
	splx(s);
	return 0;
}

static int
COMPATNAME(route_abort)(struct socket *so)
{
	KASSERT(solocked(so));

	panic("route_abort");

	return EOPNOTSUPP;
}

static int
COMPATNAME(route_ioctl)(struct socket *so, u_long cmd, void *nam,
    struct ifnet * ifp)
{
	return EOPNOTSUPP;
}

static int
COMPATNAME(route_stat)(struct socket *so, struct stat *ub)
{
	KASSERT(solocked(so));

	return 0;
}

static int
COMPATNAME(route_peeraddr)(struct socket *so, struct sockaddr *nam)
{
	struct rawcb *rp = sotorawcb(so);

	KASSERT(solocked(so));
	KASSERT(rp != NULL);
	KASSERT(nam != NULL);

	if (rp->rcb_faddr == NULL)
		return ENOTCONN;

	raw_setpeeraddr(rp, nam);
	return 0;
}

static int
COMPATNAME(route_sockaddr)(struct socket *so, struct sockaddr *nam)
{
	struct rawcb *rp = sotorawcb(so);

	KASSERT(solocked(so));
	KASSERT(rp != NULL);
	KASSERT(nam != NULL);

	if (rp->rcb_faddr == NULL)
		return ENOTCONN;

	raw_setsockaddr(rp, nam);
	return 0;
}

static int
COMPATNAME(route_rcvd)(struct socket *so, int flags, struct lwp *l)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
COMPATNAME(route_recvoob)(struct socket *so, struct mbuf *m, int flags)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
COMPATNAME(route_send)(struct socket *so, struct mbuf *m,
    struct sockaddr *nam, struct mbuf *control, struct lwp *l)
{
	int error = 0;
	int s;

	KASSERT(solocked(so));

	s = splsoftnet();
	error = raw_send(so, m, nam, control, l);
	splx(s);

	return error;
}

static int
COMPATNAME(route_sendoob)(struct socket *so, struct mbuf *m,
    struct mbuf *control)
{
	KASSERT(solocked(so));

	m_freem(m);
	m_freem(control);

	return EOPNOTSUPP;
}
static int
COMPATNAME(route_purgeif)(struct socket *so, struct ifnet *ifp)
{

	panic("route_purgeif");

	return EOPNOTSUPP;
}

/*ARGSUSED*/
int
COMPATNAME(route_output)(struct mbuf *m, ...)
{
	struct sockproto proto = { .sp_family = PF_XROUTE, };
	struct rt_xmsghdr *rtm = NULL;
	struct rt_xmsghdr *old_rtm = NULL;
	struct rtentry *rt = NULL;
	struct rtentry *saved_nrt = NULL;
	struct rt_addrinfo info;
	int len, error = 0;
	struct ifnet *ifp = NULL;
	struct ifaddr *ifa = NULL;
	struct socket *so;
	va_list ap;
	sa_family_t family;

	va_start(ap, m);
	so = va_arg(ap, struct socket *);
	va_end(ap);

#define senderr(e) do { error = e; goto flush;} while (/*CONSTCOND*/ 0)
	if (m == NULL || ((m->m_len < sizeof(int32_t)) &&
	   (m = m_pullup(m, sizeof(int32_t))) == NULL))
		return ENOBUFS;
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("%s", __func__);
	len = m->m_pkthdr.len;
	if (len < sizeof(*rtm) ||
	    len != mtod(m, struct rt_xmsghdr *)->rtm_msglen) {
		info.rti_info[RTAX_DST] = NULL;
		senderr(EINVAL);
	}
	R_Malloc(rtm, struct rt_xmsghdr *, len);
	if (rtm == NULL) {
		info.rti_info[RTAX_DST] = NULL;
		senderr(ENOBUFS);
	}
	m_copydata(m, 0, len, rtm);
	if (rtm->rtm_version != RTM_XVERSION) {
		info.rti_info[RTAX_DST] = NULL;
		senderr(EPROTONOSUPPORT);
	}
	rtm->rtm_pid = curproc->p_pid;
	memset(&info, 0, sizeof(info));
	info.rti_addrs = rtm->rtm_addrs;
	if (rt_xaddrs(rtm->rtm_type, (const char *)(rtm + 1), len + (char *)rtm,
	    &info)) {
		senderr(EINVAL);
	}
	info.rti_flags = rtm->rtm_flags;
#ifdef RTSOCK_DEBUG
	if (info.rti_info[RTAX_DST]->sa_family == AF_INET) {
		char abuf[INET_ADDRSTRLEN];
		printf("%s: extracted info.rti_info[RTAX_DST] %s\n", __func__,
		    RT_IN_PRINT(abuf, RTAX_DST));
	}
#endif /* RTSOCK_DEBUG */
	if (info.rti_info[RTAX_DST] == NULL ||
	    (info.rti_info[RTAX_DST]->sa_family >= AF_MAX)) {
		senderr(EINVAL);
	}
	if (info.rti_info[RTAX_GATEWAY] != NULL &&
	    (info.rti_info[RTAX_GATEWAY]->sa_family >= AF_MAX)) {
		senderr(EINVAL);
	}

	/*
	 * Verify that the caller has the appropriate privilege; RTM_GET
	 * is the only operation the non-superuser is allowed.
	 */
	if (kauth_authorize_network(curlwp->l_cred, KAUTH_NETWORK_ROUTE,
	    0, rtm, NULL, NULL) != 0)
		senderr(EACCES);

	switch (rtm->rtm_type) {

	case RTM_ADD:
		if (info.rti_info[RTAX_GATEWAY] == NULL) {
			senderr(EINVAL);
		}
		error = rtrequest1(rtm->rtm_type, &info, &saved_nrt);
		if (error == 0) {
			rt_setmetrics(rtm->rtm_inits, rtm, saved_nrt);
			rtfree(saved_nrt);
		}
		break;

	case RTM_DELETE:
		error = rtrequest1(rtm->rtm_type, &info, &saved_nrt);
		if (error == 0) {
			rt = saved_nrt;
			goto report;
		}
		break;

	case RTM_GET:
	case RTM_CHANGE:
	case RTM_LOCK:
                /* XXX This will mask info.rti_info[RTAX_DST] with
		 * info.rti_info[RTAX_NETMASK] before
                 * searching.  It did not used to do that.  --dyoung
		 */
		rt = NULL;
		error = rtrequest1(RTM_GET, &info, &rt);
		if (error != 0)
			senderr(error);
		if (rtm->rtm_type != RTM_GET) {/* XXX: too grotty */
			if (memcmp(info.rti_info[RTAX_DST], rt_getkey(rt),
			    info.rti_info[RTAX_DST]->sa_len) != 0)
				senderr(ESRCH);
			if (info.rti_info[RTAX_NETMASK] == NULL &&
			    rt_mask(rt) != NULL)
				senderr(ETOOMANYREFS);
		}

		switch (rtm->rtm_type) {
		case RTM_GET:
		report:
			info.rti_info[RTAX_DST] = rt_getkey(rt);
			info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
			info.rti_info[RTAX_NETMASK] = rt_mask(rt);
			info.rti_info[RTAX_TAG] = rt_gettag(rt);
			if ((rtm->rtm_addrs & (RTA_IFP | RTA_IFA)) == 0)
				;
			else if ((ifp = rt->rt_ifp) != NULL) {
				const struct ifaddr *rtifa;
				info.rti_info[RTAX_IFP] = ifp->if_dl->ifa_addr;
                                /* rtifa used to be simply rt->rt_ifa.
                                 * If rt->rt_ifa != NULL, then
                                 * rt_get_ifa() != NULL.  So this
                                 * ought to still be safe. --dyoung
				 */
				rtifa = rt_get_ifa(rt);
				info.rti_info[RTAX_IFA] = rtifa->ifa_addr;
#ifdef RTSOCK_DEBUG
				if (info.rti_info[RTAX_IFA]->sa_family ==
				    AF_INET) {
					char ibuf[INET_ADDRSTRLEN];
					char abuf[INET_ADDRSTRLEN];
					printf("%s: copying out RTAX_IFA %s "
					    "for info.rti_info[RTAX_DST] %s "
					    "ifa_getifa %p ifa_seqno %p\n",
					    __func__,
					    RT_IN_PRINT(ibuf, RTAX_IFA),
					    RT_IN_PRINT(abuf, RTAX_DST),
					    (void *)rtifa->ifa_getifa,
					    rtifa->ifa_seqno);
				}
#endif /* RTSOCK_DEBUG */
				if (ifp->if_flags & IFF_POINTOPOINT) {
					info.rti_info[RTAX_BRD] =
					    rtifa->ifa_dstaddr;
				} else
					info.rti_info[RTAX_BRD] = NULL;
				rtm->rtm_index = ifp->if_index;
			} else {
				info.rti_info[RTAX_IFP] = NULL;
				info.rti_info[RTAX_IFA] = NULL;
			}
			(void)rt_msg2(rtm->rtm_type, &info, NULL, NULL, &len);
			if (len > rtm->rtm_msglen) {
				old_rtm = rtm;
				R_Malloc(rtm, struct rt_xmsghdr *, len);
				if (rtm == NULL)
					senderr(ENOBUFS);
				(void)memcpy(rtm, old_rtm, old_rtm->rtm_msglen);
			}
			(void)rt_msg2(rtm->rtm_type, &info, rtm, NULL, 0);
			rtm->rtm_flags = rt->rt_flags;
			rtm_setmetrics(rt, rtm);
			rtm->rtm_addrs = info.rti_addrs;
			break;

		case RTM_CHANGE:
			/*
			 * new gateway could require new ifaddr, ifp;
			 * flags may also be different; ifp may be specified
			 * by ll sockaddr when protocol address is ambiguous
			 */
			if ((error = rt_getifa(&info)) != 0)
				senderr(error);
			if (info.rti_info[RTAX_GATEWAY] &&
			    rt_setgate(rt, info.rti_info[RTAX_GATEWAY]))
				senderr(EDQUOT);
			if (info.rti_info[RTAX_TAG])
				rt_settag(rt, info.rti_info[RTAX_TAG]);
			/* new gateway could require new ifaddr, ifp;
			   flags may also be different; ifp may be specified
			   by ll sockaddr when protocol address is ambiguous */
			if (info.rti_info[RTAX_IFP] &&
			    (ifa = ifa_ifwithnet(info.rti_info[RTAX_IFP])) &&
			    (ifp = ifa->ifa_ifp) && (info.rti_info[RTAX_IFA] ||
			    info.rti_info[RTAX_GATEWAY])) {
				if (info.rti_info[RTAX_IFA] == NULL ||
				    (ifa = ifa_ifwithaddr(
				    info.rti_info[RTAX_IFA])) == NULL)
					ifa = ifaof_ifpforaddr(
					    info.rti_info[RTAX_IFA] ?
					    info.rti_info[RTAX_IFA] :
					    info.rti_info[RTAX_GATEWAY], ifp);
			} else if ((info.rti_info[RTAX_IFA] &&
			    (ifa = ifa_ifwithaddr(info.rti_info[RTAX_IFA]))) ||
			    (info.rti_info[RTAX_GATEWAY] &&
			    (ifa = ifa_ifwithroute(rt->rt_flags,
			    rt_getkey(rt), info.rti_info[RTAX_GATEWAY])))) {
				ifp = ifa->ifa_ifp;
			}
			if (ifa) {
				struct ifaddr *oifa = rt->rt_ifa;
				if (oifa != ifa) {
					if (oifa && oifa->ifa_rtrequest) {
						oifa->ifa_rtrequest(RTM_DELETE,
						    rt, &info);
					}
					rt_replace_ifa(rt, ifa);
					rt->rt_ifp = ifp;
				}
			}
			if (ifp && rt->rt_ifp != ifp)
				rt->rt_ifp = ifp;
			rt_setmetrics(rtm->rtm_inits, rtm, rt);
			if (rt->rt_flags != info.rti_flags)
				rt->rt_flags = (info.rti_flags & ~PRESERVED_RTF)
				    | (rt->rt_flags & PRESERVED_RTF);
			if (rt->rt_ifa && rt->rt_ifa->ifa_rtrequest)
				rt->rt_ifa->ifa_rtrequest(RTM_ADD, rt, &info);
			/*FALLTHROUGH*/
		case RTM_LOCK:
			rt->rt_rmx.rmx_locks &= ~(rtm->rtm_inits);
			rt->rt_rmx.rmx_locks |=
			    (rtm->rtm_inits & rtm->rtm_rmx.rmx_locks);
			break;
		}
		break;

	default:
		senderr(EOPNOTSUPP);
	}

flush:
	if (rtm) {
		if (error)
			rtm->rtm_errno = error;
		else
			rtm->rtm_flags |= RTF_DONE;
	}
	family = info.rti_info[RTAX_DST] ? info.rti_info[RTAX_DST]->sa_family :
	    0;
	/* We cannot free old_rtm until we have stopped using the
	 * pointers in info, some of which may point to sockaddrs
	 * in old_rtm.
	 */
	if (old_rtm != NULL)
		Free(old_rtm);
	if (rt)
		rtfree(rt);
    {
	struct rawcb *rp = NULL;
	/*
	 * Check to see if we don't want our own messages.
	 */
	if ((so->so_options & SO_USELOOPBACK) == 0) {
		if (COMPATNAME(route_info).ri_cb.any_count <= 1) {
			if (rtm)
				Free(rtm);
			m_freem(m);
			return error;
		}
		/* There is another listener, so construct message */
		rp = sotorawcb(so);
	}
	if (rtm) {
		m_copyback(m, 0, rtm->rtm_msglen, rtm);
		if (m->m_pkthdr.len < rtm->rtm_msglen) {
			m_freem(m);
			m = NULL;
		} else if (m->m_pkthdr.len > rtm->rtm_msglen)
			m_adj(m, rtm->rtm_msglen - m->m_pkthdr.len);
		Free(rtm);
	}
	if (rp)
		rp->rcb_proto.sp_family = 0; /* Avoid us */
	if (family)
		proto.sp_protocol = family;
	if (m)
		raw_input(m, &proto, &COMPATNAME(route_info).ri_src,
		    &COMPATNAME(route_info).ri_dst);
	if (rp)
		rp->rcb_proto.sp_family = PF_XROUTE;
    }
	return error;
}

static void
rt_setmetrics(int which, const struct rt_xmsghdr *in, struct rtentry *out)
{
#define metric(f, e) if (which & (f)) out->rt_rmx.e = in->rtm_rmx.e;
	metric(RTV_RPIPE, rmx_recvpipe);
	metric(RTV_SPIPE, rmx_sendpipe);
	metric(RTV_SSTHRESH, rmx_ssthresh);
	metric(RTV_RTT, rmx_rtt);
	metric(RTV_RTTVAR, rmx_rttvar);
	metric(RTV_HOPCOUNT, rmx_hopcount);
	metric(RTV_MTU, rmx_mtu);
#undef metric
	if (which & RTV_EXPIRE) {
		out->rt_rmx.rmx_expire = in->rtm_rmx.rmx_expire ?
		    time_wall_to_mono(in->rtm_rmx.rmx_expire) : 0;
	}
}

static void
rtm_setmetrics(const struct rtentry *in, struct rt_xmsghdr *out)
{
#define metric(e) out->rtm_rmx.e = in->rt_rmx.e;
	metric(rmx_recvpipe);
	metric(rmx_sendpipe);
	metric(rmx_ssthresh);
	metric(rmx_rtt);
	metric(rmx_rttvar);
	metric(rmx_hopcount);
	metric(rmx_mtu);
#undef metric
	out->rtm_rmx.rmx_expire = in->rt_rmx.rmx_expire ?
	    time_mono_to_wall(in->rt_rmx.rmx_expire) : 0;
}

static int
rt_xaddrs(u_char rtmtype, const char *cp, const char *cplim,
    struct rt_addrinfo *rtinfo)
{
	const struct sockaddr *sa = NULL;	/* Quell compiler warning */
	int i;

	for (i = 0; i < RTAX_MAX && cp < cplim; i++) {
		if ((rtinfo->rti_addrs & (1 << i)) == 0)
			continue;
		rtinfo->rti_info[i] = sa = (const struct sockaddr *)cp;
		RT_XADVANCE(cp, sa);
	}

	/*
	 * Check for extra addresses specified, except RTM_GET asking
	 * for interface info.
	 */
	if (rtmtype == RTM_GET) {
		if (((rtinfo->rti_addrs &
		    (~((1 << RTAX_IFP) | (1 << RTAX_IFA)))) & (~0 << i)) != 0)
			return 1;
	} else if ((rtinfo->rti_addrs & (~0 << i)) != 0)
		return 1;
	/* Check for bad data length.  */
	if (cp != cplim) {
		if (i == RTAX_NETMASK + 1 && sa != NULL &&
		    cp - RT_XROUNDUP(sa->sa_len) + sa->sa_len == cplim)
			/*
			 * The last sockaddr was info.rti_info[RTAX_NETMASK].
			 * We accept this for now for the sake of old
			 * binaries or third party softwares.
			 */
			;
		else
			return 1;
	}
	return 0;
}

static int
rt_getlen(int type)
{
#ifndef COMPAT_RTSOCK
	CTASSERT(__alignof(struct ifa_msghdr) >= sizeof(uint64_t));
	CTASSERT(__alignof(struct if_msghdr) >= sizeof(uint64_t));
	CTASSERT(__alignof(struct if_announcemsghdr) >= sizeof(uint64_t));
	CTASSERT(__alignof(struct rt_msghdr) >= sizeof(uint64_t));
#endif

	switch (type) {
	case RTM_DELADDR:
	case RTM_NEWADDR:
	case RTM_CHGADDR:
		return sizeof(struct ifa_xmsghdr);

	case RTM_OOIFINFO:
#ifdef COMPAT_14
		return sizeof(struct if_msghdr14);
#else
#ifdef DIAGNOSTIC
		printf("RTM_OOIFINFO\n");
#endif
		return -1;
#endif
	case RTM_OIFINFO:
#ifdef COMPAT_50
		return sizeof(struct if_msghdr50);
#else
#ifdef DIAGNOSTIC
		printf("RTM_OIFINFO\n");
#endif
		return -1;
#endif

	case RTM_IFINFO:
		return sizeof(struct if_xmsghdr);

	case RTM_IFANNOUNCE:
	case RTM_IEEE80211:
		return sizeof(struct if_xannouncemsghdr);

	default:
		return sizeof(struct rt_xmsghdr);
	}
}


struct mbuf *
COMPATNAME(rt_msg1)(int type, struct rt_addrinfo *rtinfo, void *data, int datalen)
{
	struct rt_xmsghdr *rtm;
	struct mbuf *m;
	int i;
	const struct sockaddr *sa;
	int len, dlen;

	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return m;
	MCLAIM(m, &COMPATNAME(routedomain).dom_mowner);

	if ((len = rt_getlen(type)) == -1)
		goto out;
	if (len > MHLEN + MLEN)
		panic("%s: message too long", __func__);
	else if (len > MHLEN) {
		m->m_next = m_get(M_DONTWAIT, MT_DATA);
		if (m->m_next == NULL)
			goto out;
		MCLAIM(m->m_next, m->m_owner);
		m->m_pkthdr.len = len;
		m->m_len = MHLEN;
		m->m_next->m_len = len - MHLEN;
	} else {
		m->m_pkthdr.len = m->m_len = len;
	}
	m->m_pkthdr.rcvif = NULL;
	m_copyback(m, 0, datalen, data);
	if (len > datalen)
		(void)memset(mtod(m, char *) + datalen, 0, len - datalen);
	rtm = mtod(m, struct rt_xmsghdr *);
	for (i = 0; i < RTAX_MAX; i++) {
		if ((sa = rtinfo->rti_info[i]) == NULL)
			continue;
		rtinfo->rti_addrs |= (1 << i);
		dlen = RT_XROUNDUP(sa->sa_len);
		m_copyback(m, len, sa->sa_len, sa);
		if (dlen != sa->sa_len) {
			/*
			 * Up to 6 + 1 nul's since roundup is to
			 * sizeof(uint64_t) (8 bytes)
			 */
			m_copyback(m, len + sa->sa_len,
			    dlen - sa->sa_len, "\0\0\0\0\0\0");
		}
		len += dlen;
	}
	if (m->m_pkthdr.len != len)
		goto out;
	rtm->rtm_msglen = len;
	rtm->rtm_version = RTM_XVERSION;
	rtm->rtm_type = type;
	return m;
out:
	m_freem(m);
	return NULL;
}

/*
 * rt_msg2
 *
 *	 fills 'cp' or 'w'.w_tmem with the routing socket message and
 *		returns the length of the message in 'lenp'.
 *
 * if walkarg is 0, cp is expected to be 0 or a buffer large enough to hold
 *	the message
 * otherwise walkarg's w_needed is updated and if the user buffer is
 *	specified and w_needed indicates space exists the information is copied
 *	into the temp space (w_tmem). w_tmem is [re]allocated if necessary,
 *	if the allocation fails ENOBUFS is returned.
 */
static int
rt_msg2(int type, struct rt_addrinfo *rtinfo, void *cpv, struct rt_walkarg *w,
	int *lenp)
{
	int i;
	int len, dlen, second_time = 0;
	char *cp0, *cp = cpv;

	rtinfo->rti_addrs = 0;
again:
	if ((len = rt_getlen(type)) == -1)
		return EINVAL;

	if ((cp0 = cp) != NULL)
		cp += len;
	for (i = 0; i < RTAX_MAX; i++) {
		const struct sockaddr *sa;

		if ((sa = rtinfo->rti_info[i]) == NULL)
			continue;
		rtinfo->rti_addrs |= (1 << i);
		dlen = RT_XROUNDUP(sa->sa_len);
		if (cp) {
			int diff = dlen - sa->sa_len;
			(void)memcpy(cp, sa, (size_t)sa->sa_len);
			cp += sa->sa_len;
			if (diff > 0) {
				(void)memset(cp, 0, (size_t)diff);
				cp += diff;
			}
		}
		len += dlen;
	}
	if (cp == NULL && w != NULL && !second_time) {
		struct rt_walkarg *rw = w;

		rw->w_needed += len;
		if (rw->w_needed <= 0 && rw->w_where) {
			if (rw->w_tmemsize < len) {
				if (rw->w_tmem)
					free(rw->w_tmem, M_RTABLE);
				rw->w_tmem = malloc(len, M_RTABLE, M_NOWAIT);
				if (rw->w_tmem)
					rw->w_tmemsize = len;
				else
					rw->w_tmemsize = 0;
			}
			if (rw->w_tmem) {
				cp = rw->w_tmem;
				second_time = 1;
				goto again;
			} else {
				rw->w_tmemneeded = len;
				return ENOBUFS;
			}
		}
	}
	if (cp) {
		struct rt_xmsghdr *rtm = (struct rt_xmsghdr *)cp0;

		rtm->rtm_version = RTM_XVERSION;
		rtm->rtm_type = type;
		rtm->rtm_msglen = len;
	}
	if (lenp)
		*lenp = len;
	return 0;
}

/*
 * This routine is called to generate a message from the routing
 * socket indicating that a redirect has occurred, a routing lookup
 * has failed, or that a protocol has detected timeouts to a particular
 * destination.
 */
void
COMPATNAME(rt_missmsg)(int type, const struct rt_addrinfo *rtinfo, int flags,
    int error)
{
	struct rt_xmsghdr rtm;
	struct mbuf *m;
	const struct sockaddr *sa = rtinfo->rti_info[RTAX_DST];
	struct rt_addrinfo info = *rtinfo;

	COMPATCALL(rt_missmsg, (type, rtinfo, flags, error));
	if (COMPATNAME(route_info).ri_cb.any_count == 0)
		return;
	memset(&rtm, 0, sizeof(rtm));
	rtm.rtm_flags = RTF_DONE | flags;
	rtm.rtm_errno = error;
	m = COMPATNAME(rt_msg1)(type, &info, &rtm, sizeof(rtm));
	if (m == NULL)
		return;
	mtod(m, struct rt_xmsghdr *)->rtm_addrs = info.rti_addrs;
	COMPATNAME(route_enqueue)(m, sa ? sa->sa_family : 0);
}

/*
 * This routine is called to generate a message from the routing
 * socket indicating that the status of a network interface has changed.
 */
void
COMPATNAME(rt_ifmsg)(struct ifnet *ifp)
{
	struct if_xmsghdr ifm;
	struct mbuf *m;
	struct rt_addrinfo info;

	COMPATCALL(rt_ifmsg, (ifp));
	if (COMPATNAME(route_info).ri_cb.any_count == 0)
		return;
	(void)memset(&info, 0, sizeof(info));
	(void)memset(&ifm, 0, sizeof(ifm));
	ifm.ifm_index = ifp->if_index;
	ifm.ifm_flags = ifp->if_flags;
	ifm.ifm_data = ifp->if_data;
	ifm.ifm_addrs = 0;
	m = COMPATNAME(rt_msg1)(RTM_IFINFO, &info, &ifm, sizeof(ifm));
	if (m == NULL)
		return;
	COMPATNAME(route_enqueue)(m, 0);
#ifdef COMPAT_14
	compat_14_rt_oifmsg(ifp);
#endif
#ifdef COMPAT_50
	compat_50_rt_oifmsg(ifp);
#endif
}


/*
 * This is called to generate messages from the routing socket
 * indicating a network interface has had addresses associated with it.
 * if we ever reverse the logic and replace messages TO the routing
 * socket indicate a request to configure interfaces, then it will
 * be unnecessary as the routing socket will automatically generate
 * copies of it.
 */
void
COMPATNAME(rt_newaddrmsg)(int cmd, struct ifaddr *ifa, int error,
    struct rtentry *rt)
{
#define	cmdpass(__cmd, __pass)	(((__cmd) << 2) | (__pass))
	struct rt_addrinfo info;
	const struct sockaddr *sa;
	int pass;
	struct mbuf *m;
	struct ifnet *ifp;
	struct rt_xmsghdr rtm;
	struct ifa_xmsghdr ifam;
	int ncmd;

	KASSERT(ifa != NULL);
	ifp = ifa->ifa_ifp;
	COMPATCALL(rt_newaddrmsg, (cmd, ifa, error, rt));
	if (COMPATNAME(route_info).ri_cb.any_count == 0)
		return;
	for (pass = 1; pass < 3; pass++) {
		memset(&info, 0, sizeof(info));
		switch (cmdpass(cmd, pass)) {
		case cmdpass(RTM_ADD, 1):
		case cmdpass(RTM_CHANGE, 1):
		case cmdpass(RTM_DELETE, 2):
		case cmdpass(RTM_NEWADDR, 1):
		case cmdpass(RTM_DELADDR, 1):
		case cmdpass(RTM_CHGADDR, 1):
			switch (cmd) {
			case RTM_ADD:
				ncmd = RTM_NEWADDR;
				break;
			case RTM_DELETE:
				ncmd = RTM_DELADDR;
				break;
			case RTM_CHANGE:
				ncmd = RTM_CHGADDR;
				break;
			default:
				ncmd = cmd;
			}
			info.rti_info[RTAX_IFA] = sa = ifa->ifa_addr;
			KASSERT(ifp->if_dl != NULL);
			info.rti_info[RTAX_IFP] = ifp->if_dl->ifa_addr;
			info.rti_info[RTAX_NETMASK] = ifa->ifa_netmask;
			info.rti_info[RTAX_BRD] = ifa->ifa_dstaddr;
			memset(&ifam, 0, sizeof(ifam));
			ifam.ifam_index = ifp->if_index;
			ifam.ifam_metric = ifa->ifa_metric;
			ifam.ifam_flags = ifa->ifa_flags;
			m = COMPATNAME(rt_msg1)(ncmd, &info, &ifam, sizeof(ifam));
			if (m == NULL)
				continue;
			mtod(m, struct ifa_xmsghdr *)->ifam_addrs =
			    info.rti_addrs;
			break;
		case cmdpass(RTM_ADD, 2):
		case cmdpass(RTM_CHANGE, 2):
		case cmdpass(RTM_DELETE, 1):
			if (rt == NULL)
				continue;
			info.rti_info[RTAX_NETMASK] = rt_mask(rt);
			info.rti_info[RTAX_DST] = sa = rt_getkey(rt);
			info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
			memset(&rtm, 0, sizeof(rtm));
			rtm.rtm_index = ifp->if_index;
			rtm.rtm_flags |= rt->rt_flags;
			rtm.rtm_errno = error;
			m = COMPATNAME(rt_msg1)(cmd, &info, &rtm, sizeof(rtm));
			if (m == NULL)
				continue;
			mtod(m, struct rt_xmsghdr *)->rtm_addrs = info.rti_addrs;
			break;
		default:
			continue;
		}
#ifdef DIAGNOSTIC
		if (m == NULL)
			panic("%s: called with wrong command", __func__);
#endif
		COMPATNAME(route_enqueue)(m, sa ? sa->sa_family : 0);
	}
#undef cmdpass
}

static struct mbuf *
rt_makeifannouncemsg(struct ifnet *ifp, int type, int what,
    struct rt_addrinfo *info)
{
	struct if_xannouncemsghdr ifan;

	memset(info, 0, sizeof(*info));
	memset(&ifan, 0, sizeof(ifan));
	ifan.ifan_index = ifp->if_index;
	strlcpy(ifan.ifan_name, ifp->if_xname, sizeof(ifan.ifan_name));
	ifan.ifan_what = what;
	return COMPATNAME(rt_msg1)(type, info, &ifan, sizeof(ifan));
}

/*
 * This is called to generate routing socket messages indicating
 * network interface arrival and departure.
 */
void
COMPATNAME(rt_ifannouncemsg)(struct ifnet *ifp, int what)
{
	struct mbuf *m;
	struct rt_addrinfo info;

	COMPATCALL(rt_ifannouncemsg, (ifp, what));
	if (COMPATNAME(route_info).ri_cb.any_count == 0)
		return;
	m = rt_makeifannouncemsg(ifp, RTM_IFANNOUNCE, what, &info);
	if (m == NULL)
		return;
	COMPATNAME(route_enqueue)(m, 0);
}

/*
 * This is called to generate routing socket messages indicating
 * IEEE80211 wireless events.
 * XXX we piggyback on the RTM_IFANNOUNCE msg format in a clumsy way.
 */
void
COMPATNAME(rt_ieee80211msg)(struct ifnet *ifp, int what, void *data,
	size_t data_len)
{
	struct mbuf *m;
	struct rt_addrinfo info;

	COMPATCALL(rt_ieee80211msg, (ifp, what, data, data_len));
	if (COMPATNAME(route_info).ri_cb.any_count == 0)
		return;
	m = rt_makeifannouncemsg(ifp, RTM_IEEE80211, what, &info);
	if (m == NULL)
		return;
	/*
	 * Append the ieee80211 data.  Try to stick it in the
	 * mbuf containing the ifannounce msg; otherwise allocate
	 * a new mbuf and append.
	 *
	 * NB: we assume m is a single mbuf.
	 */
	if (data_len > M_TRAILINGSPACE(m)) {
		struct mbuf *n = m_get(M_NOWAIT, MT_DATA);
		if (n == NULL) {
			m_freem(m);
			return;
		}
		(void)memcpy(mtod(n, void *), data, data_len);
		n->m_len = data_len;
		m->m_next = n;
	} else if (data_len > 0) {
		(void)memcpy(mtod(m, uint8_t *) + m->m_len, data, data_len);
		m->m_len += data_len;
	}
	if (m->m_flags & M_PKTHDR)
		m->m_pkthdr.len += data_len;
	mtod(m, struct if_xannouncemsghdr *)->ifan_msglen += data_len;
	COMPATNAME(route_enqueue)(m, 0);
}

/*
 * This is used in dumping the kernel table via sysctl().
 */
static int
sysctl_dumpentry(struct rtentry *rt, void *v)
{
	struct rt_walkarg *w = v;
	int error = 0, size;
	struct rt_addrinfo info;

	if (w->w_op == NET_RT_FLAGS && !(rt->rt_flags & w->w_arg))
		return 0;
	memset(&info, 0, sizeof(info));
	info.rti_info[RTAX_DST] = rt_getkey(rt);
	info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
	info.rti_info[RTAX_NETMASK] = rt_mask(rt);
	info.rti_info[RTAX_TAG] = rt_gettag(rt);
	if (rt->rt_ifp) {
		const struct ifaddr *rtifa;
		info.rti_info[RTAX_IFP] = rt->rt_ifp->if_dl->ifa_addr;
		/* rtifa used to be simply rt->rt_ifa.  If rt->rt_ifa != NULL,
		 * then rt_get_ifa() != NULL.  So this ought to still be safe.
		 * --dyoung
		 */
		rtifa = rt_get_ifa(rt);
		info.rti_info[RTAX_IFA] = rtifa->ifa_addr;
		if (rt->rt_ifp->if_flags & IFF_POINTOPOINT)
			info.rti_info[RTAX_BRD] = rtifa->ifa_dstaddr;
	}
	if ((error = rt_msg2(RTM_GET, &info, 0, w, &size)))
		return error;
	if (w->w_where && w->w_tmem && w->w_needed <= 0) {
		struct rt_xmsghdr *rtm = (struct rt_xmsghdr *)w->w_tmem;

		rtm->rtm_flags = rt->rt_flags;
		rtm->rtm_use = rt->rt_use;
		rtm_setmetrics(rt, rtm);
		KASSERT(rt->rt_ifp != NULL);
		rtm->rtm_index = rt->rt_ifp->if_index;
		rtm->rtm_errno = rtm->rtm_pid = rtm->rtm_seq = 0;
		rtm->rtm_addrs = info.rti_addrs;
		if ((error = copyout(rtm, w->w_where, size)) != 0)
			w->w_where = NULL;
		else
			w->w_where = (char *)w->w_where + size;
	}
	return error;
}

static int
sysctl_iflist(int af, struct rt_walkarg *w, int type)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;
	struct	rt_addrinfo info;
	int	len, error = 0;

	memset(&info, 0, sizeof(info));
	IFNET_FOREACH(ifp) {
		if (w->w_arg && w->w_arg != ifp->if_index)
			continue;
		if (IFADDR_EMPTY(ifp))
			continue;
		info.rti_info[RTAX_IFP] = ifp->if_dl->ifa_addr;
		switch (type) {
		case NET_RT_IFLIST:
			error = rt_msg2(RTM_IFINFO, &info, NULL, w, &len);
			break;
#ifdef COMPAT_14
		case NET_RT_OOIFLIST:
			error = rt_msg2(RTM_OOIFINFO, &info, NULL, w, &len);
			break;
#endif
#ifdef COMPAT_50
		case NET_RT_OIFLIST:
			error = rt_msg2(RTM_OIFINFO, &info, NULL, w, &len);
			break;
#endif
		default:
			panic("sysctl_iflist(1)");
		}
		if (error)
			return error;
		info.rti_info[RTAX_IFP] = NULL;
		if (w->w_where && w->w_tmem && w->w_needed <= 0) {
			switch (type) {
			case NET_RT_IFLIST: {
				struct if_xmsghdr *ifm;

				ifm = (struct if_xmsghdr *)w->w_tmem;
				ifm->ifm_index = ifp->if_index;
				ifm->ifm_flags = ifp->if_flags;
				ifm->ifm_data = ifp->if_data;
				ifm->ifm_addrs = info.rti_addrs;
				error = copyout(ifm, w->w_where, len);
				if (error)
					return error;
				w->w_where = (char *)w->w_where + len;
				break;
			}

#ifdef COMPAT_14
			case NET_RT_OOIFLIST:
				error = compat_14_iflist(ifp, w, &info, len);
				if (error)
					return error;
				break;
#endif
#ifdef COMPAT_50
			case NET_RT_OIFLIST:
				error = compat_50_iflist(ifp, w, &info, len);
				if (error)
					return error;
				break;
#endif
			default:
				panic("sysctl_iflist(2)");
			}
		}
		IFADDR_FOREACH(ifa, ifp) {
			if (af && af != ifa->ifa_addr->sa_family)
				continue;
			info.rti_info[RTAX_IFA] = ifa->ifa_addr;
			info.rti_info[RTAX_NETMASK] = ifa->ifa_netmask;
			info.rti_info[RTAX_BRD] = ifa->ifa_dstaddr;
			if ((error = rt_msg2(RTM_NEWADDR, &info, 0, w, &len)))
				return error;
			if (w->w_where && w->w_tmem && w->w_needed <= 0) {
				struct ifa_xmsghdr *ifam;

				ifam = (struct ifa_xmsghdr *)w->w_tmem;
				ifam->ifam_index = ifa->ifa_ifp->if_index;
				ifam->ifam_flags = ifa->ifa_flags;
				ifam->ifam_metric = ifa->ifa_metric;
				ifam->ifam_addrs = info.rti_addrs;
				error = copyout(w->w_tmem, w->w_where, len);
				if (error)
					return error;
				w->w_where = (char *)w->w_where + len;
			}
		}
		info.rti_info[RTAX_IFA] = info.rti_info[RTAX_NETMASK] =
		    info.rti_info[RTAX_BRD] = NULL;
	}
	return 0;
}

static int
sysctl_rtable(SYSCTLFN_ARGS)
{
	void 	*where = oldp;
	size_t	*given = oldlenp;
	int	i, s, error = EINVAL;
	u_char  af;
	struct	rt_walkarg w;

	if (namelen == 1 && name[0] == CTL_QUERY)
		return sysctl_query(SYSCTLFN_CALL(rnode));

	if (newp)
		return EPERM;
	if (namelen != 3)
		return EINVAL;
	af = name[0];
	w.w_tmemneeded = 0;
	w.w_tmemsize = 0;
	w.w_tmem = NULL;
again:
	/* we may return here if a later [re]alloc of the t_mem buffer fails */
	if (w.w_tmemneeded) {
		w.w_tmem = malloc(w.w_tmemneeded, M_RTABLE, M_WAITOK);
		w.w_tmemsize = w.w_tmemneeded;
		w.w_tmemneeded = 0;
	}
	w.w_op = name[1];
	w.w_arg = name[2];
	w.w_given = *given;
	w.w_needed = 0 - w.w_given;
	w.w_where = where;

	s = splsoftnet();
	switch (w.w_op) {

	case NET_RT_DUMP:
	case NET_RT_FLAGS:
		for (i = 1; i <= AF_MAX; i++)
			if ((af == 0 || af == i) &&
			    (error = rt_walktree(i, sysctl_dumpentry, &w)))
				break;
		break;

#ifdef COMPAT_14
	case NET_RT_OOIFLIST:
		error = sysctl_iflist(af, &w, w.w_op);
		break;
#endif
#ifdef COMPAT_50
	case NET_RT_OIFLIST:
		error = sysctl_iflist(af, &w, w.w_op);
		break;
#endif
	case NET_RT_IFLIST:
		error = sysctl_iflist(af, &w, w.w_op);
		break;
	}
	splx(s);

	/* check to see if we couldn't allocate memory with NOWAIT */
	if (error == ENOBUFS && w.w_tmem == 0 && w.w_tmemneeded)
		goto again;

	if (w.w_tmem)
		free(w.w_tmem, M_RTABLE);
	w.w_needed += w.w_given;
	if (where) {
		*given = (char *)w.w_where - (char *)where;
		if (*given < w.w_needed)
			return ENOMEM;
	} else {
		*given = (11 * w.w_needed) / 10;
	}
	return error;
}

/*
 * Routing message software interrupt routine
 */
static void
COMPATNAME(route_intr)(void *cookie)
{
	struct sockproto proto = { .sp_family = PF_XROUTE, };
	struct route_info * const ri = &COMPATNAME(route_info);
	struct mbuf *m;
	int s;

	mutex_enter(softnet_lock);
	KERNEL_LOCK(1, NULL);
	while (!IF_IS_EMPTY(&ri->ri_intrq)) {
		s = splnet();
		IF_DEQUEUE(&ri->ri_intrq, m);
		splx(s);
		if (m == NULL)
			break;
		proto.sp_protocol = M_GETCTX(m, uintptr_t);
		raw_input(m, &proto, &ri->ri_src, &ri->ri_dst);
	}
	KERNEL_UNLOCK_ONE(NULL);
	mutex_exit(softnet_lock);
}

/*
 * Enqueue a message to the software interrupt routine.
 */
void
COMPATNAME(route_enqueue)(struct mbuf *m, int family)
{
	struct route_info * const ri = &COMPATNAME(route_info);
	int s, wasempty;

	s = splnet();
	if (IF_QFULL(&ri->ri_intrq)) {
		IF_DROP(&ri->ri_intrq);
		m_freem(m);
	} else {
		wasempty = IF_IS_EMPTY(&ri->ri_intrq);
		M_SETCTX(m, (uintptr_t)family);
		IF_ENQUEUE(&ri->ri_intrq, m);
		if (wasempty)
			softint_schedule(ri->ri_sih);
	}
	splx(s);
}

static void
COMPATNAME(route_init)(void)
{
	struct route_info * const ri = &COMPATNAME(route_info);

#ifndef COMPAT_RTSOCK
	rt_init();
#endif

	sysctl_net_route_setup(NULL);
	ri->ri_intrq.ifq_maxlen = ri->ri_maxqlen;
	ri->ri_sih = softint_establish(SOFTINT_NET | SOFTINT_MPSAFE,
	    COMPATNAME(route_intr), NULL);
}

/*
 * Definitions of protocols supported in the ROUTE domain.
 */
#ifndef COMPAT_RTSOCK
PR_WRAP_USRREQS(route);
#else
PR_WRAP_USRREQS(compat_50_route);
#endif

static const struct pr_usrreqs route_usrreqs = {
	.pr_attach	= COMPATNAME(route_attach_wrapper),
	.pr_detach	= COMPATNAME(route_detach_wrapper),
	.pr_accept	= COMPATNAME(route_accept_wrapper),
	.pr_bind	= COMPATNAME(route_bind_wrapper),
	.pr_listen	= COMPATNAME(route_listen_wrapper),
	.pr_connect	= COMPATNAME(route_connect_wrapper),
	.pr_connect2	= COMPATNAME(route_connect2_wrapper),
	.pr_disconnect	= COMPATNAME(route_disconnect_wrapper),
	.pr_shutdown	= COMPATNAME(route_shutdown_wrapper),
	.pr_abort	= COMPATNAME(route_abort_wrapper),
	.pr_ioctl	= COMPATNAME(route_ioctl_wrapper),
	.pr_stat	= COMPATNAME(route_stat_wrapper),
	.pr_peeraddr	= COMPATNAME(route_peeraddr_wrapper),
	.pr_sockaddr	= COMPATNAME(route_sockaddr_wrapper),
	.pr_rcvd	= COMPATNAME(route_rcvd_wrapper),
	.pr_recvoob	= COMPATNAME(route_recvoob_wrapper),
	.pr_send	= COMPATNAME(route_send_wrapper),
	.pr_sendoob	= COMPATNAME(route_sendoob_wrapper),
	.pr_purgeif	= COMPATNAME(route_purgeif_wrapper),
};

static const struct protosw COMPATNAME(route_protosw)[] = {
	{
		.pr_type = SOCK_RAW,
		.pr_domain = &COMPATNAME(routedomain),
		.pr_flags = PR_ATOMIC|PR_ADDR,
		.pr_input = raw_input,
		.pr_output = COMPATNAME(route_output),
		.pr_ctlinput = raw_ctlinput,
		.pr_usrreqs = &route_usrreqs,
		.pr_init = raw_init,
	},
};

struct domain COMPATNAME(routedomain) = {
	.dom_family = PF_XROUTE,
	.dom_name = DOMAINNAME,
	.dom_init = COMPATNAME(route_init),
	.dom_protosw = COMPATNAME(route_protosw),
	.dom_protoswNPROTOSW =
	    &COMPATNAME(route_protosw)[__arraycount(COMPATNAME(route_protosw))],
};

static void
sysctl_net_route_setup(struct sysctllog **clog)
{
	const struct sysctlnode *rnode = NULL;

	sysctl_createv(clog, 0, NULL, &rnode,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, DOMAINNAME,
		       SYSCTL_DESCR("PF_ROUTE information"),
		       NULL, 0, NULL, 0,
		       CTL_NET, PF_XROUTE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "rtable",
		       SYSCTL_DESCR("Routing table information"),
		       sysctl_rtable, 0, NULL, 0,
		       CTL_NET, PF_XROUTE, 0 /* any protocol */, CTL_EOL);

	sysctl_createv(clog, 0, &rnode, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "stats",
		       SYSCTL_DESCR("Routing statistics"),
		       NULL, 0, &rtstat, sizeof(rtstat),
		       CTL_CREATE, CTL_EOL);
}
