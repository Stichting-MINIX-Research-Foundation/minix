#	$NetBSD: Makefile.inc,v 1.1 2006/03/15 22:07:09 kleink Exp $

# gen sources
.PATH:	${.CURDIR}/gen
CPPFLAGS+=-I${.CURDIR}/gen

SRCS+=	nan.c nanf.c nanl.c

MAN+=	nan.3

MLINKS+=nan.3 nanf.3 \
	nan.3 nanl.3
