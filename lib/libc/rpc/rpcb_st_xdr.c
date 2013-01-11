/*	$NetBSD: rpcb_st_xdr.c,v 1.9 2012/06/25 22:32:45 abs Exp $	*/

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
 * Copyright 1991 Sun Microsystems, Inc.
 * rpcb_stat_xdr.c
 */

/*
 * This file was generated from rpcb_prot.x, but includes only those
 * routines used with the rpcbind stats facility.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: rpcb_st_xdr.c,v 1.9 2012/06/25 22:32:45 abs Exp $");
#endif

#include "namespace.h"
#include <rpc/rpc.h>

#include <assert.h>

/* Link list of all the stats about getport and getaddr */

#ifdef __weak_alias
__weak_alias(xdr_rpcbs_addrlist,_xdr_rpcbs_addrlist)
__weak_alias(xdr_rpcbs_rmtcalllist,_xdr_rpcbs_rmtcalllist)
__weak_alias(xdr_rpcbs_proc,_xdr_rpcbs_proc)
__weak_alias(xdr_rpcbs_addrlist_ptr,_xdr_rpcbs_addrlist_ptr)
__weak_alias(xdr_rpcbs_rmtcalllist_ptr,_xdr_rpcbs_rmtcalllist_ptr)
__weak_alias(xdr_rpcb_stat,_xdr_rpcb_stat)
__weak_alias(xdr_rpcb_stat_byvers,_xdr_rpcb_stat_byvers)
#endif

bool_t
xdr_rpcbs_addrlist(XDR *xdrs, rpcbs_addrlist *objp)
{

	_DIAGASSERT(objp != NULL);

	    if (!xdr_u_int32_t(xdrs, &objp->prog)) {
		return (FALSE);
	    }
	    if (!xdr_u_int32_t(xdrs, &objp->vers)) {
		return (FALSE);
	    }
	    if (!xdr_int(xdrs, &objp->success)) {
		return (FALSE);
	    }
	    if (!xdr_int(xdrs, &objp->failure)) {
		return (FALSE);
	    }
	    if (!xdr_string(xdrs, &objp->netid, (u_int)~0)) {
		return (FALSE);
	    }

	    if (!xdr_pointer(xdrs, (char **)(void *)&objp->next,
			(u_int)sizeof (rpcbs_addrlist),
			(xdrproc_t)xdr_rpcbs_addrlist)) {
		return (FALSE);
	    }

	return (TRUE);
}

/* Link list of all the stats about rmtcall */

