/*	$NetBSD: getnameinfo.c,v 1.54 2013/08/16 15:27:12 christos Exp $	*/
/*	$KAME: getnameinfo.c,v 1.45 2000/09/25 22:43:56 itojun Exp $	*/

/*
 * Copyright (c) 2000 Ben Harris.
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
 * Issues to be discussed:
 * - Thread safe-ness must be checked
 * - RFC2553 says that we should raise error on short buffer.  X/Open says
 *   we need to truncate the result.  We obey RFC2553 (and X/Open should be
 *   modified).  ipngwg rough consensus seems to follow RFC2553.
 * - What is "local" in NI_FQDN?
 * - NI_NAMEREQD and NI_NUMERICHOST conflict with each other.
 * - (KAME extension) always attach textual scopeid (fe80::1%lo0), if
 *   sin6_scope_id is filled - standardization status?
 *   XXX breaks backward compat for code that expects no scopeid.
 *   beware on merge.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: getnameinfo.c,v 1.54 2013/08/16 15:27:12 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <net/if.h>
#if !defined(__minix)
#include <net/if_dl.h>
#include <net/if_ieee1394.h>
#include <net/if_types.h>
#include <netatalk/at.h>
#endif /* !defined(__minix) */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <assert.h>
#include <limits.h>
#include <netdb.h>
#include <resolv.h>
#include <stddef.h>
#include <string.h>

#include "servent.h"
#include "hostent.h"

#ifdef __weak_alias
__weak_alias(getnameinfo,_getnameinfo)
#endif

static const struct afd {
	int		a_af;
	socklen_t	a_addrlen;
	socklen_t	a_socklen;
	int		a_off;
} afdl [] = {
#ifdef INET6
	{PF_INET6, sizeof(struct in6_addr), sizeof(struct sockaddr_in6),
		offsetof(struct sockaddr_in6, sin6_addr)},
#endif
	{PF_INET, sizeof(struct in_addr), sizeof(struct sockaddr_in),
		offsetof(struct sockaddr_in, sin_addr)},
	{0, 0, 0, 0},
};

struct sockinet {
	u_char	si_len;
	u_char	si_family;
	u_short	si_port;
};

static int getnameinfo_inet(const struct sockaddr *, socklen_t, char *,
    socklen_t, char *, socklen_t, int);
#ifdef INET6
static int ip6_parsenumeric(const struct sockaddr *, const char *, char *,
				 socklen_t, int);
static int ip6_sa2str(const struct sockaddr_in6 *, char *, size_t, int);
#endif
#if !defined(__minix)
static int getnameinfo_atalk(const struct sockaddr *, socklen_t, char *,
    socklen_t, char *, socklen_t, int);
static int getnameinfo_local(const struct sockaddr *, socklen_t, char *,
    socklen_t, char *, socklen_t, int);

static int getnameinfo_link(const struct sockaddr *, socklen_t, char *,
    socklen_t, char *, socklen_t, int);
static int hexname(const uint8_t *, size_t, char *, socklen_t);
#endif /* !defined(__minix) */

/*
 * Top-level getnameinfo() code.  Look at the address family, and pick an
 * appropriate function to call.
 */
int
getnameinfo(const struct sockaddr *sa, socklen_t salen,
	char *host, socklen_t hostlen,
	char *serv, socklen_t servlen,
	int flags)
{

	switch (sa->sa_family) {
#if !defined(__minix)
	case AF_APPLETALK:
		return getnameinfo_atalk(sa, salen, host, hostlen,
		    serv, servlen, flags);
#endif /* !defined(__minix) */
	case AF_INET:
	case AF_INET6:
		return getnameinfo_inet(sa, salen, host, hostlen,
		    serv, servlen, flags);
#if !defined(__minix)
	case AF_LINK:
		return getnameinfo_link(sa, salen, host, hostlen,
		    serv, servlen, flags);
	case AF_LOCAL:
		return getnameinfo_local(sa, salen, host, hostlen,
		    serv, servlen, flags);
#endif /* !defined(__minix) */
	default:
		return EAI_FAMILY;
	}
}

#if !defined(__minix)
/*
 * getnameinfo_atalk():
 * Format an AppleTalk address into a printable format.
 */
