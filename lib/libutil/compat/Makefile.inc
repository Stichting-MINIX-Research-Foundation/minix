#	$NetBSD: Makefile.inc,v 1.2 2009/01/11 02:57:18 christos Exp $

.PATH: ${.CURDIR}/compat

CPPFLAGS+=-I${.CURDIR}/../libc -I${.CURDIR}/../../sys
SRCS+=compat_passwd.c compat_loginx.c compat_login.c compat_parsedate.c \
    compat_login_cap.c
