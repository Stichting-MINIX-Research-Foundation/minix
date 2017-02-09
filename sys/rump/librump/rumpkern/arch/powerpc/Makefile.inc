# $NetBSD: Makefile.inc,v 1.6 2015/04/22 20:10:56 pooka Exp $

.if ${MACHINE_ARCH} == "powerpc" \
    || (defined(MLIBDIR) && "${MLIBDIR}" == "powerpc")
CPPFLAGS+=	-DARCH_ELFSIZE=32
.else
CPPFLAGS+=	-DARCH_ELFSIZE=64
.endif

.PATH:	${RUMPTOP}/librump/rumpkern/arch/generic
SRCS+=	rump_generic_abi.c
SRCS+=	rump_generic_cpu.c rump_generic_kobj.c rump_generic_pmap.c
