#	$NetBSD: Makefile,v 1.5 2009/11/06 15:14:37 joerg Exp $

HOSTPROGNAME=	${_TOOL_PREFIX}m4
HOST_SRCDIR=	usr.bin/m4
HOST_CPPFLAGS+=	-I.

.include "${.CURDIR}/../Makefile.host"

# Don't build any of those; we don't have lex and yacc available yet
.y.c .y.h .l.c:
	@true

parser.c: ${.CURDIR}/bootstrap/parser.c
	@ln -sf ${.ALLSRC:M*.c} $@
parser.h: ${.CURDIR}/bootstrap/parser.h
	@ln -sf ${.ALLSRC:M*.h} $@
tokenizer.c: ${.CURDIR}/bootstrap/tokenizer.c
	@ln -sf ${.ALLSRC:M*.c} $@
