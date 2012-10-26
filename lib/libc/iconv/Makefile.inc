#	$NetBSD: Makefile.inc,v 1.5 2012/01/20 16:31:29 joerg Exp $

.PATH:	${ARCHDIR}/iconv ${.CURDIR}/iconv

SRCS+=	iconv.c
MAN+=	iconv.3
MLINKS+=iconv.3 iconv_open.3 iconv.3 iconv_close.3

CPPFLAGS.iconv.c+=	-I${LIBCDIR}/citrus
