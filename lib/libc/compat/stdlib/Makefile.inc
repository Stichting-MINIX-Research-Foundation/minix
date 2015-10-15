#	$NetBSD: Makefile.inc,v 1.4 2014/06/12 18:44:36 christos Exp $

.PATH: ${COMPATDIR}/stdlib
CPPFLAGS+=-I${COMPATDIR}/stdlib -I${COMPATDIR}/../stdlib
SRCS+=compat_putenv.c compat_random.c compat_unsetenv.c
