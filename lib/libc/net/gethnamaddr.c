/*	$NetBSD: gethnamaddr.c,v 1.84 2013/08/27 09:56:12 christos Exp $	*/

/*
 * ++Copyright++ 1985, 1988, 1993
 * -
 * Copyright (c) 1985, 1988, 1993
 *    The Regents of the University of California.  All rights reserved.
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
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)gethostnamadr.c	8.1 (Berkeley) 6/4/93";
static char rcsid[] = "Id: gethnamaddr.c,v 8.21 1997/06/01 20:34:37 vixie Exp ";
#else
__RCSID("$NetBSD: gethnamaddr.c,v 1.84 2013/08/27 09:56:12 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#if defined(_LIBC)
#include "namespace.h"
#endif
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <resolv.h>
#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>

#ifndef LOG_AUTH
# define LOG_AUTH 0
#endif

#define MULTI_PTRS_ARE_ALIASES 1	/* XXX - experimental */

#include <nsswitch.h>
#include <stdlib.h>
#include <string.h>

#ifdef YP
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#endif

#include "hostent.h"

#if defined(_LIBC) && defined(__weak_alias)
__weak_alias(gethostbyaddr,_gethostbyaddr)
__weak_alias(gethostbyname,_gethostbyname)
__weak_alias(gethostent,_gethostent)
#endif

#define maybe_ok(res, nm, ok) (((res)->options & RES_NOCHECKNAME) != 0U || \
                               (ok)(nm) != 0)
#define maybe_hnok(res, hn) maybe_ok((res), (hn), res_hnok)
#define maybe_dnok(res, dn) maybe_ok((res), (dn), res_dnok)


static const char AskedForGot[] =
    "gethostby*.getanswer: asked for \"%s\", got \"%s\"";


#ifdef YP
static char *__ypdomain;
#endif

#define	MAXPACKET	(64*1024)

typedef union {
	HEADER hdr;
	u_char buf[MAXPACKET];
} querybuf;

typedef union {
	int32_t al;
	char ac;
} align;

#ifdef DEBUG
static void debugprintf(const char *, res_state, ...)
	__attribute__((__format__(__printf__, 1, 3)));
#endif
static struct hostent *getanswer(const querybuf *, int, const char *, int,
    res_state, struct hostent *, char *, size_t, int *);
static void map_v4v6_address(const char *, char *);
static void map_v4v6_hostent(struct hostent *, char **, char *);
static void addrsort(char **, int, res_state);

void dns_service(void);
#undef dn_skipname
int dn_skipname(const u_char *, const u_char *);

#ifdef YP
static struct hostent *_yp_hostent(char *, int, struct getnamaddr *);
#endif

static struct hostent *gethostbyname_internal(const char *, int, res_state,
    struct hostent *, char *, size_t, int *);

static const ns_src default_dns_files[] = {
	{ NSSRC_FILES, 	NS_SUCCESS },
	{ NSSRC_DNS, 	NS_SUCCESS },
	{ 0, 0 }
};


#ifdef DEBUG
static void
debugprintf(const char *msg, res_state res, ...)
{
	_DIAGASSERT(msg != NULL);

	if (res->options & RES_DEBUG) {
		int save = errno;
		va_list ap;

		va_start (ap, res);
		vprintf(msg, ap);
		va_end (ap);
		
		errno = save;
	}
}
#else
# define debugprintf(msg, res, num) /*nada*/
#endif

#define BOUNDED_INCR(x) \
	do { \
		cp += (x); \
		if (cp > eom) { \
			h_errno = NO_RECOVERY; \
			return NULL; \
		} \
	} while (/*CONSTCOND*/0)

#define BOUNDS_CHECK(ptr, count) \
	do { \
		if ((ptr) + (count) > eom) { \
			h_errno = NO_RECOVERY; \
			return NULL; \
		} \
	} while (/*CONSTCOND*/0)

