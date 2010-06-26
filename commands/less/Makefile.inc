#	$NetBSD: Makefile.inc,v 1.3 2001/07/26 14:20:47 mrg Exp $

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif

CWARNFLAGS+=	-Wno-strict-prototypes -Wno-missing-prototypes
