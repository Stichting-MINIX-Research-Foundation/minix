/* @(#)e_fmod.c 1.3 95/01/18 */
/*-
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: e_fmodl.c,v 1.2 2013/11/14 15:25:22 martin Exp $");
#if 0
__FBSDID("$FreeBSD: head/lib/msun/src/e_fmodl.c 181063 2008-07-31 20:09:47Z das $");
#endif

#include "namespace.h"

#include <float.h>
#include <stdint.h>

#include "math.h"
#include "math_private.h"
#include <machine/ieee.h>

#ifdef __HAVE_LONG_DOUBLE

#define	BIAS (LDBL_MAX_EXP - 1)

#if EXT_FRACLBITS > 32
typedef	uint64_t manl_t;
#else
typedef	uint32_t manl_t;
#endif

#if EXT_FRACHBITS > 32
typedef	uint64_t manh_t;
#else
typedef	uint32_t manh_t;
#endif

/*
 * These macros add and remove an explicit integer bit in front of the
 * fractional mantissa, if the architecture doesn't have such a bit by
 * default already.
 */
#ifdef LDBL_IMPLICIT_NBIT
#define	SET_NBIT(hx)	((hx) | (1ULL << EXT_FRACHBITS))
#define	HFRAC_BITS	EXT_FRACHBITS
#define	LDBL_NBIT	0
#else
#define	SET_NBIT(hx)	(hx)
#define	HFRAC_BITS	(EXT_FRACHBITS - 1)
#endif

#define	MANL_SHIFT	(EXT_FRACLBITS - 1)

static const long double one = 1.0, Zero[] = {0.0, -0.0,};

/*
 * fmodl(x,y)
 * Return x mod y in exact arithmetic
 * Method: shift and subtract
 *
 * Assumptions:
 * - The low part of the mantissa fits in a manl_t exactly.
 * - The high part of the mantissa fits in an int64_t with enough room
 *   for an explicit integer bit in front of the fractional bits.
 */
long double
__ieee754_fmodl(long double x, long double y)
{
	union ieee_ext_u ux = { .extu_ld = x, };
	union ieee_ext_u uy = { .extu_ld = y, };
	int64_t hx,hz;	/* We need a carry bit even if EXT_FRACHBITS is 32. */
	manh_t hy;
	manl_t lx,ly,lz;
	int ix,iy,n,sx;

	sx = ux.extu_sign;

    /* purge off exception values */
	if((uy.extu_exp|uy.extu_frach|uy.extu_fracl)==0 || /* y=0 */
	   (ux.extu_exp == BIAS + LDBL_MAX_EXP) ||	 /* or x not finite */
	   (uy.extu_exp == BIAS + LDBL_MAX_EXP &&
	    ((uy.extu_frach&~LDBL_NBIT)|uy.extu_fracl)!=0)) /* or y is NaN */
	    return (x*y)/(x*y);
	if(ux.extu_exp<=uy.extu_exp) {
	    if((ux.extu_exp<uy.extu_exp) ||
	       (ux.extu_frach<=uy.extu_frach &&
		(ux.extu_frach<uy.extu_frach ||
		 ux.extu_fracl<uy.extu_fracl))) {
		return x;		/* |x|<|y| return x or x-y */
	    }
	    if(ux.extu_frach==uy.extu_frach && ux.extu_fracl==uy.extu_fracl) {
		return Zero[sx];	/* |x|=|y| return x*0*/
	    }
	}

    /* determine ix = ilogb(x) */
	if(ux.extu_exp == 0) {	/* subnormal x */
	    ux.extu_ld *= 0x1.0p512;
	    ix = ux.extu_exp - (BIAS + 512);
	} else {
	    ix = ux.extu_exp - BIAS;
	}

    /* determine iy = ilogb(y) */
	if(uy.extu_exp == 0) {	/* subnormal y */
	    uy.extu_ld *= 0x1.0p512;
	    iy = uy.extu_exp - (BIAS + 512);
	} else {
	    iy = uy.extu_exp - BIAS;
	}

    /* set up {hx,lx}, {hy,ly} and align y to x */
	hx = SET_NBIT(ux.extu_frach);
	hy = SET_NBIT(uy.extu_frach);
	lx = ux.extu_fracl;
	ly = uy.extu_fracl;

    /* fix point fmod */
	n = ix - iy;

	while(n--) {
	    hz=hx-hy;lz=lx-ly; if(lx<ly) hz -= 1;
	    if(hz<0){hx = hx+hx+(lx>>MANL_SHIFT); lx = lx+lx;}
	    else {
		if ((hz|lz)==0)		/* return sign(x)*0 */
		    return Zero[sx];
		hx = hz+hz+(lz>>MANL_SHIFT); lx = lz+lz;
	    }
	}
	hz=hx-hy;lz=lx-ly; if(lx<ly) hz -= 1;
	if(hz>=0) {hx=hz;lx=lz;}

    /* convert back to floating value and restore the sign */
	if((hx|lx)==0)			/* return sign(x)*0 */
	    return Zero[sx];
	while(hx<(1LL<<HFRAC_BITS)) {	/* normalize x */
	    hx = hx+hx+(lx>>MANL_SHIFT); lx = lx+lx;
	    iy -= 1;
	}
	ux.extu_frach = hx; /* The mantissa is truncated here if needed. */
	ux.extu_fracl = lx;
	if (iy < LDBL_MIN_EXP) {
	    ux.extu_exp = iy + (BIAS + 512);
	    ux.extu_ld *= 0x1p-512;
	} else {
	    ux.extu_exp = iy + BIAS;
	}
	x = ux.extu_ld * one;	/* create necessary signal */
	return x;		/* exact output */
}

#endif /* __HAVE_LONG_DOUBLE */
