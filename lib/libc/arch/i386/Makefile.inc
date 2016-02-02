#	$NetBSD: Makefile.inc,v 1.21 2015/07/15 14:27:49 pooka Exp $

.if ${RUMPRUN} != "yes" && !defined(__MINIX)
SRCS+=	__sigaction14_sigtramp.c __sigtramp2.S
.endif
