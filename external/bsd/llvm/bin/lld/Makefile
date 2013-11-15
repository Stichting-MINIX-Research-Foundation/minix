#	$NetBSD: Makefile,v 1.2 2013/10/14 01:37:39 joerg Exp $

PROG_CXX=	lld
NOMAN=		yes

.include <bsd.init.mk>

.PATH: ${LLD_SRCDIR}/tools/lld

SRCS=	lld.cpp

LLD_LIBS+= \
	lldDriver \
	lldReaderWriterELF \
	lldReaderWriterELFHexagon \
	lldReaderWriterELFPPC \
	lldReaderWriterELFX86 \
	lldReaderWriterELFX86_64 \
	lldReaderWriterMachO \
	lldReaderWriterPECOFF \
	lldReaderWriter \
	lldReaderWriterYAML \
	lldPasses \
	lldCore \
	lldReaderWriterNative \

LLVM_LIBS+= \
	Object \
	Option \
	Support

.include "${.PARSEDIR}/../../link.mk"

.include <bsd.prog.mk>