static struct hostent *
getanswer(const querybuf *answer, int anslen, const char *qname, int qtype,
    res_state res, struct hostent *hent, char *buf, size_t buflen, int *he)
{
	const HEADER *hp;
	const u_char *cp;
	int n;
	size_t qlen;
	const u_char *eom, *erdata;
	char *bp, **ap, **hap, *ep;
	int type, class, ancount, qdcount;
	int haveanswer, had_error;
	int toobig = 0;
	char tbuf[MAXDNAME];
	char *aliases[MAXALIASES];
	char *addr_ptrs[MAXADDRS];
	const char *tname;
	int (*name_ok)(const char *);

	_DIAGASSERT(answer != NULL);
	_DIAGASSERT(qname != NULL);

	tname = qname;
	hent->h_name = NULL;
	eom = answer->buf + anslen;
	switch (qtype) {
	case T_A:
	case T_AAAA:
		name_ok = res_hnok;
		break;
	case T_PTR:
		name_ok = res_dnok;
		break;
	default:
		return NULL;	/* XXX should be abort(); */
	}
	/*
	 * find first satisfactory answer
	 */
	hp = &answer->hdr;
	ancount = ntohs(hp->ancount);
	qdcount = ntohs(hp->qdcount);
	bp = buf;
	ep = buf + buflen;
	cp = answer->buf;
	BOUNDED_INCR(HFIXEDSZ);
	if (qdcount != 1)
		goto no_recovery;

	n = dn_expand(answer->buf, eom, cp, bp, (int)(ep - bp));
	if ((n < 0) || !maybe_ok(res, bp, name_ok))
		goto no_recovery;

	BOUNDED_INCR(n + QFIXEDSZ);
	if (qtype == T_A || qtype == T_AAAA) {
		/* res_send() has already verified that the query name is the
		 * same as the one we sent; this just gets the expanded name
		 * (i.e., with the succeeding search-domain tacked on).
		 */
		n = (int)strlen(bp) + 1;		/* for the \0 */
		if (n >= MAXHOSTNAMELEN)
			goto no_recovery;
		hent->h_name = bp;
		bp += n;
		/* The qname can be abbreviated, but h_name is now absolute. */
		qname = hent->h_name;
	}
	hent->h_aliases = ap = aliases;
	hent->h_addr_list = hap = addr_ptrs;
	*ap = NULL;
	*hap = NULL;
	haveanswer = 0;
	had_error = 0;
	while (ancount-- > 0 && cp < eom && !had_error) {
		n = dn_expand(answer->buf, eom, cp, bp, (int)(ep - bp));
		if ((n < 0) || !maybe_ok(res, bp, name_ok)) {
			had_error++;
			continue;
		}
		cp += n;			/* name */
		BOUNDS_CHECK(cp, 3 * INT16SZ + INT32SZ);
		type = _getshort(cp);
 		cp += INT16SZ;			/* type */
		class = _getshort(cp);
 		cp += INT16SZ + INT32SZ;	/* class, TTL */
		n = _getshort(cp);
		cp += INT16SZ;			/* len */
		BOUNDS_CHECK(cp, n);
		erdata = cp + n;
		if (class != C_IN) {
			/* XXX - debug? syslog? */
			cp += n;
			continue;		/* XXX - had_error++ ? */
		}
		if ((qtype == T_A || qtype == T_AAAA) && type == T_CNAME) {
			if (ap >= &aliases[MAXALIASES-1])
				continue;
			n = dn_expand(answer->buf, eom, cp, tbuf,
			    (int)sizeof tbuf);
			if ((n < 0) || !maybe_ok(res, tbuf, name_ok)) {
				had_error++;
				continue;
			}
			cp += n;
			if (cp != erdata)
				goto no_recovery;
			/* Store alias. */
			*ap++ = bp;
			n = (int)strlen(bp) + 1;	/* for the \0 */
			if (n >= MAXHOSTNAMELEN) {
				had_error++;
				continue;
			}
			bp += n;
			/* Get canonical name. */
			n = (int)strlen(tbuf) + 1;	/* for the \0 */
			if (n > ep - bp || n >= MAXHOSTNAMELEN) {
				had_error++;
				continue;
			}
			strlcpy(bp, tbuf, (size_t)(ep - bp));
			hent->h_name = bp;
			bp += n;
			continue;
		}
		if (qtype == T_PTR && type == T_CNAME) {
			n = dn_expand(answer->buf, eom, cp, tbuf,
			    (int)sizeof tbuf);
			if (n < 0 || !maybe_dnok(res, tbuf)) {
				had_error++;
				continue;
			}
			cp += n;
			if (cp != erdata)
				goto no_recovery;
			/* Get canonical name. */
			n = (int)strlen(tbuf) + 1;	/* for the \0 */
			if (n > ep - bp || n >= MAXHOSTNAMELEN) {
				had_error++;
				continue;
			}
			strlcpy(bp, tbuf, (size_t)(ep - bp));
			tname = bp;
			bp += n;
			continue;
		}
		if (type != qtype) {
			if (type != T_KEY && type != T_SIG)
				syslog(LOG_NOTICE|LOG_AUTH,
	       "gethostby*.getanswer: asked for \"%s %s %s\", got type \"%s\"",
				       qname, p_class(C_IN), p_type(qtype),
				       p_type(type));
			cp += n;
			continue;		/* XXX - had_error++ ? */
		}
		switch (type) {
		case T_PTR:
			if (strcasecmp(tname, bp) != 0) {
				syslog(LOG_NOTICE|LOG_AUTH,
				       AskedForGot, qname, bp);
				cp += n;
				continue;	/* XXX - had_error++ ? */
			}
			n = dn_expand(answer->buf, eom, cp, bp, (int)(ep - bp));
			if ((n < 0) || !maybe_hnok(res, bp)) {
				had_error++;
				break;
			}
#if MULTI_PTRS_ARE_ALIASES
			cp += n;
			if (cp != erdata)
				goto no_recovery;
			if (!haveanswer)
				hent->h_name = bp;
			else if (ap < &aliases[MAXALIASES-1])
				*ap++ = bp;
			else
				n = -1;
			if (n != -1) {
				n = (int)strlen(bp) + 1;	/* for the \0 */
				if (n >= MAXHOSTNAMELEN) {
					had_error++;
					break;
				}
				bp += n;
			}
			break;
#else
			hent->h_name = bp;
			if (res->options & RES_USE_INET6) {
				n = strlen(bp) + 1;	/* for the \0 */
				if (n >= MAXHOSTNAMELEN) {
					had_error++;
					break;
				}
				bp += n;
				map_v4v6_hostent(hent, &bp, ep);
			}
			goto success;
#endif
		case T_A:
		case T_AAAA:
			if (strcasecmp(hent->h_name, bp) != 0) {
				syslog(LOG_NOTICE|LOG_AUTH,
				       AskedForGot, hent->h_name, bp);
				cp += n;
				continue;	/* XXX - had_error++ ? */
			}
			if (n != hent->h_length) {
				cp += n;
				continue;
			}
			if (type == T_AAAA) {
				struct in6_addr in6;
				memcpy(&in6, cp, NS_IN6ADDRSZ);
				if (IN6_IS_ADDR_V4MAPPED(&in6)) {
					cp += n;
					continue;
				}
			}
			if (!haveanswer) {
				int nn;

				hent->h_name = bp;
				nn = (int)strlen(bp) + 1;	/* for the \0 */
				bp += nn;
			}

			bp += sizeof(align) -
			    (size_t)((u_long)bp % sizeof(align));

			if (bp + n >= ep) {
				debugprintf("size (%d) too big\n", res, n);
				had_error++;
				continue;
			}
			if (hap >= &addr_ptrs[MAXADDRS - 1]) {
				if (!toobig++) {
					debugprintf("Too many addresses (%d)\n",
						res, MAXADDRS);
				}
				cp += n;
				continue;
			}
			(void)memcpy(*hap++ = bp, cp, (size_t)n);
			bp += n;
			cp += n;
			if (cp != erdata)
				goto no_recovery;
			break;
		default:
			abort();
		}
		if (!had_error)
			haveanswer++;
	}
	if (haveanswer) {
		*ap = NULL;
		*hap = NULL;
		/*
		 * Note: we sort even if host can take only one address
		 * in its return structures - should give it the "best"
		 * address in that case, not some random one
		 */
		if (res->nsort && haveanswer > 1 && qtype == T_A)
			addrsort(addr_ptrs, haveanswer, res);
		if (!hent->h_name) {
			n = (int)strlen(qname) + 1;	/* for the \0 */
			if (n > ep - bp || n >= MAXHOSTNAMELEN)
				goto no_recovery;
			strlcpy(bp, qname, (size_t)(ep - bp));
			hent->h_name = bp;
			bp += n;
		}
		if (res->options & RES_USE_INET6)
			map_v4v6_hostent(hent, &bp, ep);
	    	goto success;
	}
no_recovery:
	*he = NO_RECOVERY;
	return NULL;
success:
	bp = (char *)ALIGN(bp);
	n = (int)(ap - aliases);
	qlen = (n + 1) * sizeof(*hent->h_aliases);
	if ((size_t)(ep - bp) < qlen)
		goto nospc;
	hent->h_aliases = (void *)bp;
	memcpy(bp, aliases, qlen);

	bp += qlen;
	n = (int)(hap - addr_ptrs);
	qlen = (n + 1) * sizeof(*hent->h_addr_list);
	if ((size_t)(ep - bp) < qlen)
		goto nospc;
	hent->h_addr_list = (void *)bp;
	memcpy(bp, addr_ptrs, qlen);
	*he = NETDB_SUCCESS;
	return hent;
nospc:
	errno = ENOSPC;
	*he = NETDB_INTERNAL;
	return NULL;
}

