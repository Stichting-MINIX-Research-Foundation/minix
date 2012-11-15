/*	$NetBSD: getnetnamadr.c,v 1.42 2012/03/13 21:13:41 christos Exp $	*/

/* Copyright (c) 1993 Carlos Leandro and Rui Salgueiro
 *	Dep. Matematica Universidade de Coimbra, Portugal, Europe
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */
/*
 * Copyright (c) 1983, 1993
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
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)getnetbyaddr.c	8.1 (Berkeley) 6/4/93";
static char sccsid_[] = "from getnetnamadr.c	1.4 (Coimbra) 93/06/03";
static char rcsid[] = "Id: getnetnamadr.c,v 8.8 1997/06/01 20:34:37 vixie Exp ";
#else
__RCSID("$NetBSD: getnetnamadr.c,v 1.42 2012/03/13 21:13:41 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <nsswitch.h>
#include <resolv.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef YP
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#endif

#ifdef __weak_alias
__weak_alias(getnetbyaddr,_getnetbyaddr)
__weak_alias(getnetbyname,_getnetbyname)
#endif

extern int _net_stayopen;

#define BYADDR 0
#define BYNAME 1
#define	MAXALIASES	35

#define	MAXPACKET	(64*1024)

typedef union {
	HEADER	hdr;
	u_char	buf[MAXPACKET];
} querybuf;

typedef union {
	long	al;
	char	ac;
} align;

#ifdef YP
static char *__ypdomain;
static char *__ypcurrent;
static int   __ypcurrentlen;
#endif

static	struct netent net_entry;
static	char *net_aliases[MAXALIASES];

static int		parse_reversed_addr(const char *, in_addr_t *);
static struct netent	*getnetanswer(querybuf *, int, int);
static int		_files_getnetbyaddr(void *, void *, va_list);
static int		_files_getnetbyname(void *, void *, va_list);
static int		_dns_getnetbyaddr(void *, void *, va_list);
static int		_dns_getnetbyname(void *, void *, va_list);
#ifdef YP
static int		_yp_getnetbyaddr(void *, void *, va_list);
static int		_yp_getnetbyname(void *, void *, va_list);
static struct netent	*_ypnetent(char *);
#endif

/*
 * parse_reversed_addr --
 *	parse str, which should be of the form 'd.c.b.a.IN-ADDR.ARPA'
 *	(a PTR as per RFC 1101) and convert into an in_addr_t of the
 *	address 'a.b.c.d'.
 *	returns 0 on success (storing in *result), or -1 on error.
 */
static int
parse_reversed_addr(const char *str, in_addr_t *result)
{
	unsigned long	octet[4];
	const char	*sp;
	char		*ep;
	int		octidx;

	sp = str;
				/* find the four octets 'd.b.c.a.' */
	for (octidx = 0; octidx < 4; octidx++) {
					/* ensure it's a number */
		if (!isdigit((unsigned char)*sp))
			return -1;
		octet[octidx] = strtoul(sp, &ep, 10);
					/* with a trailing '.' */
		if (*ep != '.')
			return -1;
					/* and is 0 <= octet <= 255 */
		if (octet[octidx] > 255)
			return -1;
		sp = ep + 1;
	}
				/* ensure trailer is correct */
	if (strcasecmp(sp, "IN-ADDR.ARPA") != 0)
		return -1;
	*result = 0;
				/* build result from octets in reverse */
	for (octidx = 3; octidx >= 0; octidx--) {
		*result <<= 8;
		*result |= (in_addr_t)(octet[octidx] & 0xff);
	}
	return 0;
}

