#	$NetBSD: des.inc,v 1.1 2016/10/17 00:24:13 joerg Exp $

.PATH.S: ${.PARSEDIR}
CPPFLAGS+=	-DDES_INT -DDES_UNROLL
.include "${.CURDIR}/des.inc"
