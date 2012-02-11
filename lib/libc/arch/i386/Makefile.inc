#	$NetBSD: Makefile.inc,v 1.20 2006/06/17 18:04:23 uwe Exp $

.if !defined(__MINIX)
SRCS+=	__sigaction14_sigtramp.c __sigtramp2.S
.endif
