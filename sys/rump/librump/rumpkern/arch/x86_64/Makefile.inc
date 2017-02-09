#	$NetBSD: Makefile.inc,v 1.8 2014/02/12 22:28:44 pooka Exp $
#

.include "${RUMPTOP}/librump/rumpkern/arch/x86/Makefile.inc"

.PATH:	${RUMPTOP}/../arch/amd64/amd64
SRCS+=	kobj_machdep.c
