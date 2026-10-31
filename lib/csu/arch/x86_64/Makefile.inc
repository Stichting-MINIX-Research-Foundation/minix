# $NetBSD: Makefile.inc,v 1.1 2010/08/07 18:01:34 joerg Exp $

CPPFLAGS+=	-DELFSIZE=64

# clang for x86_64 emits __attribute__((constructor)) functions into
# .init_array, but the legacy crtbegin.S walked only .ctors, so constructors
# never ran.  Use the .init_array mechanism (common crtbegin.c, walked by
# crt0-common.c's _init()), matching aarch64/riscv.
CPPFLAGS+=	-I${ARCHDIR}
CPPFLAGS+=	-DHAVE_INITFINI_ARRAY
