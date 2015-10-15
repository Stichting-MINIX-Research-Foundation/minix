#	$NetBSD: Makefile.inc,v 1.10 2014/02/19 13:01:51 skrll Exp $

#	$OpenBSD: Makefile.inc,v 1.3 1999/12/25 06:02:10 mickey Exp $

SRCS+=	__sigaction14_sigtramp.c __sigtramp2.S

SRCS+= bcopy.c

CPPFLAGS+= -I.