/* ARGSUSED */
static int
getnameinfo_atalk(const struct sockaddr *sa, socklen_t salen,
    char *host, socklen_t hostlen, char *serv, socklen_t servlen,
    int flags)
{
	char numserv[8];
	int n, m=0;

	const struct sockaddr_at *sat =
	    (const struct sockaddr_at *)(const void *)sa;

	if (serv != NULL && servlen > 0) {
		snprintf(numserv, sizeof(numserv), "%u", sat->sat_port);
		if (strlen(numserv) + 1 > servlen)
			return EAI_MEMORY;
		strlcpy(serv, numserv, servlen);
	}

        n = snprintf(host, hostlen, "%u.%u",
	    ntohs(sat->sat_addr.s_net), sat->sat_addr.s_node);

	if (n < 0 || (socklen_t)(m+n) >= hostlen)
		goto errout;

	m += n;

	if (sat->sat_range.r_netrange.nr_phase) {
        	n = snprintf(host+m, hostlen-m, " phase %u",
			sat->sat_range.r_netrange.nr_phase);

		if (n < 0 || (socklen_t)(m+n) >= hostlen)
			goto errout;

		m += n;
	}
	if (sat->sat_range.r_netrange.nr_firstnet) {
        	n = snprintf(host+m, hostlen-m, " range %u - %u",
			ntohs(sat->sat_range.r_netrange.nr_firstnet),
			ntohs(sat->sat_range.r_netrange.nr_lastnet ));

		if (n < 0 || (socklen_t)(m+n) >= hostlen)
			goto errout;

		m += n;
	}

	return 0;

errout:
	if (host && hostlen>0)
		host[m] = '\0';	/* XXX ??? */

	return EAI_MEMORY;
}

/*
 * getnameinfo_local():
 * Format an local address into a printable format.
 */
/* ARGSUSED */
static int
getnameinfo_local(const struct sockaddr *sa, socklen_t salen,
    char *host, socklen_t hostlen, char *serv, socklen_t servlen,
    int flags)
{
	const struct sockaddr_un *sun =
	    (const struct sockaddr_un *)(const void *)sa;

	if (serv != NULL && servlen > 0)
		serv[0] = '\0';

	if (host && hostlen > 0)
		strlcpy(host, sun->sun_path,
		    MIN(sizeof(sun->sun_path) + 1, hostlen));

	return 0;
}
#endif /* !defined(__minix) */

/*
 * getnameinfo_inet():
 * Format an IPv4 or IPv6 sockaddr into a printable string.
 */
static int
getnameinfo_inet(const struct sockaddr *sa, socklen_t salen,
	char *host, socklen_t hostlen,
	char *serv, socklen_t servlen,
	int flags)
{
	const struct afd *afd;
	struct servent *sp;
	struct hostent *hp;
	u_short port;
	int family, i;
	const char *addr;
	uint32_t v4a;
	char numserv[512];
	char numaddr[512];

	/* sa is checked below */
	/* host may be NULL */
	/* serv may be NULL */

	if (sa == NULL)
		return EAI_FAIL;

	family = sa->sa_family;
	for (i = 0; afdl[i].a_af; i++)
		if (afdl[i].a_af == family) {
			afd = &afdl[i];
			goto found;
		}
	return EAI_FAMILY;

 found:
	if (salen != afd->a_socklen)
		return EAI_FAIL;

	/* network byte order */
	port = ((const struct sockinet *)(const void *)sa)->si_port;
	addr = (const char *)(const void *)sa + afd->a_off;

	if (serv == NULL || servlen == 0) {
		/*
		 * do nothing in this case.
		 * in case you are wondering if "&&" is more correct than
		 * "||" here: rfc2553bis-03 says that serv == NULL OR
		 * servlen == 0 means that the caller does not want the result.
		 */
	} else {
		struct servent_data svd;
		struct servent sv;

		if (flags & NI_NUMERICSERV)
			sp = NULL;
		else {
			(void)memset(&svd, 0, sizeof(svd));
			sp = getservbyport_r(port,
				(flags & NI_DGRAM) ? "udp" : "tcp", &sv, &svd);
		}
		if (sp) {
			if (strlen(sp->s_name) + 1 > servlen) {
				endservent_r(&svd);
				return EAI_MEMORY;
			}
			strlcpy(serv, sp->s_name, servlen);
			endservent_r(&svd);
		} else {
			snprintf(numserv, sizeof(numserv), "%u", ntohs(port));
			if (strlen(numserv) + 1 > servlen)
				return EAI_MEMORY;
			strlcpy(serv, numserv, servlen);
		}
	}

	switch (sa->sa_family) {
	case AF_INET:
		v4a = (uint32_t)
		    ntohl(((const struct sockaddr_in *)
		    (const void *)sa)->sin_addr.s_addr);
		if (IN_MULTICAST(v4a) || IN_EXPERIMENTAL(v4a))
			flags |= NI_NUMERICHOST;
		v4a >>= IN_CLASSA_NSHIFT;
		if (v4a == 0)
			flags |= NI_NUMERICHOST;
		break;
#ifdef INET6
	case AF_INET6:
	    {
		const struct sockaddr_in6 *sin6;
		sin6 = (const struct sockaddr_in6 *)(const void *)sa;
		switch (sin6->sin6_addr.s6_addr[0]) {
		case 0x00:
			if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr))
				;
			else if (IN6_IS_ADDR_LOOPBACK(&sin6->sin6_addr))
				;
			else
				flags |= NI_NUMERICHOST;
			break;
		default:
			if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
				flags |= NI_NUMERICHOST;
			}
			else if (IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr))
				flags |= NI_NUMERICHOST;
			break;
		}
	    }
		break;
