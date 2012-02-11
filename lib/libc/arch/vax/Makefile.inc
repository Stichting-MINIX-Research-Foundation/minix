#	$NetBSD: Makefile.inc,v 1.9 2010/07/06 05:59:52 mrg Exp $

SRCS+=	__longjmp14.c
SRCS+=	__sigaction14_sigtramp.c __sigtramp3.S

CPPFLAGS+= -I.
CPPFLAGS.assym.h+=-D__LIBC12_SOURCE__
