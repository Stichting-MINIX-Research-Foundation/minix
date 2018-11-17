#	$NetBSD: Makefile.inc,v 1.21 2014/01/29 23:37:18 joerg Exp $

SRCS+=	byte_swap_2.S byte_swap_4.S
SRCS+=	ffs.S
SRCS+=	memcmp.S memcpy.S memset.S memmove.S strcmp.S strncmp.S

.if !empty(MACHINE_ARCH:Mearm*)
SRCS+=	unwind_stub.c
.endif
.if empty(MACHINE_ARCH:Mearmv7*)
CPUFLAGS.ffs.S+=	-marm
.endif
CPUFLAGS.divide.S+=	-marm
CPUFLAGS.memcmp.S+=	-marm
CPUFLAGS.memcpy.S+=	-marm
CPUFLAGS.memmove.S+=	-marm
CPUFLAGS.memset.S+=	-marm
.if empty(CPPFLAGS:M-D_STANDALONE)
CPUFLAGS.strcpy.S+=	-marm
CPUFLAGS.strlcpy.S+=	-marm
CPUFLAGS.strncpy.S+=	-marm
.endif
