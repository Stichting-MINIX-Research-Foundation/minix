#	$NetBSD: Makefile.inc,v 1.3 2003/01/18 11:33:18 thorpej Exp $

.include <bsd.own.mk>

# Our sources
.PATH: ${.CURDIR}/thread-stub

SRCS+=	__isthreaded.c thread-stub.c
