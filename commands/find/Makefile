#	$NetBSD: Makefile,v 1.12 2006/12/14 20:55:56 he Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93

.include <bsd.own.mk>

PROG=	find
SRCS=	find.c function.c ls.c main.c misc.c operator.c option.c

.if ${COMPILER_TYPE} != "gnu"
LDADD+=-lminixutil
DPADD+=${LIBMINIXUTIL}
.else
LDADD+=        -lutil
DPADD+=        ${LIBUTIL}
.endif

.include <bsd.prog.mk>
