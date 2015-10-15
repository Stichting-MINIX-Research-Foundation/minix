#	$NetBSD: Makefile,v 1.6 2015/04/14 14:08:36 christos Exp $

.include "bsd.own.mk"

PROG=	yacc
SRCS=	closure.c error.c lalr.c lr0.c main.c mkpar.c output.c reader.c \
	symtab.c verbose.c warshall.c graph.c mstring.c yaccpar.c

CPPFLAGS+=	-DMAXTABLE=INT_MAX
CPPFLAGS+=	-DGCC_NORETURN=__dead

COPTS.output.c += -Wno-format-nonliteral
COPTS.reader.c += -Wno-format-nonliteral

.include <bsd.prog.mk>