struct hostent *
gethostbyname_r(const char *name, struct hostent *hp, char *buf, size_t buflen,
    int *he)
{
	res_state res = __res_get_state();

	if (res == NULL) {
		*he = NETDB_INTERNAL;
		return NULL;
	}

	_DIAGASSERT(name != NULL);

	if (res->options & RES_USE_INET6) {
		hp = gethostbyname_internal(name, AF_INET6, res, hp, buf,
		    buflen, he);
		if (hp) {
			__res_put_state(res);
			return hp;
		}
	}
	hp = gethostbyname_internal(name, AF_INET, res, hp, buf, buflen, he);
	__res_put_state(res);
	return hp;
}

struct hostent *
gethostbyname2_r(const char *name, int af, struct hostent *hp, char *buf,
    size_t buflen, int *he)
{
	res_state res = __res_get_state();

	if (res == NULL) {
		*he = NETDB_INTERNAL;
		return NULL;
	}
	hp = gethostbyname_internal(name, af, res, hp, buf, buflen, he);
	__res_put_state(res);
	return hp;
}

static struct hostent *
gethostbyname_internal(const char *name, int af, res_state res,
    struct hostent *hp, char *buf, size_t buflen, int *he)
{
	const char *cp;
	struct getnamaddr info;
	size_t size;
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_hf_gethtbyname, NULL)
		{ NSSRC_DNS, _dns_gethtbyname, NULL },	/* force -DHESIOD */
		NS_NIS_CB(_yp_gethtbyname, NULL)
		NS_NULL_CB
	};

	_DIAGASSERT(name != NULL);

	switch (af) {
	case AF_INET:
		size = NS_INADDRSZ;
		break;
	case AF_INET6:
		size = NS_IN6ADDRSZ;
		break;
	default:
		*he = NETDB_INTERNAL;
		errno = EAFNOSUPPORT;
		return NULL;
	}
	if (buflen < size)
		goto nospc;

	hp->h_addrtype = af;
	hp->h_length = (int)size;

	/*
	 * if there aren't any dots, it could be a user-level alias.
	 * this is also done in res_nquery() since we are not the only
	 * function that looks up host names.
	 */
	if (!strchr(name, '.') && (cp = __hostalias(name)))
		name = cp;

	/*
	 * disallow names consisting only of digits/dots, unless
	 * they end in a dot.
	 */
	if (isdigit((u_char) name[0]))
		for (cp = name;; ++cp) {
			if (!*cp) {
				if (*--cp == '.')
					break;
				/*
				 * All-numeric, no dot at the end.
				 * Fake up a hostent as if we'd actually
				 * done a lookup.
				 */
				goto fake;
			}
			if (!isdigit((u_char) *cp) && *cp != '.') 
				break;
		}
	if ((isxdigit((u_char) name[0]) && strchr(name, ':') != NULL) ||
	    name[0] == ':')
		for (cp = name;; ++cp) {
			if (!*cp) {
				if (*--cp == '.')
					break;
				/*
				 * All-IPv6-legal, no dot at the end.
				 * Fake up a hostent as if we'd actually
				 * done a lookup.
				 */
				goto fake;
			}
			if (!isxdigit((u_char) *cp) && *cp != ':' && *cp != '.')
				break;
		}

	*he = NETDB_INTERNAL;
	info.hp = hp;
	info.buf = buf;
	info.buflen = buflen;
	info.he = he;
	if (nsdispatch(&info, dtab, NSDB_HOSTS, "gethostbyname",
	    default_dns_files, name, strlen(name), af) != NS_SUCCESS)
		return NULL;
	*he = NETDB_SUCCESS;
	return hp;
