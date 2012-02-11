/*	$NetBSD: compat_sigreturn13.S,v 1.1 2006/02/18 00:21:28 matt Exp $	*/

#include "SYS.h"

WARN_REFERENCES(sigreturn, \
    "warning: reference to compatibility sigreturn()")

PSEUDO(sigreturn,compat_13_sigreturn13)
