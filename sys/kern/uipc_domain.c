/*	$NetBSD: uipc_domain.c,v 1.96 2014/12/02 19:45:58 christos Exp $	*/

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
 *	@(#)uipc_domain.c	8.3 (Berkeley) 2/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uipc_domain.c,v 1.96 2014/12/02 19:45:58 christos Exp $");

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/mbuf.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/kauth.h>

#include <netatalk/at.h>
#include <net/if_dl.h>
#include <netinet/in.h>

MALLOC_DECLARE(M_SOCKADDR);

MALLOC_DEFINE(M_SOCKADDR, "sockaddr", "socket endpoints");

void	pffasttimo(void *);
void	pfslowtimo(void *);

struct domainhead domains = STAILQ_HEAD_INITIALIZER(domains);
static struct domain *domain_array[AF_MAX];

callout_t pffasttimo_ch, pfslowtimo_ch;

/*
 * Current time values for fast and slow timeouts.  We can use u_int
 * relatively safely.  The fast timer will roll over in 27 years and
 * the slow timer in 68 years.
 */
u_int	pfslowtimo_now;
u_int	pffasttimo_now;

static struct sysctllog *domain_sysctllog;
static void sysctl_net_setup(void);

/* ensure successful linkage even without any domains in link sets */
static struct domain domain_dummy;
__link_set_add_rodata(domains,domain_dummy);

void
domaininit(bool attach)
{
	__link_set_decl(domains, struct domain);
	struct domain * const * dpp;
	struct domain *rt_domain = NULL;

	sysctl_net_setup();

	/*
	 * Add all of the domains.  Make sure the PF_ROUTE
	 * domain is added last.
	 */
	if (attach) {
		__link_set_foreach(dpp, domains) {
			if (*dpp == &domain_dummy)
				continue;
			if ((*dpp)->dom_family == PF_ROUTE)
				rt_domain = *dpp;
			else
				domain_attach(*dpp);
		}
		if (rt_domain)
			domain_attach(rt_domain);
	}

	callout_init(&pffasttimo_ch, CALLOUT_MPSAFE);
	callout_init(&pfslowtimo_ch, CALLOUT_MPSAFE);

	callout_reset(&pffasttimo_ch, 1, pffasttimo, NULL);
	callout_reset(&pfslowtimo_ch, 1, pfslowtimo, NULL);
}

void
domain_attach(struct domain *dp)
{
	const struct protosw *pr;

	STAILQ_INSERT_TAIL(&domains, dp, dom_link);
	if (dp->dom_family < __arraycount(domain_array))
		domain_array[dp->dom_family] = dp;

	if (dp->dom_init)
		(*dp->dom_init)();

#ifdef MBUFTRACE
	if (dp->dom_mowner.mo_name[0] == '\0') {
		strncpy(dp->dom_mowner.mo_name, dp->dom_name,
		    sizeof(dp->dom_mowner.mo_name));
		MOWNER_ATTACH(&dp->dom_mowner);
	}
#endif
	for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++) {
		if (pr->pr_init)
			(*pr->pr_init)();
	}

	if (max_linkhdr < 16)		/* XXX */
		max_linkhdr = 16;
	max_hdr = max_linkhdr + max_protohdr;
	max_datalen = MHLEN - max_hdr;
}

struct domain *
pffinddomain(int family)
{
	struct domain *dp;

	if (family < __arraycount(domain_array) && domain_array[family] != NULL)
		return domain_array[family];

	DOMAIN_FOREACH(dp)
		if (dp->dom_family == family)
			return dp;
	return NULL;
}

const struct protosw *
pffindtype(int family, int type)
{
	struct domain *dp;
	const struct protosw *pr;

	dp = pffinddomain(family);
	if (dp == NULL)
		return NULL;

	for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
		if (pr->pr_type && pr->pr_type == type)
			return pr;

	return NULL;
}

const struct protosw *
pffindproto(int family, int protocol, int type)
{
	struct domain *dp;
	const struct protosw *pr;
	const struct protosw *maybe = NULL;

	if (family == 0)
		return NULL;

	dp = pffinddomain(family);
	if (dp == NULL)
		return NULL;

	for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++) {
		if ((pr->pr_protocol == protocol) && (pr->pr_type == type))
			return pr;

		if (type == SOCK_RAW && pr->pr_type == SOCK_RAW &&
		    pr->pr_protocol == 0 && maybe == NULL)
			maybe = pr;
	}
	return maybe;
}

