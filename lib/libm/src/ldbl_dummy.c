/* $NetBSD: ldbl_dummy.c,v 1.1 2013/11/12 17:36:14 joerg Exp $ */

/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
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

/*
 * Simple long double -> double wrappers for various transcendental functions.
 * They work neither on the additional range of long double nor do they use
 * the additional precision. They exist as stop gap fix for various programs
 * picking up long double, e.g. via the C++ run time.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: ldbl_dummy.c,v 1.1 2013/11/12 17:36:14 joerg Exp $");

#include "namespace.h"
#include <math.h>

__weak_alias(atan2l, _atan2l)
__weak_alias(hypotl, _hypotl)
__weak_alias(logl, _logl)
__weak_alias(log10l, _log10l)
__weak_alias(expl, _expl)
__weak_alias(exp2l, _exp2l)
__weak_alias(powl, _powl)
__weak_alias(cosl, _cosl)
__weak_alias(sinl, _sinl)
__weak_alias(tanl, _tanl)
__weak_alias(coshl, _coshl)
__weak_alias(sinhl, _sinhl)
__weak_alias(tanhl, _tanhl)
__weak_alias(acosl, _acosl)
__weak_alias(asinl, _asinl)
__weak_alias(atanl, _atanl)
__weak_alias(acoshl, _acoshl)
__weak_alias(asinhl, _asinhl)
__weak_alias(atanhl, _atanhl)

long double
atan2l(long double y, long double x)
{
	return atan2(y, x);
}

long double
hypotl(long double x, long double y)
{
	return hypot(x, y);
}

long double
logl(long double x)
{
	return log(x);
}

long double
log10l(long double x)
{
	return log10(x);
}

long double
expl(long double x)
{
	return exp(x);
}

long double
exp2l(long double x)
{
	return exp2(x);
}

long double
powl(long double x, long double y)
{
	return pow(x, y);
}

long double
cosl(long double x)
{
	return cos(x);
}

long double
sinl(long double x)
{
	return sin(x);
}


long double
tanl(long double x)
{
	return tan(x);
}

long double
sinhl(long double x)
{
	return sinh(x);
}

long double
coshl(long double x)
{
	return cosh(x);
}

long double
tanhl(long double x)
{
	return tanh(x);
}

long double
acosl(long double x)
{
	return acos(x);
}

long double
asinl(long double x)
{
	return asin(x);
}

long double
atanl(long double x)
{
	return atan(x);
}

long double
asinhl(long double x)
{
	return asinh(x);
}

long double
acoshl(long double x)
{
	return acosh(x);
}

long double
atanhl(long double x)
{
	return atanh(x);
}
