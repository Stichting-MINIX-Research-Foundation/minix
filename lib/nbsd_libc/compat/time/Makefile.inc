#	$NetBSD: Makefile.inc,v 1.2 2009/01/11 03:46:25 christos Exp $

.PATH: ${COMPATDIR}/time
SRCS+= compat_localtime.c compat_difftime.c
