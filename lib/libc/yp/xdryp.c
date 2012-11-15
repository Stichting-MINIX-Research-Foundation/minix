/*	$NetBSD: xdryp.c,v 1.32 2012/03/20 16:30:26 matt Exp $	*/

/*
 * Copyright (c) 1996 Jason R. Thorpe <thorpej@NetBSD.org>.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project
 *	by Jason R. Thorpe.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
__RCSID("$NetBSD: xdryp.c,v 1.32 2012/03/20 16:30:26 matt Exp $");
#endif

/*
 * XDR routines used by the YP protocol.  Note that these routines do
 * not strictly conform to the RPC definition in yp.x.  This file
 * replicates the functions exported by the Sun YP API; reality is
 * often inaccurate.
 */

#include "namespace.h"

#include <sys/param.h>
#include <sys/socket.h>

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

#ifdef __weak_alias
__weak_alias(xdr_datum,_xdr_datum)
__weak_alias(xdr_domainname,_xdr_domainname)
__weak_alias(xdr_mapname,_xdr_mapname)
__weak_alias(xdr_peername,_xdr_peername)
__weak_alias(xdr_yp_inaddr,_xdr_yp_inaddr)
__weak_alias(xdr_ypall,_xdr_ypall)
__weak_alias(xdr_ypbind_resp,_xdr_ypbind_resp)
__weak_alias(xdr_ypbind_setdom,_xdr_ypbind_setdom)
__weak_alias(xdr_ypdomain_wrap_string,_xdr_ypdomain_wrap_string)
__weak_alias(xdr_ypmap_parms,_xdr_ypmap_parms)
__weak_alias(xdr_ypmap_wrap_string,_xdr_ypmap_wrap_string)
__weak_alias(xdr_ypmaplist,_xdr_ypmaplist)
__weak_alias(xdr_ypowner_wrap_string,_xdr_ypowner_wrap_string)
__weak_alias(xdr_yppushresp_xfr,_xdr_yppushresp_xfr)
__weak_alias(xdr_ypreq_key,_xdr_ypreq_key)
__weak_alias(xdr_ypreq_nokey,_xdr_ypreq_nokey)
__weak_alias(xdr_ypreq_xfr,_xdr_ypreq_xfr)
__weak_alias(xdr_ypresp_key_val,_xdr_ypresp_key_val)
__weak_alias(xdr_ypresp_maplist,_xdr_ypresp_maplist)
__weak_alias(xdr_ypresp_master,_xdr_ypresp_master)
__weak_alias(xdr_ypresp_order,_xdr_ypresp_order)
__weak_alias(xdr_ypresp_val,_xdr_ypresp_val)
#endif

/*
 * Functions used only within this file.
 */
static	bool_t xdr_ypbind_binding(XDR *, struct ypbind_binding *);
static	bool_t xdr_ypbind_resptype(XDR *, enum ypbind_resptype *);
static	bool_t xdr_ypstat(XDR *, enum ypbind_resptype *);
static	bool_t xdr_ypmaplist_str(XDR *, char *);

__warn_references(xdr_domainname,
    "warning: this program uses xdr_domainname(), which is deprecated and buggy.")

bool_t
xdr_domainname(XDR *xdrs, char *objp)
{

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(objp != NULL);

	return xdr_string(xdrs, &objp, YPMAXDOMAIN);
}

__warn_references(xdr_peername,
    "warning: this program uses xdr_peername(), which is deprecated and buggy.")

bool_t
xdr_peername(XDR *xdrs, char *objp)
{

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(objp != NULL);

	return xdr_string(xdrs, &objp, YPMAXPEER);
}

__warn_references(xdr_mapname,
    "warning: this program uses xdr_mapname(), which is deprecated and buggy.")

bool_t
xdr_mapname(XDR *xdrs, char *objp)
{

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(objp != NULL);

	return xdr_string(xdrs, &objp, YPMAXMAP);
}

bool_t
xdr_ypdomain_wrap_string(XDR *xdrs, char **objp)
{

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(objp != NULL);

	return xdr_string(xdrs, objp, YPMAXDOMAIN);
}

bool_t
xdr_ypmap_wrap_string(XDR *xdrs, char **objp)
{

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(objp != NULL);

	return xdr_string(xdrs, objp, YPMAXMAP);
}

bool_t
xdr_ypowner_wrap_string(XDR *xdrs, char **objp)
{

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(objp != NULL);

	return xdr_string(xdrs, objp, YPMAXPEER);
}

