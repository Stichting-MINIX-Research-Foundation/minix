/*	$NetBSD: gethnamaddr.c,v 1.79 2012/09/09 16:42:23 christos Exp $	*/

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
__RCSID("$NetBSD: gethnamaddr.c,v 1.79 2012/09/09 16:42:23 christos Exp $");
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

#if defined(_LIBC) && defined(__weak_alias)
__weak_alias(gethostbyaddr,_gethostbyaddr)
__weak_alias(gethostbyname,_gethostbyname)
__weak_alias(gethostent,_gethostent)
#endif

#define maybe_ok(res, nm, ok) (((res)->options & RES_NOCHECKNAME) != 0U || \
                               (ok)(nm) != 0)
#define maybe_hnok(res, hn) maybe_ok((res), (hn), res_hnok)
#define maybe_dnok(res, dn) maybe_ok((res), (dn), res_dnok)


#define	MAXALIASES	35
#define	MAXADDRS	35

static const char AskedForGot[] =
			  "gethostby*.getanswer: asked for \"%s\", got \"%s\"";

static char *h_addr_ptrs[MAXADDRS + 1];

#ifdef YP
static char *__ypdomain;
#endif

static struct hostent host;
static char *host_aliases[MAXALIASES];
static char hostbuf[8*1024];
static u_int32_t host_addr[16 / sizeof(u_int32_t)];	/* IPv4 or IPv6 */
static FILE *hostf = NULL;
static int stayopen = 0;

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
    res_state);
static void map_v4v6_address(const char *, char *);
static void map_v4v6_hostent(struct hostent *, char **, char *);
static void addrsort(char **, int, res_state);

void _sethtent(int);
void _endhtent(void);
struct hostent *_gethtent(void);
void ht_sethostent(int);
void ht_endhostent(void);
struct hostent *ht_gethostbyname(char *);
struct hostent *ht_gethostbyaddr(const char *, int, int);
void dns_service(void);
#undef dn_skipname
int dn_skipname(const u_char *, const u_char *);
int _gethtbyaddr(void *, void *, va_list);
int _gethtbyname(void *, void *, va_list);
struct hostent *_gethtbyname2(const char *, int);
int _dns_gethtbyaddr(void *, void *, va_list);
int _dns_gethtbyname(void *, void *, va_list);
#ifdef YP
struct hostent *_yphostent(char *, int);
int _yp_gethtbyaddr(void *, void *, va_list);
int _yp_gethtbyname(void *, void *, va_list);
#endif

