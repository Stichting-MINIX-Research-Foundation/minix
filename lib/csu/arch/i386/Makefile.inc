# $NetBSD: Makefile.inc,v 1.1 2010/08/07 18:01:33 joerg Exp $

CPPFLAGS+=	-DELFSIZE=32

# i386 is dual-stack: HAVE_INITFINI_ARRAY makes crt0 walk
# .preinit_array/.init_array/.fini_array (as on earm/arm), and crt0-common.c
# additionally retains the legacy extern _init/_fini calls (crti.S +
# crtbegin.S' .ctor walk) for in-tree-compiler binaries. See crt0-common.c.
CPPFLAGS+=	-DHAVE_INITFINI_ARRAY


