/*	$NetBSD: compat_msgctl.S,v 1.1 2006/07/07 06:59:04 ross Exp $	*/

#include "SYS.h"

WARN_REFERENCES(msgctl, \
    "warning: reference to compatibility msgctl(); include <sys/msg.h> for correct reference")

PSEUDO(msgctl,compat_14_msgctl)
