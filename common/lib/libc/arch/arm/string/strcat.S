/* $NetBSD: strcat.S,v 1.1 2013/01/23 05:46:17 matt Exp $ */

#if defined(_STANDALONE)
#include "strcat_naive.S"
#else
#include "strcat_arm.S"
#endif
