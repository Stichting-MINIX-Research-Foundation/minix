/*	$NetBSD: raw_ip.c,v 1.153 2015/08/24 22:21:26 pooka Exp $	*/

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
 * Copyright (c) 1982, 1986, 1988, 1993
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
 *	@(#)raw_ip.c	8.7 (Berkeley) 5/15/95
 */

/*
 * Raw interface to IP protocol.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: raw_ip.c,v 1.153 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#include "opt_compat_netbsd.h"
#include "opt_ipsec.h"
#include "opt_mrouting.h"
#endif

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kauth.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_private.h>
#include <netinet/ip_mroute.h>
#include <netinet/ip_icmp.h>
#include <netinet/in_pcb.h>
#include <netinet/in_proto.h>
#include <netinet/in_var.h>

#ifdef IPSEC
#include <netipsec/ipsec.h>
#include <netipsec/ipsec_var.h>
#include <netipsec/ipsec_private.h>
#endif	/* IPSEC */

#ifdef COMPAT_50
#include <compat/sys/socket.h>
#endif

struct inpcbtable rawcbtable;

int	 rip_pcbnotify(struct inpcbtable *, struct in_addr,
    struct in_addr, int, int, void (*)(struct inpcb *, int));
static int	 rip_connect_pcb(struct inpcb *, struct sockaddr_in *);
static void	 rip_disconnect1(struct inpcb *);

static void sysctl_net_inet_raw_setup(struct sysctllog **);

/*
 * Nominal space allocated to a raw ip socket.
 */
#define	RIPSNDQ		8192
#define	RIPRCVQ		8192

static u_long		rip_sendspace = RIPSNDQ;
static u_long		rip_recvspace = RIPRCVQ;

/*
 * Raw interface to IP protocol.
 */

/*
 * Initialize raw connection block q.
 */
void
rip_init(void)
{

	sysctl_net_inet_raw_setup(NULL);
	in_pcbinit(&rawcbtable, 1, 1);
}

static void
rip_sbappendaddr(struct inpcb *last, struct ip *ip, const struct sockaddr *sa,
    int hlen, struct mbuf *opts, struct mbuf *n)
{
	if (last->inp_flags & INP_NOHEADER)
		m_adj(n, hlen);
	if (last->inp_flags & INP_CONTROLOPTS 
#ifdef SO_OTIMESTAMP
	    || last->inp_socket->so_options & SO_OTIMESTAMP
#endif
	    || last->inp_socket->so_options & SO_TIMESTAMP)
		ip_savecontrol(last, &opts, ip, n);
	if (sbappendaddr(&last->inp_socket->so_rcv, sa, n, opts) == 0) {
		/* should notify about lost packet */
		m_freem(n);
		if (opts)
			m_freem(opts);
	} else
		sorwakeup(last->inp_socket);
}

/*
 * Setup generic address and protocol structures
 * for raw_input routine, then pass them along with
 * mbuf chain.
 */
void
rip_input(struct mbuf *m, ...)
{
	int hlen, proto;
	struct ip *ip = mtod(m, struct ip *);
	struct inpcb_hdr *inph;
	struct inpcb *inp;
	struct inpcb *last = NULL;
	struct mbuf *n, *opts = NULL;
	struct sockaddr_in ripsrc;
	va_list ap;

	va_start(ap, m);
	(void)va_arg(ap, int);		/* ignore value, advance ap */
	proto = va_arg(ap, int);
	va_end(ap);

	sockaddr_in_init(&ripsrc, &ip->ip_src, 0);

	/*
	 * XXX Compatibility: programs using raw IP expect ip_len
	 * XXX to have the header length subtracted, and in host order.
	 * XXX ip_off is also expected to be host order.
	 */
	hlen = ip->ip_hl << 2;
	ip->ip_len = ntohs(ip->ip_len) - hlen;
	NTOHS(ip->ip_off);

	TAILQ_FOREACH(inph, &rawcbtable.inpt_queue, inph_queue) {
		inp = (struct inpcb *)inph;
		if (inp->inp_af != AF_INET)
			continue;
		if (inp->inp_ip.ip_p && inp->inp_ip.ip_p != proto)
			continue;
		if (!in_nullhost(inp->inp_laddr) &&
		    !in_hosteq(inp->inp_laddr, ip->ip_dst))
			continue;
		if (!in_nullhost(inp->inp_faddr) &&
		    !in_hosteq(inp->inp_faddr, ip->ip_src))
			continue;
		if (last == NULL)
			;
#if defined(IPSEC)
		/* check AH/ESP integrity. */
		else if (ipsec_used &&
		    ipsec4_in_reject_so(m, last->inp_socket)) {
			IPSEC_STATINC(IPSEC_STAT_IN_POLVIO);
			/* do not inject data to pcb */
		}
#endif /*IPSEC*/
		else if ((n = m_copypacket(m, M_DONTWAIT)) != NULL) {
			rip_sbappendaddr(last, ip, sintosa(&ripsrc), hlen, opts,
			    n);
			opts = NULL;
		}
		last = inp;
	}
#if defined(IPSEC)
	/* check AH/ESP integrity. */
	if (ipsec_used && last != NULL
	    && ipsec4_in_reject_so(m, last->inp_socket)) {
		m_freem(m);
		IPSEC_STATINC(IPSEC_STAT_IN_POLVIO);
		IP_STATDEC(IP_STAT_DELIVERED);
		/* do not inject data to pcb */
	} else
#endif /*IPSEC*/
	if (last != NULL)
		rip_sbappendaddr(last, ip, sintosa(&ripsrc), hlen, opts, m);
	else if (inetsw[ip_protox[ip->ip_p]].pr_input == rip_input) {
		uint64_t *ips;

		icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_PROTOCOL,
		    0, 0);
		ips = IP_STAT_GETREF();
		ips[IP_STAT_NOPROTO]++;
		ips[IP_STAT_DELIVERED]--;
		IP_STAT_PUTREF();
	} else
		m_freem(m);
	return;
}

