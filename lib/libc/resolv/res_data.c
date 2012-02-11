/*	$NetBSD: res_data.c,v 1.14 2009/10/24 05:35:37 christos Exp $	*/

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1995-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#ifdef notdef
static const char rcsid[] = "Id: res_data.c,v 1.7 2008/12/11 09:59:00 marka Exp";
#else
__RCSID("$NetBSD: res_data.c,v 1.14 2009/10/24 05:35:37 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "port_before.h"

#include "namespace.h"
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <ctype.h>
#include <netdb.h>
#include <resolv.h>
#include <res_update.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "port_after.h"

#include "res_private.h"

#ifdef __weak_alias
__weak_alias(res_init,_res_init)
__weak_alias(res_mkquery,_res_mkquery)
__weak_alias(res_query,_res_query)
__weak_alias(res_search,_res_search)
__weak_alias(res_send,__res_send)
__weak_alias(res_close,__res_close)
/* XXX: these leaked in the old bind8 libc */
__weak_alias(res_querydomain,__res_querydomain)
__weak_alias(res_send_setqhook,__res_send_setqhook)
__weak_alias(res_send_setrhook,__res_send_setrhook)
#if 0
__weak_alias(p_query,__p_query)
__weak_alias(fp_query,__fp_query)
__weak_alias(fp_nquery,__fp_nquery)
__weak_alias(res_isourserver,__res_isourserver)
__weak_alias(res_opt,_res_opt)
__weak_alias(hostalias,__hostalias)
#endif
#endif

const char *_res_opcodes[] = {
	"QUERY",
	"IQUERY",
	"CQUERYM",
	"CQUERYU",	/*%< experimental */
	"NOTIFY",	/*%< experimental */
	"UPDATE",
	"6",
	"7",
	"8",
	"9",
	"10",
	"11",
	"12",
	"13",
	"ZONEINIT",
	"ZONEREF",
};

#ifdef BIND_UPDATE
const char *_res_sectioncodes[] = {
	"ZONE",
	"PREREQUISITES",
	"UPDATE",
	"ADDITIONAL",
};
#endif

#ifndef __BIND_NOSTATIC 
extern struct __res_state _nres;

/* Proto. */

int  res_ourserver_p(const res_state, const struct sockaddr *);

int
res_init(void) {
	int rv;
#ifdef COMPAT__RES
	/*
	 * Compatibility with program that were accessing _res directly
	 * to set options. We keep another struct res that is the same
	 * size as the original res structure, and then copy fields to
	 * it so that we achieve the same initialization
	 */
	extern void *__res_get_old_state(void);
	extern void __res_put_old_state(void *);
	res_state ores = __res_get_old_state();

	if (ores->options != 0)
		_nres.options = ores->options;
	if (ores->retrans != 0)
		_nres.retrans = ores->retrans;
	if (ores->retry != 0)
		_nres.retry = ores->retry;
#endif

	/*
	 * These three fields used to be statically initialized.  This made
	 * it hard to use this code in a shared library.  It is necessary,
	 * now that we're doing dynamic initialization here, that we preserve
	 * the old semantics: if an application modifies one of these three
	 * fields of _res before res_init() is called, res_init() will not
	 * alter them.  Of course, if an application is setting them to
	 * _zero_ before calling res_init(), hoping to override what used
	 * to be the static default, we can't detect it and unexpected results
	 * will follow.  Zero for any of these fields would make no sense,
	 * so one can safely assume that the applications were already getting
	 * unexpected results.
	 *
	 * _nres.options is tricky since some apps were known to diddle the bits
	 * before res_init() was first called. We can't replicate that semantic
	 * with dynamic initialization (they may have turned bits off that are
	 * set in RES_DEFAULT).  Our solution is to declare such applications
	 * "broken".  They could fool us by setting RES_INIT but none do (yet).
	 */
	if (!_nres.retrans)
		_nres.retrans = RES_TIMEOUT;
	if (!_nres.retry)
		_nres.retry = 4;
	if (!(_nres.options & RES_INIT))
		_nres.options = RES_DEFAULT;

	/*
	 * This one used to initialize implicitly to zero, so unless the app
	 * has set it to something in particular, we can randomize it now.
	 */
	if (!_nres.id)
		_nres.id = res_nrandomid(&_nres);

	rv = __res_vinit(&_nres, 1);
#ifdef COMPAT__RES
	__res_put_old_state(&_nres);
#endif
	return rv;
}

void
p_query(const u_char *msg) {
	fp_query(msg, stdout);
}

void
fp_query(const u_char *msg, FILE *file) {
	fp_nquery(msg, PACKETSZ, file);
}

void
fp_nquery(const u_char *msg, int len, FILE *file) {
	if ((_nres.options & RES_INIT) == 0U && res_init() == -1)
		return;

	res_pquery(&_nres, msg, len, file);
}

int
res_mkquery(int op,			/*!< opcode of query  */
	    const char *dname,		/*!< domain name  */
	    int class, int type,	/*!< class and type of query  */
	    const u_char *data,		/*!< resource record data  */
	    int datalen,		/*!< length of data  */
	    const u_char *newrr_in,	/*!< new rr for modify or append  */
	    u_char *buf,		/*!< buffer to put query  */
	    int buflen)			/*!< size of buffer  */
{
	if ((_nres.options & RES_INIT) == 0U && res_init() == -1) {
		RES_SET_H_ERRNO(&_nres, NETDB_INTERNAL);
		return (-1);
	}
	return (res_nmkquery(&_nres, op, dname, class, type,
			     data, datalen,
			     newrr_in, buf, buflen));
}

#ifdef _LIBRESOLV
int
res_mkupdate(ns_updrec *rrecp_in, u_char *buf, int buflen) {
	if ((_nres.options & RES_INIT) == 0U && res_init() == -1) {
		RES_SET_H_ERRNO(&_nres, NETDB_INTERNAL);
		return (-1);
	}

	return (res_nmkupdate(&_nres, rrecp_in, buf, buflen));
}
#endif

int
res_query(const char *name,	/*!< domain name  */
	  int class, int type,	/*!< class and type of query  */
	  u_char *answer,	/*!< buffer to put answer  */
	  int anslen)		/*!< size of answer buffer  */
{
	if ((_nres.options & RES_INIT) == 0U && res_init() == -1) {
		RES_SET_H_ERRNO(&_nres, NETDB_INTERNAL);
		return (-1);
	}
	return (res_nquery(&_nres, name, class, type, answer, anslen));
}

void
res_send_setqhook(res_send_qhook hook) {
	_nres.qhook = hook;
}

void
res_send_setrhook(res_send_rhook hook) {
	_nres.rhook = hook;
}

int
res_isourserver(const struct sockaddr_in *inp) {
	return (res_ourserver_p(&_nres, (const struct sockaddr *)(const void *)inp));
}

int
res_send(const u_char *buf, int buflen, u_char *ans, int anssiz) {
	if ((_nres.options & RES_INIT) == 0U && res_init() == -1) {
		/* errno should have been set by res_init() in this case. */
		return (-1);
	}

	return (res_nsend(&_nres, buf, buflen, ans, anssiz));
}

#ifdef _LIBRESOLV
int
res_sendsigned(const u_char *buf, int buflen, ns_tsig_key *key,
	       u_char *ans, int anssiz)
{
	if ((_nres.options & RES_INIT) == 0U && res_init() == -1) {
		/* errno should have been set by res_init() in this case. */
		return (-1);
	}

	return (res_nsendsigned(&_nres, buf, buflen, key, ans, anssiz));
}
#endif

void
res_close(void) {
	res_nclose(&_nres);
}

#ifdef _LIBRESOLV
int
res_update(ns_updrec *rrecp_in) {
	if ((_nres.options & RES_INIT) == 0U && res_init() == -1) {
		RES_SET_H_ERRNO(&_nres, NETDB_INTERNAL);
		return (-1);
	}

	return (res_nupdate(&_nres, rrecp_in, NULL));
}
#endif

int
res_search(const char *name,	/*!< domain name  */
	   int class, int type,	/*!< class and type of query  */
	   u_char *answer,	/*!< buffer to put answer  */
	   int anslen)		/*!< size of answer  */
{
	if ((_nres.options & RES_INIT) == 0U && res_init() == -1) {
		RES_SET_H_ERRNO(&_nres, NETDB_INTERNAL);
		return (-1);
	}

	return (res_nsearch(&_nres, name, class, type, answer, anslen));
}

int
res_querydomain(const char *name,
		const char *domain,
		int class, int type,	/*!< class and type of query  */
		u_char *answer,		/*!< buffer to put answer  */
		int anslen)		/*!< size of answer  */
{
	if ((_nres.options & RES_INIT) == 0U && res_init() == -1) {
		RES_SET_H_ERRNO(&_nres, NETDB_INTERNAL);
		return (-1);
	}

	return (res_nquerydomain(&_nres, name, domain,
				 class, type,
				 answer, anslen));
}

int
res_opt(int a, u_char *b, int c, int d)
{
	return res_nopt(&_nres, a, b, c, d);
}

u_int
res_randomid(void) {
	if ((_nres.options & RES_INIT) == 0U && res_init() == -1) {
		RES_SET_H_ERRNO(&_res, NETDB_INTERNAL);
		return (u_int)-1;
	}

	return (res_nrandomid(&_nres));
}

const char *
hostalias(const char *name) {
	static char abuf[MAXDNAME];

	return (res_hostalias(&_nres, name, abuf, sizeof abuf));
}

#ifdef ultrix
int
local_hostname_length(const char *hostname) {
	int len_host, len_domain;

	if (!*_nres.defdname)
		res_init();
	len_host = strlen(hostname);
	len_domain = strlen(_nres.defdname);
	if (len_host > len_domain &&
	    !strcasecmp(hostname + len_host - len_domain, _nres.defdname) &&
	    hostname[len_host - len_domain - 1] == '.')
		return (len_host - len_domain - 1);
	return (0);
}
#endif /*ultrix*/

#endif

/*! \file */
