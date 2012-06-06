#	$NetBSD: Makefile,v 1.3 2011/08/14 13:29:26 christos Exp $

.include "bsd.own.mk"

PROG=	yacc
SRCS=	closure.c error.c lalr.c lr0.c main.c mkpar.c output.c reader.c \
	skeleton.c symtab.c verbose.c warshall.c graph.c

CPPFLAGS+=	-DGCC_NORETURN=__dead

COPTS.output.c += -Wno-format-nonliteral
COPTS.reader.c += -Wno-format-nonliteral

.include <bsd.prog.mk>
