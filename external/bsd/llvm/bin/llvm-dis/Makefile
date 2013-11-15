#	$NetBSD: Makefile,v 1.3 2013/01/23 15:02:55 joerg Exp $

PROG_CXX=	llvm-dis
NOMAN=		yes

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/tools/llvm-dis

SRCS=	llvm-dis.cpp

LLVM_LIBS+= \
	Analysis \
	BitReader \
	IR \
	Support

.include "${.PARSEDIR}/../../link.mk"

.include <bsd.prog.mk>
