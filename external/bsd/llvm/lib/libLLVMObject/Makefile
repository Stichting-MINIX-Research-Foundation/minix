#	$NetBSD: Makefile,v 1.8 2013/09/02 14:34:56 joerg Exp $

LIB=	LLVMObject
NOGCCERROR=	yes

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/lib/Object

SRCS+=	Archive.cpp \
	Binary.cpp \
	COFFObjectFile.cpp \
	COFFYAML.cpp \
	ELF.cpp \
	ELFObjectFile.cpp \
	ELFYAML.cpp \
	Error.cpp \
	MachOObjectFile.cpp \
	MachOUniversal.cpp \
	Object.cpp \
	ObjectFile.cpp \
	YAML.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
