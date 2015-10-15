/*	$NetBSD: s_copysignl.c,v 1.5 2015/05/14 19:26:12 joerg Exp $	*/

/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
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
__RCSID("$NetBSD: s_copysignl.c,v 1.5 2015/05/14 19:26:12 joerg Exp $");
#include "namespace.h"

#include <math.h>
#include <machine/ieee.h>

#if defined(__HAVE_LONG_DOUBLE) || defined(__HAVE_IBM_LONGDOUBLE)

#ifdef __weak_alias
__weak_alias(copysignl, _copysignl)
#endif

/*
 * copysignl(long double x, long double y)
 * This function returns a value with the magnitude of x and the sign of y.
 */
#ifdef EXT_EXP_INFNAN
long double
copysignl(long double x, long double y)
{
	union ieee_ext_u ux, uy;

	ux.extu_ld = x;
	uy.extu_ld = y;

	ux.extu_ext.ext_sign = uy.extu_ext.ext_sign;

	return (ux.extu_ld);
}
#elif defined(__HAVE_IBM_LONGDOUBLE)
long double
copysignl(long double x, long double y)
{
	union ldbl_u ux, uy;

	ux.ldblu_ld = x;
	uy.ldblu_ld = y;
	ux.ldblu_d[0] = copysign(ux.ldblu_d[0], uy.ldblu_d[0]);
	ux.ldblu_d[1] = copysign(ux.ldblu_d[1], uy.ldblu_d[1]);

	return ux.ldblu_ld;
}
#endif
#endif /* __HAVE_LONG_DOUBLE || __HAVE_IBM_LONGDOUBLE */
