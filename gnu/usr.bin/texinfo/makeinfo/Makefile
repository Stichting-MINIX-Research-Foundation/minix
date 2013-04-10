#	$NetBSD: Makefile,v 1.7 2011/05/26 12:56:29 joerg Exp $

PROG=		makeinfo
SRCS=		cmds.c defun.c files.c float.c footnote.c \
		html.c index.c insertion.c lang.c \
		macro.c makeinfo.c multi.c node.c \
		sectioning.c toc.c xml.c xref.c

CPPFLAGS+=	-I${IDIST}/makeinfo

WARNS?=		0	# pointer sign issues

.include <bsd.prog.mk>

.PATH: ${IDIST}/makeinfo ${IDIST}/doc
