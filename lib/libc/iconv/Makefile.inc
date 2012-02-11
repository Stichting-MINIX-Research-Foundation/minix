#	$NetBSD: Makefile.inc,v 1.4 2009/01/11 02:46:28 christos Exp $

.PATH:	${ARCHDIR}/iconv ${.CURDIR}/iconv

SRCS+=	iconv.c
MAN+=	iconv.3
MLINKS+=iconv.3 iconv_open.3 iconv.3 iconv_close.3

.if ${CITRUS} == "yes"
CPPFLAGS.iconv.c+=	-DHAVE_CITRUS -I${LIBCDIR}/citrus
.else
CPPFLAGS.iconv.c+=	-UHAVE_CITRUS
.endif
