/* $NetBSD: gd_qnan.h,v 1.1 2006/01/25 15:33:28 kleink Exp $ */

#define f_QNAN 0x7fa00000
#define d_QNAN0 0x7ff40000
#define d_QNAN1 0x0
#ifdef _LP64
#define ld_QNAN0 0x7fff4000
#define ld_QNAN1 0x0
#define ld_QNAN2 0x0
#define ld_QNAN3 0x0
#endif
