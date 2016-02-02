#	$NetBSD: Makefile,v 1.7 2015/01/29 20:41:37 joerg Exp $

LIB=	lldReaderWriterMachO

.include <bsd.init.mk>

.PATH: ${LLD_SRCDIR}/lib/ReaderWriter/MachO

SRCS+=	ArchHandler_arm64.cpp \
	ArchHandler_x86.cpp \
	ArchHandler_x86_64.cpp \
	ArchHandler_arm.cpp \
	ArchHandler.cpp \
	CompactUnwindPass.cpp \
	GOTPass.cpp \
	MachOLinkingContext.cpp \
	MachONormalizedFileBinaryReader.cpp \
	MachONormalizedFileBinaryWriter.cpp \
	MachONormalizedFileFromAtoms.cpp \
	MachONormalizedFileToAtoms.cpp \
	MachONormalizedFileYAML.cpp \
	ShimPass.cpp \
	StubsPass.cpp \
	WriterMachO.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
