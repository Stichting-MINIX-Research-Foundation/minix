#	$NetBSD: Makefile,v 1.5 2011/11/02 17:49:20 christos Exp $

HOSTPROGNAME=	${_TOOL_PREFIX}tic
HOST_SRCDIR=	usr.bin/tic
HOST_SRCS=	compile.c hash.c

TERMINFODIR=	${NETBSDSRCDIR}/lib/libterminfo

.include "${.CURDIR}/../Makefile.host"
.include "${TERMINFODIR}/Makefile.hash"

.PATH:		${TERMINFODIR}
