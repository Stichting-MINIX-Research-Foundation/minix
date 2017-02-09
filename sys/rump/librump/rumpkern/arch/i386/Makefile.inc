#	$NetBSD: Makefile.inc,v 1.7 2014/02/12 22:28:43 pooka Exp $
#

.include "${RUMPTOP}/librump/rumpkern/arch/x86/Makefile.inc"

.PATH:	${RUMPTOP}/../arch/i386/i386
SRCS+=	kobj_machdep.c
