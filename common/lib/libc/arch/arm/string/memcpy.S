/*	$NetBSD: memcpy.S,v 1.8 2015/06/07 11:41:59 joerg Exp $	*/

#include <arm/cdefs.h>

#if !defined(__XSCALE__) || defined(_STANDALONE)
#include "memcpy_arm.S"
#else
#include "memcpy_xscale.S"
#endif

#if defined(__ARM_EABI__) && !defined(_RUMPKERNEL)
STRONG_ALIAS(__aeabi_memcpy, memcpy)
STRONG_ALIAS(__aeabi_memcpy4, memcpy)
STRONG_ALIAS(__aeabi_memcpy8, memcpy)
#endif
