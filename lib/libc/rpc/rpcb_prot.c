/*	$NetBSD: rpcb_prot.c,v 1.10 2012/06/25 22:32:45 abs Exp $	*/

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */
/*
 * Copyright (c) 1986-1991 by Sun Microsystems Inc. 
 */

/* #ident	"@(#)rpcb_prot.c	1.13	94/04/24 SMI" */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)rpcb_prot.c 1.9 89/04/21 Copyr 1984 Sun Micro";
#else
__RCSID("$NetBSD: rpcb_prot.c,v 1.10 2012/06/25 22:32:45 abs Exp $");
#endif
#endif

/*
 * rpcb_prot.c
 * XDR routines for the rpcbinder version 3.
 *
 * Copyright (C) 1984, 1988, Sun Microsystems, Inc.
 */

#include "namespace.h"

#include <rpc/rpc.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/rpcb_prot.h>

#include <assert.h>

#ifdef __weak_alias
__weak_alias(xdr_rpcb,_xdr_rpcb)
__weak_alias(xdr_rpcblist_ptr,_xdr_rpcblist_ptr)
__weak_alias(xdr_rpcblist,_xdr_rpcblist)
__weak_alias(xdr_rpcb_entry,_xdr_rpcb_entry)
__weak_alias(xdr_rpcb_entry_list_ptr,_xdr_rpcb_entry_list_ptr)
__weak_alias(xdr_rpcb_rmtcallargs,_xdr_rpcb_rmtcallargs)
__weak_alias(xdr_rpcb_rmtcallres,_xdr_rpcb_rmtcallres)
__weak_alias(xdr_netbuf,_xdr_netbuf)
#endif


bool_t
xdr_rpcb(XDR *xdrs, RPCB *objp)
{

	_DIAGASSERT(objp != NULL);

	if (!xdr_u_int32_t(xdrs, &objp->r_prog)) {
		return (FALSE);
	}
	if (!xdr_u_int32_t(xdrs, &objp->r_vers)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->r_netid, (u_int)~0)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->r_addr, (u_int)~0)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->r_owner, (u_int)~0)) {
		return (FALSE);
	}
	return (TRUE);
}

/*
 * rpcblist_ptr implements a linked list.  The RPCL definition from
 * rpcb_prot.x is:
 *
 * struct rpcblist {
 * 	rpcb		rpcb_map;
 *	struct rpcblist *rpcb_next;
 * };
 * typedef rpcblist *rpcblist_ptr;
 *
 * Recall that "pointers" in XDR are encoded as a boolean, indicating whether
 * there's any data behind the pointer, followed by the data (if any exists).
 * The boolean can be interpreted as ``more data follows me''; if FALSE then
 * nothing follows the boolean; if TRUE then the boolean is followed by an
 * actual struct rpcb, and another rpcblist_ptr (declared in RPCL as "struct
 * rpcblist *").
 *
 * This could be implemented via the xdr_pointer type, though this would
 * result in one recursive call per element in the list.  Rather than do that
 * we can ``unwind'' the recursion into a while loop and use xdr_reference to
 * serialize the rpcb elements.
 */

bool_t
xdr_rpcblist_ptr(XDR *xdrs, rpcblist_ptr *rp)
{
	/*
	 * more_elements is pre-computed in case the direction is
	 * XDR_ENCODE or XDR_FREE.  more_elements is overwritten by
	 * xdr_bool when the direction is XDR_DECODE.
	 */
	bool_t more_elements;
	int freeing;
	rpcblist_ptr next;
	rpcblist_ptr next_copy;

	_DIAGASSERT(xdrs != NULL);
	/* XXX: rp may be NULL ??? */

	freeing = (xdrs->x_op == XDR_FREE);
	next = NULL;

	for (;;) {
		more_elements = (bool_t)(*rp != NULL);
		if (! xdr_bool(xdrs, &more_elements)) {
			return (FALSE);
		}
		if (! more_elements) {
			return (TRUE);  /* we are done */
		}
		/*
		 * the unfortunate side effect of non-recursion is that in
		 * the case of freeing we must remember the next object
		 * before we free the current object ...
		 */
		if (freeing && *rp)
			next = (*rp)->rpcb_next;
		if (! xdr_reference(xdrs, (caddr_t *)rp,
		    (u_int)sizeof (rpcblist), (xdrproc_t)xdr_rpcb)) {
			return (FALSE);
		}
		if (freeing) {
			next_copy = next;
			rp = &next_copy;
			/*
			 * Note that in the subsequent iteration, next_copy
			 * gets nulled out by the xdr_reference
			 * but next itself survives.
			 */
		} else if (*rp) {
			rp = &((*rp)->rpcb_next);
		}
	}
	/*NOTREACHED*/
}

/*
 * xdr_rpcblist() is specified to take a RPCBLIST **, but is identical in
 * functionality to xdr_rpcblist_ptr().
 */
bool_t
xdr_rpcblist(XDR *xdrs, RPCBLIST **rp)
{
	bool_t	dummy;

	dummy = xdr_rpcblist_ptr(xdrs, (rpcblist_ptr *)rp);
	return (dummy);
}