bool_t
xdr_datum(XDR *xdrs, datum *objp)
{

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(objp != NULL);

	return xdr_bytes(xdrs, __UNCONST(&objp->dptr),
	    (u_int *)&objp->dsize, YPMAXRECORD);
}

bool_t
xdr_ypreq_key(XDR *xdrs, struct ypreq_key *objp)
{

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(objp != NULL);

	if (!xdr_ypdomain_wrap_string(xdrs, __UNCONST(&objp->domain)))
		return FALSE;

	if (!xdr_ypmap_wrap_string(xdrs, __UNCONST(&objp->map)))
		return FALSE;

	if (!xdr_datum(xdrs, &objp->keydat))
		return FALSE;

	return TRUE;
}

bool_t
xdr_ypreq_nokey(XDR *xdrs, struct ypreq_nokey *objp)
{

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(objp != NULL);

	if (!xdr_ypdomain_wrap_string(xdrs, __UNCONST(&objp->domain)))
		return FALSE;

	if (!xdr_ypmap_wrap_string(xdrs, __UNCONST(&objp->map)))
		return FALSE;

	return TRUE;
}

bool_t
xdr_yp_inaddr(XDR *xdrs, struct in_addr *objp)
{

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(objp != NULL);

	return xdr_opaque(xdrs, (caddr_t)(void *)&objp->s_addr,
	    (u_int)sizeof objp->s_addr);
}

static bool_t
xdr_ypbind_binding(XDR *xdrs, struct ypbind_binding *objp)
{

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(objp != NULL);

	if (!xdr_yp_inaddr(xdrs, &objp->ypbind_binding_addr))
		return FALSE;

	if (!xdr_opaque(xdrs, (void *)&objp->ypbind_binding_port,
	    (u_int)sizeof objp->ypbind_binding_port))
		return FALSE;

	return TRUE;
}

static bool_t
xdr_ypbind_resptype(XDR *xdrs, enum ypbind_resptype *objp)
{

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(objp != NULL);

	return xdr_enum(xdrs, (enum_t *)(void *)objp);
}

static bool_t
xdr_ypstat(XDR *xdrs, enum ypbind_resptype *objp)
{

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(objp != NULL);

	return xdr_enum(xdrs, (enum_t *)(void *)objp);
}

bool_t
xdr_ypbind_resp(XDR *xdrs, struct ypbind_resp *objp)
{

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(objp != NULL);

	if (!xdr_ypbind_resptype(xdrs, &objp->ypbind_status))
		return FALSE;

	switch (objp->ypbind_status) {
	case YPBIND_FAIL_VAL:
		return xdr_u_int(xdrs,
		    (u_int *)&objp->ypbind_respbody.ypbind_error);

	case YPBIND_SUCC_VAL:
		return xdr_ypbind_binding(xdrs,
		    &objp->ypbind_respbody.ypbind_bindinfo);

	default:
		return FALSE;
	}
	/* NOTREACHED */
}

bool_t
xdr_ypresp_val(XDR *xdrs, struct ypresp_val *objp)
{

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(objp != NULL);

	if (!xdr_ypstat(xdrs, (enum ypbind_resptype *)(void *)&objp->status))
		return FALSE;

	if (!xdr_datum(xdrs, &objp->valdat))
		return FALSE;

	return TRUE;
}

bool_t
xdr_ypbind_setdom(XDR *xdrs, struct ypbind_setdom *objp)
{
	char *cp;

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(objp != NULL);

	cp = objp->ypsetdom_domain;

	if (!xdr_ypdomain_wrap_string(xdrs, &cp))
		return FALSE;

	if (!xdr_ypbind_binding(xdrs, &objp->ypsetdom_binding))
		return FALSE;

	if (!xdr_u_int(xdrs, &objp->ypsetdom_vers))
		return FALSE;

	return TRUE;
}

bool_t
xdr_ypresp_key_val(XDR *xdrs, struct ypresp_key_val *objp)
{

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(objp != NULL);

	if (!xdr_ypstat(xdrs, (enum ypbind_resptype *)(void *)&objp->status))
		return FALSE;

	if (!xdr_datum(xdrs, &objp->valdat))
		return FALSE;

	if (!xdr_datum(xdrs, &objp->keydat))
		return FALSE;

	return TRUE;
}

