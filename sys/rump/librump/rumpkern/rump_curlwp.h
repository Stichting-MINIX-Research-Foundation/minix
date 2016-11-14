/*	$NetBSD: rump_curlwp.h,v 1.2 2014/03/16 15:30:05 pooka Exp $	*/

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

#ifndef _SYS_RUMP_CURLWP_H_
#define _SYS_RUMP_CURLWP_H_

struct lwp *    rump_lwproc_curlwp_hypercall(void);

/* hattrick numbers to avoid someone accidentally using "1" as the value */
#define RUMP_CURLWP_HYPERCALL     10501
#define RUMP_CURLWP___THREAD      20502
#define RUMP_CURLWP_REGISTER      30503
#define RUMP_CURLWP_DEFAULT       RUMP_CURLWP_HYPERCALL

#ifndef RUMP_CURLWP
#define RUMP_CURLWP RUMP_CURLWP_DEFAULT
#endif

/* provides rump_curlwp_fast() */
#if RUMP_CURLWP == RUMP_CURLWP_HYPERCALL
#include "rump_curlwp_hypercall.h"
#elif RUMP_CURLWP == RUMP_CURLWP___THREAD
#include "rump_curlwp___thread.h"
#elif RUMP_CURLWP == RUMP_CURLWP_REGISTER
#error "RUMP_CURLWP_REGISTER not yet implemented"
#else
#error "unknown RUMP_CURLWP"
#endif

#endif /* _SYS_RUMP_CURLWP_H_ */
