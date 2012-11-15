#	$NetBSD: Makefile.inc,v 1.3 2012/04/20 17:31:30 christos Exp $

.PATH: ${COMPATDIR}/stdlib
CPPFLAGS+=-I${COMPATDIR}/stdlib -I${COMPATDIR}/../stdlib
SRCS+=compat_putenv.c compat_unsetenv.c
