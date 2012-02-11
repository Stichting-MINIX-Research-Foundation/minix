/*	$NetBSD: res_state.c,v 1.8 2009/01/11 02:46:29 christos Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
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
__RCSID("$NetBSD: res_state.c,v 1.8 2009/01/11 02:46:29 christos Exp $");
#endif

#include <sys/types.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <resolv.h>

struct __res_state _nres
# if defined(__BIND_RES_TEXT)
	= { .retrans = RES_TIMEOUT, }	/*%< Motorola, et al. */
# endif
	;

res_state __res_get_state_nothread(void);
void __res_put_state_nothread(res_state);

#ifdef __weak_alias
__weak_alias(__res_get_state, __res_get_state_nothread)
__weak_alias(__res_put_state, __res_put_state_nothread)
/* Source compatibility; only for single threaded programs */
__weak_alias(__res_state, __res_get_state_nothread)
#endif

res_state
__res_get_state_nothread(void)
{
	if ((_nres.options & RES_INIT) == 0 && res_ninit(&_nres) == -1) {
		h_errno = NETDB_INTERNAL;
		return NULL;
	}
	return &_nres;
}

void
/*ARGSUSED*/
__res_put_state_nothread(res_state res)
{
}
