/*	$NetBSD: svc_auth_unix.c,v 1.20 2012/06/25 22:32:45 abs Exp $	*/

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

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char *sccsid = "@(#)svc_auth_unix.c 1.28 88/02/08 Copyr 1984 Sun Micro";
static char *sccsid = "@(#)svc_auth_unix.c	2.3 88/08/01 4.0 RPCSRC";
#else
__RCSID("$NetBSD: svc_auth_unix.c,v 1.20 2012/06/25 22:32:45 abs Exp $");
#endif
#endif

/*
 * svc_auth_unix.c
 * Handles UNIX flavor authentication parameters on the service side of rpc.
 * There are two svc auth implementations here: AUTH_UNIX and AUTH_SHORT.
 * _svcauth_unix does full blown unix style uid,gid+gids auth,
 * _svcauth_short uses a shorthand auth to index into a cache of longhand auths.
 * Note: the shorthand has been gutted for efficiency.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#include "namespace.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <rpc/rpc.h>

/*
 * Unix longhand authenticator
 */
enum auth_stat
_svcauth_unix(struct svc_req *rqst, struct rpc_msg *msg)
{
	enum auth_stat stat;
	XDR xdrs;
	struct authunix_parms *aup;
	int32_t *buf;
	struct area {
		struct authunix_parms area_aup;
		char area_machname[MAX_MACHINE_NAME+1];
		int area_gids[NGRPS];
	} *area;
	u_int auth_len;
	size_t str_len, gid_len, i;

	_DIAGASSERT(rqst != NULL);
	_DIAGASSERT(msg != NULL);

	area = (struct area *) rqst->rq_clntcred;
	aup = &area->area_aup;
	aup->aup_machname = area->area_machname;
	aup->aup_gids = area->area_gids;
	auth_len = (u_int)msg->rm_call.cb_cred.oa_length;
	xdrmem_create(&xdrs, msg->rm_call.cb_cred.oa_base, auth_len,XDR_DECODE);
	buf = XDR_INLINE(&xdrs, auth_len);
	if (buf != NULL) {
		aup->aup_time = IXDR_GET_INT32(buf);
		str_len = (size_t)IXDR_GET_U_INT32(buf);
		if (str_len > MAX_MACHINE_NAME) {
			stat = AUTH_BADCRED;
			goto done;
		}
		memmove(aup->aup_machname, buf, str_len);
		aup->aup_machname[str_len] = 0;
		str_len = RNDUP(str_len);
		buf += str_len / sizeof (int32_t);
		aup->aup_uid = (int)IXDR_GET_INT32(buf);
		aup->aup_gid = (int)IXDR_GET_INT32(buf);
		gid_len = (size_t)IXDR_GET_U_INT32(buf);
		if (gid_len > NGRPS) {
			stat = AUTH_BADCRED;
			goto done;
		}
		_DIAGASSERT(__type_fit(u_int, gid_len));
		aup->aup_len = (u_int)gid_len;
		for (i = 0; i < gid_len; i++) {
			aup->aup_gids[i] = (int)IXDR_GET_INT32(buf);
		}
		/*
		 * five is the smallest unix credentials structure -
		 * timestamp, hostname len (0), uid, gid, and gids len (0).
		 */
		if ((5 + gid_len) * BYTES_PER_XDR_UNIT + str_len > auth_len) {
			(void) printf("bad auth_len gid %ld str %ld auth %u\n",
			    (long)gid_len, (long)str_len, auth_len);
			stat = AUTH_BADCRED;
			goto done;
		}
	} else if (! xdr_authunix_parms(&xdrs, aup)) {
		xdrs.x_op = XDR_FREE;
		(void)xdr_authunix_parms(&xdrs, aup);
		stat = AUTH_BADCRED;
		goto done;
	}
	rqst->rq_xprt->xp_verf.oa_flavor = AUTH_NULL;
	rqst->rq_xprt->xp_verf.oa_length = 0;
	stat = AUTH_OK;
done:
	XDR_DESTROY(&xdrs);
	return (stat);
}


/*
 * Shorthand unix authenticator
 * Looks up longhand in a cache.
 */
/*ARGSUSED*/
enum auth_stat 
_svcauth_short(struct svc_req *rqst, struct rpc_msg *msg)
{
	return (AUTH_REJECTEDCRED);
}
