#	$NetBSD: Makefile,v 1.2 2013/02/04 15:00:32 joerg Exp $

PROG_CXX=	llvm-bcanalyzer
NOMAN=		yes

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/tools/llvm-bcanalyzer

SRCS=	llvm-bcanalyzer.cpp

LLVM_LIBS+= \
	BitReader \
	Support

.include "${.PARSEDIR}/../../link.mk"

.include <bsd.prog.mk>