void *
sockaddr_addr(struct sockaddr *sa, socklen_t *slenp)
{
	const struct domain *dom;

	if ((dom = pffinddomain(sa->sa_family)) == NULL ||
	    dom->dom_sockaddr_addr == NULL)
		return NULL;

	return (*dom->dom_sockaddr_addr)(sa, slenp);
}

const void *
sockaddr_const_addr(const struct sockaddr *sa, socklen_t *slenp)
{
	const struct domain *dom;

	if ((dom = pffinddomain(sa->sa_family)) == NULL ||
	    dom->dom_sockaddr_const_addr == NULL)
		return NULL;

	return (*dom->dom_sockaddr_const_addr)(sa, slenp);
}

const struct sockaddr *
sockaddr_any_by_family(int family)
{
	const struct domain *dom;

	if ((dom = pffinddomain(family)) == NULL)
		return NULL;

	return dom->dom_sa_any;
}

const struct sockaddr *
sockaddr_any(const struct sockaddr *sa)
{
	return sockaddr_any_by_family(sa->sa_family);
}

const void *
sockaddr_anyaddr(const struct sockaddr *sa, socklen_t *slenp)
{
	const struct sockaddr *any;

	if ((any = sockaddr_any(sa)) == NULL)
		return NULL;

	return sockaddr_const_addr(any, slenp);
}

#ifdef DIAGNOSTIC
static void
sockaddr_checklen(const struct sockaddr *sa)
{
	socklen_t len = 0;
	switch (sa->sa_family) {
	case AF_INET:
		len = sizeof(struct sockaddr_in);
		break;
	case AF_INET6:
		len = sizeof(struct sockaddr_in6);
		break;
	case AF_UNIX:
		len = sizeof(struct sockaddr_un);
		break;
	case AF_LINK:
		len = sizeof(struct sockaddr_dl);
		// As long as it is not 0...
		if (sa->sa_len != 0)
			return;
		break;
	case AF_APPLETALK:
		len = sizeof(struct sockaddr_at);
		break;
	default:
		printf("%s: Unhandled af=%hhu socklen=%hhu\n", __func__,
		    sa->sa_family, sa->sa_len);
		return;
	}
	if (len != sa->sa_len) {
		char buf[512];
		sockaddr_format(sa, buf, sizeof(buf));
		printf("%s: %p bad len af=%hhu socklen=%hhu len=%u [%s]\n",
		    __func__, sa, sa->sa_family, sa->sa_len,
		    (unsigned)len, buf);
	}
}
#else
#define sockaddr_checklen(sa) ((void)0)
#endif

struct sockaddr *
sockaddr_alloc(sa_family_t af, socklen_t socklen, int flags)
{
	struct sockaddr *sa;
	socklen_t reallen = MAX(socklen, offsetof(struct sockaddr, sa_data[0]));

	if ((sa = malloc(reallen, M_SOCKADDR, flags)) == NULL)
		return NULL;

	sa->sa_family = af;
	sa->sa_len = reallen;
	sockaddr_checklen(sa);
	return sa;
}

struct sockaddr *
sockaddr_copy(struct sockaddr *dst, socklen_t socklen,
    const struct sockaddr *src)
{
	if (__predict_false(socklen < src->sa_len)) {
		panic("%s: source too long, %d < %d bytes", __func__, socklen,
		    src->sa_len);
	}
	sockaddr_checklen(src);
	return memcpy(dst, src, src->sa_len);
}

struct sockaddr *
sockaddr_externalize(struct sockaddr *dst, socklen_t socklen,
    const struct sockaddr *src)
{
	struct domain *dom;

	dom = pffinddomain(src->sa_family);

	if (dom != NULL && dom->dom_sockaddr_externalize != NULL)
		return (*dom->dom_sockaddr_externalize)(dst, socklen, src);

	return sockaddr_copy(dst, socklen, src);
}

