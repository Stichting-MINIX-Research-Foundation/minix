# $NetBSD: Makefile.inc,v 1.5 2015/04/22 20:10:56 pooka Exp $

CPPFLAGS+=	-DARCH_ELFSIZE=32

.PATH:  ${RUMPTOP}/../arch/arm/arm32
SRCS+=  	kobj_machdep.c

.PATH:	${RUMPTOP}/librump/rumpkern/arch/generic
SRCS+=  	rump_generic_abi.c rump_generic_cpu.c rump_generic_pmap.c
