/*	$NetBSD: compat_sigaction13.S,v 1.1 2006/02/18 00:21:28 matt Exp $	*/

#include "SYS.h"

WARN_REFERENCES(sigaction, \
    "warning: reference to compatibility sigaction(); include <signal.h> for correct reference")

PSEUDO(sigaction,compat_13_sigaction13)
