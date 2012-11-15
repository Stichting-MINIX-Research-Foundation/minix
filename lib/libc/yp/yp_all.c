/*	$NetBSD: yp_all.c,v 1.13 2012/06/25 22:32:46 abs Exp $	 */

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
__RCSID("$NetBSD: yp_all.c,v 1.13 2012/06/25 22:32:46 abs Exp $");
#endif

#include "namespace.h"
#include <string.h>
#include <err.h>
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include "local.h"

extern struct timeval _yplib_timeout;

#ifdef __weak_alias
__weak_alias(yp_all,_yp_all)
#endif

int
yp_all(const char *indomain, const char *inmap,
    struct ypall_callback *incallback)
{
	struct ypreq_nokey yprnk;
	struct dom_binding *ysd;
	struct sockaddr_in clnt_sin;
	enum clnt_stat  status;
	CLIENT         *clnt;
	int             clnt_sock;

	if (_yp_invalid_domain(indomain))
		return YPERR_BADARGS;
	if (inmap == NULL || *inmap == '\0'
	    || strlen(inmap) > YPMAXMAP)
		return YPERR_BADARGS;
	if (incallback == NULL)
		return YPERR_BADARGS;

	if (_yp_dobind(indomain, &ysd) != 0)
		return YPERR_DOMAIN;

	clnt_sock = RPC_ANYSOCK;
	clnt_sin = ysd->dom_server_addr;
	clnt_sin.sin_port = 0;
	clnt = clnttcp_create(&clnt_sin, YPPROG, YPVERS, &clnt_sock, 0, 0);
	if (clnt == NULL) {
		warnx("clnttcp_create failed");
		return YPERR_PMAP;
	}
	yprnk.domain = indomain;
	yprnk.map = inmap;

	status = clnt_call(clnt, (rpcproc_t)YPPROC_ALL,
	    (xdrproc_t)xdr_ypreq_nokey, &yprnk,
	    (xdrproc_t)xdr_ypall, (char *)(void *)incallback, _yplib_timeout);
	clnt_destroy(clnt);

	/* not really needed... */
	__yp_unbind(ysd);

	if (status != RPC_SUCCESS)
		return YPERR_RPC;

	return 0;
}
