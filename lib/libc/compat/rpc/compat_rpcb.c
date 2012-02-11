/*	$NetBSD: compat_rpcb.c,v 1.2 2009/01/11 03:41:28 christos Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: compat_rpcb.c,v 1.2 2009/01/11 03:41:28 christos Exp $");
#endif /* LIBC_SCCS and not lint */


#define __LIBC12_SOURCE__

#include "namespace.h"
#include <sys/types.h>
#include <sys/time.h>
#include <compat/sys/time.h>
#include <rpc/rpcb_clnt.h>
#include <compat/include/rpc/rpcb_clnt.h>

__warn_references(rpcb_rmtcall,
    "warning: reference to compatibility rpcb_rmtcall(); include <rpc/rpcb_clnt.h> to generate correct reference")
__warn_references(rpcb_gettime,
    "warning: reference to compatibility rpcb_gettime(); include <rpc/rpcb_clnt.h> to generate correct reference")

#ifdef __weak_alias
__weak_alias(rpcb_rmtcall, _rpcb_rmtcall)
__weak_alias(rpcb_gettime, _rpcb_gettime)
#endif

enum clnt_stat
rpcb_rmtcall(const struct netconfig *nc,
    const char *name, const rpcprog_t prog, const rpcvers_t vers,
    const rpcproc_t proc, const xdrproc_t inproc, const char *inbuf,
    const xdrproc_t outproc, caddr_t outbuf,
    const struct timeval50 tout50, const struct netbuf *nb)
{
	struct timeval tout;
	timeval50_to_timeval(&tout50, &tout);
	return __rpcb_rmtcall50(nc, name, prog, vers, proc, inproc, inbuf,
	    outproc, outbuf, tout, nb);
}

bool_t
rpcb_gettime(const char *name, int32_t *t50)
{
	time_t t;
	bool_t rv = __rpcb_gettime50(name, &t);
	*t50 = (int32_t)t;
	return rv;
}
