#	$NetBSD: Makefile.inc,v 1.3 2014/08/23 02:24:22 matt Exp $

KMINCLUDES=
KMSRCS=	

CPPFLAGS+=	-I.

SRCS+=		__sigaction14_sigtramp.c __sigtramp2.S
