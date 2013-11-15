#	$NetBSD: Makefile,v 1.1 2013/09/02 14:34:57 joerg Exp $

LIB=	lldReaderWriterPECOFF

.include <bsd.init.mk>

.PATH: ${LLD_SRCDIR}/lib/ReaderWriter/PECOFF

SRCS+=	PECOFFLinkingContext.cpp \
	ReaderCOFF.cpp \
	ReaderImportHeader.cpp \
	WriterPECOFF.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
