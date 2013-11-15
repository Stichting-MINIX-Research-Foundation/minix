#	$NetBSD: Makefile,v 1.3 2013/07/07 21:36:43 joerg Exp $

PROG_CXX=	llvm-symbolizer
NOMAN=		yes

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/tools/llvm-symbolizer

SRCS=	LLVMSymbolize.cpp \
	llvm-symbolizer.cpp

LLVM_LIBS+= \
	DebugInfo \
	Object \
	Support

LDADD+=	-lz
DPADD+=	${LIBZ}

.include "${.PARSEDIR}/../../link.mk"

.include <bsd.prog.mk>
