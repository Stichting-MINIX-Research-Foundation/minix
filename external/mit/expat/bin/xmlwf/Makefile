#	$NetBSD: Makefile,v 1.1 2012/02/29 20:07:56 tron Exp $

.include <bsd.init.mk>

PROG=	xmlwf

.PATH:	${EXPATSRCDIR}/xmlwf ${EXPATSRCDIR}/doc

SRCS=	codepage.c unixfilemap.c xmlfile.c xmlwf.c
CPPFLAGS+=-DHAVE_UNISTD_H

.include <bsd.prog.mk>
