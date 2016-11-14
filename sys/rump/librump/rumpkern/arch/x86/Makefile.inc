#	$NetBSD: Makefile.inc,v 1.2 2015/04/22 18:12:39 pooka Exp $
#

.PATH:	${RUMPTOP}/librump/rumpkern/arch/x86

SRCS+=	rump_x86_cpu.c rump_x86_spl.c rump_x86_cpu_counter.c
SRCS+=	rump_x86_spinlock.c rump_x86_pmap.c
SRCS+=	rump_x86_abi.c