static struct hostent *gethostbyname_internal(const char *, int, res_state);

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
    res_state res)
{
	const HEADER *hp;
	const u_char *cp;
	int n;
	const u_char *eom, *erdata;
	char *bp, **ap, **hap, *ep;
	int type, class, ancount, qdcount;
	int haveanswer, had_error;
	int toobig = 0;
	char tbuf[MAXDNAME];
	const char *tname;
	int (*name_ok)(const char *);

	_DIAGASSERT(answer != NULL);
	_DIAGASSERT(qname != NULL);

	tname = qname;
	host.h_name = NULL;
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
	bp = hostbuf;
	ep = hostbuf + sizeof hostbuf;
	cp = answer->buf;
	BOUNDED_INCR(HFIXEDSZ);
	if (qdcount != 1) {
		h_errno = NO_RECOVERY;
		return NULL;
	}
	n = dn_expand(answer->buf, eom, cp, bp, (int)(ep - bp));
	if ((n < 0) || !maybe_ok(res, bp, name_ok)) {
		h_errno = NO_RECOVERY;
		return NULL;
	}
	BOUNDED_INCR(n + QFIXEDSZ);
	if (qtype == T_A || qtype == T_AAAA) {
		/* res_send() has already verified that the query name is the
		 * same as the one we sent; this just gets the expanded name
		 * (i.e., with the succeeding search-domain tacked on).
		 */
		n = (int)strlen(bp) + 1;		/* for the \0 */
		if (n >= MAXHOSTNAMELEN) {
			h_errno = NO_RECOVERY;
			return NULL;
		}
		host.h_name = bp;
		bp += n;
		/* The qname can be abbreviated, but h_name is now absolute. */
		qname = host.h_name;
	}
	ap = host_aliases;
	*ap = NULL;
	host.h_aliases = host_aliases;
	hap = h_addr_ptrs;
	*hap = NULL;
	host.h_addr_list = h_addr_ptrs;
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
			if (ap >= &host_aliases[MAXALIASES-1])
				continue;
			n = dn_expand(answer->buf, eom, cp, tbuf, (int)sizeof tbuf);
			if ((n < 0) || !maybe_ok(res, tbuf, name_ok)) {
				had_error++;
				continue;
			}
			cp += n;
			if (cp != erdata) {
				h_errno = NO_RECOVERY;
				return NULL;
			}
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
			host.h_name = bp;
			bp += n;
			continue;
		}
		if (qtype == T_PTR && type == T_CNAME) {
			n = dn_expand(answer->buf, eom, cp, tbuf, (int)sizeof tbuf);
			if (n < 0 || !maybe_dnok(res, tbuf)) {
				had_error++;
				continue;
			}
			cp += n;
			if (cp != erdata) {
				h_errno = NO_RECOVERY;
				return NULL;
			}
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
			if (cp != erdata) {
				h_errno = NO_RECOVERY;
				return NULL;
			}
			if (!haveanswer)
				host.h_name = bp;
			else if (ap < &host_aliases[MAXALIASES-1])
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
			host.h_name = bp;
			if (res->options & RES_USE_INET6) {
				n = strlen(bp) + 1;	/* for the \0 */
				if (n >= MAXHOSTNAMELEN) {
					had_error++;
					break;
				}
				bp += n;
				map_v4v6_hostent(&host, &bp, ep);
			}
			h_errno = NETDB_SUCCESS;
			return &host;
#endif
		case T_A:
		case T_AAAA:
			if (strcasecmp(host.h_name, bp) != 0) {
				syslog(LOG_NOTICE|LOG_AUTH,
				       AskedForGot, host.h_name, bp);
				cp += n;
				continue;	/* XXX - had_error++ ? */
			}
			if (n != host.h_length) {
				cp += n;
				continue;
			}
			if (type == T_AAAA) {
				struct in6_addr in6;
				memcpy(&in6, cp, IN6ADDRSZ);
				if (IN6_IS_ADDR_V4MAPPED(&in6)) {
					cp += n;
					continue;
				}
			}
			if (!haveanswer) {
				int nn;

				host.h_name = bp;
				nn = (int)strlen(bp) + 1;	/* for the \0 */
				bp += nn;
			}

			bp += sizeof(align) -
			    (size_t)((u_long)bp % sizeof(align));

			if (bp + n >= &hostbuf[sizeof hostbuf]) {
				debugprintf("size (%d) too big\n", res, n);
				had_error++;
				continue;
			}
			if (hap >= &h_addr_ptrs[MAXADDRS-1]) {
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
			if (cp != erdata) {
				h_errno = NO_RECOVERY;
				return NULL;
			}
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
			addrsort(h_addr_ptrs, haveanswer, res);
		if (!host.h_name) {
			n = (int)strlen(qname) + 1;	/* for the \0 */
			if (n > ep - bp || n >= MAXHOSTNAMELEN)
				goto no_recovery;
			strlcpy(bp, qname, (size_t)(ep - bp));
			host.h_name = bp;
			bp += n;
		}
		if (res->options & RES_USE_INET6)
			map_v4v6_hostent(&host, &bp, ep);
		h_errno = NETDB_SUCCESS;
		return &host;
	}
 no_recovery:
	h_errno = NO_RECOVERY;
	return NULL;
}

struct hostent *
gethostbyname(const char *name)
{
	struct hostent *hp;
	res_state res = __res_get_state();

	if (res == NULL)
		return NULL;

	_DIAGASSERT(name != NULL);

	if (res->options & RES_USE_INET6) {
		hp = gethostbyname_internal(name, AF_INET6, res);
		if (hp) {
			__res_put_state(res);
			return hp;
		}
	}
	hp = gethostbyname_internal(name, AF_INET, res);
	__res_put_state(res);
	return hp;
}

struct hostent *
gethostbyname2(const char *name, int af)
{
	struct hostent *hp;
	res_state res = __res_get_state();

	if (res == NULL)
		return NULL;
	hp = gethostbyname_internal(name, af, res);
	__res_put_state(res);
	return hp;
}