int
rip_pcbnotify(struct inpcbtable *table,
    struct in_addr faddr, struct in_addr laddr, int proto, int errno,
    void (*notify)(struct inpcb *, int))
{
	struct inpcb_hdr *inph, *ninph;
	int nmatch;

	nmatch = 0;
	TAILQ_FOREACH_SAFE(inph, &table->inpt_queue, inph_queue, ninph) {
		struct inpcb *inp = (struct inpcb *)inph;
		if (inp->inp_af != AF_INET)
			continue;
		if (inp->inp_ip.ip_p && inp->inp_ip.ip_p != proto)
			continue;
		if (in_hosteq(inp->inp_faddr, faddr) &&
		    in_hosteq(inp->inp_laddr, laddr)) {
			(*notify)(inp, errno);
			nmatch++;
		}
	}

	return nmatch;
}

void *
rip_ctlinput(int cmd, const struct sockaddr *sa, void *v)
{
	struct ip *ip = v;
	void (*notify)(struct inpcb *, int) = in_rtchange;
	int errno;

	if (sa->sa_family != AF_INET ||
	    sa->sa_len != sizeof(struct sockaddr_in))
		return NULL;
	if ((unsigned)cmd >= PRC_NCMDS)
		return NULL;
	errno = inetctlerrmap[cmd];
	if (PRC_IS_REDIRECT(cmd))
		notify = in_rtchange, ip = 0;
	else if (cmd == PRC_HOSTDEAD)
		ip = 0;
	else if (errno == 0)
		return NULL;
	if (ip) {
		rip_pcbnotify(&rawcbtable, satocsin(sa)->sin_addr,
		    ip->ip_src, ip->ip_p, errno, notify);

		/* XXX mapped address case */
	} else
		in_pcbnotifyall(&rawcbtable, satocsin(sa)->sin_addr, errno,
		    notify);
	return NULL;
}

/*
 * Generate IP header and pass packet to ip_output.
 * Tack on options user may have setup with control call.
 */