nospc:
	*he = NETDB_INTERNAL;
	errno = ENOSPC;
	return NULL;
fake:
	HENT_ARRAY(hp->h_addr_list, 1, buf, buflen);
	HENT_ARRAY(hp->h_aliases, 0, buf, buflen);

	hp->h_aliases[0] = NULL;
	if (size > buflen)
		goto nospc;

	if (inet_pton(af, name, buf) <= 0) {
		*he = HOST_NOT_FOUND;
		return NULL;
	}
	hp->h_addr_list[0] = buf;
	hp->h_addr_list[1] = NULL;
	buf += size;
	buflen -= size;
	HENT_SCOPY(hp->h_name, name, buf, buflen);
	if (res->options & RES_USE_INET6)
		map_v4v6_hostent(hp, &buf, buf + buflen);
	*he = NETDB_SUCCESS;
	return hp;
}

struct hostent *
gethostbyaddr_r(const void *addr, socklen_t len, int af, struct hostent *hp,
    char *buf, size_t buflen, int *he)
{
	const u_char *uaddr = (const u_char *)addr;
	socklen_t size;
	struct getnamaddr info;
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_hf_gethtbyaddr, NULL)
		{ NSSRC_DNS, _dns_gethtbyaddr, NULL },	/* force -DHESIOD */
		NS_NIS_CB(_yp_gethtbyaddr, NULL)
		NS_NULL_CB
	};
	
	_DIAGASSERT(addr != NULL);

	if (af == AF_INET6 && len == NS_IN6ADDRSZ &&
	    (IN6_IS_ADDR_LINKLOCAL((const struct in6_addr *)addr) ||
	     IN6_IS_ADDR_SITELOCAL((const struct in6_addr *)addr))) {
		*he = HOST_NOT_FOUND;
		return NULL;
	}
	if (af == AF_INET6 && len == NS_IN6ADDRSZ &&
	    (IN6_IS_ADDR_V4MAPPED((const struct in6_addr *)addr) ||
	     IN6_IS_ADDR_V4COMPAT((const struct in6_addr *)addr))) {
		/* Unmap. */
		uaddr += NS_IN6ADDRSZ - NS_INADDRSZ;
		addr = uaddr;
		af = AF_INET;
		len = NS_INADDRSZ;
	}
	switch (af) {
	case AF_INET:
		size = NS_INADDRSZ;
		break;
	case AF_INET6:
		size = NS_IN6ADDRSZ;
		break;
	default:
		errno = EAFNOSUPPORT;
		*he = NETDB_INTERNAL;
		return NULL;
	}
	if (size != len) {
		errno = EINVAL;
		*he = NETDB_INTERNAL;
		return NULL;
	}
	info.hp = hp;
	info.buf = buf;
	info.buflen = buflen;
	info.he = he;
	*he = NETDB_INTERNAL;
	if (nsdispatch(&info, dtab, NSDB_HOSTS, "gethostbyaddr",
	    default_dns_files, uaddr, len, af) != NS_SUCCESS)
		return NULL;
	*he = NETDB_SUCCESS;
	return hp;
}