int
sockaddr_cmp(const struct sockaddr *sa1, const struct sockaddr *sa2)
{
	int len, rc;
	struct domain *dom;

	if (sa1->sa_family != sa2->sa_family)
		return sa1->sa_family - sa2->sa_family;

	dom = pffinddomain(sa1->sa_family);

	if (dom != NULL && dom->dom_sockaddr_cmp != NULL)
		return (*dom->dom_sockaddr_cmp)(sa1, sa2);

	len = MIN(sa1->sa_len, sa2->sa_len);

	if (dom == NULL || dom->dom_sa_cmplen == 0) {
		if ((rc = memcmp(sa1, sa2, len)) != 0)
			return rc;
		return sa1->sa_len - sa2->sa_len;
	}

	if ((rc = memcmp((const char *)sa1 + dom->dom_sa_cmpofs,
		         (const char *)sa2 + dom->dom_sa_cmpofs,
			 MIN(dom->dom_sa_cmplen,
			     len - MIN(len, dom->dom_sa_cmpofs)))) != 0)
		return rc;

	return MIN(dom->dom_sa_cmplen + dom->dom_sa_cmpofs, sa1->sa_len) -
	       MIN(dom->dom_sa_cmplen + dom->dom_sa_cmpofs, sa2->sa_len);
}

struct sockaddr *
sockaddr_dup(const struct sockaddr *src, int flags)
{
	struct sockaddr *dst;

	if ((dst = sockaddr_alloc(src->sa_family, src->sa_len, flags)) == NULL)
		return NULL;

	return sockaddr_copy(dst, dst->sa_len, src);
}

void
sockaddr_free(struct sockaddr *sa)
{
	free(sa, M_SOCKADDR);
}

static int
sun_print(char *buf, size_t len, const void *v)
{
	const struct sockaddr_un *sun = v;
	return snprintf(buf, len, "%s", sun->sun_path);
}

int
sockaddr_format(const struct sockaddr *sa, char *buf, size_t len)
{
	size_t plen = 0;

	if (sa == NULL)
		return strlcpy(buf, "(null)", len);

	switch (sa->sa_family) {
	case AF_LOCAL:
		plen = strlcpy(buf, "unix: ", len);
		break;
	case AF_INET:
		plen = strlcpy(buf, "inet: ", len);
		break;
	case AF_INET6:
		plen = strlcpy(buf, "inet6: ", len);
		break;
	case AF_LINK:
		plen = strlcpy(buf, "link: ", len);
		break;
	case AF_APPLETALK:
		plen = strlcpy(buf, "atalk: ", len);
		break;
	default:
		return snprintf(buf, len, "(unknown socket family %d)",
		    (int)sa->sa_family);
	}

	buf += plen;
	if (plen > len)
		len = 0;
	else
		len -= plen;

	switch (sa->sa_family) {
	case AF_LOCAL:
		return sun_print(buf, len, sa);
	case AF_INET:
		return sin_print(buf, len, sa);
	case AF_INET6:
		return sin6_print(buf, len, sa);
	case AF_LINK:
		return sdl_print(buf, len, sa);
	case AF_APPLETALK:
		return sat_print(buf, len, sa);
	default:
		panic("bad family %hhu", sa->sa_family);
	}
}

/*
 * sysctl helper to stuff PF_LOCAL pcbs into sysctl structures
 */