static struct netent *
getnetanswer(querybuf *answer, int anslen, int net_i)
{
	static char	n_name[MAXDNAME];
	static char	netbuf[PACKETSZ];

	HEADER		*hp;
	u_char		*cp;
	int		n;
	u_char		*eom;
	int		type, class, ancount, qdcount, haveanswer;
	char		*in, *bp, **ap, *ep;

	_DIAGASSERT(answer != NULL);

	/*
	 * find first satisfactory answer
	 *
	 *      answer --> +------------+  ( MESSAGE )
	 *		   |   Header   |
	 *		   +------------+
	 *		   |  Question  | the question for the name server
	 *		   +------------+
	 *		   |   Answer   | RRs answering the question
	 *		   +------------+
	 *		   | Authority  | RRs pointing toward an authority
	 *		   | Additional | RRs holding additional information
	 *		   +------------+
	 */
	eom = answer->buf + anslen;
	hp = &answer->hdr;
	ancount = ntohs(hp->ancount); /* #/records in the answer section */
	qdcount = ntohs(hp->qdcount); /* #/entries in the question section */
	bp = netbuf;
	ep = netbuf + sizeof(netbuf);
	cp = answer->buf + HFIXEDSZ;
	if (!qdcount) {
		if (hp->aa)
			h_errno = HOST_NOT_FOUND;
		else
			h_errno = TRY_AGAIN;
		return NULL;
	}
	while (qdcount-- > 0) {
		n = __dn_skipname(cp, eom);
		if (n < 0 || (cp + n + QFIXEDSZ) > eom) {
			h_errno = NO_RECOVERY;
			return(NULL);
		}
		cp += n + QFIXEDSZ;
	}
	ap = net_aliases;
	*ap = NULL;
	net_entry.n_aliases = net_aliases;
	haveanswer = 0;
	n_name[0] = '\0';
	while (--ancount >= 0 && cp < eom) {
		n = dn_expand(answer->buf, eom, cp, bp, (int)(ep - bp));
		if ((n < 0) || !res_dnok(bp))
			break;
		cp += n;
		(void)strlcpy(n_name, bp, sizeof(n_name));
		GETSHORT(type, cp);
		GETSHORT(class, cp);
		cp += INT32SZ;		/* TTL */
		GETSHORT(n, cp);
		if (class == C_IN && type == T_PTR) {
			n = dn_expand(answer->buf, eom, cp, bp, (int)(ep - bp));
			if ((n < 0) || !res_hnok(bp)) {
				cp += n;
				return NULL;
			}
			cp += n;
			*ap++ = bp;
			bp += strlen(bp) + 1;
			net_entry.n_addrtype =
				(class == C_IN) ? AF_INET : AF_UNSPEC;
			haveanswer++;
		}
	}
	if (haveanswer) {
		*ap = NULL;
		switch (net_i) {
		case BYADDR:
			net_entry.n_name = *net_entry.n_aliases;
			net_entry.n_net = 0L;
			break;
		case BYNAME:
			ap = net_entry.n_aliases;
		next_alias:
			in = *ap++;
			if (in == NULL) {
				h_errno = HOST_NOT_FOUND;
				return NULL;
			}
			net_entry.n_name = n_name;
			if (parse_reversed_addr(in, &net_entry.n_net) == -1)
				goto next_alias;
			break;
		}
		net_entry.n_aliases++;
#if (defined(__sparc__) && defined(_LP64)) ||		\
    defined(__alpha__) ||				\
    (defined(__i386__) && defined(_LP64)) ||		\
    (defined(__sh__) && defined(_LP64))
		net_entry.__n_pad0 = 0;
#endif
		return &net_entry;
	}
	h_errno = TRY_AGAIN;
	return NULL;
}

/*ARGSUSED*/
static int
_files_getnetbyaddr(void *cbrv, void *cbdata, va_list ap)
{
	struct netent	**retval = va_arg(ap, struct netent **);
	uint32_t	  net	 = va_arg(ap, uint32_t);
	int		  type	 = va_arg(ap, int);

	struct netent	 *np;

	setnetent(_net_stayopen);
	while ((np = getnetent()) != NULL)
		if (np->n_addrtype == type && np->n_net == net)
			break;
	if (!_net_stayopen)
		endnetent();

	if (np != NULL) {
		*retval = np;
		return NS_SUCCESS;
	} else {
		h_errno = HOST_NOT_FOUND;
		return NS_NOTFOUND;
	}
}

/*ARGSUSED*/
static int
_dns_getnetbyaddr(void *cbrv, void *cbdata, va_list ap)
{
	struct netent	**retval = va_arg(ap, struct netent **);
	uint32_t	  net	 = va_arg(ap, uint32_t);
	int		  type	 = va_arg(ap, int);

	unsigned int	 netbr[4];
	int		 nn, anslen;
	querybuf	*buf;
	char		 qbuf[MAXDNAME];
	uint32_t	 net2;
	struct netent	*np;
	res_state	 res;

	if (type != AF_INET)
		return NS_UNAVAIL;

	for (nn = 4, net2 = net; net2; net2 >>= 8)
		netbr[--nn] = (unsigned int)(net2 & 0xff);
	switch (nn) {
	default:
		return NS_UNAVAIL;
	case 3: 	/* Class A */
		snprintf(qbuf, sizeof(qbuf), "0.0.0.%u.in-addr.arpa", netbr[3]);
		break;
	case 2: 	/* Class B */
		snprintf(qbuf, sizeof(qbuf), "0.0.%u.%u.in-addr.arpa",
		    netbr[3], netbr[2]);
		break;
	case 1: 	/* Class C */
		snprintf(qbuf, sizeof(qbuf), "0.%u.%u.%u.in-addr.arpa",
		    netbr[3], netbr[2], netbr[1]);
		break;
	case 0: 	/* Class D - E */
		snprintf(qbuf, sizeof(qbuf), "%u.%u.%u.%u.in-addr.arpa",
		    netbr[3], netbr[2], netbr[1], netbr[0]);
		break;
	}
	buf = malloc(sizeof(*buf));
	if (buf == NULL) {
		h_errno = NETDB_INTERNAL;
		return NS_NOTFOUND;
	}
	res = __res_get_state();
	if (res == NULL) {
		free(buf);
		return NS_NOTFOUND;
	}
	anslen = res_nquery(res, qbuf, C_IN, T_PTR, buf->buf,
	    (int)sizeof(buf->buf));
	if (anslen < 0) {
		free(buf);
#ifdef DEBUG
		if (res->options & RES_DEBUG)
			printf("res_query failed\n");
#endif
		__res_put_state(res);
		return NS_NOTFOUND;
	}
	__res_put_state(res);
	np = getnetanswer(buf, anslen, BYADDR);
	free(buf);
	if (np) {
		/* maybe net should be unsigned? */
		uint32_t u_net = net;

		/* Strip trailing zeros */
		while ((u_net & 0xff) == 0 && u_net != 0)
			u_net >>= 8;
		np->n_net = u_net;
	}

	if (np != NULL) {
		*retval = np;
		return NS_SUCCESS;
	} else {
		h_errno = HOST_NOT_FOUND;
		return NS_NOTFOUND;
	}
}

