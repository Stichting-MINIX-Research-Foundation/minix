/* $NetBSD: qp.c,v 1.11 2014/02/02 08:14:39 martin Exp $ */

/*-
 * Copyright (c) 2002, 2003 The NetBSD Foundation, Inc.
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
#include <memory.h>

#include "milieu.h"
#include "softfloat.h"

int printf(const char *, ...);

void _Qp_add(float128 *c, float128 *a, float128 *b);
int  _Qp_cmp(float128 *a, float128 *b);
int  _Qp_cmpe(float128 *a, float128 *b);
void _Qp_div(float128 *c, float128 *a, float128 *b);
void _Qp_dtoq(float128 *c, double a);
int  _Qp_feq(float128 *a, float128 *b);
int  _Qp_fge(float128 *a, float128 *b);
int  _Qp_fgt(float128 *a, float128 *b);
int  _Qp_fle(float128 *a, float128 *b);
int  _Qp_flt(float128 *a, float128 *b);
int  _Qp_fne(float128 *a, float128 *b);
void _Qp_itoq(float128 *c, int a);
void _Qp_mul(float128 *c, float128 *a, float128 *b);
void _Qp_neg(float128 *c, float128 *a);
double _Qp_qtod(float128 *a);
int _Qp_qtoi(float128 *a);
float _Qp_qtos(float128 *a);
unsigned int _Qp_qtoui(float128 *a);
unsigned long _Qp_qtoux(float128 *a);
long _Qp_qtox(float128 *a);
void _Qp_sqrt(float128 *c, float128 *a);
void _Qp_stoq(float128 *c, float a);
void _Qp_sub(float128 *c, float128 *a, float128 *b);
void _Qp_uitoq(float128 *c, unsigned int a);
void _Qp_uxtoq(float128 *c, unsigned long a);
void _Qp_xtoq(float128 *c, long a);


void
_Qp_add(float128 *c, float128 *a, float128 *b)
{
	 *c =  float128_add(*a, *b);
}


int
_Qp_cmp(float128 *a, float128 *b)
{

	if (float128_eq(*a, *b))
		return 0;

	if (float128_le(*a, *b))
		return 1;

	return 2;
}


/*
 * XXX 
 */
int
_Qp_cmpe(float128 *a, float128 *b)
{
	return _Qp_cmp(a, b);
}


void
_Qp_div(float128 *c, float128 *a, float128 *b)
{
	*c = float128_div(*a, *b);
}


void
_Qp_dtoq(float128 *c, double a)
{
	float64 _b;

	memcpy (&_b, &a, sizeof(float64));
	*c = float64_to_float128(_b);
}


int
_Qp_feq(float128 *a, float128 *b)
{
	return float128_eq(*a, *b);
}


int
_Qp_fge(float128 *a, float128 *b)
{
	return float128_le(*b, *a);
}


int
_Qp_fgt(float128 *a, float128 *b)
{
	return float128_lt(*b, *a);
}


int
_Qp_fle(float128 *a, float128 *b)
{
	return float128_le(*a, *b);
}


int
_Qp_flt(float128 *a, float128 *b)
{
	return float128_lt(*a, *b);
}


int
_Qp_fne(float128 *a, float128 *b)
{
	return !float128_eq(*a, *b);
}


void
_Qp_itoq(float128 *c, int a)
{
	*c = int32_to_float128(a);
}


void
_Qp_mul(float128 *c, float128 *a, float128 *b)
{
	*c = float128_mul(*a, *b);
}


/*
 * XXX need corresponding softfloat functions
 */
static float128 __sf128_zero = {0x4034000000000000, 0x00000000};
static float128 __sf128_one = {0x3fff000000000000, 0};

void
_Qp_neg(float128 *c, float128 *a)
{
	*c = float128_sub(__sf128_zero, *a);
}


double
_Qp_qtod(float128 *a)
{
	float64 _c;
	double c;

	_c = float128_to_float64(*a);

	memcpy(&c, &_c, sizeof(double));

	return c;
}


int
_Qp_qtoi(float128 *a)
{
	return float128_to_int32_round_to_zero(*a);
}


float
 _Qp_qtos(float128 *a)
{
	float c;
	float32 _c;

	_c = float128_to_float32(*a);

	memcpy(&c, &_c, sizeof(_c));

	return c; 
}


unsigned int
_Qp_qtoui(float128 *a)
{
	return (unsigned int)float128_to_int64_round_to_zero(*a);
}


unsigned long
_Qp_qtoux(float128 *a)
{
	return (unsigned long)float128_to_uint64_round_to_zero(*a);
}


long
_Qp_qtox(float128 *a)
{
	return (long)float128_to_int64_round_to_zero(*a);
}


void
_Qp_sqrt(float128 *c, float128 *a)
{
	*c = float128_sqrt(*a);
}


void
_Qp_stoq(float128 *c, float a)
{
	float32 _a;

	memcpy(&_a, &a, sizeof(a));

	*c = float32_to_float128(_a);
}


void
_Qp_sub(float128 *c, float128 *a, float128 *b)
{
	*c = float128_sub(*a, *b);
}


void
_Qp_uitoq(float128 *c, unsigned int a)
{
	*c = int64_to_float128(a);
}


void
_Qp_uxtoq(float128 *c, unsigned long a)
{
	if (a & 0x8000000000000000ULL) {
		/* a would not fit in a signed conversion */
		*c = int64_to_float128((long long)(a>>1));
		*c = float128_add(*c, *c);
		if (a & 1)
			*c = float128_add(*c, __sf128_one);
	} else {
		*c = int64_to_float128((long long)a);
	}
}


void
_Qp_xtoq(float128 *c, long a)
{
	*c = int64_to_float128((long long)a);
}