int
rip_output(struct mbuf *m, ...)
{
	struct inpcb *inp;
	struct ip *ip;
	struct mbuf *opts;
	int flags;
	va_list ap;

	va_start(ap, m);
	inp = va_arg(ap, struct inpcb *);
	va_end(ap);

	flags =
	    (inp->inp_socket->so_options & SO_DONTROUTE) | IP_ALLOWBROADCAST
	    | IP_RETURNMTU;

	/*
	 * If the user handed us a complete IP packet, use it.
	 * Otherwise, allocate an mbuf for a header and fill it in.
	 */
	if ((inp->inp_flags & INP_HDRINCL) == 0) {
		if ((m->m_pkthdr.len + sizeof(struct ip)) > IP_MAXPACKET) {
			m_freem(m);
			return (EMSGSIZE);
		}
		M_PREPEND(m, sizeof(struct ip), M_DONTWAIT);
		if (!m)
			return (ENOBUFS);
		ip = mtod(m, struct ip *);
		ip->ip_tos = 0;
		ip->ip_off = htons(0);
		ip->ip_p = inp->inp_ip.ip_p;
		ip->ip_len = htons(m->m_pkthdr.len);
		ip->ip_src = inp->inp_laddr;
		ip->ip_dst = inp->inp_faddr;
		ip->ip_ttl = MAXTTL;
		opts = inp->inp_options;
	} else {
		if (m->m_pkthdr.len > IP_MAXPACKET) {
			m_freem(m);
			return (EMSGSIZE);
		}
		ip = mtod(m, struct ip *);

		/*
		 * If the mbuf is read-only, we need to allocate
		 * a new mbuf for the header, since we need to
		 * modify the header.
		 */
		if (M_READONLY(m)) {
			int hlen = ip->ip_hl << 2;

			m = m_copyup(m, hlen, (max_linkhdr + 3) & ~3);
			if (m == NULL)
				return (ENOMEM);	/* XXX */
			ip = mtod(m, struct ip *);
		}

		/* XXX userland passes ip_len and ip_off in host order */
		if (m->m_pkthdr.len != ip->ip_len) {
			m_freem(m);
			return (EINVAL);
		}
		HTONS(ip->ip_len);
		HTONS(ip->ip_off);
		if (ip->ip_id != 0 || m->m_pkthdr.len < IP_MINFRAGSIZE)
			flags |= IP_NOIPNEWID;
		opts = NULL;
		/* XXX prevent ip_output from overwriting header fields */
		flags |= IP_RAWOUTPUT;
		IP_STATINC(IP_STAT_RAWOUT);
	}

	/*
	 * IP output.  Note: if IP_RETURNMTU flag is set, the MTU size
	 * will be stored in inp_errormtu.
	 */
	return ip_output(m, opts, &inp->inp_route, flags, inp->inp_moptions,
	     inp->inp_socket);
}

/*
 * Raw IP socket option processing.
 */
int
rip_ctloutput(int op, struct socket *so, struct sockopt *sopt)
{
	struct inpcb *inp = sotoinpcb(so);
	int error = 0;
	int optval;

	if (sopt->sopt_level == SOL_SOCKET && sopt->sopt_name == SO_NOHEADER) {
		if (op == PRCO_GETOPT) {
			optval = (inp->inp_flags & INP_NOHEADER) ? 1 : 0;
			error = sockopt_set(sopt, &optval, sizeof(optval));
		} else if (op == PRCO_SETOPT) {
			error = sockopt_getint(sopt, &optval);
			if (error)
				goto out;
			if (optval) {
				inp->inp_flags &= ~INP_HDRINCL;
				inp->inp_flags |= INP_NOHEADER;
			} else
				inp->inp_flags &= ~INP_NOHEADER;
		}
		goto out;
	} else if (sopt->sopt_level != IPPROTO_IP)
		return ip_ctloutput(op, so, sopt);

	switch (op) {

	case PRCO_SETOPT:
		switch (sopt->sopt_name) {
		case IP_HDRINCL:
			error = sockopt_getint(sopt, &optval);
			if (error)
				break;
			if (optval)
				inp->inp_flags |= INP_HDRINCL;
			else
				inp->inp_flags &= ~INP_HDRINCL;
			break;

#ifdef MROUTING
		case MRT_INIT:
		case MRT_DONE:
		case MRT_ADD_VIF:
		case MRT_DEL_VIF:
		case MRT_ADD_MFC:
		case MRT_DEL_MFC:
		case MRT_ASSERT:
		case MRT_API_CONFIG:
		case MRT_ADD_BW_UPCALL:
		case MRT_DEL_BW_UPCALL:
			error = ip_mrouter_set(so, sopt);
			break;
#endif

		default:
			error = ip_ctloutput(op, so, sopt);
			break;
		}
		break;

	case PRCO_GETOPT:
		switch (sopt->sopt_name) {
		case IP_HDRINCL:
			optval = inp->inp_flags & INP_HDRINCL;
			error = sockopt_set(sopt, &optval, sizeof(optval));
			break;

#ifdef MROUTING
		case MRT_VERSION:
		case MRT_ASSERT:
		case MRT_API_SUPPORT:
		case MRT_API_CONFIG:
			error = ip_mrouter_get(so, sopt);
			break;
#endif

		default:
			error = ip_ctloutput(op, so, sopt);
			break;
		}
		break;
	}
 out:
	return error;
}

int
rip_connect_pcb(struct inpcb *inp, struct sockaddr_in *addr)
{

	if (IFNET_EMPTY())
		return (EADDRNOTAVAIL);
	if (addr->sin_family != AF_INET)
		return (EAFNOSUPPORT);
	inp->inp_faddr = addr->sin_addr;
	return (0);
}

static void
rip_disconnect1(struct inpcb *inp)
{

	inp->inp_faddr = zeroin_addr;
}

