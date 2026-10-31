# $NetBSD: Makefile.inc,v 1.1 2010/08/07 18:01:33 joerg Exp $

CPPFLAGS+=	-DELFSIZE=32

# Same .init_array vs .ctors issue as x86_64: clang's constructors are collected
# into .init_array by the elf_i386_minix linker script, but the legacy
# crtbegin.S walked only .ctors.  Use the .init_array mechanism (common
# crtbegin.c, walked by crt0-common.c's _init()).
CPPFLAGS+=	-I${ARCHDIR}
CPPFLAGS+=	-DHAVE_INITFINI_ARRAY