bool_t
xdr_rpcbs_rmtcalllist(XDR *xdrs, rpcbs_rmtcalllist *objp)
{
	int32_t *buf;

	_DIAGASSERT(xdrs != NULL);
	_DIAGASSERT(objp != NULL);

	if (xdrs->x_op == XDR_ENCODE) {
	buf = XDR_INLINE(xdrs, 6 * BYTES_PER_XDR_UNIT);
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
		if (!xdr_int(xdrs, &objp->success)) {
			return (FALSE);
		}
		if (!xdr_int(xdrs, &objp->failure)) {
			return (FALSE);
		}
		if (!xdr_int(xdrs, &objp->indirect)) {
			return (FALSE);
		}
	} else {
		IXDR_PUT_U_INT32(buf, objp->prog);
		IXDR_PUT_U_INT32(buf, objp->vers);
		IXDR_PUT_U_INT32(buf, objp->proc);
		IXDR_PUT_INT32(buf, objp->success);
		IXDR_PUT_INT32(buf, objp->failure);
		IXDR_PUT_INT32(buf, objp->indirect);
	}
	if (!xdr_string(xdrs, &objp->netid, (u_int)~0)) {
		return (FALSE);
	}
	if (!xdr_pointer(xdrs, (char **)(void *)&objp->next,
			(u_int)sizeof (rpcbs_rmtcalllist),
			(xdrproc_t)xdr_rpcbs_rmtcalllist)) {
		return (FALSE);
	}
	return (TRUE);
	} else if (xdrs->x_op == XDR_DECODE) {
	buf = XDR_INLINE(xdrs, 6 * BYTES_PER_XDR_UNIT);
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
		if (!xdr_int(xdrs, &objp->success)) {
			return (FALSE);
		}
		if (!xdr_int(xdrs, &objp->failure)) {
			return (FALSE);
		}
		if (!xdr_int(xdrs, &objp->indirect)) {
			return (FALSE);
		}
	} else {
		objp->prog = (rpcprog_t)IXDR_GET_U_INT32(buf);
		objp->vers = (rpcvers_t)IXDR_GET_U_INT32(buf);
		objp->proc = (rpcproc_t)IXDR_GET_U_INT32(buf);
		objp->success = (int)IXDR_GET_INT32(buf);
		objp->failure = (int)IXDR_GET_INT32(buf);
		objp->indirect = (int)IXDR_GET_INT32(buf);
	}
	if (!xdr_string(xdrs, &objp->netid, (u_int)~0)) {
		return (FALSE);
	}
	if (!xdr_pointer(xdrs, (char **)(void *)&objp->next,
			(u_int)sizeof (rpcbs_rmtcalllist),
			(xdrproc_t)xdr_rpcbs_rmtcalllist)) {
		return (FALSE);
	}
	return (TRUE);
	}
	if (!xdr_u_int32_t(xdrs, &objp->prog)) {
		return (FALSE);
	}
	if (!xdr_u_int32_t(xdrs, &objp->vers)) {
		return (FALSE);
	}
	if (!xdr_u_int32_t(xdrs, &objp->proc)) {
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->success)) {
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->failure)) {
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->indirect)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->netid, (u_int)~0)) {
		return (FALSE);
	}
	if (!xdr_pointer(xdrs, (char **)(void *)&objp->next,
			(u_int)sizeof (rpcbs_rmtcalllist),
			(xdrproc_t)xdr_rpcbs_rmtcalllist)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_rpcbs_proc(XDR *xdrs, rpcbs_proc objp)
{
	if (!xdr_vector(xdrs, (char *)(void *)objp, RPCBSTAT_HIGHPROC,
	    (u_int)sizeof (int), (xdrproc_t)xdr_int)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_rpcbs_addrlist_ptr(XDR *xdrs, rpcbs_addrlist_ptr *objp)
{
	if (!xdr_pointer(xdrs, (char **)objp, (u_int)sizeof (rpcbs_addrlist),
			(xdrproc_t)xdr_rpcbs_addrlist)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_rpcbs_rmtcalllist_ptr(XDR *xdrs, rpcbs_rmtcalllist_ptr *objp)
{
	if (!xdr_pointer(xdrs, (char **)objp, (u_int)sizeof (rpcbs_rmtcalllist),
			(xdrproc_t)xdr_rpcbs_rmtcalllist)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_rpcb_stat(XDR *xdrs, rpcb_stat *objp)
{

	_DIAGASSERT(objp != NULL);

	if (!xdr_rpcbs_proc(xdrs, objp->info)) {
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->setinfo)) {
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->unsetinfo)) {
		return (FALSE);
	}
	if (!xdr_rpcbs_addrlist_ptr(xdrs, &objp->addrinfo)) {
		return (FALSE);
	}
	return (TRUE);
}

/*
 * One rpcb_stat structure is returned for each version of rpcbind
 * being monitored.
 */
bool_t
xdr_rpcb_stat_byvers(XDR *xdrs, rpcb_stat_byvers objp)
{
	if (!xdr_vector(xdrs, (char *)(void *)objp, RPCBVERS_STAT,
	    (u_int)sizeof (rpcb_stat), (xdrproc_t)xdr_rpcb_stat)) {
		return (FALSE);
	}
	return (TRUE);
}