struct hostent *
gethostent_r(FILE *hf, struct hostent *hent, char *buf, size_t buflen, int *he)
{
	char *p, *name;
	char *cp, **q;
	int af, len;
	size_t llen, anum;
	char *aliases[MAXALIASES];
	struct in6_addr host_addr;

	if (hf == NULL) {
		*he = NETDB_INTERNAL;
		errno = EINVAL;
		return NULL;
	}
 again:
	if ((p = fgetln(hf, &llen)) == NULL) {
		*he = HOST_NOT_FOUND;
		return NULL;
	}
	if (llen < 1)
		goto again;
	if (*p == '#')
		goto again;
	p[llen] = '\0';
	if (!(cp = strpbrk(p, "#\n")))
		goto again;
	*cp = '\0';
	if (!(cp = strpbrk(p, " \t")))
		goto again;
	*cp++ = '\0';
	if (inet_pton(AF_INET6, p, &host_addr) > 0) {
		af = AF_INET6;
		len = NS_IN6ADDRSZ;
	} else if (inet_pton(AF_INET, p, &host_addr) > 0) {
		res_state res = __res_get_state();
		if (res == NULL)
			return NULL;
		if (res->options & RES_USE_INET6) {
			map_v4v6_address(buf, buf);
			af = AF_INET6;
			len = NS_IN6ADDRSZ;
		} else {
			af = AF_INET;
			len = NS_INADDRSZ;
		}
		__res_put_state(res);
	} else {
		goto again;
	}
	/* if this is not something we're looking for, skip it. */
	if (hent->h_addrtype != 0 && hent->h_addrtype != af)
		goto again;
	if (hent->h_length != 0 && hent->h_length != len)
		goto again;

	while (*cp == ' ' || *cp == '\t')
		cp++;
	if ((cp = strpbrk(name = cp, " \t")) != NULL)
		*cp++ = '\0';
	q = aliases;
	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		if (q >= &aliases[__arraycount(aliases)])
			goto nospc;
		*q++ = cp;
		if ((cp = strpbrk(cp, " \t")) != NULL)
			*cp++ = '\0';
	}
	hent->h_length = len;
	hent->h_addrtype = af;
	HENT_ARRAY(hent->h_addr_list, 1, buf, buflen);
	anum = (size_t)(q - aliases);
	HENT_ARRAY(hent->h_aliases, anum, buf, buflen);
	HENT_COPY(hent->h_addr_list[0], &host_addr, hent->h_length, buf,
	    buflen);
	hent->h_addr_list[1] = NULL;

	HENT_SCOPY(hent->h_name, name, buf, buflen);
	for (size_t i = 0; i < anum; i++)
		HENT_SCOPY(hent->h_aliases[i], aliases[i], buf, buflen);
	hent->h_aliases[anum] = NULL;

	*he = NETDB_SUCCESS;
	return hent;
nospc:
	errno = ENOSPC;
	*he = NETDB_INTERNAL;
	return NULL;
}

static void
map_v4v6_address(const char *src, char *dst)
{
	u_char *p = (u_char *)dst;
	char tmp[NS_INADDRSZ];
	int i;

	_DIAGASSERT(src != NULL);
	_DIAGASSERT(dst != NULL);

	/* Stash a temporary copy so our caller can update in place. */
	(void)memcpy(tmp, src, NS_INADDRSZ);
	/* Mark this ipv6 addr as a mapped ipv4. */
	for (i = 0; i < 10; i++)
		*p++ = 0x00;
	*p++ = 0xff;
	*p++ = 0xff;
	/* Retrieve the saved copy and we're done. */
	(void)memcpy(p, tmp, NS_INADDRSZ);
}

