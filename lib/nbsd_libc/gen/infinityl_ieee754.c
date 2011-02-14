/*	$NetBSD: infinityl_ieee754.c,v 1.1 2011/01/17 23:53:03 matt Exp $	*/

/*
 * IEEE-compatible infinityl.c for 64-bit or 128-bit long double format.
 * This is public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: infinityl_ieee754.c,v 1.1 2011/01/17 23:53:03 matt Exp $");
#endif /* LIBC_SCCS and not lint */

#include <math.h>
#include <machine/endian.h>
#include <machine/ieee.h>

#ifdef __HAVE_LONG_DOUBLE
#define	LDBL_EXPBITS		EXT_EXPBITS
#define	LDBL_EXP_INFNAN		EXT_EXP_INFNAN
#else
#define	LDBL_EXPBITS		DBL_EXPBITS
#define	LDBL_EXP_INFNAN		DBL_EXP_INFNAN
#endif

#define	EXP_INFNAN		(LDBL_EXP_INFNAN << (31 - LDBL_EXPBITS))

const union __long_double_u __infinityl = { {
#if BYTE_ORDER == BIG_ENDIAN
	[0] = (EXP_INFNAN >> 24) & 0x7f,
	[1] = (EXP_INFNAN >> 16) & 0xff,
	[2] = (EXP_INFNAN >>  8) & 0xff,
	[3] = (EXP_INFNAN >>  0) & 0xff,
#else
	[sizeof(long double)-4] = (EXP_INFNAN >>  0) & 0xff,
	[sizeof(long double)-3] = (EXP_INFNAN >>  8) & 0xff,
	[sizeof(long double)-2] = (EXP_INFNAN >> 16) & 0xff,
	[sizeof(long double)-1] = (EXP_INFNAN >> 24) & 0x7f,
#endif
} };
