# $NetBSD: Makefile.inc,v 1.2 2015/02/22 17:38:19 riastradh Exp $

.if !defined(RUMPKERNEL)
CFLAGS+=	 -mfloat-abi=soft
.endif
