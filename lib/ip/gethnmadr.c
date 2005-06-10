/*
 * Copyright (c) 1985, 1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)gethostnamadr.c	6.41 (Berkeley) 6/1/90";
#endif /* LIBC_SCCS and not lint */

#ifdef _MINIX
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <net/hton.h>
#include <net/gen/nameser.h>
#include <net/gen/netdb.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <net/gen/resolv.h>
#include <net/gen/socket.h>
#else
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <resolv.h>
#endif /* AMOEABA */

#define	MAXALIASES	35
#define	MAXADDRS	35

static char *h_addr_ptrs[MAXADDRS + 1];

#ifdef _MINIX
struct in_addr
{
	ipaddr_t s_addr;
};
union querybuf;

extern int dn_skipname _ARGS(( const u_char *comp_dn, const u_char *eom ));
#define getshort _getshort
static struct hostent *getanswer _ARGS(( union querybuf *answer, int anslen, 
	int iquery ));
#define bcmp memcmp
#define bcopy(s, d, l) memcpy(d, s, l)
#endif /* _MINIX */

static struct hostent host;
static char *host_aliases[MAXALIASES];
static char hostbuf[BUFSIZ+1];
static struct in_addr host_addr;

#ifndef _MINIX
char *strpbrk();
#endif /* !_MINIX */

#if PACKETSZ > 1024
#define	MAXPACKET	PACKETSZ
#else
#define	MAXPACKET	1024
#endif

typedef union querybuf
{
	dns_hdr_t hdr;
	u_char buf[MAXPACKET];
} querybuf_t;

typedef union align {
    long al;
    char ac;
} align_t;

static struct hostent *
getanswer(answer, anslen, iquery)
	querybuf_t *answer;
	int anslen;
	int iquery;
{
	register dns_hdr_t *hp;
	register u_char *cp;
	register int n;
	u_char *eom;
	char *bp, **ap;
	int type, class, buflen, ancount, qdcount;
	int haveanswer, getclass = C_ANY;
	char **hap;

	eom = answer->buf + anslen;
	/*
	 * find first satisfactory answer
	 */
	hp = &answer->hdr;
	ancount = ntohs(hp->dh_ancount);
	qdcount = ntohs(hp->dh_qdcount);
	bp = hostbuf;
	buflen = sizeof(hostbuf);
	cp = answer->buf + sizeof(dns_hdr_t);
	if (qdcount) {
		if (iquery) {
			if ((n = dn_expand((u_char *)answer->buf, eom,
			     cp, (u_char *)bp, buflen)) < 0) {
				h_errno = NO_RECOVERY;
				return ((struct hostent *) NULL);
			}
			cp += n + QFIXEDSZ;
			host.h_name = bp;
			n = strlen(bp) + 1;
			bp += n;
			buflen -= n;
		} else
			cp += dn_skipname(cp, eom) + QFIXEDSZ;
		while (--qdcount > 0)
			cp += dn_skipname(cp, eom) + QFIXEDSZ;
	} else if (iquery) {
		if (hp->dh_flag1 & DHF_AA)
			h_errno = HOST_NOT_FOUND;
		else
			h_errno = TRY_AGAIN;
		return ((struct hostent *) NULL);
	}
	ap = host_aliases;
	*ap = NULL;
	host.h_aliases = host_aliases;
	hap = h_addr_ptrs;
	*hap = NULL;
#if BSD >= 43 || defined(h_addr)	/* new-style hostent structure */
	host.h_addr_list = h_addr_ptrs;
#endif
	haveanswer = 0;
	while (--ancount >= 0 && cp < eom) {
		if ((n = dn_expand((u_char *)answer->buf, eom, cp, (u_char *)bp,
			buflen)) < 0)
			break;
		cp += n;
		type = getshort(cp);
 		cp += sizeof(u_short);
		class = getshort(cp);
 		cp += sizeof(u_short) + sizeof(u_long);
		n = getshort(cp);
		cp += sizeof(u_short);
		if (type == T_CNAME) {
			cp += n;
			if (ap >= &host_aliases[MAXALIASES-1])
				continue;
			*ap++ = bp;
			n = strlen(bp) + 1;
			bp += n;
			buflen -= n;
			continue;
		}
		if (iquery && type == T_PTR) {
			if ((n = dn_expand((u8_t *)answer->buf, eom,
			    cp, (u8_t *)bp, buflen)) < 0) {
				cp += n;
				continue;
			}
			cp += n;
			host.h_name = bp;
			return(&host);
		}
		if (iquery || type != T_A)  {
#ifdef DEBUG
			if (_res.options & RES_DEBUG)
				printf("unexpected answer type %d, size %d\n",
					type, n);
#endif
			cp += n;
			continue;
		}
		if (haveanswer) {
			if (n != host.h_length) {
				cp += n;
				continue;
			}
			if (class != getclass) {
				cp += n;
				continue;
			}
		} else {
			host.h_length = n;
			getclass = class;
			host.h_addrtype = (class == C_IN) ? AF_INET : AF_UNSPEC;
			if (!iquery) {
				host.h_name = bp;
				bp += strlen(bp) + 1;
			}
		}

		bp += (size_t)(sizeof(align_t) - 
						((u_long)bp % sizeof(align_t)));

		if (bp + n >= &hostbuf[sizeof(hostbuf)]) {
#ifdef DEBUG
			if (_res.options & RES_DEBUG)
				printf("size (%d) too big\n", n);
#endif
			break;
		}
		bcopy(cp, *hap++ = bp, n);
		bp +=n;
		cp += n;
		haveanswer++;
	}
	if (haveanswer) {
		*ap = NULL;
#if BSD >= 43 || defined(h_addr)	/* new-style hostent structure */
		*hap = NULL;
#else
		host.h_addr = h_addr_ptrs[0];
#endif
		return (&host);
	} else {
		h_errno = TRY_AGAIN;
		return ((struct hostent *) NULL);
	}
}

