/* $NetBSD: gd_qnan.h,v 1.1 2006/01/25 15:33:28 kleink Exp $ */

#define f_QNAN 0x7fc00000
#define d_QNAN0 0x7ff80000
#define d_QNAN1 0x0
#ifndef __mc68010__
#define ld_QNAN0 0x7fff0000
#define ld_QNAN1 0x40000000
#define ld_QNAN2 0x0
#endif
