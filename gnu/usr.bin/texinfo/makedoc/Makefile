#	$NetBSD: Makefile,v 1.4 2008/09/09 06:31:33 lukem Exp $

NOMAN=
MAKEDOC=

.include "../Makefile.inc"

HOSTPROG=	makedoc
SRCS=		makedoc.c xexit.c xmalloc.c xstrdup.c

.PATH: ${IDIST}/info ${IDIST}/lib
HOST_CPPFLAGS+= -I${IDIST}/info -I${IDIST}/lib -I${.CURDIR}/../common
HOST_CPPFLAGS+= -DHOSTTOOL

.include <bsd.hostprog.mk>
