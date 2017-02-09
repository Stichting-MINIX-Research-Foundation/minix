# $NetBSD: Makefile.inc,v 1.3 2015/04/22 20:10:56 pooka Exp $

CPPFLAGS+=	-DARCH_ELFSIZE=32

.PATH:	${RUMPTOP}/librump/rumpkern/arch/generic
SRCS+=	rump_generic_abi.c
SRCS+= 	rump_generic_cpu.c rump_generic_kobj.c rump_generic_pmap.c