static void
map_v4v6_hostent(struct hostent *hp, char **bpp, char *ep)
{
	char **ap;

	_DIAGASSERT(hp != NULL);
	_DIAGASSERT(bpp != NULL);
	_DIAGASSERT(ep != NULL);

	if (hp->h_addrtype != AF_INET || hp->h_length != NS_INADDRSZ)
		return;
	hp->h_addrtype = AF_INET6;
	hp->h_length = NS_IN6ADDRSZ;
	for (ap = hp->h_addr_list; *ap; ap++) {
		int i = (int)(sizeof(align) -
		    (size_t)((u_long)*bpp % sizeof(align)));

		if (ep - *bpp < (i + NS_IN6ADDRSZ)) {
			/* Out of memory.  Truncate address list here.  XXX */
			*ap = NULL;
			return;
		}
		*bpp += i;
		map_v4v6_address(*ap, *bpp);
		*ap = *bpp;
		*bpp += NS_IN6ADDRSZ;
	}
}

static void
addrsort(char **ap, int num, res_state res)
{
	int i, j;
	char **p;
	short aval[MAXADDRS];
	int needsort = 0;

	_DIAGASSERT(ap != NULL);

	p = ap;
	for (i = 0; i < num; i++, p++) {
	    for (j = 0 ; (unsigned)j < res->nsort; j++)
		if (res->sort_list[j].addr.s_addr == 
		    (((struct in_addr *)(void *)(*p))->s_addr &
		    res->sort_list[j].mask))
			break;
	    aval[i] = j;
	    if (needsort == 0 && i > 0 && j < aval[i-1])
		needsort = i;
	}
	if (!needsort)
	    return;

	while (needsort < num) {
	    for (j = needsort - 1; j >= 0; j--) {
		if (aval[j] > aval[j+1]) {
		    char *hp;

		    i = aval[j];
		    aval[j] = aval[j+1];
		    aval[j+1] = i;

		    hp = ap[j];
		    ap[j] = ap[j+1];
		    ap[j+1] = hp;
		} else
		    break;
	    }
	    needsort++;
	}
}


/*ARGSUSED*/
int
_dns_gethtbyname(void *rv, void *cb_data, va_list ap)
{
	querybuf *buf;
	int n, type;
	struct hostent *hp;
	const char *name;
	res_state res;
	struct getnamaddr *info = rv;

	_DIAGASSERT(rv != NULL);

	name = va_arg(ap, char *);
	/* NOSTRICT skip string len */(void)va_arg(ap, int);
	info->hp->h_addrtype = va_arg(ap, int);

	switch (info->hp->h_addrtype) {
	case AF_INET:
		info->hp->h_length = NS_INADDRSZ;
		type = T_A;
		break;
	case AF_INET6:
		info->hp->h_length = NS_IN6ADDRSZ;
		type = T_AAAA;
		break;
	default:
		return NS_UNAVAIL;
	}
	buf = malloc(sizeof(*buf));
	if (buf == NULL) {
		*info->he = NETDB_INTERNAL;
		return NS_NOTFOUND;
	}
	res = __res_get_state();
	if (res == NULL) {
		free(buf);
		return NS_NOTFOUND;
	}
	n = res_nsearch(res, name, C_IN, type, buf->buf, (int)sizeof(buf->buf));
	if (n < 0) {
		free(buf);
		debugprintf("res_nsearch failed (%d)\n", res, n);
		__res_put_state(res);
		return NS_NOTFOUND;
	}
	hp = getanswer(buf, n, name, type, res, info->hp, info->buf,
	    info->buflen, info->he);
	free(buf);
	__res_put_state(res);
	if (hp == NULL)
		switch (h_errno) {
		case HOST_NOT_FOUND:
			return NS_NOTFOUND;
		case TRY_AGAIN:
			return NS_TRYAGAIN;
		default:
			return NS_UNAVAIL;
		}
	return NS_SUCCESS;
}

