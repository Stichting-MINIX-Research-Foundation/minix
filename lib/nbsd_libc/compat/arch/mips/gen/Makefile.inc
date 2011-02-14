#	$NetBSD: Makefile.inc,v 1.3 2006/07/03 12:58:55 drochner Exp $

# objects built from assembler sources (need lint stubs)
SRCS+=	compat_setjmp.S compat_sigsetjmp.S

# objects built from C sources in compat/gen
SRCS+=	compat_frexp_ieee754.c
#SRCS+=	compat_ldexp_ieee754.c compat_modf_ieee754.c