struct netent *
getnetbyaddr(uint32_t net, int net_type)
{
	int		 rv;
	struct netent	*retval;

	static const ns_dtab dtab[] = {
		NS_FILES_CB(_files_getnetbyaddr, NULL)
		{ NSSRC_DNS, _dns_getnetbyaddr, NULL },	/* force -DHESIOD */
		NS_NIS_CB(_yp_getnetbyaddr, NULL)
		NS_NULL_CB
	};

	retval = NULL;
	h_errno = NETDB_INTERNAL;
	rv = nsdispatch(NULL, dtab, NSDB_NETWORKS, "getnetbyaddr",
	    __nsdefaultsrc, &retval, net, net_type);
	if (rv == NS_SUCCESS) {
		h_errno = NETDB_SUCCESS;
		return retval;
	}
	return NULL;
}

/*ARGSUSED*/
static int
_files_getnetbyname(void *cbrv, void *cbdata, va_list ap)
{
	struct netent	**retval = va_arg(ap, struct netent **);
	const char	 *name	 = va_arg(ap, const char *);

	struct netent	 *np;
	char		**cp;

	setnetent(_net_stayopen);
	while ((np = getnetent()) != NULL) {
		if (strcasecmp(np->n_name, name) == 0)
			break;
		for (cp = np->n_aliases; *cp != 0; cp++)
			if (strcasecmp(*cp, name) == 0)
				goto found;
	}
found:
	if (!_net_stayopen)
		endnetent();

	if (np != NULL) {
		*retval = np;
		return NS_SUCCESS;
	} else {
		h_errno = HOST_NOT_FOUND;
		return NS_NOTFOUND;
	}
}

/*ARGSUSED*/
static int
_dns_getnetbyname(void *cbrv, void *cbdata, va_list ap)
{
	struct netent	**retval = va_arg(ap, struct netent **);
	const char	 *name	 = va_arg(ap, const char *);

	int		 anslen;
	querybuf	*buf;
	char		 qbuf[MAXDNAME];
	struct netent	*np;
	res_state	 res;

	strlcpy(&qbuf[0], name, sizeof(qbuf));
	buf = malloc(sizeof(*buf));
	if (buf == NULL) {
		h_errno = NETDB_INTERNAL;
		return NS_NOTFOUND;
	}
	res = __res_get_state();
	if (res == NULL) {
		free(buf);
		return NS_NOTFOUND;
	}
	anslen = res_nsearch(res, qbuf, C_IN, T_PTR, buf->buf,
	    (int)sizeof(buf->buf));
	if (anslen < 0) {
		free(buf);
#ifdef DEBUG
		if (res->options & RES_DEBUG)
			printf("res_search failed\n");
#endif
		__res_put_state(res);
		return NS_NOTFOUND;
	}
	__res_put_state(res);
	np = getnetanswer(buf, anslen, BYNAME);
	free(buf);

	if (np != NULL) {
		*retval = np;
		return NS_SUCCESS;
	} else {
		h_errno = HOST_NOT_FOUND;
		return NS_NOTFOUND;
	}
}

