#	$NetBSD: Makefile,v 1.3 2013/11/11 23:51:36 joerg Exp $

LIB=	lldReaderWriterMachO

.include <bsd.init.mk>

.PATH: ${LLD_SRCDIR}/lib/ReaderWriter/MachO

SRCS+=	MachOLinkingContext.cpp \
	MachONormalizedFileBinaryReader.cpp \
	MachONormalizedFileFromAtoms.cpp \
	MachONormalizedFileBinaryWriter.cpp \
	MachONormalizedFileYAML.cpp \
	ReferenceKinds.cpp \
	WriterMachO.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
