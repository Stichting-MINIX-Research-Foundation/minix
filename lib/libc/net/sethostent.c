/*	$NetBSD: sethostent.c,v 1.20 2014/03/17 13:24:23 christos Exp $	*/

/*
 * Copyright (c) 1985, 1993
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
static char sccsid[] = "@(#)sethostent.c	8.1 (Berkeley) 6/4/93";
static char rcsid[] = "Id: sethostent.c,v 8.5 1996/09/28 06:51:07 vixie Exp ";
#else
__RCSID("$NetBSD: sethostent.c,v 1.20 2014/03/17 13:24:23 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <nsswitch.h>
#include <netdb.h>
#include <resolv.h>
#include <errno.h>
#include <stdlib.h>

#include "hostent.h"

#ifdef __weak_alias
__weak_alias(sethostent,_sethostent)
__weak_alias(endhostent,_endhostent)
#endif

#ifndef _REENTRANT
void	res_close(void);
#endif

static struct hostent *_hf_gethtbyname2(const char *, int, struct getnamaddr *);

void
/*ARGSUSED*/
sethostent(int stayopen)
{
#ifndef _REENTRANT
	if ((_res.options & RES_INIT) == 0 && res_init() == -1)
		return;
	if (stayopen)
		_res.options |= RES_STAYOPEN | RES_USEVC;
#endif
	sethostent_r(&_h_file);
}

void
endhostent(void)
{
#ifndef _REENTRANT
	_res.options &= ~(RES_STAYOPEN | RES_USEVC);
	res_close();
#endif
	endhostent_r(&_h_file);
}
static const char *_h_hosts = _PATH_HOSTS;

void
_hf_sethostsfile(const char *f) {
	_h_hosts = f;
}

void
sethostent_r(FILE **hf)
{
	if (!*hf)
		*hf = fopen(_h_hosts, "re");
	else
		rewind(*hf);
}

void
endhostent_r(FILE **hf)
{
	if (*hf) {
		(void)fclose(*hf);
		*hf = NULL;
	}
}

/*ARGSUSED*/
int
_hf_gethtbyname(void *rv, void *cb_data, va_list ap)
{
	struct hostent *hp;
	const char *name;
	int af;
	struct getnamaddr *info = rv;

	_DIAGASSERT(rv != NULL);

	name = va_arg(ap, char *);
	/* NOSTRICT skip string len */(void)va_arg(ap, int);
	af = va_arg(ap, int);

#if 0
	{
		res_state res = __res_get_state();
		if (res == NULL)
			return NS_NOTFOUND;
		if (res->options & RES_USE_INET6)
			hp = _hf_gethtbyname2(name, AF_INET6, info);
		else
			hp = NULL;
		if (hp == NULL)
			hp = _hf_gethtbyname2(name, AF_INET, info);
		__res_put_state(res);
	}
#else
	hp = _hf_gethtbyname2(name, af, info);
#endif
	if (hp == NULL) {
		*info->he = HOST_NOT_FOUND;
		return NS_NOTFOUND;
	}
	return NS_SUCCESS;
}

struct hostent *
_hf_gethtbyname2(const char *name, int af, struct getnamaddr *info)
{
	struct hostent *hp, hent;
	char *buf, *ptr;
	size_t len, anum, num, i;
	FILE *hf;
	char *aliases[MAXALIASES];
	char *addr_ptrs[MAXADDRS];

	_DIAGASSERT(name != NULL);

	hf = NULL;
	sethostent_r(&hf);
	if (hf == NULL) {
		errno = EINVAL;
		*info->he = NETDB_INTERNAL;
		return NULL;
	}

	if ((ptr = buf = malloc(len = info->buflen)) == NULL) {
		*info->he = NETDB_INTERNAL;
		return NULL;
	}

	anum = 0;		/* XXX: gcc */
	hent.h_name = NULL;	/* XXX: gcc */
	hent.h_addrtype = 0;	/* XXX: gcc */
	hent.h_length = 0;	/* XXX: gcc */

	for (num = 0; num < MAXADDRS;) {
		info->hp->h_addrtype = af;
		info->hp->h_length = 0;

		hp = gethostent_r(hf, info->hp, info->buf, info->buflen,
		    info->he);
		if (hp == NULL)
			break;

		if (strcasecmp(hp->h_name, name) != 0) {
			char **cp;
			for (cp = hp->h_aliases; *cp != NULL; cp++)
				if (strcasecmp(*cp, name) == 0)
					break;
			if (*cp == NULL) continue;
		}

		if (num == 0) {
			hent.h_addrtype = af = hp->h_addrtype;
			hent.h_length = hp->h_length;

			HENT_SCOPY(hent.h_name, hp->h_name, ptr, len);
			for (anum = 0; hp->h_aliases[anum]; anum++) {
				if (anum >= __arraycount(aliases))
					goto nospc;
				HENT_SCOPY(aliases[anum], hp->h_aliases[anum],
				    ptr, len);
			}
			ptr = (void *)ALIGN(ptr);
			if ((size_t)(ptr - buf) >= info->buflen)
				goto nospc;
		}

		if (num >= __arraycount(addr_ptrs))
			goto nospc;
		HENT_COPY(addr_ptrs[num], hp->h_addr_list[0], hp->h_length, ptr,
		    len);
		num++;
	}
	endhostent_r(&hf);

	if (num == 0) {
		*info->he = HOST_NOT_FOUND;
		free(buf);
		return NULL;
	}

	hp = info->hp;
	ptr = info->buf;
	len = info->buflen;

	hp->h_addrtype = hent.h_addrtype;
	hp->h_length = hent.h_length;

	HENT_ARRAY(hp->h_aliases, anum, ptr, len);
	HENT_ARRAY(hp->h_addr_list, num, ptr, len);

	for (i = 0; i < num; i++)
		HENT_COPY(hp->h_addr_list[i], addr_ptrs[i], hp->h_length, ptr,
		    len);
	hp->h_addr_list[num] = NULL;

	HENT_SCOPY(hp->h_name, hent.h_name, ptr, len);

	for (i = 0; i < anum; i++)
		HENT_SCOPY(hp->h_aliases[i], aliases[i], ptr, len);
	hp->h_aliases[anum] = NULL;

	free(buf);
	return hp;
nospc:
	*info->he = NETDB_INTERNAL;
	free(buf);
	errno = ENOSPC;
	return NULL;
}

/*ARGSUSED*/
int
_hf_gethtbyaddr(void *rv, void *cb_data, va_list ap)
{
	struct hostent *hp;
	const unsigned char *addr;
	struct getnamaddr *info = rv;
	FILE *hf;

	_DIAGASSERT(rv != NULL);

	addr = va_arg(ap, unsigned char *);
	info->hp->h_length = va_arg(ap, int);
	info->hp->h_addrtype = va_arg(ap, int);
	
	hf = NULL;
	sethostent_r(&hf);
	if (hf == NULL) {
		*info->he = NETDB_INTERNAL;
		return NS_UNAVAIL;
	}
	while ((hp = gethostent_r(hf, info->hp, info->buf, info->buflen,
	    info->he)) != NULL)
		if (!memcmp(hp->h_addr_list[0], addr, (size_t)hp->h_length))
			break;
	endhostent_r(&hf);

	if (hp == NULL) {
		*info->he = HOST_NOT_FOUND;
		return NS_NOTFOUND;
	}
	return NS_SUCCESS;
}
