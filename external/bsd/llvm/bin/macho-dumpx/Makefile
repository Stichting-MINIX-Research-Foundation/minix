#	$NetBSD: Makefile,v 1.2 2013/01/23 15:02:56 joerg Exp $

ROG_CXX=	machdo-dumpx
NOMAN=		yes

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/tools/macho-dump

SRCS=	macho-dump.cpp

LLVM_LIBS+= \
	Object \
	IR \
	Support

.include "${.PARSEDIR}/../../link.mk"

.include <bsd.prog.mk>