static struct hostent *
gethostbyname_internal(const char *name, int af, res_state res)
{
	const char *cp;
	char *bp, *ep;
	int size;
	struct hostent *hp;
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_gethtbyname, NULL)
		{ NSSRC_DNS, _dns_gethtbyname, NULL },	/* force -DHESIOD */
		NS_NIS_CB(_yp_gethtbyname, NULL)
		NS_NULL_CB
	};

	_DIAGASSERT(name != NULL);

	switch (af) {
	case AF_INET:
		size = INADDRSZ;
		break;
	case AF_INET6:
		size = IN6ADDRSZ;
		break;
	default:
		h_errno = NETDB_INTERNAL;
		errno = EAFNOSUPPORT;
		return NULL;
	}

	host.h_addrtype = af;
	host.h_length = size;

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
				if (inet_pton(af, name,
				    (char *)(void *)host_addr) <= 0) {
					h_errno = HOST_NOT_FOUND;
					return NULL;
				}
				strncpy(hostbuf, name, MAXDNAME);
				hostbuf[MAXDNAME] = '\0';
				bp = hostbuf + MAXDNAME;
				ep = hostbuf + sizeof hostbuf;
				host.h_name = hostbuf;
				host.h_aliases = host_aliases;
				host_aliases[0] = NULL;
				h_addr_ptrs[0] = (char *)(void *)host_addr;
				h_addr_ptrs[1] = NULL;
				host.h_addr_list = h_addr_ptrs;
				if (res->options & RES_USE_INET6)
					map_v4v6_hostent(&host, &bp, ep);
				h_errno = NETDB_SUCCESS;
				return &host;
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
				if (inet_pton(af, name,
				    (char *)(void *)host_addr) <= 0) {
					h_errno = HOST_NOT_FOUND;
					return NULL;
				}
				strncpy(hostbuf, name, MAXDNAME);
				hostbuf[MAXDNAME] = '\0';
				bp = hostbuf + MAXDNAME;
				ep = hostbuf + sizeof hostbuf;
				host.h_name = hostbuf;
				host.h_aliases = host_aliases;
				host_aliases[0] = NULL;
				h_addr_ptrs[0] = (char *)(void *)host_addr;
				h_addr_ptrs[1] = NULL;
				host.h_addr_list = h_addr_ptrs;
				h_errno = NETDB_SUCCESS;
				return &host;
			}
			if (!isxdigit((u_char) *cp) && *cp != ':' && *cp != '.')
				break;
		}

	hp = NULL;
	h_errno = NETDB_INTERNAL;
	if (nsdispatch(&hp, dtab, NSDB_HOSTS, "gethostbyname",
	    default_dns_files, name, strlen(name), af) != NS_SUCCESS)
		return NULL;
	h_errno = NETDB_SUCCESS;
	return hp;
}

struct hostent *
gethostbyaddr(const char *addr,	/* XXX should have been def'd as u_char! */
    socklen_t len, int af)
{
	const u_char *uaddr = (const u_char *)addr;
	socklen_t size;
	struct hostent *hp;
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_gethtbyaddr, NULL)
		{ NSSRC_DNS, _dns_gethtbyaddr, NULL },	/* force -DHESIOD */
		NS_NIS_CB(_yp_gethtbyaddr, NULL)
		NS_NULL_CB
	};
	
	_DIAGASSERT(addr != NULL);

	if (af == AF_INET6 && len == IN6ADDRSZ &&
	    (IN6_IS_ADDR_LINKLOCAL((const struct in6_addr *)(const void *)uaddr) ||
	     IN6_IS_ADDR_SITELOCAL((const struct in6_addr *)(const void *)uaddr))) {
		h_errno = HOST_NOT_FOUND;
		return NULL;
	}
	if (af == AF_INET6 && len == IN6ADDRSZ &&
	    (IN6_IS_ADDR_V4MAPPED((const struct in6_addr *)(const void *)uaddr) ||
	     IN6_IS_ADDR_V4COMPAT((const struct in6_addr *)(const void *)uaddr))) {
		/* Unmap. */
		addr += IN6ADDRSZ - INADDRSZ;
		uaddr += IN6ADDRSZ - INADDRSZ;
		af = AF_INET;
		len = INADDRSZ;
	}
	switch (af) {
	case AF_INET:
		size = INADDRSZ;
		break;
	case AF_INET6:
		size = IN6ADDRSZ;
		break;
	default:
		errno = EAFNOSUPPORT;
		h_errno = NETDB_INTERNAL;
		return NULL;
	}
	if (size != len) {
		errno = EINVAL;
		h_errno = NETDB_INTERNAL;
		return NULL;
	}
	hp = NULL;
	h_errno = NETDB_INTERNAL;
	if (nsdispatch(&hp, dtab, NSDB_HOSTS, "gethostbyaddr",
	    default_dns_files, uaddr, len, af) != NS_SUCCESS)
		return NULL;
	h_errno = NETDB_SUCCESS;
	return hp;
}

