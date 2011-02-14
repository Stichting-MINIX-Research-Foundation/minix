/*	$NetBSD: memcpy.S,v 1.1 2005/12/20 19:28:49 christos Exp $	*/

#if !defined(__XSCALE__) || defined(_STANDALONE)
#include "memcpy_arm.S"
#else
#include "memcpy_xscale.S"
#endif
