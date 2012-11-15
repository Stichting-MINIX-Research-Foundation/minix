#	$NetBSD: Makefile.inc,v 1.2 2011/03/12 07:55:09 matt Exp $

.include <bsd.own.mk>

# Our sources
.PATH:	${.PARSEDIR} ${ARCHDIR}/tls

SRCS+=			tls.c
CPPFLAGS.tls.c+=	-D_LIBC_SOURCE
