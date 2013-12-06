/* $NetBSD: strncpy.S,v 1.3 2013/08/20 21:35:24 matt Exp $ */

#define STRNCPY

#if defined(_STANDALONE)
#if defined(__thumb__)
#include "strcpy_thumb.S"
#else
#include "strcpy_naive.S"
#endif
#else
#include "strcpy_arm.S"
#endif
