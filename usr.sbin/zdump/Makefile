#	$NetBSD: Makefile,v 1.10 2015/04/28 19:59:14 martin Exp $

.include "${.PARSEDIR}/../zic/Makefile.inc"
.include <bsd.own.mk>

PROG=	zdump
SRCS=	zdump.c
MAN=	zdump.8

TZVERSION!= ${TOOL_SED} -ne 's/VERSION=[ 	]*//p' ${NETBSDSRCDIR}/lib/libc/time/Makefile
CPPFLAGS+=-DPKGVERSION='"(tzcode) "' -DTZVERSION='"${TZVERSION}"'

COPTS.zdump.c += -Wno-format-nonliteral

.PATH: ${NETBSDSRCDIR}/lib/libc/time
.include <bsd.prog.mk>
