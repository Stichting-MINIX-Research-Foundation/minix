/*	$NetBSD: memcpy.S,v 1.5 2013/08/20 07:25:52 matt Exp $	*/

#include <arm/cdefs.h>

#if !defined(__XSCALE__) || defined(_STANDALONE)
#include "memcpy_arm.S"
#else
#include "memcpy_xscale.S"
#endif