bool_t
xdr_rpcb_entry(XDR *xdrs, rpcb_entry *objp)
{

	_DIAGASSERT(objp != NULL);

	if (!xdr_string(xdrs, &objp->r_maddr, (u_int)~0)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->r_nc_netid, (u_int)~0)) {
		return (FALSE);
	}
	if (!xdr_u_int32_t(xdrs, &objp->r_nc_semantics)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->r_nc_protofmly, (u_int)~0)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->r_nc_proto, (u_int)~0)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_rpcb_entry_list_ptr(XDR *xdrs, rpcb_entry_list_ptr *rp)
{
	/*
	 * more_elements is pre-computed in case the direction is
	 * XDR_ENCODE or XDR_FREE.  more_elements is overwritten by
	 * xdr_bool when the direction is XDR_DECODE.
	 */
	bool_t more_elements;
	int freeing;
	rpcb_entry_list_ptr next;
	rpcb_entry_list_ptr next_copy;

	_DIAGASSERT(xdrs != NULL);
	/* XXX: rp is allowed to be NULL ??? */

	freeing = (xdrs->x_op == XDR_FREE);
	next = NULL;

	for (;;) {
		more_elements = (bool_t)(*rp != NULL);
		if (! xdr_bool(xdrs, &more_elements)) {
			return (FALSE);
		}
		if (! more_elements) {
			return (TRUE);  /* we are done */
		}
		/*
		 * the unfortunate side effect of non-recursion is that in
		 * the case of freeing we must remember the next object
		 * before we free the current object ...
		 */
		if (freeing && *rp)
			next = (*rp)->rpcb_entry_next;
		if (! xdr_reference(xdrs, (caddr_t *)rp,
		    (u_int)sizeof (rpcb_entry_list),
				    (xdrproc_t)xdr_rpcb_entry)) {
			return (FALSE);
		}
		if (freeing) {
			next_copy = next;
			rp = &next_copy;
			/*
			 * Note that in the subsequent iteration, next_copy
			 * gets nulled out by the xdr_reference
			 * but next itself survives.
			 */
		} else if (*rp) {
			rp = &((*rp)->rpcb_entry_next);
		}
	}
	/*NOTREACHED*/
}

/*
 * XDR remote call arguments
 * written for XDR_ENCODE direction only
 */
bool_t
xdr_rpcb_rmtcallargs(XDR *xdrs, struct rpcb_rmtcallargs *p)
{
	struct r_rpcb_rmtcallargs *objp =
	    (struct r_rpcb_rmtcallargs *)(void *)p;
	u_int lenposition, argposition, position;
	int32_t *buf;

	_DIAGASSERT(p != NULL);

	buf = XDR_INLINE(xdrs, 3 * BYTES_PER_XDR_UNIT);
	if (buf == NULL) {
		if (!xdr_u_int32_t(xdrs, &objp->prog)) {
			return (FALSE);
		}
		if (!xdr_u_int32_t(xdrs, &objp->vers)) {
			return (FALSE);
		}
		if (!xdr_u_int32_t(xdrs, &objp->proc)) {
			return (FALSE);
		}
	} else {
		IXDR_PUT_U_INT32(buf, objp->prog);
		IXDR_PUT_U_INT32(buf, objp->vers);
		IXDR_PUT_U_INT32(buf, objp->proc);
	}

	/*
	 * All the jugglery for just getting the size of the arguments
	 */
	lenposition = XDR_GETPOS(xdrs);
	if (! xdr_u_int(xdrs, &(objp->args.args_len))) {
		return (FALSE);
	}
	argposition = XDR_GETPOS(xdrs);
	if (! (*objp->xdr_args)(xdrs, objp->args.args_val)) {
		return (FALSE);
	}
	position = XDR_GETPOS(xdrs);
	objp->args.args_len = (u_int)((u_long)position - (u_long)argposition);
	XDR_SETPOS(xdrs, lenposition);
	if (! xdr_u_int(xdrs, &(objp->args.args_len))) {
		return (FALSE);
	}
	XDR_SETPOS(xdrs, position);
	return (TRUE);
}

/*
 * XDR remote call results
 * written for XDR_DECODE direction only
 */
bool_t
xdr_rpcb_rmtcallres(XDR *xdrs, struct rpcb_rmtcallres *p)
{
	bool_t dummy;
	struct r_rpcb_rmtcallres *objp = (struct r_rpcb_rmtcallres *)(void *)p;

	_DIAGASSERT(p != NULL);

	if (!xdr_string(xdrs, &objp->addr, (u_int)~0)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->results.results_len)) {
		return (FALSE);
	}
	dummy = (*(objp->xdr_res))(xdrs, objp->results.results_val);
	return (dummy);
}

bool_t
xdr_netbuf(XDR *xdrs, struct netbuf *objp)
{
	bool_t dummy;

	_DIAGASSERT(objp != NULL);

	if (!xdr_u_int32_t(xdrs, (u_int32_t *) &objp->maxlen)) {
		return (FALSE);
	}
	dummy = xdr_bytes(xdrs, (char **)(void *)&(objp->buf),
			(u_int *)&(objp->len), objp->maxlen);
	return (dummy);
}
