/*	$NetBSD: yp_first.c,v 1.16 2012/06/25 22:32:46 abs Exp $	 */

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
__RCSID("$NetBSD: yp_first.c,v 1.16 2012/06/25 22:32:46 abs Exp $");
#endif

#include "namespace.h"
#include <stdlib.h>
#include <string.h>
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include "local.h"

extern struct timeval _yplib_timeout;
extern int _yplib_nerrs;
extern int _yplib_bindtries;

#ifdef __weak_alias
__weak_alias(yp_first,_yp_first)
__weak_alias(yp_next,_yp_next)
#endif

int
yp_first(const char *indomain, const char *inmap, char **outkey,
    int *outkeylen, char **outval, int *outvallen)
{
	struct ypresp_key_val yprkv;
	struct ypreq_nokey yprnk;
	struct dom_binding *ysd;
	int r, nerrs = 0;

	if (outkey == NULL || outkeylen == NULL || \
	    outval == NULL || outvallen == NULL)
		return YPERR_BADARGS;
	*outkey = *outval = NULL;
	*outkeylen = *outvallen = 0;
	if (_yp_invalid_domain(indomain))
		return YPERR_BADARGS;
	if (inmap == NULL || *inmap == '\0'
	    || strlen(inmap) > YPMAXMAP)
		return YPERR_BADARGS;

again:
	if (_yp_dobind(indomain, &ysd) != 0)
		return YPERR_DOMAIN;

	yprnk.domain = indomain;
	yprnk.map = inmap;
	(void)memset(&yprkv, 0, sizeof yprkv);

	r = clnt_call(ysd->dom_client, (rpcproc_t)YPPROC_FIRST,
	    (xdrproc_t)xdr_ypreq_nokey,
	    &yprnk, (xdrproc_t)xdr_ypresp_key_val, &yprkv, _yplib_timeout);
	if (r != RPC_SUCCESS) {
		if (_yplib_bindtries <= 0 && ++nerrs == _yplib_nerrs) {
			clnt_perror(ysd->dom_client, "yp_first: clnt_call");
			nerrs = 0;
		} else if (_yplib_bindtries > 0 && ++nerrs == _yplib_bindtries)
			return YPERR_YPSERV;
		ysd->dom_vers = -1;
		goto again;
	}
	if (!(r = ypprot_err(yprkv.status))) {
		*outkeylen = yprkv.keydat.dsize;
		if ((*outkey = malloc((size_t)(*outkeylen + 1))) == NULL)
			r = YPERR_RESRC;
		else {
			(void)memcpy(*outkey, yprkv.keydat.dptr, 
			    (size_t)*outkeylen);
			(*outkey)[*outkeylen] = '\0';
		}
		*outvallen = yprkv.valdat.dsize;
		if ((*outval = malloc((size_t)(*outvallen + 1))) == NULL)
			r = YPERR_RESRC;
		else {
			(void)memcpy(*outval, yprkv.valdat.dptr,
			    (size_t)*outvallen);
			(*outval)[*outvallen] = '\0';
		}
	}
	xdr_free((xdrproc_t)xdr_ypresp_key_val, (char *)(void *)&yprkv);
	__yp_unbind(ysd);
	if (r != 0) {
		if (*outkey) {
			free(*outkey);
			*outkey = NULL;
		}
		if (*outval) {
			free(*outval);
			*outval = NULL;
		}
	}
	return r;
}

int
yp_next(const char *indomain, const char *inmap, const char *inkey,
    int inkeylen, char **outkey, int *outkeylen, char **outval, int *outvallen)
{
	struct ypresp_key_val yprkv;
	struct ypreq_key yprk;
	struct dom_binding *ysd;
	int r, nerrs = 0;

	if (outkey == NULL || outkeylen == NULL || \
	    outval == NULL || outvallen == NULL || \
	    inkey == NULL)
		return YPERR_BADARGS;
	*outkey = *outval = NULL;
	*outkeylen = *outvallen = 0;

	if (_yp_invalid_domain(indomain))
		return YPERR_BADARGS;
	if (inmap == NULL || *inmap == '\0'
	    || strlen(inmap) > YPMAXMAP)
		return YPERR_BADARGS;

again:
	if (_yp_dobind(indomain, &ysd) != 0)
		return YPERR_DOMAIN;

	yprk.domain = indomain;
	yprk.map = inmap;
	yprk.keydat.dptr = inkey;
	yprk.keydat.dsize = inkeylen;
	(void)memset(&yprkv, 0, sizeof yprkv);

	r = clnt_call(ysd->dom_client, (rpcproc_t)YPPROC_NEXT,
	    (xdrproc_t)xdr_ypreq_key,
	    &yprk, (xdrproc_t)xdr_ypresp_key_val, &yprkv, _yplib_timeout);
	if (r != RPC_SUCCESS) {
		if (_yplib_bindtries <= 0 && ++nerrs == _yplib_nerrs) {
			clnt_perror(ysd->dom_client, "yp_next: clnt_call");
			nerrs = 0;
		} else if (_yplib_bindtries > 0 && ++nerrs == _yplib_bindtries)
			return YPERR_YPSERV;
		ysd->dom_vers = -1;
		goto again;
	}
	if (!(r = ypprot_err(yprkv.status))) {
		*outkeylen = yprkv.keydat.dsize;
		if ((*outkey = malloc((size_t)(*outkeylen + 1))) == NULL)
			r = YPERR_RESRC;
		else {
			(void)memcpy(*outkey, yprkv.keydat.dptr,
			    (size_t)*outkeylen);
			(*outkey)[*outkeylen] = '\0';
		}
		*outvallen = yprkv.valdat.dsize;
		if ((*outval = malloc((size_t)(*outvallen + 1))) == NULL)
			r = YPERR_RESRC;
		else {
			(void)memcpy(*outval, yprkv.valdat.dptr,
			    (size_t)*outvallen);
			(*outval)[*outvallen] = '\0';
		}
	}
	xdr_free((xdrproc_t)xdr_ypresp_key_val, (char *)(void *)&yprkv);
	__yp_unbind(ysd);
	if (r != 0) {
		if (*outkey) {
			free(*outkey);
			*outkey = NULL;
		}
		if (*outval) {
			free(*outval);
			*outval = NULL;
		}
	}
	return r;
}
