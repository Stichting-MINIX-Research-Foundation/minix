/*	$NetBSD: rump_curlwp___thread.h,v 1.2 2014/03/16 15:30:05 pooka Exp $	*/

/*-
 * Copyright (c) 2014 Antti Kantee.  All Rights Reserved.
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
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

extern __thread struct lwp *curlwp_storage;
#ifdef RUMP__CURLWP_PRIVATE
#include <rump/rumpuser.h>

__thread struct lwp *curlwp_storage;
static void
lwproc_curlwpop(enum rumplwpop op, struct lwp *l)
{

	switch (op) {
	case RUMPUSER_LWP_CREATE:
	case RUMPUSER_LWP_DESTROY:
		break;
	case RUMPUSER_LWP_SET:
		KASSERT(curlwp_storage == NULL);
		curlwp_storage = l;
		break;
	case RUMPUSER_LWP_CLEAR:
		KASSERT(curlwp_storage == l);
		curlwp_storage = NULL;
		break;
	}
	/*
	 * Need to keep hypercall layer in sync, since it can use curlwp
	 * for things like mutexes.  Should be fixed/adjusted somehow.
	 */
	rumpuser_curlwpop(op, l);
}
#endif

static inline struct lwp * __attribute__((const))
rump_curlwp_fast(void)
{

	return curlwp_storage;
}
