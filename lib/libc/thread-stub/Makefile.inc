#	$NetBSD: Makefile.inc,v 1.4 2013/04/05 20:15:42 christos Exp $

.include <bsd.own.mk>

# Our sources
.PATH: ${.CURDIR}/thread-stub

SRCS+=	__isthreaded.c thread-stub.c thread-stub-init.c
