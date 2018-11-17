#	$NetBSD: des.inc,v 1.2 2016/10/15 12:19:02 christos Exp $

CPPFLAGS+=	-DDES_UNROLL

.include "${.CURDIR}/des.inc"
