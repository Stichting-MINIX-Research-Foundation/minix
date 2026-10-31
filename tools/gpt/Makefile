#	$NetBSD: Makefile,v 1.5 2019/05/17 13:51:36 nakayama Exp $

HOSTPROGNAME=	${_TOOL_PREFIX}gpt
HOST_SRCDIR=	sbin/gpt
HOST_CPPFLAGS+=	-I${.CURDIR}

HOST_SRCS+= opendisk.c dehumanize_number.c

.PATH.c: ${.CURDIR}/../../lib/libutil ${.CURDIR}/../../lib/libc/gen

.include "${.CURDIR}/../Makefile.nbincludes"
.include "${.CURDIR}/../Makefile.host"
