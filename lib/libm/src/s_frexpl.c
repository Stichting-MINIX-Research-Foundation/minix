/*	$NetBSD: s_frexpl.c,v 1.3 2013/02/12 21:40:19 martin Exp $	*/

/*-
 * Copyright (c) 2004-2005 David Schultz <das@FreeBSD.ORG>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/lib/msun/src/s_frexpl.c,v 1.1 2005/03/07 04:54:51 das Exp $
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: s_frexpl.c,v 1.3 2013/02/12 21:40:19 martin Exp $");

#include <machine/ieee.h>
#include <float.h>
#include <math.h>

#include "math_private.h"

#ifdef __HAVE_LONG_DOUBLE
#if LDBL_MAX_EXP != 0x4000
#error "Unsupported long double format"
#endif

long double
frexpl(long double x, int *ex)
{
	union ieee_ext_u u;

	u.extu_ld = x;
	switch (u.extu_ext.ext_exp) {
	case 0:		/* 0 or subnormal */
		if ((u.extu_ext.ext_fracl | u.extu_ext.ext_frach) == 0) {
			*ex = 0;
		} else {
			u.extu_ld *= 0x1.0p514;
			*ex = u.extu_ext.ext_exp - 0x4200;
			u.extu_ext.ext_exp = 0x3ffe;
		}
		break;
	case 0x7fff:	/* infinity or NaN; value of *ex is unspecified */
		break;
	default:	/* normal */
		*ex = u.extu_ext.ext_exp - 0x3ffe;
		u.extu_ext.ext_exp = 0x3ffe;
		break;
	}
	return (u.extu_ld);
}
#endif