struct hostent *
gethostbyname(name)
	_CONST char *name;
{
	querybuf_t buf;
	register _CONST char *cp;
	int n;

	/*
	 * disallow names consisting only of digits/dots, unless
	 * they end in a dot.
	 */
	if (isdigit(name[0]))
		for (cp = name;; ++cp) {
			if (!*cp) {
				if (*--cp == '.')
					break;
				/*
				 * All-numeric, no dot at the end.
				 * Fake up a hostent as if we'd actually
				 * done a lookup.  What if someone types
				 * 255.255.255.255?  The test below will
				 * succeed spuriously... ???
				 */
				if ((host_addr.s_addr = inet_addr(name)) == -1) {
					h_errno = HOST_NOT_FOUND;
					return((struct hostent *) NULL);
				}
				host.h_name = (char *) name;
				host.h_aliases = host_aliases;
				host_aliases[0] = NULL;
				host.h_addrtype = AF_INET;
				host.h_length = sizeof(u_long);
				h_addr_ptrs[0] = (char *)&host_addr;
				h_addr_ptrs[1] = (char *)0;
#if BSD >= 43 || defined(h_addr)	/* new-style hostent structure */
				host.h_addr_list = h_addr_ptrs;
#else
				host.h_addr = h_addr_ptrs[0];
#endif
				return (&host);
			}
			if (!isdigit(*cp) && *cp != '.') 
				break;
		}

	if ((n = res_search((char*)name, C_IN, T_A, buf.buf, sizeof(buf))) < 0) {
#ifdef DEBUG
		if (_res.options & RES_DEBUG)
			printf("res_search failed\n");
#endif
		return ((struct hostent *) NULL);
	}
	return (getanswer(&buf, n, 0));
}

struct hostent *
gethostbyaddr(addr, len, type)
	const char *addr;
	int len, type;
{
	int n;
	querybuf_t buf;
	register struct hostent *hp;
	char qbuf[MAXDNAME];
	
	if (type != AF_INET)
		return ((struct hostent *) NULL);
	(void)sprintf(qbuf, "%u.%u.%u.%u.in-addr.arpa",
		((unsigned)addr[3] & 0xff),
		((unsigned)addr[2] & 0xff),
		((unsigned)addr[1] & 0xff),
		((unsigned)addr[0] & 0xff));
	n = res_query(qbuf, C_IN, T_PTR, (u8_t *)&buf, sizeof(buf));
	if (n < 0) {
#ifdef DEBUG
		if (_res.options & RES_DEBUG)
			printf("res_query failed\n");
#endif
		return ((struct hostent *) NULL);
	}
	hp = getanswer(&buf, n, 1);
	if (hp == NULL)
		return ((struct hostent *) NULL);
	hp->h_addrtype = type;
	hp->h_length = len;
	h_addr_ptrs[0] = (char *)&host_addr;
	h_addr_ptrs[1] = (char *)0;
	host_addr = *(struct in_addr *)addr;
#if BSD < 43 && !defined(h_addr)	/* new-style hostent structure */
	hp->h_addr = h_addr_ptrs[0];
#endif
	return(hp);
}
