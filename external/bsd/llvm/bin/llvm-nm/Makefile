#	$NetBSD: Makefile,v 1.3 2013/07/07 21:36:43 joerg Exp $

PROG_CXX=	llvm-nm
NOMAN=		yes

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/tools/llvm-nm

SRCS=	llvm-nm.cpp

LLVM_LIBS+= \
	BitReader \
	IR \
	Object \
	Support

.include "${.PARSEDIR}/../../link.mk"

.include <bsd.prog.mk>
