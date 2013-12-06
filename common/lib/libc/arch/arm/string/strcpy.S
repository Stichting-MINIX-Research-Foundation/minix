/* $NetBSD: strcpy.S,v 1.5 2013/08/20 21:35:24 matt Exp $ */

#if defined(_STANDALONE)
#ifdef __thumb__
#include "strcpy_thumb.S"
#else
#include "strcpy_naive.S"
#endif
#else
#include "strcpy_arm.S"
#endif
