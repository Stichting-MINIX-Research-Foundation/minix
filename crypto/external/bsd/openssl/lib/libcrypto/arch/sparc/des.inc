#	$NetBSD: des.inc,v 1.2 2014/03/02 09:02:43 nakayama Exp $

.if ${MACHINE} == "sparc64"
.PATH.S: ${.PARSEDIR}
DES_SRCS = des_enc-sparc.S fcrypt_b.c
CPPFLAGS+=	-DDES_UNROLL -DDES_PTR
.else
CPPFLAGS+=	-DDES_UNROLL
.endif

.include "${.CURDIR}/des.inc"
