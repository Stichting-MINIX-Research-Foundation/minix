#	$NetBSD: Makefile,v 1.4 2013/09/02 14:34:55 joerg Exp $

PROG_CXX=	llvm-ar
NOMAN=		yes

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/tools/llvm-ar

SRCS=	llvm-ar.cpp

LLVM_LIBS+= \
	Object \
	BitReader \
	Support

.include "${.PARSEDIR}/../../link.mk"

.include <bsd.prog.mk>
