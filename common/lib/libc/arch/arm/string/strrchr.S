/* $NetBSD: strrchr.S,v 1.4 2013/08/20 08:07:30 matt Exp $ */

#include <machine/asm.h>

STRONG_ALIAS(rindex, strrchr)  

#if defined(_STANDALONE) || (defined(__thumb__) && !defined(_ARM_ARCH_T2))
#include "strrchr_naive.S"
#else
#include "strrchr_arm.S"
#endif
