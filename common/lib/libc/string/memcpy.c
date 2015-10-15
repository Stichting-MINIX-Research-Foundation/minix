/*	$NetBSD: memcpy.c,v 1.2 2013/12/02 21:21:33 joerg Exp $	*/

#define MEMCOPY
#include "bcopy.c"

#if defined(__ARM_EABI__)
__strong_alias(__aeabi_memcpy, memcpy)
#endif
