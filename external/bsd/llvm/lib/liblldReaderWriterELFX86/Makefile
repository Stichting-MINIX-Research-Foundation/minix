#	$NetBSD: Makefile,v 1.2 2015/01/29 20:41:37 joerg Exp $

LIB=	lldReaderWriterELFX86

.include <bsd.init.mk>

.PATH: ${LLD_SRCDIR}/lib/ReaderWriter/ELF/X86

CPPFLAGS+=	-I${LLD_SRCDIR}/lib/ReaderWriter/ELF

SRCS+=	X86LinkingContext.cpp \
	X86RelocationHandler.cpp \
	X86TargetHandler.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
