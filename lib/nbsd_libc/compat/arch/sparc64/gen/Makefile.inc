#	$NetBSD: Makefile.inc,v 1.2 2006/07/03 13:06:12 drochner Exp $

SRCS+=	compat_setjmp.S compat_sigsetjmp.S

# objects built from C sources in compat/gen
SRCS+=	compat_frexp_ieee754.c compat_ldexp_ieee754.c
#SRCS+=	compat_modf_ieee754.c
