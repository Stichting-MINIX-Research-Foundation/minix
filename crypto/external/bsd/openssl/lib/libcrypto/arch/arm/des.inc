#	$NetBSD: des.inc,v 1.3 2015/03/21 15:56:54 martin Exp $

.PATH.S: ${.PARSEDIR}
CPPFLAGS+=	-DDES_INT -DDES_UNROLL
.include "${.CURDIR}/des.inc"
