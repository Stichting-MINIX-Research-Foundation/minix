/* $NetBSD: strlen.S,v 1.4 2013/08/20 08:07:30 matt Exp $ */

#include <arm/cdefs.h>

#if defined(_STANDALONE) || (defined(__thumb__) && !defined(_ARM_ARCH_T2))
#include "strlen_naive.S"
#else
#include "strlen_arm.S"
#endif
