/*	$NetBSD: fpsetround.c,v 1.7 2013/10/28 01:06:36 mrg Exp $	*/

/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: fpsetround.c,v 1.7 2013/10/28 01:06:36 mrg Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <sys/types.h>
#include <ieeefp.h>

#ifdef __weak_alias
__weak_alias(fpsetround,_fpsetround)
#endif

fp_rnd
fpsetround(fp_rnd rnd_dir)
{
	fp_rnd old;
	fp_rnd new;
#ifdef SOFTFLOATSPARC64_FOR_GCC
	extern fp_rnd _softfloat_float_rounding_mode;
#endif

	__asm("st %%fsr,%0" : "=m" (*&old));

#ifdef SOFTFLOATSPARC64_FOR_GCC
	_softfloat_float_rounding_mode = rnd_dir;
#endif

	new = old;
	new &= ~(0x03 << 30); 
	new |= ((rnd_dir & 0x03) << 30);

	__asm("ld %0,%%fsr" : : "m" (*&new));

	return ((uint32_t)old >> 30) & 0x03;
}