#endif
	}
	if (host == NULL || hostlen == 0) {
		/*
		 * do nothing in this case.
		 * in case you are wondering if "&&" is more correct than
		 * "||" here: rfc2553bis-03 says that host == NULL or
		 * hostlen == 0 means that the caller does not want the result.
		 */
	} else if (flags & NI_NUMERICHOST) {
		size_t numaddrlen;

		/* NUMERICHOST and NAMEREQD conflicts with each other */
		if (flags & NI_NAMEREQD)
			return EAI_NONAME;

		switch(afd->a_af) {
#ifdef INET6
		case AF_INET6:
		{
			int error;

			if ((error = ip6_parsenumeric(sa, addr, host,
						      hostlen, flags)) != 0)
				return(error);
			break;
		}
#endif
		default:
			if (inet_ntop(afd->a_af, addr, numaddr,
			    (socklen_t)sizeof(numaddr)) == NULL)
				return EAI_SYSTEM;
			numaddrlen = strlen(numaddr);
			if (numaddrlen + 1 > hostlen) /* don't forget terminator */
				return EAI_MEMORY;
			strlcpy(host, numaddr, hostlen);
			break;
		}
	} else {
		struct hostent hent;
		char hbuf[4096];
		int he;
		hp = gethostbyaddr_r(addr, afd->a_addrlen, afd->a_af, &hent,
		    hbuf, sizeof(hbuf), &he);

		if (hp) {
#if 0
			/*
			 * commented out, since "for local host" is not
			 * implemented here - see RFC2553 p30
			 */
			if (flags & NI_NOFQDN) {
				char *p;
				p = strchr(hp->h_name, '.');
				if (p)
					*p = '\0';
			}
#endif
			if (strlen(hp->h_name) + 1 > hostlen) {
				return EAI_MEMORY;
			}
			strlcpy(host, hp->h_name, hostlen);
		} else {
			if (flags & NI_NAMEREQD)
				return EAI_NONAME;
			switch(afd->a_af) {
#ifdef INET6
			case AF_INET6:
			{
				int error;

				if ((error = ip6_parsenumeric(sa, addr, host,
							      hostlen,
							      flags)) != 0)
					return(error);
				break;
			}
#endif
			default:
				if (inet_ntop(afd->a_af, addr, host,
				    hostlen) == NULL)
					return EAI_SYSTEM;
				break;
			}
		}
	}
	return(0);
}

#ifdef INET6
static int
ip6_parsenumeric(const struct sockaddr *sa, const char *addr, char *host,
	socklen_t hostlen, int flags)
{
	size_t numaddrlen;
	char numaddr[512];

	_DIAGASSERT(sa != NULL);
	_DIAGASSERT(addr != NULL);
	_DIAGASSERT(host != NULL);

	if (inet_ntop(AF_INET6, addr, numaddr, (socklen_t)sizeof(numaddr))
	    == NULL)
		return EAI_SYSTEM;

	numaddrlen = strlen(numaddr);
	if (numaddrlen + 1 > hostlen) /* don't forget terminator */
		return EAI_OVERFLOW;
	strlcpy(host, numaddr, hostlen);

	if (((const struct sockaddr_in6 *)(const void *)sa)->sin6_scope_id) {
		char zonebuf[MAXHOSTNAMELEN];
		int zonelen;

		zonelen = ip6_sa2str(
		    (const struct sockaddr_in6 *)(const void *)sa,
		    zonebuf, sizeof(zonebuf), flags);
		if (zonelen < 0)
			return EAI_OVERFLOW;
		if ((size_t) zonelen + 1 + numaddrlen + 1 > hostlen)
			return EAI_OVERFLOW;
		/* construct <numeric-addr><delim><zoneid> */
		memcpy(host + numaddrlen + 1, zonebuf,
		    (size_t)zonelen);
		host[numaddrlen] = SCOPE_DELIMITER;
		host[numaddrlen + 1 + zonelen] = '\0';
	}

	return 0;
}