static int
rip_attach(struct socket *so, int proto)
{
	struct inpcb *inp;
	int error;

	KASSERT(sotoinpcb(so) == NULL);
	sosetlock(so);

	if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
		error = soreserve(so, rip_sendspace, rip_recvspace);
		if (error) {
			return error;
		}
	}

	error = in_pcballoc(so, &rawcbtable);
	if (error) {
		return error;
	}
	inp = sotoinpcb(so);
	inp->inp_ip.ip_p = proto;
	KASSERT(solocked(so));

	return 0;
}

static void
rip_detach(struct socket *so)
{
	struct inpcb *inp;

	KASSERT(solocked(so));
	inp = sotoinpcb(so);
	KASSERT(inp != NULL);

#ifdef MROUTING
	extern struct socket *ip_mrouter;
	if (so == ip_mrouter) {
		ip_mrouter_done();
	}
#endif
	in_pcbdetach(inp);
}

static int
rip_accept(struct socket *so, struct sockaddr *nam)
{
	KASSERT(solocked(so));

	panic("rip_accept");

	return EOPNOTSUPP;
}

static int
rip_bind(struct socket *so, struct sockaddr *nam, struct lwp *l)
{
	struct inpcb *inp = sotoinpcb(so);
	struct sockaddr_in *addr = (struct sockaddr_in *)nam;
	int error = 0;
	int s;
	struct ifaddr *ia;

	KASSERT(solocked(so));
	KASSERT(inp != NULL);
	KASSERT(nam != NULL);

	if (addr->sin_len != sizeof(*addr))
		return EINVAL;

	s = splsoftnet();
	if (IFNET_EMPTY()) {
		error = EADDRNOTAVAIL;
		goto release;
	}
	if (addr->sin_family != AF_INET) {
		error = EAFNOSUPPORT;
		goto release;
	}
	if ((ia = ifa_ifwithaddr(sintosa(addr))) == 0 &&
	    !in_nullhost(addr->sin_addr))
	{
		error = EADDRNOTAVAIL;
		goto release;
	}
        if (ia && ((struct in_ifaddr *)ia)->ia4_flags &
	            (IN6_IFF_NOTREADY | IN_IFF_DETACHED))
	{
		error = EADDRNOTAVAIL;
		goto release;
	}

	inp->inp_laddr = addr->sin_addr;

release:
	splx(s);
	return error;
}

static int
rip_listen(struct socket *so, struct lwp *l)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
rip_connect(struct socket *so, struct sockaddr *nam, struct lwp *l)
{
	struct inpcb *inp = sotoinpcb(so);
	int error = 0;
	int s;

	KASSERT(solocked(so));
	KASSERT(inp != NULL);
	KASSERT(nam != NULL);

	s = splsoftnet();
	error = rip_connect_pcb(inp, (struct sockaddr_in *)nam);
	if (! error)
		soisconnected(so);
	splx(s);

	return error;
}

static int
rip_connect2(struct socket *so, struct socket *so2)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
rip_disconnect(struct socket *so)
{
	struct inpcb *inp = sotoinpcb(so);
	int s;

	KASSERT(solocked(so));
	KASSERT(inp != NULL);

	s = splsoftnet();
	soisdisconnected(so);
	rip_disconnect1(inp);
	splx(s);

	return 0;
}

static int
rip_shutdown(struct socket *so)
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
rip_abort(struct socket *so)
{
	KASSERT(solocked(so));

	panic("rip_abort");

	return EOPNOTSUPP;
}

static int
rip_ioctl(struct socket *so, u_long cmd, void *nam, struct ifnet *ifp)
{
	return in_control(so, cmd, nam, ifp);
}

static int
rip_stat(struct socket *so, struct stat *ub)
{
	KASSERT(solocked(so));

	/* stat: don't bother with a blocksize. */
	return 0;
}

static int
rip_peeraddr(struct socket *so, struct sockaddr *nam)
{
	int s;

	KASSERT(solocked(so));
	KASSERT(sotoinpcb(so) != NULL);
	KASSERT(nam != NULL);

	s = splsoftnet();
	in_setpeeraddr(sotoinpcb(so), (struct sockaddr_in *)nam);
	splx(s);

	return 0;
}

static int
rip_sockaddr(struct socket *so, struct sockaddr *nam)
{
	int s;

	KASSERT(solocked(so));
	KASSERT(sotoinpcb(so) != NULL);
	KASSERT(nam != NULL);

	s = splsoftnet();
	in_setsockaddr(sotoinpcb(so), (struct sockaddr_in *)nam);
	splx(s);

	return 0;
}

