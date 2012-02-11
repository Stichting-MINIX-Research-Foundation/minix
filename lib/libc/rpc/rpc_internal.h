/*	$NetBSD: rpc_internal.h,v 1.6 2009/04/04 15:31:08 christos Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Frank van der Linden.
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
/*
 * Private include file for XDR functions only used internally in libc.
 * These are not exported interfaces.
 */

bool_t __xdrrec_getrec(XDR *, enum xprt_stat *, bool_t);
bool_t __xdrrec_setnonblock(XDR *, int);
void __xprt_unregister_unlocked(SVCXPRT *);
bool_t __svc_clean_idle(fd_set *, int, bool_t);

u_int __rpc_get_a_size(int);
int __rpc_dtbsize(void);
struct netconfig *__rpcgettp(int);
int  __rpc_get_default_domain(char **);

char *__rpc_taddr2uaddr_af(int, const struct netbuf *);
struct netbuf *__rpc_uaddr2taddr_af(int, const char *);
int __rpc_fixup_addr(struct netbuf *, const struct netbuf *);
int __rpc_sockinfo2netid(struct __rpc_sockinfo *, const char **);
int __rpc_seman2socktype(int);
int __rpc_socktype2seman(int);
void *rpc_nullproc(CLIENT *);
int __rpc_sockisbound(int);

struct netbuf *__rpcb_findaddr(rpcprog_t, rpcvers_t, const struct netconfig *,
    const char *, CLIENT **);
bool_t __rpc_control(int, void *);

char *_get_next_token(char *, int);

u_int32_t __rpc_getxid(void);
#define __RPC_GETXID()	(__rpc_getxid())

extern SVCXPRT **__svc_xports;
extern int __svc_maxrec;