static void
sysctl_dounpcb(struct kinfo_pcb *pcb, const struct socket *so)
{
	struct unpcb *unp = sotounpcb(so);
	struct sockaddr_un *un = unp->unp_addr;

	memset(pcb, 0, sizeof(*pcb));

	pcb->ki_family = so->so_proto->pr_domain->dom_family;
	pcb->ki_type = so->so_proto->pr_type;
	pcb->ki_protocol = so->so_proto->pr_protocol;
	pcb->ki_pflags = unp->unp_flags;

	pcb->ki_pcbaddr = PTRTOUINT64(unp);
	/* pcb->ki_ppcbaddr = unp has no ppcb... */
	pcb->ki_sockaddr = PTRTOUINT64(so);

	pcb->ki_sostate = so->so_state;
	/* pcb->ki_prstate = unp has no state... */

	pcb->ki_rcvq = so->so_rcv.sb_cc;
	pcb->ki_sndq = so->so_snd.sb_cc;

	un = (struct sockaddr_un *)pcb->ki_spad;
	/*
	 * local domain sockets may bind without having a local
	 * endpoint.  bleah!
	 */
	if (unp->unp_addr != NULL) {
		/*
		 * We've added one to sun_len when allocating to
		 * hold terminating NUL which we want here.  See
		 * makeun().
		 */
		memcpy(un, unp->unp_addr,
		    min(sizeof(pcb->ki_spad), unp->unp_addr->sun_len + 1));
	}
	else {
		un->sun_len = offsetof(struct sockaddr_un, sun_path);
		un->sun_family = pcb->ki_family;
	}
	if (unp->unp_conn != NULL) {
		un = (struct sockaddr_un *)pcb->ki_dpad;
		if (unp->unp_conn->unp_addr != NULL) {
			memcpy(un, unp->unp_conn->unp_addr,
			    min(sizeof(pcb->ki_dpad), unp->unp_conn->unp_addr->sun_len + 1));
		}
		else {
			un->sun_len = offsetof(struct sockaddr_un, sun_path);
			un->sun_family = pcb->ki_family;
		}
	}

	pcb->ki_inode = unp->unp_ino;
	pcb->ki_vnode = PTRTOUINT64(unp->unp_vnode);
	pcb->ki_conn = PTRTOUINT64(unp->unp_conn);
	pcb->ki_refs = PTRTOUINT64(unp->unp_refs);
	pcb->ki_nextref = PTRTOUINT64(unp->unp_nextref);
}

static int
sysctl_unpcblist(SYSCTLFN_ARGS)
{
	struct file *fp, *dfp;
	struct socket *so;
	struct kinfo_pcb pcb;
	char *dp;
	size_t len, needed, elem_size, out_size;
	int error, elem_count, pf, type;

	if (namelen == 1 && name[0] == CTL_QUERY)
		return sysctl_query(SYSCTLFN_CALL(rnode));

	if (namelen != 4)
		return EINVAL;

	if (oldp != NULL) {
		len = *oldlenp;
		elem_size = name[2];
		elem_count = name[3];
		if (elem_size != sizeof(pcb))
			return EINVAL;
	} else {
		len = 0;
		elem_size = sizeof(pcb);
		elem_count = INT_MAX;
	}
	error = 0;
	dp = oldp;
	out_size = elem_size;
	needed = 0;

	if (name - oname != 4)
		return EINVAL;

	pf = oname[1];
	type = oname[2];

	/*
	 * allocate dummy file descriptor to make position in list.
	 */
	sysctl_unlock();
	if ((dfp = fgetdummy()) == NULL) {
	 	sysctl_relock();
		return ENOMEM;
	}

	/*
	 * there's no "list" of local domain sockets, so we have
	 * to walk the file list looking for them.  :-/
	 */
	mutex_enter(&filelist_lock);
	LIST_FOREACH(fp, &filehead, f_list) {
		if (fp->f_count == 0 || fp->f_type != DTYPE_SOCKET ||
		    fp->f_socket == NULL)
			continue;
		so = fp->f_socket;
		if (so->so_type != type)
			continue;
		if (so->so_proto->pr_domain->dom_family != pf)
			continue;
		if (kauth_authorize_network(l->l_cred, KAUTH_NETWORK_SOCKET,
		    KAUTH_REQ_NETWORK_SOCKET_CANSEE, so, NULL, NULL) != 0)
			continue;
		if (len >= elem_size && elem_count > 0) {
			mutex_enter(&fp->f_lock);
			fp->f_count++;
			mutex_exit(&fp->f_lock);
			LIST_INSERT_AFTER(fp, dfp, f_list);
			mutex_exit(&filelist_lock);
			sysctl_dounpcb(&pcb, so);
			error = copyout(&pcb, dp, out_size);
			closef(fp);
			mutex_enter(&filelist_lock);
			LIST_REMOVE(dfp, f_list);
			if (error)
				break;
			dp += elem_size;
			len -= elem_size;
		}
		needed += elem_size;
		if (elem_count > 0 && elem_count != INT_MAX)
			elem_count--;
	}
	mutex_exit(&filelist_lock);
	fputdummy(dfp);
 	*oldlenp = needed;
	if (oldp == NULL)
		*oldlenp += PCB_SLOP * sizeof(struct kinfo_pcb);
 	sysctl_relock();

	return error;
}