static int
rip_rcvd(struct socket *so, int flags, struct lwp *l)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
rip_recvoob(struct socket *so, struct mbuf *m, int flags)
{
	KASSERT(solocked(so));

	return EOPNOTSUPP;
}

static int
rip_send(struct socket *so, struct mbuf *m, struct sockaddr *nam,
    struct mbuf *control, struct lwp *l)
{
	struct inpcb *inp = sotoinpcb(so);
	int error = 0;
	int s;

	KASSERT(solocked(so));
	KASSERT(inp != NULL);
	KASSERT(m != NULL);

	/*
	 * Ship a packet out.  The appropriate raw output
	 * routine handles any massaging necessary.
	 */
	if (control && control->m_len) {
		m_freem(control);
		m_freem(m);
		return EINVAL;
	}

	s = splsoftnet();
	if (nam) {
		if ((so->so_state & SS_ISCONNECTED) != 0) {
			error = EISCONN;
			goto die;
		}
		error = rip_connect_pcb(inp, (struct sockaddr_in *)nam);
		if (error) {
		die:
			m_freem(m);
			splx(s);
			return error;
		}
	} else {
		if ((so->so_state & SS_ISCONNECTED) == 0) {
			error = ENOTCONN;
			goto die;
		}
	}
	error = rip_output(m, inp);
	if (nam)
		rip_disconnect1(inp);

	splx(s);
	return error;
}

static int
rip_sendoob(struct socket *so, struct mbuf *m, struct mbuf *control)
{
	KASSERT(solocked(so));

	m_freem(m);
	m_freem(control);

	return EOPNOTSUPP;
}

static int
rip_purgeif(struct socket *so, struct ifnet *ifp)
{
	int s;

	s = splsoftnet();
	mutex_enter(softnet_lock);
	in_pcbpurgeif0(&rawcbtable, ifp);
	in_purgeif(ifp);
	in_pcbpurgeif(&rawcbtable, ifp);
	mutex_exit(softnet_lock);
	splx(s);

	return 0;
}

PR_WRAP_USRREQS(rip)
#define	rip_attach	rip_attach_wrapper
#define	rip_detach	rip_detach_wrapper
#define	rip_accept	rip_accept_wrapper
#define	rip_bind	rip_bind_wrapper
#define	rip_listen	rip_listen_wrapper
#define	rip_connect	rip_connect_wrapper
#define	rip_connect2	rip_connect2_wrapper
#define	rip_disconnect	rip_disconnect_wrapper
#define	rip_shutdown	rip_shutdown_wrapper
#define	rip_abort	rip_abort_wrapper
#define	rip_ioctl	rip_ioctl_wrapper
#define	rip_stat	rip_stat_wrapper
#define	rip_peeraddr	rip_peeraddr_wrapper
#define	rip_sockaddr	rip_sockaddr_wrapper
#define	rip_rcvd	rip_rcvd_wrapper
#define	rip_recvoob	rip_recvoob_wrapper
#define	rip_send	rip_send_wrapper
#define	rip_sendoob	rip_sendoob_wrapper
#define	rip_purgeif	rip_purgeif_wrapper

const struct pr_usrreqs rip_usrreqs = {
	.pr_attach	= rip_attach,
	.pr_detach	= rip_detach,
	.pr_accept	= rip_accept,
	.pr_bind	= rip_bind,
	.pr_listen	= rip_listen,
	.pr_connect	= rip_connect,
	.pr_connect2	= rip_connect2,
	.pr_disconnect	= rip_disconnect,
	.pr_shutdown	= rip_shutdown,
	.pr_abort	= rip_abort,
	.pr_ioctl	= rip_ioctl,
	.pr_stat	= rip_stat,
	.pr_peeraddr	= rip_peeraddr,
	.pr_sockaddr	= rip_sockaddr,
	.pr_rcvd	= rip_rcvd,
	.pr_recvoob	= rip_recvoob,
	.pr_send	= rip_send,
	.pr_sendoob	= rip_sendoob,
	.pr_purgeif	= rip_purgeif,
};

static void
sysctl_net_inet_raw_setup(struct sysctllog **clog)
{

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "inet", NULL,
		       NULL, 0, NULL, 0,
		       CTL_NET, PF_INET, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "raw",
		       SYSCTL_DESCR("Raw IPv4 settings"),
		       NULL, 0, NULL, 0,
		       CTL_NET, PF_INET, IPPROTO_RAW, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "pcblist",
		       SYSCTL_DESCR("Raw IPv4 control block list"),
		       sysctl_inpcblist, 0, &rawcbtable, 0,
		       CTL_NET, PF_INET, IPPROTO_RAW,
		       CTL_CREATE, CTL_EOL);
}
