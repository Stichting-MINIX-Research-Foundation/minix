#	$NetBSD: Makefile,v 1.5 2020/04/04 01:34:30 christos Exp $

PROG=		objdump

CPPFLAGS+=-DOBJDUMP_PRIVATE_VECTORS=""

PROGDPLIBS+=	gnuctf 	${TOP}/${BFDSUBDIR}/libgnuctf

.include "${.CURDIR}/../common/Makefile.prog"
