/*	$NetBSD: fpsetmask.c,v 1.6 2012/03/17 21:35:06 martin Exp $	*/

/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: fpsetmask.c,v 1.6 2012/03/17 21:35:06 martin Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <sys/types.h>
#include <ieeefp.h>

#ifdef __weak_alias
__weak_alias(fpsetmask,_fpsetmask)
#endif

#ifdef EXCEPTIONS_WITH_SOFTFLOAT
extern fp_except _softfloat_float_exception_mask;
#endif

fp_except
fpsetmask(mask)
	fp_except mask;
{
	fp_except old;
	fp_except new;

	__asm("st %%fsr,%0" : "=m" (*&old));

	new = old;
	new &= ~(0x1f << 23); 
	new |= ((mask & 0x1f) << 23);

	__asm("ld %0,%%fsr" : : "m" (*&new));

	old = ((uint32_t)old >> 23) & 0x1f;

#ifdef EXCEPTIONS_WITH_SOFTFLOAT
	/* update softfloat mask as well */
	old |= _softfloat_float_exception_mask;
	_softfloat_float_exception_mask = mask;
#endif

	return old;
}