/*ARGSUSED*/
int
_dns_gethtbyaddr(void *rv, void	*cb_data, va_list ap)
{
	char qbuf[MAXDNAME + 1], *qp, *ep;
	int n;
	querybuf *buf;
	struct hostent *hp;
	const unsigned char *uaddr;
	int advance;
	res_state res;
	char *bf;
	size_t blen;
	struct getnamaddr *info = rv;

	_DIAGASSERT(rv != NULL);

	uaddr = va_arg(ap, unsigned char *);
	info->hp->h_length = va_arg(ap, int);
	info->hp->h_addrtype = va_arg(ap, int);

	switch (info->hp->h_addrtype) {
	case AF_INET:
		(void)snprintf(qbuf, sizeof(qbuf), "%u.%u.%u.%u.in-addr.arpa",
		    (uaddr[3] & 0xff), (uaddr[2] & 0xff),
		    (uaddr[1] & 0xff), (uaddr[0] & 0xff));
		break;

	case AF_INET6:
		qp = qbuf;
		ep = qbuf + sizeof(qbuf) - 1;
		for (n = NS_IN6ADDRSZ - 1; n >= 0; n--) {
			advance = snprintf(qp, (size_t)(ep - qp), "%x.%x.",
			    uaddr[n] & 0xf,
			    ((unsigned int)uaddr[n] >> 4) & 0xf);
			if (advance > 0 && qp + advance < ep)
				qp += advance;
			else {
				*info->he = NETDB_INTERNAL;
				return NS_NOTFOUND;
			}
		}
		if (strlcat(qbuf, "ip6.arpa", sizeof(qbuf)) >= sizeof(qbuf)) {
			*info->he = NETDB_INTERNAL;
			return NS_NOTFOUND;
		}
		break;
	default:
		return NS_UNAVAIL;
	}

	buf = malloc(sizeof(*buf));
	if (buf == NULL) {
		*info->he = NETDB_INTERNAL;
		return NS_NOTFOUND;
	}
	res = __res_get_state();
	if (res == NULL) {
		free(buf);
		return NS_NOTFOUND;
	}
	n = res_nquery(res, qbuf, C_IN, T_PTR, buf->buf, (int)sizeof(buf->buf));
	if (n < 0) {
		free(buf);
		debugprintf("res_nquery failed (%d)\n", res, n);
		__res_put_state(res);
		return NS_NOTFOUND;
	}
	hp = getanswer(buf, n, qbuf, T_PTR, res, info->hp, info->buf,
	    info->buflen, info->he);
	free(buf);
	if (hp == NULL) {
		__res_put_state(res);
		switch (*info->he) {
		case HOST_NOT_FOUND:
			return NS_NOTFOUND;
		case TRY_AGAIN:
			return NS_TRYAGAIN;
		default:
			return NS_UNAVAIL;
		}
	}

	bf = (void *)(hp->h_addr_list + 2);
	blen = (size_t)(bf - info->buf);
	if (blen + info->hp->h_length > info->buflen)
		goto nospc;
	hp->h_addr_list[0] = bf;
	hp->h_addr_list[1] = NULL;
	(void)memcpy(bf, uaddr, (size_t)info->hp->h_length);
	if (info->hp->h_addrtype == AF_INET && (res->options & RES_USE_INET6)) {
		if (blen + NS_IN6ADDRSZ > info->buflen)
			goto nospc;
		map_v4v6_address(bf, bf);
		hp->h_addrtype = AF_INET6;
		hp->h_length = NS_IN6ADDRSZ;
	}

	__res_put_state(res);
	*info->he = NETDB_SUCCESS;
	return NS_SUCCESS;
nospc:
	*info->he = NETDB_INTERNAL;
	return NS_UNAVAIL;
}

#ifdef YP
/*ARGSUSED*/
static struct hostent *
_yp_hostent(char *line, int af, struct getnamaddr *info)
{
	struct in6_addr host_addrs[MAXADDRS];
	char *aliases[MAXALIASES];
	char *p = line;
	char *cp, **q, *ptr;
	size_t len, anum, i;
	int addrok;
	int more;
	size_t naddrs;
	struct hostent *hp = info->hp;

	_DIAGASSERT(line != NULL);

	hp->h_name = NULL;
	hp->h_addrtype = af;
	switch (af) {
	case AF_INET:
		hp->h_length = NS_INADDRSZ;
		break;
	case AF_INET6:
		hp->h_length = NS_IN6ADDRSZ;
		break;
	default:
		return NULL;
	}
	naddrs = 0;
	q = aliases;

nextline:
	/* check for host_addrs overflow */
	if (naddrs >= __arraycount(host_addrs))
		goto done;

	more = 0;
	cp = strpbrk(p, " \t");
	if (cp == NULL)
		goto done;
	*cp++ = '\0';

	/* p has should have an address */
	addrok = inet_pton(af, p, &host_addrs[naddrs]);
	if (addrok != 1) {
		/* skip to the next line */
		while (cp && *cp) {
			if (*cp == '\n') {
				cp++;
				goto nextline;
			}
			cp++;
		}
		goto done;
	}

	while (*cp == ' ' || *cp == '\t')
		cp++;
	p = cp;
	cp = strpbrk(p, " \t\n");
	if (cp != NULL) {
		if (*cp == '\n')
			more = 1;
		*cp++ = '\0';
	}
	if (!hp->h_name)
		hp->h_name = p;
	else if (strcmp(hp->h_name, p) == 0)
		;
	else if (q < &aliases[MAXALIASES - 1])
		*q++ = p;
	p = cp;
	if (more)
		goto nextline;

	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		if (*cp == '\n') {
			cp++;
			goto nextline;
		}
		if (q < &aliases[MAXALIASES - 1])
			*q++ = cp;
		cp = strpbrk(cp, " \t");
		if (cp != NULL)
			*cp++ = '\0';
	}