bool_t
xdr_ypall(XDR *xdrs, struct ypall_callback *incallback)
{
	struct ypresp_key_val out;
	char key[YPMAXRECORD], val[YPMAXRECORD];
	bool_t more, status;

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(incallback != NULL);

	/*
	 * Set up key/val struct to be used during the transaction.
	 */
	memset(&out, 0, sizeof out);
	out.keydat.dptr = key;
	out.keydat.dsize = sizeof(key);
	out.valdat.dptr = val;
	out.valdat.dsize = sizeof(val);

	for (;;) {
		/* Values pending? */
		if (!xdr_bool(xdrs, &more))
			return FALSE;		/* can't tell! */
		if (!more)
			return TRUE;		/* no more */

		/* Transfer key/value pair. */
		status = xdr_ypresp_key_val(xdrs, &out);

		/*
		 * If we succeeded, call the callback function.
		 * The callback will return TRUE when it wants
		 * no more values.  If we fail, indicate the
		 * error.
		 */
		if (status) {
			if ((*incallback->foreach)((int)out.status,
			    __UNCONST(out.keydat.dptr), out.keydat.dsize,
			    __UNCONST(out.valdat.dptr), out.valdat.dsize,
			    incallback->data))
				return TRUE;
		} else
			return FALSE;
	}
}

bool_t
xdr_ypresp_master(XDR *xdrs, struct ypresp_master *objp)
{

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(objp != NULL);

	if (!xdr_ypstat(xdrs, (enum ypbind_resptype *)(void *)&objp->status))
		return FALSE;

	if (!xdr_string(xdrs, &objp->master, YPMAXPEER))
		return FALSE;

	return TRUE;
}

static bool_t
xdr_ypmaplist_str(XDR *xdrs, char *objp)
{

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(objp != NULL);

	return xdr_string(xdrs, &objp, YPMAXMAP+1);
}

bool_t
xdr_ypmaplist(XDR *xdrs, struct ypmaplist *objp)
{

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(objp != NULL);

	if (!xdr_ypmaplist_str(xdrs, objp->ypml_name))
		return FALSE;

	if (!xdr_pointer(xdrs, (char **)(void *)&objp->ypml_next,
	    (u_int)sizeof(struct ypmaplist), (xdrproc_t)xdr_ypmaplist))
		return FALSE;

	return TRUE;
}

bool_t
xdr_ypresp_maplist(XDR *xdrs, struct ypresp_maplist *objp)
{

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(objp != NULL);

	if (!xdr_ypstat(xdrs, (enum ypbind_resptype *)(void *)&objp->status))
		return FALSE;

	if (!xdr_pointer(xdrs, (char **)(void *)&objp->list,
	    (u_int)sizeof(struct ypmaplist), (xdrproc_t)xdr_ypmaplist))
		return FALSE;

	return TRUE;
}

bool_t
xdr_ypresp_order(XDR *xdrs, struct ypresp_order *objp)
{

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(objp != NULL);

	if (!xdr_ypstat(xdrs, (enum ypbind_resptype *)(void *)&objp->status))
		return FALSE;

	if (!xdr_u_int(xdrs, &objp->ordernum))
		return FALSE;

	return TRUE;
}

bool_t
xdr_ypreq_xfr(XDR *xdrs, struct ypreq_xfr *objp)
{

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(objp != NULL);

	if (!xdr_ypmap_parms(xdrs, &objp->map_parms))
		return FALSE;

	if (!xdr_u_int(xdrs, &objp->transid))
		return FALSE;

	if (!xdr_u_int(xdrs, &objp->proto))
		return FALSE;

	if (!xdr_u_int(xdrs, &objp->port))
		return FALSE;

	return TRUE;
}

bool_t
xdr_ypmap_parms(XDR *xdrs, struct ypmap_parms *objp)
{

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(objp != NULL);

	if (!xdr_ypdomain_wrap_string(xdrs, __UNCONST(&objp->domain)))
		return FALSE;

	if (!xdr_ypmap_wrap_string(xdrs, __UNCONST(&objp->map)))
		return FALSE;

	if (!xdr_u_int(xdrs, &objp->ordernum))
		return FALSE;

	if (!xdr_ypowner_wrap_string(xdrs, &objp->owner))
		return FALSE;

	return TRUE;
}

bool_t
xdr_yppushresp_xfr(XDR *xdrs, struct yppushresp_xfr *objp)
{

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(objp != NULL);

	if (!xdr_u_int(xdrs, &objp->transid))
		return FALSE;

	if (!xdr_enum(xdrs, (enum_t *)&objp->status))
		return FALSE;

	return TRUE;
}
