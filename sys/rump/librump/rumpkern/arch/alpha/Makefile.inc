#	$NetBSD: Makefile.inc,v 1.4 2015/04/22 20:10:56 pooka Exp $
#

# some stubs
SRCS+=	rumpcrud.c

.PATH:  ${RUMPTOP}/../arch/alpha/alpha
SRCS+=  kobj_machdep.c

.PATH:	${RUMPTOP}/librump/rumpkern/arch/generic
SRCS+=	rump_generic_abi.c rump_generic_cpu.c rump_generic_pmap.c
