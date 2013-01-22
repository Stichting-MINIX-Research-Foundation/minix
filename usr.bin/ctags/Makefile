#	$NetBSD: Makefile,v 1.13 2012/08/10 12:10:27 joerg Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	ctags
CPPFLAGS+=-I${.CURDIR}
SRCS=	C.c ctags.c fortran.c lisp.c print.c tree.c yacc.c

.if !defined(HOSTPROGNAME)
COPTS.ctags.c+=	-Wno-pointer-sign
.endif

.include <bsd.prog.mk>
