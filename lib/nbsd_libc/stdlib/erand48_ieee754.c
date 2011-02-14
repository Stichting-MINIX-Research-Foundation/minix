/*	$NetBSD: erand48_ieee754.c,v 1.2 2006/03/31 11:42:31 drochner Exp $	*/

/*
 * Copyright (c) 1993 Martin Birgmeier
 * All rights reserved.
 *
 * You may redistribute unmodified or modified versions of this source
 * code provided that the above copyright notice and this and the
 * following conditions are retained.
 *
 * This software is provided ``as is'', and comes with no warranties
 * of any kind. I shall in no event be liable for anything that happens
 * to anyone/anything when using this software.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: erand48_ieee754.c,v 1.2 2006/03/31 11:42:31 drochner Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <assert.h>
#include <machine/ieee.h>

#include "rand48.h"

#ifdef __weak_alias
__weak_alias(erand48,_erand48)
#endif

double
erand48(unsigned short xseed[3])
{
	union ieee_double_u u;

	_DIAGASSERT(xseed != NULL);

	__dorand48(xseed);
	u.dblu_dbl.dbl_sign = 0;
	u.dblu_dbl.dbl_exp = DBL_EXP_BIAS; /* so we get [1,2) */
	u.dblu_dbl.dbl_frach = ((unsigned int)xseed[2] << 4)
				| ((unsigned int)xseed[1] >> 12);
	u.dblu_dbl.dbl_fracl = (((unsigned int)xseed[1] & 0x0fff) << 20)
				| ((unsigned int)xseed[0] << 4);
	return (u.dblu_d - 1);
}
