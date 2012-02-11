# $NetBSD: Makefile.inc,v 1.2 2006/06/28 14:22:15 drochner Exp $

# objects built from assembler sources
SRCS+=  compat_setjmp.S compat__setjmp.S compat_sigsetjmp.S

# objects built from C sources in compat/gen
SRCS+=  compat_frexp_ieee754.c compat_ldexp_ieee754.c compat_modf_ieee754.c
