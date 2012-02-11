#       $NetBSD: Makefile.inc,v 1.3 2010/07/06 05:59:52 mrg Exp $

.PATH: ${.CURDIR}/dlfcn

CPPFLAGS+=	-I${NETBSDSRCDIR}/libexec/ld.elf_so
CPPFLAGS+=	-I${.CURDIR}/dlfcn
SRCS+=		dlfcn_elf.c
