#	$NetBSD: des.inc,v 1.1 2009/07/19 23:30:46 christos Exp $

CPPFLAGS+=	-DDES_UNROLL

.include "${.CURDIR}/des.inc"
