/*	$NetBSD: isnanl_ieee754.c,v 1.7 2013/11/21 14:14:13 martin Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Header: isinf.c,v 1.1 91/07/08 19:03:34 torek Exp
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)isinf.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: isnanl_ieee754.c,v 1.7 2013/11/21 14:14:13 martin Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <machine/ieee.h>
#include <math.h>

#ifdef __HAVE_LONG_DOUBLE
/*
 * 7.12.3.4 isnan - test for a NaN
 *          IEEE 754 compatible 128-bit extended-precision version
 */
int
__isnanl(long double x)
{
	union ieee_ext_u u;

	u.extu_ld = x;

	return u.extu_ext.ext_exp == EXT_EXP_INFNAN 
	    && (u.extu_ext.ext_frach != 0
#if EXT_FRACHMBITS
	     || u.extu_ext.ext_frachm != 0
#endif
#if EXT_FRACLMBITS
	     || u.extu_ext.ext_fraclm != 0
#endif
	     || u.extu_ext.ext_fracl != 0);
}
#endif /* __HAVE_LONG_DOUBLE */
