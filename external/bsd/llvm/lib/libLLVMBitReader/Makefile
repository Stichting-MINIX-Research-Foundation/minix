#	$NetBSD: Makefile,v 1.2 2013/02/04 15:00:34 joerg Exp $

LIB=	LLVMBitReader

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/lib/Bitcode/Reader

SRCS+=	BitReader.cpp \
	BitcodeReader.cpp \
	BitstreamReader.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
