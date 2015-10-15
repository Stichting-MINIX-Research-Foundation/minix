#	$NetBSD: Makefile,v 1.4 2015/01/29 20:41:37 joerg Exp $

LIB=	lldReaderWriterPECOFF

.include <bsd.init.mk>

.PATH: ${LLD_SRCDIR}/lib/ReaderWriter/PECOFF

SRCS+=	EdataPass.cpp \
	IdataPass.cpp \
	LinkerGeneratedSymbolFile.cpp \
	LoadConfigPass.cpp \
	PECOFFLinkingContext.cpp \
	Pass.cpp \
	ReaderCOFF.cpp \
	ReaderImportHeader.cpp \
	WriterImportLibrary.cpp \
	WriterPECOFF.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