void
_sethtent(int f)
{
	if (!hostf)
		hostf = fopen(_PATH_HOSTS, "re");
	else
		rewind(hostf);
	stayopen = f;
}

void
_endhtent(void)
{
	if (hostf && !stayopen) {
		(void) fclose(hostf);
		hostf = NULL;
	}
}

struct hostent *
_gethtent(void)
{
	char *p;
	char *cp, **q;
	int af, len;

	if (!hostf && !(hostf = fopen(_PATH_HOSTS, "re"))) {
		h_errno = NETDB_INTERNAL;
		return NULL;
	}
 again:
	if (!(p = fgets(hostbuf, (int)sizeof hostbuf, hostf))) {
		h_errno = HOST_NOT_FOUND;
		return NULL;
	}
	if (*p == '#')
		goto again;
	if (!(cp = strpbrk(p, "#\n")))
		goto again;
	*cp = '\0';
	if (!(cp = strpbrk(p, " \t")))
		goto again;
	*cp++ = '\0';
	if (inet_pton(AF_INET6, p, (char *)(void *)host_addr) > 0) {
		af = AF_INET6;
		len = IN6ADDRSZ;
	} else if (inet_pton(AF_INET, p, (char *)(void *)host_addr) > 0) {
		res_state res = __res_get_state();
		if (res == NULL)
			return NULL;
		if (res->options & RES_USE_INET6) {
			map_v4v6_address((char *)(void *)host_addr,
			    (char *)(void *)host_addr);
			af = AF_INET6;
			len = IN6ADDRSZ;
		} else {
			af = AF_INET;
			len = INADDRSZ;
		}
		__res_put_state(res);
	} else {
		goto again;
	}
	/* if this is not something we're looking for, skip it. */
	if (host.h_addrtype != 0 && host.h_addrtype != af)
		goto again;
	if (host.h_length != 0 && host.h_length != len)
		goto again;
	h_addr_ptrs[0] = (char *)(void *)host_addr;
	h_addr_ptrs[1] = NULL;
	host.h_addr_list = h_addr_ptrs;
	host.h_length = len;
	host.h_addrtype = af;
	while (*cp == ' ' || *cp == '\t')
		cp++;
	host.h_name = cp;
	q = host.h_aliases = host_aliases;
	if ((cp = strpbrk(cp, " \t")) != NULL)
		*cp++ = '\0';
	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		if (q < &host_aliases[MAXALIASES - 1])
			*q++ = cp;
		if ((cp = strpbrk(cp, " \t")) != NULL)
			*cp++ = '\0';
	}
	*q = NULL;
	h_errno = NETDB_SUCCESS;
	return &host;
}

/*ARGSUSED*/
int
_gethtbyname(void *rv, void *cb_data, va_list ap)
{
	struct hostent *hp;
	const char *name;
	int af;

	_DIAGASSERT(rv != NULL);

	name = va_arg(ap, char *);
	/* NOSTRICT skip len */(void)va_arg(ap, int);
	af = va_arg(ap, int);

	hp = NULL;
#if 0
	{
		res_state res = __res_get_state();
		if (res == NULL)
			return NS_NOTFOUND;
		if (res->options & RES_USE_INET6)
			hp = _gethtbyname2(name, AF_INET6);
		if (hp==NULL)
			hp = _gethtbyname2(name, AF_INET);
		__res_put_state(res);
	}
#else
	hp = _gethtbyname2(name, af);
#endif
	*((struct hostent **)rv) = hp;
	if (hp == NULL) {
		h_errno = HOST_NOT_FOUND;
		return NS_NOTFOUND;
	}
	return NS_SUCCESS;
}

