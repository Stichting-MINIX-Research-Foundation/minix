#	$NetBSD: Makefile,v 1.8 2012/08/10 12:10:28 joerg Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	tr
SRCS=	str.c tr.c

.for f in str tr
COPTS.${f}.c+=  -Wno-pointer-sign
.endfor

.include <bsd.prog.mk>