static void
sysctl_net_setup(void)
{

	KASSERT(domain_sysctllog == NULL);
	sysctl_createv(&domain_sysctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "local",
		       SYSCTL_DESCR("PF_LOCAL related settings"),
		       NULL, 0, NULL, 0,
		       CTL_NET, PF_LOCAL, CTL_EOL);
	sysctl_createv(&domain_sysctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "stream",
		       SYSCTL_DESCR("SOCK_STREAM settings"),
		       NULL, 0, NULL, 0,
		       CTL_NET, PF_LOCAL, SOCK_STREAM, CTL_EOL);
	sysctl_createv(&domain_sysctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "seqpacket",
		       SYSCTL_DESCR("SOCK_SEQPACKET settings"),
		       NULL, 0, NULL, 0,
		       CTL_NET, PF_LOCAL, SOCK_SEQPACKET, CTL_EOL);
	sysctl_createv(&domain_sysctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "dgram",
		       SYSCTL_DESCR("SOCK_DGRAM settings"),
		       NULL, 0, NULL, 0,
		       CTL_NET, PF_LOCAL, SOCK_DGRAM, CTL_EOL);

	sysctl_createv(&domain_sysctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "pcblist",
		       SYSCTL_DESCR("SOCK_STREAM protocol control block list"),
		       sysctl_unpcblist, 0, NULL, 0,
		       CTL_NET, PF_LOCAL, SOCK_STREAM, CTL_CREATE, CTL_EOL);
	sysctl_createv(&domain_sysctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "pcblist",
		       SYSCTL_DESCR("SOCK_SEQPACKET protocol control "
				    "block list"),
		       sysctl_unpcblist, 0, NULL, 0,
		       CTL_NET, PF_LOCAL, SOCK_SEQPACKET, CTL_CREATE, CTL_EOL);
	sysctl_createv(&domain_sysctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "pcblist",
		       SYSCTL_DESCR("SOCK_DGRAM protocol control block list"),
		       sysctl_unpcblist, 0, NULL, 0,
		       CTL_NET, PF_LOCAL, SOCK_DGRAM, CTL_CREATE, CTL_EOL);
}

void
pfctlinput(int cmd, const struct sockaddr *sa)
{
	struct domain *dp;
	const struct protosw *pr;

	DOMAIN_FOREACH(dp) {
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++) {
			if (pr->pr_ctlinput != NULL)
				(*pr->pr_ctlinput)(cmd, sa, NULL);
		}
	}
}

void
pfctlinput2(int cmd, const struct sockaddr *sa, void *ctlparam)
{
	struct domain *dp;
	const struct protosw *pr;

	if (sa == NULL)
		return;

	DOMAIN_FOREACH(dp) {
		/*
		 * the check must be made by xx_ctlinput() anyways, to
		 * make sure we use data item pointed to by ctlparam in
		 * correct way.  the following check is made just for safety.
		 */
		if (dp->dom_family != sa->sa_family)
			continue;

		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++) {
			if (pr->pr_ctlinput != NULL)
				(*pr->pr_ctlinput)(cmd, sa, ctlparam);
		}
	}
}

void
pfslowtimo(void *arg)
{
	struct domain *dp;
	const struct protosw *pr;

	pfslowtimo_now++;

	DOMAIN_FOREACH(dp) {
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_slowtimo)
				(*pr->pr_slowtimo)();
	}
	callout_schedule(&pfslowtimo_ch, hz / PR_SLOWHZ);
}

void
pffasttimo(void *arg)
{
	struct domain *dp;
	const struct protosw *pr;

	pffasttimo_now++;

	DOMAIN_FOREACH(dp) {
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_fasttimo)
				(*pr->pr_fasttimo)();
	}
	callout_schedule(&pffasttimo_ch, hz / PR_FASTHZ);
}