/* ARGSUSED */
static int
ip6_sa2str(const struct sockaddr_in6 *sa6, char *buf, size_t bufsiz, int flags)
{
	unsigned int ifindex;
	const struct in6_addr *a6;
	int n;

	_DIAGASSERT(sa6 != NULL);
	_DIAGASSERT(buf != NULL);

	ifindex = (unsigned int)sa6->sin6_scope_id;
	a6 = &sa6->sin6_addr;

#ifdef NI_NUMERICSCOPE
	if ((flags & NI_NUMERICSCOPE) != 0) {
		n = snprintf(buf, bufsiz, "%u", sa6->sin6_scope_id);
		if (n < 0 || (size_t)n >= bufsiz)
			return -1;
		else
			return n;
	}
#endif

	/* if_indextoname() does not take buffer size.  not a good api... */
	if ((IN6_IS_ADDR_LINKLOCAL(a6) || IN6_IS_ADDR_MC_LINKLOCAL(a6)) &&
	    bufsiz >= IF_NAMESIZE) {
		char *p = if_indextoname(ifindex, buf);
		if (p) {
			return (int)strlen(p);
		}
	}

	/* last resort */
	n = snprintf(buf, bufsiz, "%u", sa6->sin6_scope_id);
	if (n < 0 || (size_t) n >= bufsiz)
		return -1;
	else
		return n;
}
#endif /* INET6 */


#if !defined(__minix)
/*
 * getnameinfo_link():
 * Format a link-layer address into a printable format, paying attention to
 * the interface type.
 */
/* ARGSUSED */
static int
getnameinfo_link(const struct sockaddr *sa, socklen_t salen,
    char *host, socklen_t hostlen, char *serv, socklen_t servlen,
    int flags)
{
	const struct sockaddr_dl *sdl =
	    (const struct sockaddr_dl *)(const void *)sa;
	const struct ieee1394_hwaddr *iha;
	int n;

	if (serv != NULL && servlen > 0)
		*serv = '\0';

	if (sdl->sdl_nlen == 0 && sdl->sdl_alen == 0 && sdl->sdl_slen == 0) {
		n = snprintf(host, hostlen, "link#%u", sdl->sdl_index);
		if (n < 0 || (socklen_t) n > hostlen) {
			*host = '\0';
			return EAI_MEMORY;
		}
		return 0;
	}

	switch (sdl->sdl_type) {
#ifdef IFT_ECONET
	case IFT_ECONET:
		if (sdl->sdl_alen < 2)
			return EAI_FAMILY;
		if (CLLADDR(sdl)[1] == 0)
			n = snprintf(host, hostlen, "%u", CLLADDR(sdl)[0]);
		else
			n = snprintf(host, hostlen, "%u.%u",
			    CLLADDR(sdl)[1], CLLADDR(sdl)[0]);
		if (n < 0 || (socklen_t) n >= hostlen) {
			*host = '\0';
			return EAI_MEMORY;
		} else
			return 0;
#endif
	case IFT_IEEE1394:
		if (sdl->sdl_alen < sizeof(iha->iha_uid))
			return EAI_FAMILY;
		iha =
		    (const struct ieee1394_hwaddr *)(const void *)CLLADDR(sdl);
		return hexname(iha->iha_uid, sizeof(iha->iha_uid),
		    host, hostlen);
	/*
	 * The following have zero-length addresses.
	 * IFT_ATM	(net/if_atmsubr.c)
	 * IFT_FAITH	(net/if_faith.c)
	 * IFT_GIF	(net/if_gif.c)
	 * IFT_LOOP	(net/if_loop.c)
	 * IFT_PPP	(net/if_ppp.c, net/if_spppsubr.c)
	 * IFT_SLIP	(net/if_sl.c, net/if_strip.c)
	 * IFT_STF	(net/if_stf.c)
	 * IFT_L2VLAN	(net/if_vlan.c)
	 * IFT_PROPVIRTUAL (net/if_bridge.h>
	 */
	/*
	 * The following use IPv4 addresses as link-layer addresses:
	 * IFT_OTHER	(net/if_gre.c)
	 */
	case IFT_ARCNET: /* default below is believed correct for all these. */
	case IFT_ETHER:
	case IFT_FDDI:
	case IFT_HIPPI:
	case IFT_ISO88025:
	default:
		return hexname((const uint8_t *)CLLADDR(sdl),
		    (size_t)sdl->sdl_alen, host, hostlen);
	}
}

static int
hexname(const uint8_t *cp, size_t len, char *host, socklen_t hostlen)
{
	int n;
	size_t i;
	char *outp = host;

	*outp = '\0';
	for (i = 0; i < len; i++) {
		n = snprintf(outp, hostlen, "%s%02x",
		    i ? ":" : "", cp[i]);
		if (n < 0 || (socklen_t) n >= hostlen) {
			*host = '\0';
			return EAI_MEMORY;
		}
		outp += n;
		hostlen -= n;
	}
	return 0;
}
#endif /* !defined(__minix) */
