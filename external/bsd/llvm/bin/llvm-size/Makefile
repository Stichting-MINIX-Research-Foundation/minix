#	$NetBSD: Makefile,v 1.1 2011/10/11 13:53:58 joerg Exp $

PROG_CXX=	llvm-size
NOMAN=		yes

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/tools/llvm-size

SRCS=	llvm-size.cpp

LLVM_LIBS+= \
	Object \
	Support

.include "${.PARSEDIR}/../../link.mk"

.include <bsd.prog.mk>
