#	$NetBSD: Makefile,v 1.1 2013/01/23 15:02:59 joerg Exp $

LIB=	LLVMOption

NOGCCERROR=	yes

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/lib/Option

SRCS+=	ArgList.cpp  \
	Arg.cpp \
	OptTable.cpp \
	Option.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