struct hostent *
_gethtbyname2(const char *name, int af)
{
	struct hostent *p;
	char *tmpbuf, *ptr, **cp;
	int num;
	size_t len;

	_DIAGASSERT(name != NULL);

	_sethtent(stayopen);
	ptr = tmpbuf = NULL;
	num = 0;
	while ((p = _gethtent()) != NULL && num < MAXADDRS) {
		if (p->h_addrtype != af)
			continue;
		if (strcasecmp(p->h_name, name) != 0) {
			for (cp = p->h_aliases; *cp != NULL; cp++)
				if (strcasecmp(*cp, name) == 0)
					break;
			if (*cp == NULL) continue;
		}

		if (num == 0) {
			size_t bufsize;
			char *src;

			bufsize = strlen(p->h_name) + 2 +
				  MAXADDRS * p->h_length +
				  ALIGNBYTES;
			for (cp = p->h_aliases; *cp != NULL; cp++)
				bufsize += strlen(*cp) + 1;

			if ((tmpbuf = malloc(bufsize)) == NULL) {
				h_errno = NETDB_INTERNAL;
				return NULL;
			}

			ptr = tmpbuf;
			src = p->h_name;
			while ((*ptr++ = *src++) != '\0');
			for (cp = p->h_aliases; *cp != NULL; cp++) {
				src = *cp;
				while ((*ptr++ = *src++) != '\0');
			}
			*ptr++ = '\0';

			ptr = (char *)(void *)ALIGN(ptr);
		}

		(void)memcpy(ptr, p->h_addr_list[0], (size_t)p->h_length);
		ptr += p->h_length;
		num++;
	}
	_endhtent();
	if (num == 0) return NULL;

	len = ptr - tmpbuf;
	if (len > (sizeof(hostbuf) - ALIGNBYTES)) {
		free(tmpbuf);
		errno = ENOSPC;
		h_errno = NETDB_INTERNAL;
		return NULL;
	}
	ptr = memcpy((void *)ALIGN(hostbuf), tmpbuf, len);
	free(tmpbuf);

	host.h_name = ptr;
	while (*ptr++);

	cp = host_aliases;
	while (*ptr) {
		*cp++ = ptr;
		while (*ptr++);
	}
	ptr++;
	*cp = NULL;

	ptr = (char *)(void *)ALIGN(ptr);
	cp = h_addr_ptrs;
	while (num--) {
		*cp++ = ptr;
		ptr += host.h_length;
	}
	*cp = NULL;

	return &host;
}

/*ARGSUSED*/
int
_gethtbyaddr(void *rv, void *cb_data, va_list ap)
{
	struct hostent *p;
	const unsigned char *addr;
	int len, af;

	_DIAGASSERT(rv != NULL);

	addr = va_arg(ap, unsigned char *);
	len = va_arg(ap, int);
	af = va_arg(ap, int);
	
	host.h_length = len;
	host.h_addrtype = af;

	_sethtent(stayopen);
	while ((p = _gethtent()) != NULL)
		if (p->h_addrtype == af && !memcmp(p->h_addr, addr,
		    (size_t)len))
			break;
	_endhtent();
	*((struct hostent **)rv) = p;
	if (p==NULL) {
		h_errno = HOST_NOT_FOUND;
		return NS_NOTFOUND;
	}
	return NS_SUCCESS;
}

static void
map_v4v6_address(const char *src, char *dst)
{
	u_char *p = (u_char *)dst;
	char tmp[INADDRSZ];
	int i;

	_DIAGASSERT(src != NULL);
	_DIAGASSERT(dst != NULL);

	/* Stash a temporary copy so our caller can update in place. */
	(void)memcpy(tmp, src, INADDRSZ);
	/* Mark this ipv6 addr as a mapped ipv4. */
	for (i = 0; i < 10; i++)
		*p++ = 0x00;
	*p++ = 0xff;
	*p++ = 0xff;
	/* Retrieve the saved copy and we're done. */
	(void)memcpy((void *)p, tmp, INADDRSZ);
}

