#	$NetBSD: Makefile,v 1.2 2011/10/11 13:54:01 joerg Exp $

HOSTPROGNAME=	${_TOOL_PREFIX}llvm-tblgen
HOSTPROG_CXX=	${_TOOL_PREFIX}llvm-tblgen
HOST_SRCDIR=	external/bsd/llvm/bin/tblgen
# MINIX, for OSX Crosscompilation support
CPPFLAGS+=	-D_NLS_PRIVATE=1

.include "${.CURDIR}/../Makefile.host"
