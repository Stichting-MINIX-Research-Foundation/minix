/*	$NetBSD: yp_match.c,v 1.19 2012/03/20 16:30:26 matt Exp $	 */

/*
 * Copyright (c) 1992, 1993 Theo de Raadt <deraadt@fsa.ca>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: yp_match.c,v 1.19 2012/03/20 16:30:26 matt Exp $");
#endif

#include "namespace.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include "local.h"

#define YPMATCHCACHE

extern struct timeval _yplib_timeout;
extern int _yplib_nerrs;
extern int _yplib_bindtries;
extern char _yp_domain[];

#ifdef __weak_alias
__weak_alias(yp_match,_yp_match)
#endif

#ifdef YPMATCHCACHE
int _yplib_cache = 5;

static struct ypmatch_ent {
	struct ypmatch_ent 	*next;
	char     		*map, *key;
	char           		*val;
	int             	 keylen, vallen;
	time_t          	 expire_t;
} *ypmc;

static bool_t ypmatch_add(const char *, const char *, int, char *, int);
static bool_t ypmatch_find(const char *, const char *, int, const char **,
    int *);

static bool_t
ypmatch_add(const char *map, const char *key, int keylen, char *val, int vallen)
{
	struct ypmatch_ent *ep;
	time_t t;

	_DIAGASSERT(map != NULL);
	_DIAGASSERT(key != NULL);
	_DIAGASSERT(val != NULL);

	(void)time(&t);

	for (ep = ypmc; ep; ep = ep->next)
		if (ep->expire_t < t)
			break;
	if (ep == NULL) {
		if ((ep = malloc(sizeof *ep)) == NULL)
			return 0;
		(void)memset(ep, 0, sizeof *ep);
		if (ypmc)
			ep->next = ypmc;
		ypmc = ep;
	}

	if (ep->key) {
		free(ep->key);
		ep->key = NULL;
	}
	if (ep->val) {
		free(ep->val);
		ep->val = NULL;
	}

	if ((ep->key = malloc((size_t)keylen)) == NULL)
		return 0;

	if ((ep->val = malloc((size_t)vallen)) == NULL) {
		free(ep->key);
		ep->key = NULL;
		return 0;
	}

	ep->keylen = keylen;
	ep->vallen = vallen;

	(void)memcpy(ep->key, key, (size_t)ep->keylen);
	(void)memcpy(ep->val, val, (size_t)ep->vallen);

	if (ep->map) {
		if (strcmp(ep->map, map)) {
			free(ep->map);
			if ((ep->map = strdup(map)) == NULL)
				return 0;
		}
	} else {
		if ((ep->map = strdup(map)) == NULL)
			return 0;
	}

	ep->expire_t = t + _yplib_cache;
	return 1;
}

static bool_t
ypmatch_find(const char *map, const char *key, int keylen, const char **val,
	int *vallen)
{
	struct ypmatch_ent *ep;
	time_t          t;

	_DIAGASSERT(map != NULL);
	_DIAGASSERT(key != NULL);
	_DIAGASSERT(val != NULL);

	if (ypmc == NULL)
		return 0;

	(void) time(&t);

	for (ep = ypmc; ep; ep = ep->next) {
		if (ep->keylen != keylen)
			continue;
		if (strcmp(ep->map, map))
			continue;
		if (memcmp(ep->key, key, (size_t)keylen))
			continue;
		if (t > ep->expire_t)
			continue;

		*val = ep->val;
		*vallen = ep->vallen;
		return 1;
	}
	return 0;
}
#endif

int
yp_match(const char *indomain, const char *inmap, const char *inkey,
	int inkeylen, char **outval, int *outvallen)
{
	struct dom_binding *ysd;
	struct ypresp_val yprv;
	struct ypreq_key yprk;
	int r, nerrs = 0;

	if (outval == NULL || outvallen == NULL)
		return YPERR_BADARGS;
	*outval = NULL;
	*outvallen = 0;

	if (_yp_invalid_domain(indomain))
		return YPERR_BADARGS;
	if (inmap == NULL || *inmap == '\0'
	    || strlen(inmap) > YPMAXMAP)
		return YPERR_BADARGS;
	if (inkey == NULL || inkeylen == 0)
		return YPERR_BADARGS;

again:
	if (_yp_dobind(indomain, &ysd) != 0)
		return YPERR_DOMAIN;

#ifdef YPMATCHCACHE
	if (!strcmp(_yp_domain, indomain) && ypmatch_find(inmap, inkey,
			 inkeylen, &yprv.valdat.dptr, &yprv.valdat.dsize)) {
		*outvallen = yprv.valdat.dsize;
		if ((*outval = malloc((size_t)(*outvallen + 1))) == NULL)
			return YPERR_YPERR;
		(void)memcpy(*outval, yprv.valdat.dptr, (size_t)*outvallen);
		(*outval)[*outvallen] = '\0';
		return 0;
	}
#endif

	yprk.domain = indomain;
	yprk.map = inmap;
	yprk.keydat.dptr = __UNCONST(inkey);
	yprk.keydat.dsize = inkeylen;

	memset(&yprv, 0, sizeof yprv);

	r = clnt_call(ysd->dom_client, (rpcproc_t)YPPROC_MATCH,
		      (xdrproc_t)xdr_ypreq_key, &yprk,
		      (xdrproc_t)xdr_ypresp_val, &yprv, 
		      _yplib_timeout);
	if (r != RPC_SUCCESS) {
		if (_yplib_bindtries <= 0 && ++nerrs == _yplib_nerrs) {
			clnt_perror(ysd->dom_client, "yp_match: clnt_call");
			nerrs = 0;
		}
		else if (_yplib_bindtries > 0 && ++nerrs == _yplib_bindtries) {
			return YPERR_YPSERV;
		}
		ysd->dom_vers = -1;
		goto again;
	}
	if (!(r = ypprot_err(yprv.status))) {
		*outvallen = yprv.valdat.dsize;
		if ((*outval = malloc((size_t)(*outvallen + 1))) == NULL)
			return YPERR_YPERR;
		(void)memcpy(*outval, yprv.valdat.dptr, (size_t)*outvallen);
		(*outval)[*outvallen] = '\0';
#ifdef YPMATCHCACHE
		if (strcmp(_yp_domain, indomain) == 0)
			if (!ypmatch_add(inmap, inkey, inkeylen,
					 *outval, *outvallen))
				r = YPERR_RESRC;
#endif
	}
	xdr_free((xdrproc_t)xdr_ypresp_val, (char *)(void *)&yprv);
	__yp_unbind(ysd);
	if (r != 0) {
		if (*outval) {
			free(*outval);
			*outval = NULL;
		}
	}
	return r;
}