done:
	if (hp->h_name == NULL)
		return NULL;

	ptr = info->buf;
	len = info->buflen;

	anum = (size_t)(q - aliases);
	HENT_ARRAY(hp->h_addr_list, naddrs, ptr, len);
	HENT_ARRAY(hp->h_aliases, anum, ptr, len);

	for (i = 0; i < naddrs; i++)
		HENT_COPY(hp->h_addr_list[i], &host_addrs[i], hp->h_length,
		    ptr, len);
	hp->h_addr_list[naddrs] = NULL;

	HENT_SCOPY(hp->h_name, hp->h_name, ptr, len);

	for (i = 0; i < anum; i++)
		HENT_SCOPY(hp->h_aliases[i], aliases[i], ptr, len);
	hp->h_aliases[anum] = NULL;

	return hp;
nospc:
	*info->he = NETDB_INTERNAL;
	errno = ENOSPC;
	return NULL;
}

/*ARGSUSED*/
int
_yp_gethtbyaddr(void *rv, void *cb_data, va_list ap)
{
	struct hostent *hp = NULL;
	char *ypcurrent;
	int ypcurrentlen, r;
	char name[INET6_ADDRSTRLEN];	/* XXX enough? */
	const unsigned char *uaddr;
	int af;
	const char *map;
	struct getnamaddr *info = rv;

	_DIAGASSERT(rv != NULL);

	uaddr = va_arg(ap, unsigned char *);
	/* NOSTRICT skip len */(void)va_arg(ap, int);
	af = va_arg(ap, int);
	
	if (!__ypdomain) {
		if (_yp_check(&__ypdomain) == 0)
			return NS_UNAVAIL;
	}
	/*
	 * XXX unfortunately, we cannot support IPv6 extended scoped address
	 * notation here.  gethostbyaddr() is not scope-aware.  too bad.
	 */
	if (inet_ntop(af, uaddr, name, (socklen_t)sizeof(name)) == NULL)
		return NS_UNAVAIL;
	switch (af) {
	case AF_INET:
		map = "hosts.byaddr";
		break;
	default:
		map = "ipnodes.byaddr";
		break;
	}
	ypcurrent = NULL;
	r = yp_match(__ypdomain, map, name,
		(int)strlen(name), &ypcurrent, &ypcurrentlen);
	if (r == 0)
		hp = _yp_hostent(ypcurrent, af, info);
	else
		hp = NULL;
	free(ypcurrent);
	if (hp == NULL) {
		*info->he = HOST_NOT_FOUND;
		return NS_NOTFOUND;
	}
	return NS_SUCCESS;
}

/*ARGSUSED*/
int
_yp_gethtbyname(void *rv, void *cb_data, va_list ap)
{
	struct hostent *hp;
	char *ypcurrent;
	int ypcurrentlen, r;
	const char *name;
	int af;
	const char *map;
	struct getnamaddr *info = rv;

	_DIAGASSERT(rv != NULL);

	name = va_arg(ap, char *);
	/* NOSTRICT skip string len */(void)va_arg(ap, int);
	af = va_arg(ap, int);

	if (!__ypdomain) {
		if (_yp_check(&__ypdomain) == 0)
			return NS_UNAVAIL;
	}
	switch (af) {
	case AF_INET:
		map = "hosts.byname";
		break;
	default:
		map = "ipnodes.byname";
		break;
	}
	ypcurrent = NULL;
	r = yp_match(__ypdomain, map, name,
		(int)strlen(name), &ypcurrent, &ypcurrentlen);
	if (r == 0)
		hp = _yp_hostent(ypcurrent, af, info);
	else
		hp = NULL;
	free(ypcurrent);
	if (hp == NULL) {
		*info->he = HOST_NOT_FOUND;
		return NS_NOTFOUND;
	}
	return NS_SUCCESS;
}
#endif

/*
 * Non-reentrant versions.
 */
FILE *_h_file;
static struct hostent h_ent;
static char h_buf[16384];

struct hostent *
gethostbyaddr(const void *addr, socklen_t len, int af) {
	return gethostbyaddr_r(addr, len, af, &h_ent, h_buf, sizeof(h_buf),
	    &h_errno);
}

struct hostent *
gethostbyname(const char *name) {
	return gethostbyname_r(name, &h_ent, h_buf, sizeof(h_buf), &h_errno);
}

struct hostent *
gethostbyname2(const char *name, int af) {
	return gethostbyname2_r(name, af, &h_ent, h_buf, sizeof(h_buf),
	    &h_errno);
}

struct hostent *
gethostent(void)
{
	if (_h_file == NULL) {
		sethostent_r(&_h_file);
		if (_h_file == NULL) {
			h_errno = NETDB_INTERNAL;
			return NULL;
		}
	}
	memset(&h_ent, 0, sizeof(h_ent));
	return gethostent_r(_h_file, &h_ent, h_buf, sizeof(h_buf), &h_errno);
}

