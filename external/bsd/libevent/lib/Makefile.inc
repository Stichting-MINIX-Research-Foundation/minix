# $NetBSD: Makefile.inc,v 1.2 2013/09/21 22:28:12 joerg Exp $
.include "../Makefile.inc"

CWARNFLAGS.clang+=	-Wno-error=unused-function
