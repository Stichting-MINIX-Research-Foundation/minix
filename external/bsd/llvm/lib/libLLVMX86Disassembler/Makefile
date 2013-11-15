#	$NetBSD: Makefile,v 1.3 2013/09/03 14:54:00 joerg Exp $

LIB=	LLVMX86Disassembler

.include <bsd.init.mk>

X86_OBJDIR!=	cd ${.CURDIR}/../libLLVMX86CodeGen && ${PRINTOBJDIR}
CPPFLAGS+=	-I${X86_OBJDIR}

.PATH: ${LLVM_SRCDIR}/lib/Target/X86/Disassembler

SRCS+=	X86Disassembler.cpp \
	X86DisassemblerDecoder.c

.for src in ${SRCS:M*.c}
CPPFLAGS.${src}+=	-std=gnu99
.endfor

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
