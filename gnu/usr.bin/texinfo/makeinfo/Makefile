#	$NetBSD: Makefile,v 1.8 2014/03/24 23:03:01 tron Exp $

PROG=		makeinfo
SRCS=		cmds.c defun.c files.c float.c footnote.c \
		html.c index.c insertion.c lang.c \
		macro.c makeinfo.c multi.c node.c \
		sectioning.c toc.c xml.c xref.c

CPPFLAGS+=	-I${IDIST}/makeinfo

WARNS?=		0	# pointer sign issues

.include <bsd.prog.mk>

CPPFLAGS:=	${CPPFLAGS:S/-D_FORTIFY_SOURCE=2//}

.PATH: ${IDIST}/makeinfo ${IDIST}/doc
