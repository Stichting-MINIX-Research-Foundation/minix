#	$NetBSD: Makefile.inc,v 1.2 2006/03/11 21:07:18 christos Exp $

.PATH: ${COMPATDIR}/stdlib
CPPFLAGS+=-I${COMPATDIR}/stdlib -I${COMPATDIR}/../stdlib
SRCS+=compat_unsetenv.c 