struct netent *
getnetbyname(const char *name)
{
	int		 rv;
	struct netent	*retval;

	static const ns_dtab dtab[] = {
		NS_FILES_CB(_files_getnetbyname, NULL)
		{ NSSRC_DNS, _dns_getnetbyname, NULL },	/* force -DHESIOD */
		NS_NIS_CB(_yp_getnetbyname, NULL)
		NS_NULL_CB
	};

	_DIAGASSERT(name != NULL);

	retval = NULL;
	h_errno = NETDB_INTERNAL;
	rv = nsdispatch(NULL, dtab, NSDB_NETWORKS, "getnetbyname",
	    __nsdefaultsrc, &retval, name);
	if (rv == NS_SUCCESS) {
		h_errno = NETDB_SUCCESS;
		return retval;
	}
	return NULL;
}

#ifdef YP
/*ARGSUSED*/
static int
_yp_getnetbyaddr(void *cbrv, void *cb_data, va_list ap)
{
	struct netent	**retval = va_arg(ap, struct netent **);
	uint32_t	  net	 = va_arg(ap, uint32_t);
	int		  type	 = va_arg(ap, int);

	struct netent	*np;
	char		 qbuf[MAXDNAME];
	unsigned int	 netbr[4];
	uint32_t	 net2;
	int		 r;

	if (type != AF_INET)
		return NS_UNAVAIL;

	if (!__ypdomain) {
		if (_yp_check(&__ypdomain) == 0)
			return NS_UNAVAIL;
	}
	np = NULL;
	if (__ypcurrent)
		free(__ypcurrent);
	__ypcurrent = NULL;
	for (r = 4, net2 = net; net2; net2 >>= 8)
		netbr[--r] = (unsigned int)(net2 & 0xff);
	switch (r) {
	default:
		return NS_UNAVAIL;
	case 3: 	/* Class A */
		snprintf(qbuf, sizeof(qbuf), "%u", netbr[3]);
		break;
	case 2: 	/* Class B */
		snprintf(qbuf, sizeof(qbuf), "%u.%u", netbr[2], netbr[3]);
		break;
	case 1: 	/* Class C */
		snprintf(qbuf, sizeof(qbuf), "%u.%u.%u", netbr[1], netbr[2],
		    netbr[3]);
		break;
	case 0: 	/* Class D - E */
		snprintf(qbuf, sizeof(qbuf), "%u.%u.%u.%u", netbr[0], netbr[1],
		    netbr[2], netbr[3]);
		break;
	}
	r = yp_match(__ypdomain, "networks.byaddr", qbuf, (int)strlen(qbuf),
	    &__ypcurrent, &__ypcurrentlen);
	if (r == 0)
		np = _ypnetent(__ypcurrent);

	if (np != NULL) {
		*retval = np;
		return NS_SUCCESS;
	} else {
		h_errno = HOST_NOT_FOUND;
		return NS_NOTFOUND;
	}
}

/*ARGSUSED*/
static int
_yp_getnetbyname(void *cbrv, void *cbdata, va_list ap)
{
	struct netent	**retval = va_arg(ap, struct netent **);
	const char	 *name	 = va_arg(ap, const char *);

	struct netent	*np;
	int		 r;

	if (!__ypdomain) {
		if (_yp_check(&__ypdomain) == 0)
			return NS_UNAVAIL;
	}
	np = NULL;
	if (__ypcurrent)
		free(__ypcurrent);
	__ypcurrent = NULL;
	r = yp_match(__ypdomain, "networks.byname", name, (int)strlen(name),
	    &__ypcurrent, &__ypcurrentlen);
	if (r == 0)
		np = _ypnetent(__ypcurrent);

	if (np != NULL) {
		*retval = np;
		return NS_SUCCESS;
	} else {
		h_errno = HOST_NOT_FOUND;
		return NS_NOTFOUND;
	}
}

static struct netent *
_ypnetent(char *line)
{
	char *cp, *p, **q;

	_DIAGASSERT(line != NULL);

	net_entry.n_name = line;
	cp = strpbrk(line, " \t");
	if (cp == NULL)
		return NULL;
	*cp++ = '\0';
	while (*cp == ' ' || *cp == '\t')
		cp++;
	p = strpbrk(cp, " \t");
	if (p != NULL)
		*p++ = '\0';
	net_entry.n_net = inet_network(cp);
#if (defined(__sparc__) && defined(_LP64)) ||		\
    defined(__alpha__) ||				\
    (defined(__i386__) && defined(_LP64)) ||		\
    (defined(__sh__) && defined(_LP64))
	net_entry.__n_pad0 = 0;
#endif
	net_entry.n_addrtype = AF_INET;
	q = net_entry.n_aliases = net_aliases;
	if (p != NULL)  {
		cp = p;
		while (cp && *cp) {
			if (*cp == ' ' || *cp == '\t') {
				cp++;
				continue;
			}
			if (q < &net_aliases[MAXALIASES - 1])
				*q++ = cp;
			cp = strpbrk(cp, " \t");
			if (cp != NULL)
				*cp++ = '\0';
		}
	}
	*q = NULL;

	return &net_entry;
}
#endif
