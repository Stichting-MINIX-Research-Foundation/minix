#	$NetBSD: asm.mk,v 1.1 2014/12/18 06:24:28 mrg Exp $

# Shared with libmesa.mk / libGL / libglapi

.if ${MACHINE} == "amd64"
CPPFLAGS+=	\
	-DUSE_SSE41 \
	-DUSE_X86_64_ASM
.elif ${MACHINE} == "i386"
CPPFLAGS+=	\
	-DUSE_X86_ASM \
	-DUSE_MMX_ASM \
	-DUSE_3DNOW_ASM \
	-DUSE_SSE_ASM
.elif ${MACHINE} == "sparc" || ${MACHINE} == "sparc64"
.endif
