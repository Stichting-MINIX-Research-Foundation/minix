/* $NetBSD: gd_qnan.h,v 1.2 2011/01/17 23:53:03 matt Exp $ */

#include <machine/endian.h>

#define f_QNAN 0x7fa00000
#if BYTE_ORDER == BIG_ENDIAN
#define d_QNAN0 0x7ff40000
#define d_QNAN1 0x0
#define ld_QNAN0 0x7fff8000
#define ld_QNAN1 0x0
#define ld_QNAN2 0x0
#define ld_QNAN3 0x0
#else
#define d_QNAN0 0x0
#define d_QNAN1 0x7ff40000
#define ld_QNAN0 0x0
#define ld_QNAN1 0x0
#define ld_QNAN2 0x0
#define ld_QNAN3 0x7fff8000
#endif