static void
map_v4v6_hostent(struct hostent *hp, char **bpp, char *ep)
{
	char **ap;

	_DIAGASSERT(hp != NULL);
	_DIAGASSERT(bpp != NULL);
	_DIAGASSERT(ep != NULL);

	if (hp->h_addrtype != AF_INET || hp->h_length != INADDRSZ)
		return;
	hp->h_addrtype = AF_INET6;
	hp->h_length = IN6ADDRSZ;
	for (ap = hp->h_addr_list; *ap; ap++) {
		int i = (int)(sizeof(align) -
		    (size_t)((u_long)*bpp % sizeof(align)));

		if (ep - *bpp < (i + IN6ADDRSZ)) {
			/* Out of memory.  Truncate address list here.  XXX */
			*ap = NULL;
			return;
		}
		*bpp += i;
		map_v4v6_address(*ap, *bpp);
		*ap = *bpp;
		*bpp += IN6ADDRSZ;
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

struct hostent *
gethostent(void)
{
	host.h_addrtype = 0;
	host.h_length = 0;
	return _gethtent();
}

/*ARGSUSED*/
int
_dns_gethtbyname(void *rv, void *cb_data, va_list ap)
{
	querybuf *buf;
	int n, type;
	struct hostent *hp;
	const char *name;
	int af;
	res_state res;

	_DIAGASSERT(rv != NULL);

	name = va_arg(ap, char *);
	/* NOSTRICT skip len */(void)va_arg(ap, int);
	af = va_arg(ap, int);

	switch (af) {
	case AF_INET:
		type = T_A;
		break;
	case AF_INET6:
		type = T_AAAA;
		break;
	default:
		return NS_UNAVAIL;
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
	n = res_nsearch(res, name, C_IN, type, buf->buf, (int)sizeof(buf->buf));
	if (n < 0) {
		free(buf);
		debugprintf("res_nsearch failed (%d)\n", res, n);
		__res_put_state(res);
		return NS_NOTFOUND;
	}
	hp = getanswer(buf, n, name, type, res);
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
	*((struct hostent **)rv) = hp;
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
	int len, af, advance;
	res_state res;

	_DIAGASSERT(rv != NULL);

	uaddr = va_arg(ap, unsigned char *);
	len = va_arg(ap, int);
	af = va_arg(ap, int);

	switch (af) {
	case AF_INET:
		(void)snprintf(qbuf, sizeof(qbuf), "%u.%u.%u.%u.in-addr.arpa",
		    (uaddr[3] & 0xff), (uaddr[2] & 0xff),
		    (uaddr[1] & 0xff), (uaddr[0] & 0xff));
		break;

	case AF_INET6:
		qp = qbuf;
		ep = qbuf + sizeof(qbuf) - 1;
		for (n = IN6ADDRSZ - 1; n >= 0; n--) {
			advance = snprintf(qp, (size_t)(ep - qp), "%x.%x.",
			    uaddr[n] & 0xf,
			    ((unsigned int)uaddr[n] >> 4) & 0xf);
			if (advance > 0 && qp + advance < ep)
				qp += advance;
			else {
				h_errno = NETDB_INTERNAL;
				return NS_NOTFOUND;
			}
		}
		if (strlcat(qbuf, "ip6.arpa", sizeof(qbuf)) >= sizeof(qbuf)) {
			h_errno = NETDB_INTERNAL;
			return NS_NOTFOUND;
		}
		break;
	default:
		abort();
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
	n = res_nquery(res, qbuf, C_IN, T_PTR, buf->buf, (int)sizeof(buf->buf));
	if (n < 0) {
		free(buf);
		debugprintf("res_nquery failed (%d)\n", res, n);
		__res_put_state(res);
		return NS_NOTFOUND;
	}
	hp = getanswer(buf, n, qbuf, T_PTR, res);
	free(buf);
	if (hp == NULL) {
		__res_put_state(res);
		switch (h_errno) {
		case HOST_NOT_FOUND:
			return NS_NOTFOUND;
		case TRY_AGAIN:
			return NS_TRYAGAIN;
		default:
			return NS_UNAVAIL;
		}
	}
	hp->h_addrtype = af;
	hp->h_length = len;
	(void)memcpy(host_addr, uaddr, (size_t)len);
	h_addr_ptrs[0] = (char *)(void *)host_addr;
	h_addr_ptrs[1] = NULL;
	if (af == AF_INET && (res->options & RES_USE_INET6)) {
		map_v4v6_address((char *)(void *)host_addr,
		    (char *)(void *)host_addr);
		hp->h_addrtype = AF_INET6;
		hp->h_length = IN6ADDRSZ;
	}

	__res_put_state(res);
	*((struct hostent **)rv) = hp;
	h_errno = NETDB_SUCCESS;
	return NS_SUCCESS;
}

#ifdef YP
/*ARGSUSED*/
struct hostent *
_yphostent(char *line, int af)
{
	static struct in_addr host_addrs[MAXADDRS];
	static struct in6_addr host6_addrs[MAXADDRS];
	char *p = line;
	char *cp, **q;
	char **hap;
	int addrok;
	int more;
	size_t naddrs;

	_DIAGASSERT(line != NULL);

	host.h_name = NULL;
	host.h_addr_list = h_addr_ptrs;
	host.h_addrtype = af;
	switch (af) {
	case AF_INET:
		host.h_length = INADDRSZ;
		break;
	case AF_INET6:
		host.h_length = IN6ADDRSZ;
		break;
	default:
		return NULL;
	}
	hap = h_addr_ptrs;
	q = host.h_aliases = host_aliases;
	naddrs = 0;

nextline:
	/* check for host_addrs overflow */
	if (naddrs >= sizeof(host_addrs) / sizeof(host_addrs[0]))
		goto done;
	if (naddrs >= sizeof(host6_addrs) / sizeof(host6_addrs[0]))
		goto done;

	more = 0;
	cp = strpbrk(p, " \t");
	if (cp == NULL)
		goto done;
	*cp++ = '\0';

	/* p has should have an address */
	addrok = 0;
	switch (af) {
	case AF_INET:
		addrok = inet_aton(p, &host_addrs[naddrs]);
		break;
	case AF_INET6:
		addrok = inet_pton(af, p, &host6_addrs[naddrs]);
		break;
	}
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

	switch (af) {
	case AF_INET:
		*hap++ = (char *)(void *)&host_addrs[naddrs++];
		break;
	case AF_INET6:
		*hap++ = (char *)(void *)&host6_addrs[naddrs++];
		break;
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
	if (!host.h_name)
		host.h_name = p;
	else if (strcmp(host.h_name, p)==0)
		;
	else if (q < &host_aliases[MAXALIASES - 1])
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
		if (q < &host_aliases[MAXALIASES - 1])
			*q++ = cp;
		cp = strpbrk(cp, " \t");
		if (cp != NULL)
			*cp++ = '\0';
	}

done:
	if (host.h_name == NULL)
		return NULL;
	*q = NULL;
	*hap = NULL;
	return &host;
}

/*ARGSUSED*/
int
_yp_gethtbyaddr(void *rv, void *cb_data, va_list ap)
{
	struct hostent *hp = NULL;
	static char *__ypcurrent;
	int __ypcurrentlen, r;
	char name[INET6_ADDRSTRLEN];	/* XXX enough? */
	const unsigned char *uaddr;
	int af;
	const char *map;

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
	if (__ypcurrent)
		free(__ypcurrent);
	__ypcurrent = NULL;
	switch (af) {
	case AF_INET:
		map = "hosts.byaddr";
		break;
	default:
		map = "ipnodes.byaddr";
		break;
	}
	r = yp_match(__ypdomain, map, name,
		(int)strlen(name), &__ypcurrent, &__ypcurrentlen);
	if (r == 0)
		hp = _yphostent(__ypcurrent, af);
	if (hp == NULL) {
		h_errno = HOST_NOT_FOUND;
		return NS_NOTFOUND;
	}
	*((struct hostent **)rv) = hp;
	return NS_SUCCESS;
}

/*ARGSUSED*/
int
_yp_gethtbyname(void *rv, void *cb_data, va_list ap)
{
	struct hostent *hp = NULL;
	static char *__ypcurrent;
	int __ypcurrentlen, r;
	const char *name;
	int af;
	const char *map;

	_DIAGASSERT(rv != NULL);

	name = va_arg(ap, char *);
	/* NOSTRICT skip len */(void)va_arg(ap, int);
	af = va_arg(ap, int);

	if (!__ypdomain) {
		if (_yp_check(&__ypdomain) == 0)
			return NS_UNAVAIL;
	}
	if (__ypcurrent)
		free(__ypcurrent);
	__ypcurrent = NULL;
	switch (af) {
	case AF_INET:
		map = "hosts.byname";
		break;
	default:
		map = "ipnodes.byname";
		break;
	}
	r = yp_match(__ypdomain, map, name,
		(int)strlen(name), &__ypcurrent, &__ypcurrentlen);
	if (r == 0)
		hp = _yphostent(__ypcurrent, af);
	if (hp == NULL) {
		h_errno = HOST_NOT_FOUND;
		return NS_NOTFOUND;
	}
	*((struct hostent **)rv) = hp;
	return NS_SUCCESS;
}
#endif
