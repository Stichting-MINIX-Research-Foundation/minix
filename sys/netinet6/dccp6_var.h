/*	$KAME: dccp6_var.h,v 1.3 2003/11/18 04:55:43 ono Exp $	*/
/*	$NetBSD: dccp6_var.h,v 1.4 2015/05/02 17:18:03 rtr Exp $ */

/*
 * Copyright (c) 2003 Joacim Häggmark
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Id: dccp6_var.h,v 1.1 2003/07/31 21:34:16 joahag-9 Exp
 */

#ifndef _NETINET_DCCP6_VAR_H_
#define _NETINET_DCCP6_VAR_H_

#ifdef _KERNEL

extern const struct	pr_usrreqs dccp6_usrreqs;
extern struct	in6pcb dccpb6;

void *	dccp6_ctlinput(int, const struct sockaddr *, void *);
int	dccp6_input(struct mbuf **, int *, int);
int	dccp6_usrreq(struct socket *, int, struct mbuf *, struct mbuf *,
		     struct mbuf *, struct lwp *);
int	dccp6_bind(struct socket *, struct sockaddr *, struct lwp *);
int	dccp6_listen(struct socket *, struct lwp *);
int	dccp6_connect(struct socket *, struct sockaddr *, struct lwp *);
int	dccp6_accept(struct socket *, struct sockaddr *);

#endif
#endif
