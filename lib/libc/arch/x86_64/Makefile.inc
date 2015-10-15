#	$NetBSD: Makefile.inc,v 1.6 2015/07/15 14:27:49 pooka Exp $

.if ${RUMPRUN} != "yes"
SRCS+=	__sigaction14_sigtramp.c __sigtramp2.S
.endif
