/*	$NetBSD: compat_msgctl.S,v 1.1 2006/02/18 00:21:28 matt Exp $	*/

#include "SYS.h"

WARN_REFERENCES(msgctl, \
    "warning: reference to compatibility msgctl(); include <sys/msg.h> for correct reference")

PSEUDO(msgctl,compat_14_msgctl)
